// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-cpu.hpp"
#include "nnet-ipc.hpp"
#include "nnet-ocl.hpp"
#include "param.hpp"
#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstdlib>
using std::cerr;
using std::condition_variable;
using std::copy_n;
using std::cout;
using std::endl;
using std::fill_n;
using std::lock_guard;
using std::move;
using std::mt19937_64;
using std::mutex;
using std::string;
using std::swap;
using std::terminate;
using std::thread;
using std::unique_ptr;
using std::unique_lock;
using std::vector;
using ErrAux::die;
using uint = unsigned int;
enum class Type : uint { Register, FeedForward, FlushON, FlushOFF, NNReset,
    End };
#if defined(USE_OPENCL_AOBA)
class NNet : public NNetOCL {};
#else
class NNet : public NNetCPU {};
#endif

SeqPRNService::SeqPRNService() noexcept {
  _mmap.open(Param::name_seq_prn, true, sizeof(uint64_t) * Param::len_seq_prn);
  uint64_t *p = static_cast<uint64_t *>(_mmap());
  mt19937_64 mt(7);
  for (uint u = 0; u < Param::len_seq_prn; ++u) p[u] = mt(); }

SeqPRN::SeqPRN() noexcept {
  _mmap.open(Param::name_seq_prn, false,
	     sizeof(uint64_t) * Param::len_seq_prn); }

struct SharedService {
  uint id_ipc_next;
  FName fn_weights;
  uint njob;
  struct { uint id; Type type; } jobs[NNAux::maxnum_nipc + 16U];
};

struct SharedIPC {
  uint nnet_id;
  float input[NNAux::size_input];
  uint size_nnmove;
  ushort nnmoves[SAux::maxsize_moves];
  float probs[SAux::maxsize_moves];
  float value; };

class Entry {
  unique_ptr<float []> _input;
  unique_ptr<uint []> _sizes_nnmove;
  unique_ptr<ushort []> _nnmoves;
  unique_ptr<float []> _probs;
  unique_ptr<float []> _values;
  unique_ptr<uint []> _ids;
  uint _ubatch, _size_batch, _wait_id;
public:
  explicit Entry(uint size_batch) noexcept :
  _input(new float [size_batch * NNAux::size_input]),
    _sizes_nnmove(new uint [size_batch]),
    _nnmoves(new ushort [size_batch * SAux::maxsize_moves]),
    _probs(new float [size_batch * SAux::maxsize_moves]),
    _values(new float [size_batch]), _ids(new uint [size_batch]),
    _ubatch(0), _size_batch(size_batch), _wait_id(0) {
    fill_n(_input.get(), size_batch * NNAux::size_input, 0.0f);
    fill_n(_sizes_nnmove.get(), size_batch, 0); }

  void add(const float *input, uint size_nnmove, const ushort *nnmoves,
	   uint id) noexcept {
    assert(_ubatch < _size_batch && input && nnmoves
	   && id < NNAux::maxnum_nipc);
    copy_n(input, NNAux::size_input, &(_input[_ubatch * NNAux::size_input]));
    copy_n(nnmoves, size_nnmove, &(_nnmoves[_ubatch * SAux::maxsize_moves]));
    _sizes_nnmove[_ubatch] = size_nnmove;
    _ids[_ubatch]          = id;
    _ubatch += 1U; }

  void push_ff(NNet &nnet) noexcept {
    assert(0 < _ubatch);
    _wait_id = nnet.push_ff(_ubatch, _input.get(), _sizes_nnmove.get(),
			    _nnmoves.get(), _probs.get(), _values.get()); }

  void wait_ff(NNet &nnet, OSI::Semaphore sem_ipc[NNAux::maxnum_nipc],
	       SharedIPC *pipc[NNAux::maxnum_nipc]) noexcept {
    nnet.wait_ff(_wait_id);
    for (uint u = 0; u < _ubatch; ++u) {
      uint id = _ids[u];
      assert(pipc[id] && sem_ipc[id].ok());
      pipc[id]->value = _values[u];
      copy_n(&(_probs[u * SAux::maxsize_moves]), _sizes_nnmove[u],
	     pipc[id]->probs);
      sem_ipc[id].inc(); }
    _ubatch = 0; }

  bool is_full() const noexcept { return _ubatch == _size_batch; }
  bool is_empty() const noexcept { return _ubatch == 0; }
};

void NNetService::worker_wait() noexcept {
  SharedIPC *pipc[NNAux::maxnum_nipc];
  for (uint u = 0; u < _nipc; ++u)
    pipc[u] = static_cast<SharedIPC *>(_mmap_ipc[u]());

  while (true) {
    unique_lock<mutex> lock(_m_entries);
    _cv_entries.wait(lock, [&]{ return ! _entries.empty(); });
    Entry e(move(_entries.front()));
    _entries.pop_front();
    lock.unlock();

    e.wait_ff(*_pnnet, _sem_ipc, pipc); } }

void NNetService::worker_push() noexcept {
  SharedService *pservice = static_cast<SharedService *>(_mmap_service());
  SharedIPC *pipc[NNAux::maxnum_nipc];
  for (uint u = 0; u < _nipc; ++u)
    pipc[u] = static_cast<SharedIPC *>(_mmap_ipc[u]());

  uint id;
  Type type;
  bool do_flush = false;
  unique_ptr<Entry> ptr_e(new Entry(_size_batch));
  while (true) {
    assert(_sem_service.ok() && _sem_service_lock.ok());
    _sem_service.dec_wait();
    _sem_service_lock.dec_wait();
    assert(0 < pservice->njob);
    pservice->njob -= 1U;
    id   = pservice->jobs[0].id;
    type = pservice->jobs[0].type;
    for (uint u = 0; u <  pservice->njob; u += 1U)
      pservice->jobs[u] = pservice->jobs[u + 1U];
    _sem_service_lock.inc();
    assert(id < _nipc || id == NNAux::maxnum_nipc);

    if (type == Type::End) {
      std::cerr << "Terminate (nnet_id: " << _nnet_id << ")" << std::endl;
      return; }

    if (type == Type::Register) {
      assert(pipc[id] && pipc[id]->nnet_id == _nnet_id && _sem_ipc[id].ok());
      _sem_ipc[id].inc();
      std::cerr << "Register id: " << id << " nnet_id: " << _nnet_id
		<< std::endl;
      continue; }

    if (type == Type::NNReset) {
      FName fn;
      {
	lock_guard<mutex> lock(_m_nnreset);
	_sem_service_lock.dec_wait();
	pservice->id_ipc_next = 0;
	_sem_service_lock.inc();
	_flag_cv_nnreset = true;
	fn = _fname;
      }
      _cv_nnreset.notify_one();
      
      uint version;
      uint64_t digest;
      NNAux::wght_t wght = NNAux::read(fn, version, digest);
      _pnnet.reset(new NNet);
#if defined(USE_OPENCL_AOBA)
      cout << "Tuning feed-forward engine of device "
	   << static_cast<int>(_device_id) << " for " << fn.get_fname()
	   << std::endl;
      cout << _pnnet->reset(_size_batch, wght, _device_id, _use_half,
			    false, true)
	   << std::flush;
#else
      _pnnet->reset(_size_batch, wght);
#endif
      continue; }
      
    if (type == Type::FeedForward) {
      assert(pipc[id]);
      ptr_e->add(pipc[id]->input, pipc[id]->size_nnmove, pipc[id]->nnmoves,
		 id);
      if (! do_flush && ! ptr_e->is_full()) continue;

      ptr_e->push_ff(*_pnnet);
      {
	lock_guard<mutex> lock(_m_entries);
	_entries.push_back(move(*ptr_e));
      }
      _cv_entries.notify_one();
      ptr_e.reset(new Entry(_size_batch));
      continue; }

    if (type == Type::FlushON) {
      {
	lock_guard<mutex> lock(_m_flush);
	_flag_cv_flush = true;
      }
      _cv_flush.notify_one();
      do_flush = true;

      if (ptr_e->is_empty()) continue;
      ptr_e->push_ff(*_pnnet);
      {
	lock_guard<mutex> lock(_m_entries);
	_entries.push_back(move(*ptr_e));
      }
      _cv_entries.notify_one();
      ptr_e.reset(new Entry(_size_batch));
      continue; }

    if (type == Type::FlushOFF) {
      {
	lock_guard<mutex> lock(_m_flush);
	_flag_cv_flush = true;
      }
      _cv_flush.notify_one();
      do_flush = false;
      continue; }

    die(ERR_INT("INTERNAL ERROR")); } }

void NNetService::nnreset(const FName &fname) noexcept {
  assert(_mmap_service.ok() && fname.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());

  unique_lock<mutex> lock(_m_nnreset);
  _fname = fname;
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = Type::NNReset;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();

  _cv_nnreset.wait(lock, [&]{ return _flag_cv_nnreset; });
  _flag_cv_nnreset = false;
  lock.unlock(); }

void NNetService::flush_on() noexcept {
  assert(_mmap_service.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = Type::FlushON;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();

  unique_lock<mutex> lock(_m_flush);
  _cv_flush.wait(lock, [&]{ return _flag_cv_flush; });
  _flag_cv_flush = false;
  lock.unlock(); }

void NNetService::flush_off() noexcept {
  assert(_mmap_service.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = Type::FlushOFF;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();

  unique_lock<mutex> lock(_m_flush);
  _cv_flush.wait(lock, [&]{ return _flag_cv_flush; });
  _flag_cv_flush = false;
  lock.unlock(); }

NNetService::NNetService(uint nnet_id, uint nipc, uint size_batch,
			 uint device_id, uint use_half) noexcept
: _flag_cv_flush(false), _flag_cv_nnreset(false), _nnet_id(nnet_id),
  _nipc(nipc), _size_batch(size_batch), _device_id(device_id),
  _use_half(use_half) {
  if (NNAux::maxnum_nnet <= nnet_id) die(ERR_INT("too many nnets"));
  if (NNAux::maxnum_nipc < nipc)     die(ERR_INT("too many processes"));

  char fn[IOAux::maxlen_path + 256U];
  uint pid = OSI::get_pid();
  sprintf(fn, "%s.%07u.%03u", Param::name_sem_lock_nnet, pid, nnet_id);
  _sem_service_lock.open(fn, true, 1U);
  sprintf(fn, "%s.%07u.%03u", Param::name_sem_nnet, pid, nnet_id);
  _sem_service.open(fn, true, 0U);
  sprintf(fn, "%s.%07u.%03u", Param::name_mmap_nnet, pid, nnet_id);
  _mmap_service.open(fn, true, sizeof(SharedService));
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p);
  p->id_ipc_next = 0;
  p->njob = 0;
  for (uint u = 0; u < nipc; ++u) {
    sprintf(fn, "%s.%07u.%03u.%03u", Param::name_sem_nnet, pid, nnet_id, u);
    _sem_ipc[u].open(fn, true, 0);
    sprintf(fn, "%s.%07u.%03u.%03u", Param::name_mmap_nnet, pid, nnet_id, u);
    _mmap_ipc[u].open(fn, true, sizeof(SharedIPC)); }

  _th_worker_push = thread(&NNetService::worker_push, this);
  _th_worker_wait = thread(&NNetService::worker_wait, this); }

NNetService::NNetService(uint nnet_id, uint nipc, uint size_batch,
			 uint device_id, uint use_half, const FName &fname)
noexcept : NNetService(nnet_id, nipc, size_batch, device_id, use_half) {
  nnreset(fname); }

NNetService::~NNetService() noexcept {
  assert(_mmap_service.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = Type::End;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();
  _th_worker_push.join();

  _sem_service_lock.close();
  _sem_service.close();
  _mmap_service.close();

  for (uint u = 0; u < _nipc; ++u) {
    _sem_ipc[u].inc();
    _sem_ipc[u].close();
    _mmap_ipc[u].close(); } }

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
  _pservice->jobs[_pservice->njob].type = Type::Register;
  _pservice->njob += 1U;
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

int NNetIPC::get_id() const noexcept { assert(ok()); return _id; }

float *NNetIPC::get_input() const noexcept {
  assert(ok() && _pipc); return _pipc->input; }

ushort *NNetIPC::get_nnmoves() const noexcept {
  assert(ok() && _pipc); return _pipc->nnmoves; }

const float *NNetIPC::get_probs() const noexcept {
  assert(ok() && _pipc); return _pipc->probs; }

float NNetIPC::get_value() const noexcept {
  assert(ok() && _pipc); return _pipc->value; }

int NNetIPC::submit_block(uint size_nnmove) noexcept {
  assert(ok() && size_nnmove <= SAux::maxsize_moves && _pipc);
  _pipc->size_nnmove = size_nnmove;
  
  assert(_sem_service_lock.ok() && _sem_service.ok());
  if (sem_wait(_sem_service_lock) < 0) return -1;
  assert(_pservice->njob <= NNAux::maxnum_nipc);
  _pservice->jobs[_pservice->njob].id   = _id;
  _pservice->jobs[_pservice->njob].type = Type::FeedForward;
  _pservice->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();
  return sem_wait(_sem_ipc); }
