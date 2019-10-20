#pragma once
#include "osi.hpp"
#include <thread>

namespace NNAux {
  constexpr uint size_ipc = 1024U;
}

class NNetService {
  using uint = unsigned int;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc[NNAux::size_ipc];
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc[NNAux::size_ipc];
  std::thread _th_worker;
  uint _nipc;
  void worker() noexcept;
  NNetService() noexcept {};
  ~NNetService() noexcept {};

public:
  static NNetService &get() noexcept;
  void start(uint n) noexcept;
  void end() noexcept;
};

class NNetIPC {
  using uint = unsigned int;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc;
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc;
  int _id;

public:
  NNetIPC() noexcept : _id(-1) {}
  void start(const char *str_wght) noexcept;
  void end() noexcept;
  bool ok() const noexcept { return 0 <= _id; }
};

