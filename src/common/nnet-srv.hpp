// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "nnet.hpp"
#include "osi.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <cstdint>

class SeqPRNService {
  OSI::MMap _mmap;

public:
  explicit SeqPRNService() noexcept;
  ~SeqPRNService() noexcept {_mmap.close(); };
};

class SeqPRN {
  OSI::MMap _mmap;

public:
  explicit SeqPRN() noexcept;
  ~SeqPRN() noexcept { _mmap.close(); };
  const uint64_t *get_ptr() const noexcept {
    return static_cast<uint64_t *>(_mmap()); }
};

class NNetService {
  using uint = unsigned int;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc[NNAux::maxnum_nipc];
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc[NNAux::maxnum_nipc];
  std::unique_ptr<class NNet> _pnnet;
  std::thread _th_worker_push;
  std::thread _th_worker_wait;
  std::condition_variable _cv_flush;
  std::condition_variable _cv_nnreset;
  std::condition_variable _cv_entries;
  std::mutex _m_flush;
  std::mutex _m_nnreset;
  std::mutex _m_entries;
  std::deque<std::unique_ptr<class Entry>> _entries;
  bool _flag_cv_flush, _flag_cv_nnreset;
  uint _nnet_id, _nipc, _size_batch, _device_id, _use_half;
  FName _fname;
  void worker_push() noexcept;
  void worker_wait() noexcept;

public:
  NNetService(uint nnet_id, uint nipc, uint size_batch, uint device_id,
	      uint use_half) noexcept;
  NNetService(uint nnet_id, uint nipc, uint size_batch, uint device_id,
	      uint use_half, const FName &fname) noexcept;
  ~NNetService() noexcept;
  void nnreset(const FName &fname) noexcept;
  void flush_on() noexcept;
  void flush_off() noexcept;
};

class NNetIPC {
  using uint = unsigned int;
  using ushort = unsigned short;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc;
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc;
  class SharedService *_pservice;
  class SharedIPC *_pipc;
  int _id;
  bool _flag_detach;
  int sem_wait(OSI::Semaphore &sem) noexcept;

public:
  NNetIPC(bool flag_detach = true) noexcept;
  int start(uint nnet_id) noexcept;
  void end() noexcept;
  int get_id() const noexcept;
  float *get_input() const noexcept;
  ushort *get_nnmoves() const noexcept;
  const float *get_probs() const noexcept;
  float get_value() const noexcept;
  int submit_block(uint size_nnmove) noexcept;
  bool ok() const noexcept { return 0 <= _id; }
};
