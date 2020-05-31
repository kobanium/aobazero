// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "osi.hpp"
#include "nnet.hpp"

class SeqPRN {
  OSI::MMap _mmap;
public:
  explicit SeqPRN(bool flag_detach) noexcept;
  ~SeqPRN() noexcept { _mmap.close(); };
  const uint64_t *get_ptr() const noexcept {
    return static_cast<uint64_t *>(_mmap()); }
};

class NNetIPC {
  using uint   = unsigned int;
  using ushort = unsigned short;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc;
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc;
  SharedService *_pservice;
  SharedIPC *_pipc;
  int _id;
  bool _flag_detach;
  int sem_wait(OSI::Semaphore &sem) noexcept;

public:
  NNetIPC(bool flag_detach = true) noexcept;
  int start(uint nnet_id) noexcept;
  void end() noexcept;
  int get_id() const noexcept;
  float *get_features() const noexcept;
  ushort *get_nnmoves() const noexcept;
  const float *get_probs() const noexcept;
  float get_value() const noexcept;
  int submit_block(uint size_nnmove) noexcept;
  bool ok() const noexcept { return 0 <= _id; }
};
