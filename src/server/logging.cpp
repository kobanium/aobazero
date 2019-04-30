// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "logging.hpp"
#include "osi.hpp"
#include <string>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
using std::ifstream;
using std::ios;
using std::lock_guard;
using std::min;
using std::string;
using ErrAux::die;
using namespace IOAux;

constexpr char fmt_tmp[]      = "%s.x_";
constexpr char fmt_arch[]     = "%s.%05" PRIi64 ".xz";
constexpr char fmt_log[]      = "%s.log";
constexpr char fmt_arch_scn[] = "%*[^.].%5[0-9].txt.xz";

Logger::~Logger() noexcept {
  _ofs_tmp.close();
  _ofs_log.close();
  _xze.end(); }

Logger::Logger(const char *dname, const char *bname, size_t len_tot) noexcept
  : _len_tot(len_tot), _len(0), _no_arch(0), _dname(dname), _fname_tmp(dname),
    _fname_log(dname), _bname(bname) {
  assert(dname && bname);

  _fname_tmp.add_fmt_fname(fmt_tmp, _bname.get_fname());
  _fname_log.add_fmt_fname(fmt_log, _bname.get_fname());

  FNameID fcurr = grab_max_file(dname, fmt_arch_scn);
  _no_arch = fcurr.get_id() + 1;
  
  ifstream ifs_log(_fname_log.get_fname(), ios::binary);
  if (!ifs_log) {
    open_all();
    return; }
  
  _ofs_tmp.open(_fname_tmp.get_fname(), ios::binary | ios::trunc);
  if (!_ofs_tmp) die(ERR_INT("cannot open %s", _fname_tmp.get_fname()));
  _xze.start(&_ofs_tmp, SIZE_MAX, 9);

  char buf[BUFSIZ];
  while(ifs_log) {
    size_t size = ifs_log.read(buf, sizeof(buf)).gcount();
    PtrLen<const char> pl(buf, size);
    if (!_xze.append(&pl)) die(ERR_INT("cannot encode log"));
    _len += size; }

  _ofs_log.open(_fname_log.get_fname(), ios::app);
  if (!_ofs_log) die(ERR_INT("cannot open %s", _fname_log.get_fname())); }

void Logger::out(const OSI::IAddr *piaddr, const char *fmt, ...) noexcept {
  assert(fmt);
  char msg[1024];
  size_t len_tot = 0;
  size_t len = make_time_stamp(msg, sizeof(msg), "%D %R ");
  len_tot += len;
  if (piaddr) {
    len = snprintf(msg + len_tot, sizeof(msg) - len_tot,
		   "%s:%-6u", piaddr->get_cipv4(), piaddr->get_port());
    len_tot += len; }
  
  va_list args;
  va_start(args, fmt);
  len = vsnprintf(msg + len_tot, sizeof(msg) - len_tot, fmt, args);
  va_end(args);
  len_tot += len;

  len_tot = min(len_tot + 1, sizeof(msg) - 1);
  msg[len_tot - 1U] = '\n';
  msg[len_tot] = '\0';
  PtrLen<const char> pl_in(msg, len_tot);

  lock_guard<mutex> lock(_m);
  _len += len_tot;
  _ofs_log.write(msg, len_tot);
  _ofs_log.flush();
  if (!_ofs_log) die(ERR_INT("cannot write to log"));
  if (!_xze.append(&pl_in)) die(ERR_INT("cannot encode log"));
  if (_len < _len_tot) return;

  close_all();
  open_all();
  
  _no_arch += 1;
  _len      = 0; }

void Logger::close_all() noexcept {
  if (!_xze.end()) die(ERR_INT("cannot encode log"));
  
  _ofs_tmp.close();
  if (!_ofs_tmp) die(ERR_INT("cannot write to archive"));

  _ofs_log.close();
  if (!_ofs_log) die(ERR_INT("cannot write to log"));

  FName fname(_dname);
  fname.add_fmt_fname(fmt_arch, _bname.get_fname(), _no_arch);
  if (rename(_fname_tmp.get_fname(), fname.get_fname()) < 0)
    die(ERR_CLL("rename")); }

void Logger::open_all() noexcept {
  _ofs_tmp.open(_fname_tmp.get_fname(), ios::binary | ios::trunc);
  if (!_ofs_tmp) die(ERR_INT("cannot open %s", _fname_tmp.get_fname()));
  
  _ofs_log.open(_fname_log.get_fname(), ios::trunc);
  if (!_ofs_log) die(ERR_INT("cannot open %s", _fname_log.get_fname()));
  
  _xze.start(&_ofs_tmp, SIZE_MAX, 9); }

