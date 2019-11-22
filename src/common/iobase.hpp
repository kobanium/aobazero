// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <set>
#include <cstddef>
#include <cstdint>

template <typename T> class PtrLen;
class FName;
class FNameID;
namespace OSI { class IAddr; }
namespace IOAux {
  static constexpr unsigned int maxlen_path = 256U;
  int64_t match_fname(const char *p, const char *fmt_scn) noexcept;
  size_t make_time_stamp(char *p, size_t n, const char *fmt) noexcept;
  bool is_weight_ok(const char *fname, uint64_t &digest) noexcept;
  bool is_weight_ok(PtrLen<const char> plxz, uint64_t &digest) noexcept;
  void grab_files(std::set<FNameID> &dir_list, const char *dname,
                  const char *fmt, int64_t min_no) noexcept;
  FNameID grab_max_file(const char *dname, const char *fmt) noexcept;
  template <typename T> T bytes_to_int(const char *p) noexcept;
  template <typename T> void int_to_bytes(const T &v, char *p) noexcept;
}

class IAddrKey {
  using uint = unsigned int;
  uint32_t _addr;
  uint _index;
  
public:
  explicit IAddrKey() noexcept {}
  explicit IAddrKey(const OSI::IAddr &iaddr) noexcept;
  IAddrKey & operator=(const IAddrKey & key) noexcept {
    _addr = key._addr; _index = key._index; return *this; }
  explicit operator uint() const noexcept { return _index; }
  bool operator==(const IAddrKey &key) const noexcept {
    return _addr == key._addr; }
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
  void reset_fname(const FName &f) noexcept { _len = 0; add_fname(f._fname); }
  void clear_fname() noexcept { _len = 0; _fname[0] = '\0'; }
  const char *get_fname() const noexcept { return _fname; }
  uint get_len_fname() const noexcept { return _len; }
  const char *get_bname() const noexcept;
  bool ok() const noexcept;
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
