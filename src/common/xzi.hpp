// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#define LZMA_API_STATIC
#include <iosfwd>
#include <cstdint>
#include <lzma.h>

class FName;
template <typename T> class PtrLen;

namespace XZAux {
  uint64_t crc64(const FName & fname) noexcept;
  uint64_t crc64(const char *p, uint64_t crc64) noexcept;
  uint64_t crc64(const char *p, size_t len, uint64_t crc64) noexcept;
}

class DevNul {};

template <typename T> class PtrLen {
public:
  T *p;
  size_t len;
  
  explicit PtrLen() noexcept {}
  ~PtrLen() noexcept {}
  
  explicit PtrLen(T *p0, size_t len0) noexcept : p(p0), len(len0) {}
  operator PtrLen<const T>() const noexcept { return PtrLen<const T>(p, len); }
  
  void clear() noexcept { len = 0; }
  bool ok() const noexcept { return p != nullptr; }
};

class XZBase {
protected:
  uint8_t _inbuf[1024 * 8];
  uint8_t _outbuf[1024 * 8];

  explicit XZBase() noexcept {}
  ~XZBase() noexcept {}
  XZBase(const XZBase &) = delete;
  XZBase & operator=(const XZBase &) = delete;

  void xzwrite(int *fd, size_t len) const noexcept;
  void xzwrite(std::ofstream *pofs, size_t len) const noexcept;
  void xzwrite(PtrLen<char> *out, size_t len) const noexcept;
  void xzwrite(DevNul *out, size_t len) const noexcept;
  size_t xzread(PtrLen<const char> *pl) noexcept;
  size_t xzread(std::ifstream *pifs) noexcept;
  size_t xzread(int *fd) noexcept;
};

template <typename T_IN, typename T_OUT>
class XZEncode : private XZBase {
  lzma_stream _strm;
  T_OUT *_out;
  size_t _maxlen_out_tot, _len_out_tot;
  bool encode(const lzma_action &a, lzma_ret &ret) noexcept;

public:
  explicit XZEncode() noexcept : _strm(LZMA_STREAM_INIT) {}
  ~XZEncode() noexcept { lzma_end(&_strm); }
  void start(T_OUT *out, size_t _maxlen_out_tot, uint32_t level,
	     bool bExt = false) noexcept;
  bool append(T_IN *in) noexcept;
  bool end() noexcept;
  
  size_t get_len_out() const noexcept { return _len_out_tot; }
};

template <typename T_IN, typename T_OUT>
class XZDecode : private XZBase {
  lzma_stream _strm;
  size_t _len_out_tot;
  uint64_t _crc64;
  
public:
  explicit XZDecode() noexcept : _strm(LZMA_STREAM_INIT) {}
  ~XZDecode() noexcept { lzma_end(&_strm); }
  void init() noexcept;
  bool decode(T_IN *in, T_OUT *out, size_t size_limit) noexcept;
  bool getline(T_IN *in, T_OUT *out, size_t len_limit,
	       const char *delims) noexcept;
  size_t get_len_out() const noexcept { return _len_out_tot; }
  uint64_t get_crc64() const noexcept { return _crc64; }
};
