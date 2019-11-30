// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "osi.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdint>

namespace NNAux {
  constexpr unsigned int maxsize_ipc = 256U;
  constexpr unsigned int maxnum_nnet =  64U;
}

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
  ~SeqPRN() noexcept {_mmap.close(); };
  const uint64_t *get_ptr() const noexcept {
    return static_cast<uint64_t *>(_mmap()); }
};

class NNetService {
  using uint = unsigned int;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc[NNAux::maxsize_ipc];
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc[NNAux::maxsize_ipc];
  std::thread _th_worker_push;
  std::condition_variable _cv_flush;
  std::mutex _m_flush;
  bool _flag_cv_flush;
  uint _nnet_id, _nipc, _size_batch, _device_id, _use_half;
  FName _fname;
  void worker_push() noexcept;

public:
  NNetService(uint nnet_id, uint nipc, uint size_batch, uint device_id,
	      uint use_half, const FName &fname,
	      bool flag_dispatch = true) noexcept;
  ~NNetService() noexcept;
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
  bool _flag_dispatch;

public:
  NNetIPC() noexcept : _id(-1) {}
  void start(uint nnet_id) noexcept;
  void end() noexcept;
  int get_id() const noexcept;
  float *get_input() const noexcept;
  ushort *get_nnmoves() const noexcept;
  const float *get_probs() const noexcept;
  float get_value() const noexcept;
  int submit_block(uint size_nnmove) noexcept;
  bool ok() const noexcept { return 0 <= _id; }
};
