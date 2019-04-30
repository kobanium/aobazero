// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

template<size_t N> class FixLStr {
  static_assert(0 < N, "N must be positive.");
  size_t _len, _lenw;
  char _buf[N];

public:
  FixLStr() noexcept : _len(0), _lenw(0) { _buf[0] = '\0'; }
  explicit FixLStr(const char *p) noexcept : FixLStr() { operator+=(p); }
  explicit FixLStr(const char *p, size_t n) noexcept : FixLStr() { add(p, n); }

  constexpr size_t size() const noexcept { return sizeof(_buf); }
  size_t len() const noexcept { return _len; }
  size_t written() const noexcept { return _lenw; }
  operator const char *() const noexcept { return _buf; }
  
  void operator+=(unsigned int u) noexcept {
    assert(_len < sizeof(_buf));
    size_t len_avail = sizeof(_buf) - _len;
    int ret = snprintf(_buf + _len, len_avail, "%u", u);
    assert(0 < ret);
    size_t len_written = static_cast<unsigned int>(ret);
    _lenw += len_written;
    _len  += std::min(len_written, len_avail - 1U); }
  
  void operator+=(const char *p) noexcept { add(p, strlen(p)); }
  
  void add(const char *p, size_t n) noexcept {
    assert(p && _len < sizeof(_buf));
    _lenw += n;
    size_t len_avail = sizeof(_buf) - _len;
    n = std::min(n, len_avail - 1U);
    memcpy(_buf + _len, p, n);
    _len += n;
    _buf[_len] = '\0'; }
};
