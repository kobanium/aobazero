// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "osi.hpp"
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

class Job {
  using uint = unsigned int;
  std::unique_ptr<char[]> _p;
  size_t _len;
  
public:
  explicit Job() noexcept {}
  ~Job() noexcept {}
  Job & operator=(const Job &) = delete;
  Job(const Job &) = delete;

  void reset() noexcept { _p.reset(nullptr); _len = 0; }
  void reset(size_t u) noexcept { _p.reset(new char [u]); _len = u; }
  char *get_p() const noexcept { return _p.get(); }
  size_t get_len() const noexcept { return _len; }
};

class JobIP : public Job, public OSI::IAddr {
  using uint = unsigned int;
  
public:
  explicit JobIP() noexcept {}
  virtual ~JobIP() noexcept {}
  JobIP & operator=(const JobIP &) = delete;
  JobIP(const JobIP &) = delete;
};

template <typename T>
class JQueue {
  using uint = unsigned int;
  std::mutex _m;
  std::condition_variable _cv;
  std::unique_ptr<T []> _T_slots;
  std::unique_ptr<T *[]> _pT_free, _pT_queue;
  uint _len_free, _len_queue, _maxlen_queue;
  T *_pT_inuse, *_pT_inprep;
  std::atomic<bool> _bEnd;
  
public:
  explicit JQueue() noexcept {}
  ~JQueue() noexcept {}
  explicit JQueue(uint maxlen_queue) noexcept;
  T *pop() noexcept;
  void push_free() noexcept;
  uint get_len() noexcept;
  T *get_free() const noexcept { return _pT_inprep; }
  void end() noexcept { _bEnd = true; _cv.notify_one(); }
};
