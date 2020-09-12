// 2019 Team AobaZero
// This source code is in the public domain.
#include "child.hpp"
#include "err.hpp"
#include "param.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <cstdio>
#include <cstring>
using std::condition_variable;
using std::copy_n;
using std::fill_n;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::unique_lock;
using std::chrono::milliseconds;
using ErrAux::die;
using uchar = unsigned char;
using uint  = unsigned int;

struct Status {
  mutex m;
  condition_variable cv;
  uint num_child = 0;
  uint num_ready = 0;
  bool is_used[Param::maxnum_child];
  bool is_ready[Param::maxnum_child * 2U];
  bool is_ready_unlock[Param::maxnum_child * 2U];
  Status() {
    num_child = 0;
    num_ready = 0;
    fill_n(is_used,  Param::maxnum_child, false);
    fill_n(is_ready, Param::maxnum_child * 2U, false); }
};

static Status status;

void Reader::worker() noexcept {
  uint len_buf = 0;
  bool eof = false;
  char ch_last = '\0';

  while (true) {
    char buf[BUFSIZ];

    while (len_buf == 0) {
      
      if (eof) {
	{
	  lock_guard<mutex> lock(status.m);
	  if (status.is_ready[_index]
	      || status.num_ready == Param::maxnum_child)
	    die(ERR_INT("Internal Error"));

	  _is_eof   = true;
	  _has_line = false;
	  _do_read  = false;
	  status.num_ready += 1U;
	  status.is_ready[_index] = true;
	}
	_cv.notify_one();
	status.cv.notify_one();
	return; }
      
      uint ret = _rh(buf, sizeof(buf));
      if (ret == 0) {
	eof = true;
	if (_len_line) { buf[0] = '\n'; ret = 1U; } }
      
      OSI::binary2text(buf, ret, ch_last);
      len_buf = ret; }
    
    uint pos = 0;
    for (; pos < len_buf && buf[pos] != '\n'; ++pos) {
      if (sizeof(_line) <= _len_line + 1U) die(ERR_INT("buffer overrun"));
      _line[_len_line++] = buf[pos]; }
    
    if (pos == len_buf) { len_buf = 0; continue; }
    
    assert(_len_line < sizeof(_line));
    pos     += 1U;
    len_buf -= pos;
    memmove(buf, buf + pos, len_buf);
    _line[_len_line] = '\0';
    buf[len_buf]     = '\0';

    {    
      unique_lock<mutex> lock(status.m);
      if (status.is_ready[_index] || status.num_ready == Param::maxnum_child)
	die(ERR_INT("Internal Error"));
      _is_eof   = false;
      _has_line = true;
      _do_read  = false;
      status.num_ready += 1U;
      status.is_ready[_index] = true;
    }

    _cv.notify_one();
    status.cv.notify_one();
    
    {    
      unique_lock<mutex> lock(status.m);
      _cv.wait(lock, [this]{ return this->_do_read; });
      if (status.is_ready[_index]) die(ERR_INT("Internal Error")); }

    _len_line = 0; } }

void Reader::reset(OSI::ReadHandle &&rh, int index) noexcept {
  assert(!_rh.ok() && rh.ok());
  if (0 <= _index) die(ERR_INT("Internal Error"));
  _line[0]  = '\0';
  _len_line = 0;
  _index    = index;
  _rh       = std::move(rh);
  _do_read  = true;
  _is_eof   = _has_line = false;
  _t = thread(&Reader::worker, this); }

void Reader::end() noexcept {
  if (_index < 0) die(ERR_INT("Internal Error"));
  _t.join();
  _rh.clear();
  _index    = -1;
  _has_line = false;
  _is_eof   = true; }

bool Reader::ok() const noexcept {
  if (_index < 0) return false;
  if (!_rh.ok()) return false;

  lock_guard<mutex> lock(status.m);
  if (_is_eof && _has_line) return false;
  return true; }

uint Reader::getline(char *line, uint size) noexcept {
  assert(line && 0 < size);
  if (_index < 0) die(ERR_INT("Internal Error"));

  unique_lock<mutex> lock(status.m);
  _cv.wait(lock, [this]{ return (this->_is_eof || this->_has_line); });
  if (! status.is_ready[_index]) die(ERR_INT("Internal Error"));
  if (status.num_ready == 0)     die(ERR_INT("Internal Error"));
  if (_is_eof) return 0;

  uint ret;
  if (_len_line < size) {
    ret = _len_line + 1U;
    copy_n(_line, ret, line); }
  else {
    ret = size;
    copy_n(_line, size - 1U, line);
    line[size - 1U] = '\0'; }

  status.is_ready[_index] = false;
  status.num_ready -= 1U;
  _has_line = false;
  _do_read  = true;
  lock.unlock();
  _cv.notify_one();
  return ret; }

uint Child::wait(uint timeout) noexcept {
  unique_lock<mutex> lock(status.m);
  status.cv.wait_for(lock, milliseconds(timeout),
		     []{ return status.num_ready; });
  copy_n(status.is_ready, Param::maxnum_child * 2, status.is_ready_unlock);
  return status.num_ready; }

bool Child::has_line_in() const noexcept {
  if (_index < 0) return false;
  return status.is_ready_unlock[_index * 2]; }

bool Child::has_line_err() const noexcept {
  if (_index < 0) return false;
  return status.is_ready_unlock[_index * 2 + 1]; }

bool Child::ok() const noexcept {
  if (is_closed()) return _index < 0;
  return 0 <= _index && _cp.ok() && _in.ok() && _err.ok(); }

void Child::open(const char *path, char * const argv[]) noexcept {
  assert(path && argv[0]);
  if (0 <= _index) die(ERR_INT("Cannot open the child process twice."));

  {
    lock_guard<mutex> lock(status.m);
    if (status.num_child == Param::maxnum_child)
      die(ERR_INT("Internal Error"));
  
    for (_index = 0; _index < static_cast<int>(Param::maxnum_child); ++_index)
      if (! status.is_used[_index]) break;
    if (_index == static_cast<int>(Param::maxnum_child))
      die(ERR_INT("Internal Error"));
  
    status.num_child += 1U;
    status.is_used[_index]          = true;
    status.is_ready[_index * 2]     = false;
    status.is_ready[_index * 2 + 1] = false; }

  _cp.open(path, argv);
  _in.reset(_cp.gen_handle_in(), _index * 2);
  _err.reset(_cp.gen_handle_err(), _index * 2 + 1); }

uint Child::getline_in(char *line, uint size) noexcept {
  assert(line && 0 < size); return _in.getline(line, size); }

uint Child::getline_err(char *line, uint size) noexcept {
  assert(line && 0 < size); return _err.getline(line, size); }

void Child::close() noexcept {
  if (_index < 0) die(ERR_INT("Cannot close the child process twice."));

  _in.end();
  _err.end();
  _cp.close();
  {
    lock_guard<mutex> lock(status.m);
    if (status.num_child == 0) die(ERR_INT("Internal Error"));
    if (! status.is_used[_index]) die(ERR_INT("Internal Error"));
    
    status.num_child -= 1U;
    status.is_used[_index] = false;
    if (status.is_ready[_index * 2]) {
      if (status.num_ready == 0) die(ERR_INT("Internal Error"));
      status.is_ready[_index * 2] = false;
      status.num_ready -= 1U; }
    if (status.is_ready[_index * 2 + 1]) {
      if (status.num_ready == 0) die(ERR_INT("Internal Error"));
      status.is_ready[_index * 2 + 1] = false;
      status.num_ready -= 1U; } }

  _index = -1; }
