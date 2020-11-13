// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "hashtbl.hpp"
#include "xzi.hpp"
#include <atomic>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <cstdint>
namespace OSI { class IAddr; }

class Wght {
  int64_t _no;
  size_t _len;
  std::unique_ptr<char []> _p;
  
public:
  explicit Wght(int64_t no, size_t len) noexcept : _no(no), _len(len),
						   _p(new char[len]) {}
  const PtrLen<char> ptrlen() const noexcept {
    return PtrLen<char>(_p.get(), _len); }
  const char *get_p() const noexcept { return _p.get(); }
  int64_t get_no() const noexcept { return _no; }
  size_t get_len() const noexcept { return _len; }
};

class WghtKeep {
  std::map<int64_t, uint64_t> _map_wght;
  std::atomic<bool> _bEndWorker;
  int64_t _i64_now;
  class Logger *_logger;
  std::shared_ptr<Wght> _pwght;
  FName _dwght;
  std::thread _thread;
  std::mutex _m;
  uint _wght_poll;
  
  // special member functions
  explicit WghtKeep() noexcept : _bEndWorker(false), _i64_now(-1) {}
  ~WghtKeep() noexcept {}
  WghtKeep(const WghtKeep &) = delete;
  WghtKeep & operator=(const WghtKeep &) = default;

  void get_new_wght() noexcept;
  void worker() noexcept;
  
public:
  // singleton technique
  static WghtKeep & get() noexcept;

  void start(Logger *logger, const char *dwght, uint wght_poll) noexcept;
  void end() noexcept;
  std::shared_ptr<const Wght> get_pw() noexcept;
  bool get_crc64(int64_t no, uint64_t &digest) const noexcept;
};

template <typename T> class JQueue;
class EMAKeep;

class RecKeep {
  using uint = unsigned int;
  struct RedunValue {
    uint64_t no, count;
    explicit RedunValue() noexcept : count(0) {}
    RedunValue & operator=(const RedunValue &value) noexcept {
      no = value.no; count = value.count; return *this; }
  };
  std::unique_ptr<EMAKeep> ema_keep_ptr;
  std::thread _thread;
  std::unique_ptr<JQueue<class JobIP>> _pJQueue;
  std::set<FNameID> _pool;
  HashTable<Key64, RedunValue> _redundancy_table;
  class Logger *_logger;
  FName _fpool_tmp, _farch_tmp;
  XZDecode<PtrLen<const char>, PtrLen<char>> _xzd;
  XZEncode<PtrLen<const char>, std::ofstream> _xze;
  std::ofstream _ofs_arch_tmp;
  size_t _maxlen_rec;
  FName _darch, _dpool;
  std::unique_ptr<char []> _prec;
  uint _maxrec_sec, _maxrec_min, _maxrec_len, _minlen_play, _minave_child;
  bool _bFirst;

  void worker() noexcept;
  void close_arch_tmp() noexcept;
  void open_arch_tmp() noexcept;

  // special member functions
  explicit RecKeep() noexcept;
  ~RecKeep() noexcept;
  RecKeep(const RecKeep &) = delete;
  RecKeep & operator=(const RecKeep &) = default;
  
public:
  // singleton technique
  static RecKeep & get() noexcept;
  
  void start(Logger *logger, const char *darch, const char *dpool,
	     uint maxlen_job, size_t maxlen_rec, size_t maxlen_recv,
	     uint log2_nindex_redun, uint minlen_play, uint minave_child)
    noexcept;
    
  void end() noexcept;
  void transact(const JobIP *pJob) noexcept;
  void add(const char *prec, size_t len_rec, const OSI::IAddr &iaddr) noexcept;
};
