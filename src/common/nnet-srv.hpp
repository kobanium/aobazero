// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "nnet.hpp"
#include "osi.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <cstdint>

class SeqPRNService {
  OSI::MMap _mmap;
public:
  explicit SeqPRNService() noexcept;
  ~SeqPRNService() noexcept {_mmap.close(); };
};

class NNetService {
  using uint = unsigned int;
  NNet::Impl _impl;
  OSI::Semaphore _sem_service_lock;
  OSI::Semaphore _sem_service;
  OSI::Semaphore _sem_ipc[NNAux::maxnum_nipc];
  OSI::MMap _mmap_service;
  OSI::MMap _mmap_ipc[NNAux::maxnum_nipc];
  std::unique_ptr<class NNet> _pnnet;
  std::thread _th_worker_srv;
  std::thread _th_worker_push;
  std::thread _th_worker_wait;
  
  std::mutex _m_entries;
  std::condition_variable _cv_entries_push;
  std::condition_variable _cv_entries_wait;
  
  std::mutex _m_srv;
  std::condition_variable _cv_srv;
  bool _flag_srv;
  
  std::deque<std::unique_ptr<class Entry>> _entries_pool;
  std::deque<std::unique_ptr<class Entry>> _entries_push;
  std::deque<std::unique_ptr<class Entry>> _entries_wait;
  bool _flag_quit;
  std::string _dtune;
  uint _nnet_id, _nipc, _size_batch, _device_id, _use_half, _thread_num;
  FName _fname;
  void worker_srv() noexcept;
  void worker_wait() noexcept;
  void worker_push() noexcept;

public:
  NNetService(NNet::Impl impl, uint nnet_id, uint nipc, uint size_batch,
	      uint device_id, uint use_half, uint thread_num,
	      const char *dtune = nullptr)
    noexcept;
  NNetService(NNet::Impl impl, uint nnet_id, uint nipc, uint size_batch,
	      uint device_id, uint use_half, uint thread_num,
	      const FName &fname, const char *dtune = nullptr) noexcept;
  ~NNetService() noexcept;
  void nnreset(const FName &fname) noexcept;
  void wait() noexcept;
};
