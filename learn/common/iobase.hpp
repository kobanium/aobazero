// Copyright (C) 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <fstream>
#include <memory>
#include <set>
#include <cassert>
#include <cstdarg>
#include <netinet/in.h>
#include <arpa/inet.h>

template <typename T> class PtrLen;
class FNameID;
namespace IOAux {
  static constexpr unsigned int maxlen_path = 256U;

  size_t make_time_stamp(char *p, size_t n, const char *fmt) noexcept;
  bool is_weight_ok(const char *fname) noexcept;
  bool is_weight_ok(PtrLen<const char> plxz) noexcept;
  void grab_files(std::set<FNameID> &dir_list, const char *dname,
                  const char *fmt, int64_t min_no) noexcept;
  FNameID grab_max_file(const char *dname, const char *fmt) noexcept;
  template <typename T> T bytes_to_int(const char *p) noexcept;
  template <typename T> void int_to_bytes(const T &v, char *p) noexcept;
}

class CINET {
  using uint   = unsigned int;
  using ushort = unsigned short;
  ushort _port;
  char _cipv4[INET_ADDRSTRLEN];

public:
  explicit CINET() noexcept { _cipv4[0] = '\0'; }
  ~CINET() noexcept {}
  CINET & operator=(const CINET &) = default;
  CINET(const CINET &) = delete;
  
  explicit CINET(const sockaddr_in &c_addr) noexcept { set_cinet(c_addr); }
  explicit CINET(const char *p, uint port) noexcept;
  void set_cinet(const CINET &cinet) noexcept { *this = cinet; }
  void set_cinet(const sockaddr_in &c_addr) noexcept;

  const char *get_cipv4() const noexcept { return _cipv4; }
  uint get_port() const noexcept { return _port; }
};

class FName {
  using uint = unsigned int;
  uint _len;
  char _fname[IOAux::maxlen_path];
  
public:
  explicit FName() noexcept : _len(0) { _fname[0] = '\0'; }
  ~FName() noexcept {}
  FName & operator=(const FName &fname) noexcept;
  explicit FName(const FName &fname) noexcept;
  
  explicit FName(const char *p) noexcept : _len(0) { add_fname(p); }
  explicit FName(const char *p1, const char *p2) noexcept;
  void add_fname(const char *p) noexcept;
  void add_fmt_fname(const char *fmt, ...) noexcept;
  void cut_fname(uint u) noexcept;
  void reset_fname(const char *p) noexcept { _len = 0; add_fname(p); }
  void clear_fname() noexcept { _len = 0; _fname[0] = '\0'; }
  const char *get_fname() const noexcept { return _fname; }
  uint get_len_fname() const noexcept { return _len; }
};

class FNameID : public FName {
  int64_t _i64;
  
public:
  explicit FNameID() noexcept : _i64(-1) {}
  explicit FNameID(int64_t i64, const FName &fname) noexcept;
  explicit FNameID(int64_t i64, const char *fname) noexcept;
  explicit FNameID(int64_t i64, const char *dname, const char *bname) noexcept;
  
  int64_t get_id() const noexcept { return _i64; }
};

bool operator<(const FNameID &fn1, const FNameID &fn2);
