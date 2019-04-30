// Copyright (C) 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "xzi.hpp"
#include "iobase.hpp"
#include <limits>
#include <type_traits>
#include <cassert>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
using std::ifstream;
using std::ios;
using std::ofstream;
using std::set;
using std::unique_ptr;
using namespace IOAux;
using namespace Err;
using uchar = unsigned char;
using uint = unsigned int;

static int64_t match_fname(const char *p, const char *fmt) noexcept {
  assert(p);
  assert(fmt);
  char buf_new[maxlen_path + 1];
  char fmt_new[maxlen_path + 4];
  char buf[1024];
  char dummy1, dummy2;

  strcpy(buf_new, p);
  strcat(buf_new, "x");
  strcpy(fmt_new, fmt);
  strcat(fmt_new, "%c%c");
  int64_t i64 = -1;
  if (sscanf(buf_new, fmt_new, buf, &dummy1, &dummy2) == 2 && dummy1 == 'x') {
    char *endptr;
    long long int v(strtoll(buf, &endptr, 10));
    if (endptr != buf && *endptr == '\0'
        && 0 <= v && v <= INT64_MAX && v < LLONG_MAX) i64 = v;
  }

  return i64; }

CINET::CINET(const char *p, uint port) noexcept : _port(port) {
  assert(p);
  uint len(0);
  for (; p[len] != '\0' && len + 1U < sizeof(_cipv4); ++len)
    _cipv4[len] = p[len];

  _cipv4[len] = '\0';
  if (p[len] != '\0') die(ERR_INT("bad address %s", _cipv4)); }

void CINET::set_cinet(const sockaddr_in &c_addr) noexcept {
  if (!inet_ntop(AF_INET, &c_addr.sin_addr, _cipv4, sizeof(_cipv4)))
    die(ERR_CLL("inet_ntop"));
  _port = c_addr.sin_port; }

FName &FName::operator=(const FName &fname) noexcept {
  if (this == &fname) return *this;
  _len = fname._len;
  if (sizeof(_fname) <= _len) Err::die(ERR_INT("FName too long"));
  memcpy(_fname, fname._fname, _len + 1U);
  return *this; }

FName::FName(const FName &fname) noexcept : _len(fname._len) {
  if (sizeof(_fname) <= _len) Err::die(ERR_INT("FName too long"));
  memcpy(_fname, fname._fname, _len + 1U); }

FName::FName(const char *p1, const char *p2) noexcept : _len(0) {
  add_fname(p1); add_fname(p2); }

void FName::add_fname(const char *p) noexcept {
  assert(p);
  if (0 < _len && _fname[_len - 1U] != '/') {
    if (sizeof(_fname) <= _len) Err::die(ERR_INT("FName too long"));
    _fname[_len++] = '/'; }
    
  for (;; ++_len, ++p) {
    if (sizeof(_fname) <= _len) Err::die(ERR_INT("FName too long"));
    if (*p == '\0') break;
    _fname[_len] = *p; }
  _fname[_len] = '\0'; }

void FName::add_fmt_fname(const char *fmt, ...) noexcept {
  assert(fmt);
  va_list ap;
  char bname[maxlen_path];

  va_start(ap, fmt);
  int ret = vsnprintf(bname, sizeof(bname), fmt, ap);
  va_end(ap);
  if (static_cast<int>(sizeof(bname)) <= ret) die(ERR_INT("FName too long"));
  add_fname(bname); }

void FName::cut_fname(uint u) noexcept {
  if (_len < u) die(ERR_INT("cannot cut %u of %u characters", u, _len));
  _len -= u;
  _fname[_len] = '\0'; }

FNameID::FNameID(int64_t i64, const FName &fname) noexcept
  : FName(fname), _i64(i64) {}

FNameID::FNameID(int64_t i64, const char *fname)
  noexcept : FName(fname), _i64(i64) {}

FNameID::FNameID(int64_t i64, const char *dname, const char *bname)
  noexcept : FName(dname, bname), _i64(i64) {}

bool operator<(const FNameID &fn1, const FNameID &fn2) {
  return fn1.get_id() < fn2.get_id(); }


size_t IOAux::make_time_stamp(char *p, size_t n, const char *fmt) noexcept {
  time_t t = time(nullptr);
  tm *ptm = localtime(&t);
  if (!ptm) die(ERR_CLL("localtime"));

  size_t len = strftime(p, n, fmt, ptm);
  if (len == 0) die(ERR_INT("strftime() failed"));
  return len; }

bool IOAux::is_weight_ok(const char *fname) noexcept {
  assert(fname);
  ifstream ifs(fname, ios::binary | ios::ate);
  size_t len(ifs.tellg());
  ifs.seekg(0, ifs.beg);
  if (!ifs) die(ERR_INT("cannot open %s", fname));
  unique_ptr<char> ptr(new char [len]);
  ifs.read(ptr.get(), len);
  return is_weight_ok(PtrLen<const char>(ptr.get(), len)); }

bool IOAux::is_weight_ok(PtrLen<const char> plxz) noexcept {
  assert(plxz.ok());
  XZDecode<PtrLen<const char>, PtrLen<char>> xzd;
  char token[256];
  PtrLen<char> pl(token,0);
  char *endptr;
  
  xzd.init();
  while (true) {
    pl.clear();
    bool bRet(xzd.getline(&plxz, &pl, sizeof(token), " \n\r\t,"));
    if (!bRet) return false;
    if (pl.len == 0) break;
    if (pl.len == sizeof(token)) return false;
    assert(pl.len < sizeof(token));
    
    pl.p[pl.len] = '\0';
    errno = 0;
    strtof(token, &endptr);
    if (*endptr != '\0' || endptr == token || errno == ERANGE) return false;
  }
  
  return true; }

void IOAux::grab_files(set<FNameID> &dir_list, const char *dname,
		       const char *fmt, int64_t min_no) noexcept {
  assert(dname);

  dir_list.clear();
  DIR *pd(opendir(dname));
  if (!pd) die(ERR_INT("cannot open directory %s", dname));

  while (true) {
    errno = 0;
    dirent *pent(readdir(pd));
    if (pent == NULL) {
      if (errno) die(ERR_CLL("readdir"));
      break; }
    
    if (maxlen_path < strlen(pent->d_name) + 1) continue;
    int64_t i64 = match_fname(pent->d_name, fmt);
    if (i64 < min_no) continue;
    dir_list.insert(FNameID(i64, dname, pent->d_name)); }
  
  if (closedir(pd) < 0) die(ERR_CLL("closedir")); }

FNameID IOAux::grab_max_file(const char *dname, const char *fmt) noexcept {
  assert(dname);
  FNameID ret;

  DIR *pd(opendir(dname));
  if (!pd) die(ERR_INT("cannot open directory %s", dname));

  while (true) {
    errno = 0;
    dirent *pent(readdir(pd));
    if (pent == NULL) {
      if (errno) die(ERR_CLL("readdir"));
      break; }

    if (maxlen_path < strlen(pent->d_name) + 1U) continue;
    int64_t i64 = match_fname(pent->d_name, fmt);
    if (i64 <= ret.get_id()) continue;
    ret = FNameID(i64, dname, pent->d_name); }

  if (closedir(pd) < 0) die(ERR_CLL("closedir"));
  return ret; }

template <typename T> T IOAux::bytes_to_int(const char *p) noexcept {
  using value_t = typename std::make_unsigned<T>::type;
  assert(p);
  value_t value = 0;
  uint u = sizeof(value_t);
  while (0 < u) {
    uchar uc = static_cast<uchar>(p[--u]);
    value *= static_cast<value_t>(256U);
    value += static_cast<value_t>(uc); }
  return static_cast<T>(value); }

template ushort IOAux::bytes_to_int(const char *p) noexcept;
template uint IOAux::bytes_to_int(const char *p) noexcept;
template int64_t IOAux::bytes_to_int(const char *p) noexcept;

template <typename T> void IOAux::int_to_bytes(const T &v, char *p) noexcept {
  using value_t = typename std::make_unsigned<T>::type;
  assert(p);

  value_t value = static_cast<value_t>(v);
  for (uint u = 0; u < sizeof(value_t); ++u) {
    uchar uc = static_cast<uchar>(value % 256U);
    p[u] = static_cast<char>(uc);
    value /= static_cast<value_t>(256U); } }

template void IOAux::int_to_bytes(const ushort &v, char *p) noexcept;
template void IOAux::int_to_bytes(const uint &v, char *p) noexcept;
template void IOAux::int_to_bytes(const int64_t &v, char *p) noexcept;
