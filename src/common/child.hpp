// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "osi.hpp"
#include <condition_variable>
#include <thread>
#include <cassert>

class Reader {
  using uint = unsigned int;
  char _line[65536];
  OSI::ReadHandle _rh;
  std::thread _t;
  std::condition_variable _cv;
  bool _is_eof, _do_read, _has_line;
  uint _len_line;
  int _index;
  void worker() noexcept;

public:
  explicit Reader() noexcept : _index(-1) {}
  void reset(OSI::ReadHandle &&rh, int index) noexcept;
  void end() noexcept;
  uint getline(char *line, uint size) noexcept;
  bool ok() const noexcept;
};

class Child {
  using uint  = unsigned int;
  using uchar = unsigned char;
  OSI::ChildProcess _cp;
  Reader _in, _err;
  int _index;

public:
  explicit Child() noexcept : _index(-1) {}
  static uint wait(uint timeout) noexcept;
  void open(const char *path, char * const argv[]) noexcept;
  void close() noexcept;
  uint getline_in(char *line, uint size) noexcept;
  uint getline_err(char *line, uint size) noexcept;
  void close_write() const noexcept { _cp.close_write(); }
  uint get_pid() const noexcept { return _cp.get_pid(); }
  bool is_closed() const noexcept { return _cp.is_closed(); }
  bool ok() const noexcept;
  size_t write(const char *msg, size_t n) const noexcept {
    assert(msg); return _cp.write(msg, n); }
  bool has_line_in() const noexcept;
  bool has_line_err() const noexcept;
};

