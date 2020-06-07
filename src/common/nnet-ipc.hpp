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
  bool _flag_detach, _do_compress;
  int sem_wait(OSI::Semaphore &sem) noexcept;
  int submit_block_child(uint size_nnmove) noexcept;

public:
  NNetIPC(bool flag_detach = true) noexcept;
  int start(uint nnet_id) noexcept;
  void end() noexcept;
  int submit_block(uint size_nnmove) noexcept;
  int submit_compressed_block(uint size_nnmove, uint n_one) noexcept;
  int get_id() const noexcept { return _id; }
  float *get_features() const noexcept { return _pipc->features; }
  void *get_compressed_features() const noexcept {
    return _pipc->compressed_features; }
  ushort *get_nnmoves() const noexcept { return _pipc->nnmove; }
  const float *get_probs() const noexcept { return _pipc->probs; }
  float get_value() const noexcept { return _pipc->value; }
  bool ok() const noexcept { return (_pipc && 0 <= _id); }
};
