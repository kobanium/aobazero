#pragma once
#include "iobase.hpp"
#include "osi.hpp"
#include <thread>

namespace NNAux {
  constexpr uint maxsize_ipc = 256U;
  constexpr uint maxnum_nnet =  64U;
}

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
