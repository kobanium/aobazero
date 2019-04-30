// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "xzi.hpp"
#include "iobase.hpp"
#include <fstream>
#include <mutex>
#include <cinttypes>
using std::ofstream;
using std::mutex;
namespace OSI { class IAddr; }

namespace Log {
  constexpr char bad_XZ_format[]      = "bad XZ format";
  constexpr char bad_CSA_format[]     = "bad CSA format";
  constexpr char conn_denied[]        = "connection denied";
  constexpr char record_ignored[]     = "record ignored";
  constexpr char too_many_conn_sec[]  = "too many connections in a second";
  constexpr char too_many_conn_min[]  = "too many connections in a minute";
  constexpr char cut_conn_min[]       = "put into the deny list";
  constexpr char too_many_conn[]      = "too many total connections";
  constexpr char bad_rec_size[]       = "closed due to too large record size";
  constexpr char conn_accepted[]      = "connection accepted";
  constexpr char closed_timeout[]     = "closed due to timeout";
  constexpr char shutdown_peer[]      = "shutdown by peer";
  constexpr char fmt_bad_cmd_d[]      = "closed due to bad command (%d)";
  constexpr char fmt_wght_sent_ll[]   = "weight (no. %" PRIi64 ") send start";
  constexpr char fmt_info_sent_ll[]   = "weight info (no. %" PRIi64 ") sent";
  constexpr char bad_wght_no_ll[]     = "bad weight no. %" PRIi64 ")";
  constexpr char bad_wght_crc64_ull[] = "bad weight crc64 %" PRIu64 ")";
  constexpr char load_list_s[]        = "loading list: %s";
  constexpr char fmt_found_wght_s[]   = "found new weight %s";
  constexpr char fmt_bad_cmd_s[]      = "closed due to bad command (%s)";
  constexpr char fmt_multiple_cmd_s[] = "closed due to multiple commands (%s)";
  constexpr char fmt_reset_s[]        = "closed due to reset by peer (%s)";
}

class Logger {
  using uint = unsigned int;
  mutex _m;
  ofstream _ofs_tmp, _ofs_log;
  XZEncode<PtrLen<const char>, ofstream> _xze;
  size_t _len_tot, _len;
  int64_t _no_arch;
  FName _dname, _fname_tmp, _fname_log, _bname;
  void close_all() noexcept;
  void open_all() noexcept;
  
public:
  ~Logger() noexcept;
  explicit Logger() = delete;
  Logger & operator=(const Logger &) = delete;
  Logger(const Logger &) = delete;

  explicit Logger(const char *dname, const char *bname,
		  size_t len_tot) noexcept;
  void out(const OSI::IAddr *piaddr, const char *fmt, ...) noexcept;
};
