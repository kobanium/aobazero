// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "err.hpp"
#include "nnet-ipc.hpp"
#include "param.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
using ErrAux::die;
using uint = unsigned int;

SeqPRN::SeqPRN(bool flag_detach) noexcept {
  char fn[IOAux::maxlen_path + 256U];
  uint pid;
  if (flag_detach) pid = OSI::get_pid();
  else             pid = OSI::get_ppid();
  sprintf(fn, "%s.%07u", Param::name_seq_prn, pid);
  _mmap.open(fn, false, sizeof(uint64_t) * Param::len_seq_prn); }

int NNetIPC::sem_wait(OSI::Semaphore &sem) noexcept {
  if (_flag_detach) sem.dec_wait();
  else {
    while (sem.dec_wait_timeout(1U) < 0)
      if (! OSI::has_parent()) return -1; }
  return 0; }

NNetIPC::NNetIPC(bool flag_detach) noexcept :
_id(-1), _flag_detach(flag_detach) {}

int NNetIPC::start(uint nnet_id) noexcept {
  assert(nnet_id < NNAux::maxnum_nipc);
  char fn[IOAux::maxlen_path + 256U];
  uint pid;
  if (_flag_detach) pid = OSI::get_pid();
  else              pid = OSI::get_ppid();
  sprintf(fn, "%s.%07u.%03u", Param::name_sem_lock_nnet, pid, nnet_id);
  _sem_service_lock.open(fn, false, 1U);
  sprintf(fn, "%s.%07u.%03u", Param::name_sem_nnet, pid, nnet_id);
  _sem_service.open(fn, false, 0U);
  sprintf(fn, "%s.%07u.%03u", Param::name_mmap_nnet, pid, nnet_id);
  _mmap_service.open(fn, false, sizeof(SharedService));

  _pservice = static_cast<SharedService *>(_mmap_service());
  if (sem_wait(_sem_service_lock) < 0) return -1;
  _id = _pservice->id_ipc_next++;
  _sem_service_lock.inc();
  if (NNAux::maxnum_nipc <= static_cast<uint>(_id))
    die(ERR_INT("too many processes"));

  sprintf(fn, "%s.%07u.%03u.%03u", Param::name_sem_nnet, pid, nnet_id, _id);
  _sem_ipc.open(fn, false, 0);
  sprintf(fn, "%s.%07u.%03u.%03u", Param::name_mmap_nnet, pid, nnet_id, _id);
  _mmap_ipc.open(fn, false, sizeof(SharedIPC));
  _pipc = static_cast<SharedIPC *>(_mmap_ipc());
  _pipc->nnet_id = nnet_id;

  if (sem_wait(_sem_service_lock) < 0) return -1;
  assert(_pservice->njob <= NNAux::maxnum_nipc);
  _pservice->jobs[_pservice->njob].id   = _id;
  _pservice->jobs[_pservice->njob].type = SrvType::Register;
  _pservice->njob += 1U;
  _do_compress = _pservice->do_compress;
  _sem_service_lock.inc();
  _sem_service.inc();
  return sem_wait(_sem_ipc); }

void NNetIPC::end() noexcept {
  assert(_sem_service_lock.ok() && _sem_service.ok());
  _sem_service_lock.close();
  _sem_service.close();

  assert(_mmap_service.ok() && _sem_ipc.ok() && _mmap_ipc.ok());
  _mmap_service.close();
  _sem_ipc.close();
  _mmap_ipc.close(); }

int NNetIPC::submit_block_child(uint size_nnmove) noexcept {
  _pipc->size_nnmove = size_nnmove;
  assert(_sem_service_lock.ok() && _sem_service.ok());
  if (sem_wait(_sem_service_lock) < 0) return -1;
  assert(_pservice->njob <= NNAux::maxnum_nipc);
  _pservice->jobs[_pservice->njob].id   = _id;
  _pservice->jobs[_pservice->njob].type = SrvType::FeedForward;
  _pservice->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();
  return sem_wait(_sem_ipc); }

int NNetIPC::submit_block(uint size_nnmove) noexcept {
  assert(ok() && size_nnmove <= SAux::maxsize_moves && _pipc);
  if (_do_compress)
    _pipc->n_one = NNAux::compress_features(_pipc->compressed_features,
					    _pipc->features);
  return submit_block_child(size_nnmove); }

int NNetIPC::submit_compressed_block(uint size_nnmove, uint n_one) noexcept {
  assert(ok() && size_nnmove <= SAux::maxsize_moves && _pipc);
  if (! _do_compress)
    NNAux::decompress_features(_pipc->features, n_one,
			       _pipc->compressed_features);
  return submit_block_child(size_nnmove); }
