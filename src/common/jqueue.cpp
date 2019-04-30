// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "jqueue.hpp"
#include <cassert>
using std::unique_lock;
using std::lock_guard;
using std::mutex;
using ErrAux::die;
using uint = unsigned int;

template <typename T>
JQueue<T>::JQueue(uint maxlen_queue) noexcept
  : _T_slots(new T [maxlen_queue + 2U]),
    _pT_free(new T *[maxlen_queue + 1U]),
    _pT_queue(new T *[maxlen_queue]), _bEnd(false) {

  _maxlen_queue = maxlen_queue;
  for (uint u(0); u < _maxlen_queue; ++u) _pT_free[u] = &( _T_slots[u] );
  _pT_inuse  = &(_T_slots[_maxlen_queue]);
  _pT_inprep = &(_T_slots[_maxlen_queue + 1U]);
  _len_free  = _maxlen_queue;
  _len_queue = 0; }

template <typename T> T *JQueue<T>::pop() noexcept {
  unique_lock<mutex> lock(_m);
  _cv.wait(lock, [&]{ return 0 < _len_queue || _bEnd; });
  if (_bEnd) return nullptr;

  assert(_len_free < _maxlen_queue + 1U);
  _pT_free[_len_free++] = _pT_inuse;
  _pT_inuse = _pT_queue[--_len_queue];
  lock.unlock();
  _cv.notify_one();

  return _pT_inuse; }

template <typename T> void JQueue<T>::push_free() noexcept {
  unique_lock<mutex> lock(_m);
  _cv.wait(lock, [&]{ return _len_queue < _maxlen_queue; });
  assert(0 < _len_free);
  _pT_queue[_len_queue++] = _pT_inprep;
  _pT_inprep = _pT_free[--_len_free];
  lock.unlock();
  _cv.notify_one(); }

template <typename T>
uint JQueue<T>::get_len() noexcept {
  lock_guard<mutex> lock(_m);
  return _len_queue; }

template class JQueue<Job>;
template class JQueue<JobIP>;
