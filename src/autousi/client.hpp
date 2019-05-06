// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <set>
#include <memory>
#include <cstdint>

class Job;
template <typename T> class JQueue;

class WghtFile {
  using uint = unsigned int;
  static std::set<FNameID> _all;
  static std::mutex _m;
  FNameID _fname;
  uint64_t _crc64;
  uint _keep_wght;
  
public:
  static void cleanup();
  explicit WghtFile(const FNameID &fxz, uint keep_wght) noexcept;
  ~WghtFile() noexcept;
  uint64_t get_crc64() const noexcept { return _crc64; }
  uint get_len_fname() const noexcept { return _fname.get_len_fname(); }
  const char *get_fname() const noexcept { return _fname.get_fname(); }
  int64_t get_id() const noexcept { return _fname.get_id(); }
};

class Client {
  using uint = unsigned int;
  volatile std::atomic<bool> _quit;
  volatile std::atomic<bool> _has_conn;
  int64_t _wght_id;
  std::unique_ptr<JQueue<Job>> _pJQueue;
  std::unique_ptr<char []> _prec_xz;
  std::unique_ptr<char []> _saddr;
  std::thread _thread_reader, _thread_sender;
  std::mutex _m;
  std::shared_ptr<const WghtFile> _wght;
  FName _dwght;
  int _ver_engine;
  uint _max_retry, _retry_count, _recvTO, _recv_bufsiz, _sendTO, _send_bufsiz;
  uint _read_bufsiz, _port, _keep_wght;
  
  Client() noexcept;
  ~Client() noexcept;
  Client(const Client &) = delete;
  Client & operator=(const Client &) = delete;

  void get_new_wght();
  void sender() noexcept;
  void reader() noexcept;
  
public:
  static Client & get() noexcept;
  void add_rec(const char *p, size_t len) noexcept;
  void start(const char *dwght, const char *cstr_addr, uint port, uint recvTO,
	     uint recv_bufsiz, uint sendTO, uint send_bufsiz, uint max_retry,
	     uint size_queue, uint keep_wght) noexcept;
  void end() noexcept;
  std::shared_ptr<const WghtFile> get_wght() noexcept;
  int get_ver_engine() const noexcept { return _ver_engine; }
  bool has_conn() const noexcept { return _has_conn; }
};
