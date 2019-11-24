#pragma once
#include "iobase.hpp"
#include "osi.hpp"
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
  uint _nnet_id, _nipc, _size_batch, _device_id, _use_half;
  FName _fname;
  void worker_push() noexcept;

public:
  NNetService(uint nnet_id, uint nipc, uint size_batch, uint device_id,
	      uint use_half, const FName &fname) noexcept;
  ~NNetService() noexcept;
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

public:
  NNetIPC() noexcept : _id(-1) {}
  void start(uint nnet_id) noexcept;
  void end() noexcept;
  float *get_input() const noexcept;
  ushort *get_nnmoves() const noexcept;
  const float *get_probs() const noexcept;
  float get_value() const noexcept;
  void submit_block(uint size_nnmove) noexcept;
  bool ok() const noexcept { return 0 <= _id; }
};
