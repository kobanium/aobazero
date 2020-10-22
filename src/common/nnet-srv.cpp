// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "err.hpp"
#include "nnet-cpu.hpp"
#include "nnet-ocl.hpp"
#include "nnet-srv.hpp"
#include "param.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <cassert>
#include <cstdio>
using std::copy_n;
using std::fill_n;
using std::lock_guard;
using std::move;
using std::mt19937_64;
using std::mutex;
using std::string;
using std::thread;
using std::to_string;
using std::unique_lock;
using std::unique_ptr;
using ErrAux::die;
using uint = unsigned int;

SeqPRNService::SeqPRNService() noexcept {
  char fn[IOAux::maxlen_path + 256U];
  uint pid = OSI::get_pid();
  sprintf(fn, "%s.%07u", Param::name_seq_prn, pid);
  _mmap.open(fn, true, sizeof(uint64_t) * Param::len_seq_prn);
  uint64_t *p = static_cast<uint64_t *>(_mmap());
  mt19937_64 mt(7);
  for (uint u = 0; u < Param::len_seq_prn; ++u) p[u] = mt(); }

class Entry {
  unique_ptr<NNInBatchBase> _nn_in_b;
  unique_ptr<float []> _probs;
  unique_ptr<float []> _values;
  unique_ptr<uint []> _ids;
  uint _size_batch, _wait_id;
  bool _do_compress;

public:
  explicit Entry(uint size_batch, bool do_compress) noexcept :
  _probs(new float [size_batch * SAux::maxsize_moves]),
    _values(new float [size_batch]), _ids(new uint [size_batch]),
    _size_batch(size_batch), _wait_id(0), _do_compress(do_compress) {
    if (do_compress) _nn_in_b.reset(new NNInBatchCompressed(size_batch));
    else             _nn_in_b.reset(new NNInBatch(size_batch)); }

  void add(const SharedIPC *pipc, uint id) noexcept {
    assert(pipc && id < NNAux::maxnum_nipc && ! is_full());
    _ids[ _nn_in_b->get_ub() ] = id;
    if (_do_compress)
      static_cast<NNInBatchCompressed *>(_nn_in_b.get())
	->add(pipc->n_one, pipc->compressed_features,
	      pipc->size_nnmove, pipc->nnmove);
    else
      static_cast<NNInBatch *>(_nn_in_b.get())
	->add(pipc->features, pipc->size_nnmove, pipc->nnmove); }

  void push_ff(NNet &nnet) noexcept {
    assert(_nn_in_b && ! is_empty());
    if (_do_compress) {
      NNInBatchCompressed *p
	= static_cast<NNInBatchCompressed *>(_nn_in_b.get());
      _wait_id = nnet.push_ff(*p, _probs.get(), _values.get()); }
    else {
      NNInBatch *p = static_cast<NNInBatch *>(_nn_in_b.get());
      _wait_id = nnet.push_ff(p->get_ub(), p->get_features(),
			      p->get_sizes_nnmove(), p->get_nnmoves(),
			      _probs.get(), _values.get()); } }

  void wait_ff(NNet &nnet, OSI::Semaphore sem_ipc[NNAux::maxnum_nipc],
	       SharedIPC *pipc[NNAux::maxnum_nipc]) noexcept {
    nnet.wait_ff(_wait_id);
    for (uint u = 0; u < _nn_in_b->get_ub(); ++u) {
      uint id = _ids[u];
      assert(pipc[id] && sem_ipc[id].ok());
      pipc[id]->value = _values[u];
      copy_n(&(_probs[u * SAux::maxsize_moves]),
	     _nn_in_b->get_sizes_nnmove()[u], pipc[id]->probs);
      sem_ipc[id].inc(); }
    _nn_in_b->erase(); }
  
  bool is_full() const noexcept  { return _nn_in_b->get_ub() == _size_batch; }
  bool is_empty() const noexcept { return _nn_in_b->get_ub() == 0; } };

void NNetService::worker_push() noexcept {
  unique_ptr<Entry> pe;
  while (true) {
    unique_lock<mutex> lock(_m_entries);
    _cv_entries_push.wait(lock, [&]{
	if (_flag_quit) return true;
	if (_entries_push.empty()) return false;
	if (16U < _entries_wait.size()) return false;
	if (_entries_wait.size() < 1U) return true;
	if (_entries_push.front()->is_full()) return true;
	return false; });
    if (_flag_quit) return;
    pe = move(_entries_push.front());
    _entries_push.pop_front();
    lock.unlock();
    
    pe->push_ff(*_pnnet);

    lock.lock();
    _entries_wait.push_back(move(pe));
    lock.unlock();
    _cv_entries_wait.notify_one(); } }

void NNetService::worker_wait() noexcept {
  SharedIPC *pipc[NNAux::maxnum_nipc];
  for (uint u = 0; u < _nipc; ++u)
    pipc[u] = static_cast<SharedIPC *>(_mmap_ipc[u]());

  unique_ptr<Entry> pe;
  while (true) {
    unique_lock<mutex> lock(_m_entries);
    _cv_entries_wait.wait(lock, [&]{
	return (0 < _entries_wait.size() || _flag_quit); });
    if (_flag_quit) return;
    pe = move(_entries_wait.front());
    _entries_wait.pop_front();
    lock.unlock();

    pe->wait_ff(*_pnnet, _sem_ipc, pipc);

    lock.lock();
    _entries_pool.push_back(move(pe));
    lock.unlock();
    _cv_entries_push.notify_one(); } }

void NNetService::worker_srv() noexcept {
  SharedService *pservice = static_cast<SharedService *>(_mmap_service());
  SharedIPC *pipc[NNAux::maxnum_nipc];
  for (uint u = 0; u < _nipc; ++u)
    pipc[u] = static_cast<SharedIPC *>(_mmap_ipc[u]());

  uint id;
  SrvType type;
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

    if (type == SrvType::End) {
      string s("Terminate (nnet_id: ");
      s += to_string(_nnet_id) + string(")");
      std::cerr << s << std::endl;
      return; }

    if (type == SrvType::Register) {
      assert(pipc[id] && pipc[id]->nnet_id == _nnet_id && _sem_ipc[id].ok());
      _sem_ipc[id].inc();
      string s("Register id: ");
      s += to_string(id) + string(" nnet_id: ") + to_string(_nnet_id);
      std::cerr << s << std::endl;
      continue; }

    if (type == SrvType::NOP) {
      {
	std::lock_guard<mutex> lock(_m_srv);
	_flag_srv = true;
      }
      _cv_srv.notify_one();
      continue; }

    if (type == SrvType::NNReset) {
      FName fn;
      lock_guard<mutex> lock_push(_m_entries);
      if (!_entries_push.empty()) die(ERR_INT("Internal Error"));
      if (!_entries_wait.empty()) die(ERR_INT("Internal Error"));
	
      if (_impl == NNet::cpublas) {
	_pnnet.reset(new NNetCPU);
	_sem_service_lock.dec_wait();
	pservice->id_ipc_next = 0;
	pservice->do_compress = _pnnet->do_compress();
	uint njob = pservice->njob;
	_sem_service_lock.inc();
	if (0 < njob) die(ERR_INT("Internal Error"));

	fn = _fname;
	{
	  std::lock_guard<mutex> lock(_m_srv);
	  _flag_srv = true;
	}
	_cv_srv.notify_one();
	static_cast<NNetCPU *>(_pnnet.get())->reset(_size_batch,
						    NNAux::read(fn),
						    _thread_num); }
      else if (_impl == NNet::opencl) {
	_pnnet.reset(new NNetOCL);
	_sem_service_lock.dec_wait();
	pservice->id_ipc_next = 0;
	pservice->do_compress = _pnnet->do_compress();
	uint njob = pservice->njob;
	_sem_service_lock.inc();
	if (0 < njob) die(ERR_INT("Internal Error"));
	
	fn = _fname;
	string s("Tuning feed-forward engine of device ");
	s += to_string(static_cast<int>(_device_id)) + string(" for ");
	s += string(fn.get_fname()) + string("\n");
	std::cout << s << std::flush;

	{
	  std::lock_guard<mutex> lock(_m_srv);
	  _flag_srv = true;
	}
	_cv_srv.notify_one();
	NNetOCL *p = static_cast<NNetOCL *>(_pnnet.get());
	std::cout << p->reset(_size_batch, NNAux::read(fn), _device_id,
			      _use_half, _use_wmma, false, _do_sleep,
			      _dtune.c_str())
		  << std::flush; }
      else die(ERR_INT("INTERNAL ERROR"));
      continue; }
      
    if (type == SrvType::FeedForward) {
      assert(pipc[id]);
      bool flag_do_notify = false;
      {
	lock_guard<mutex> lock(_m_entries);
	if (_entries_push.empty()) flag_do_notify = true;
	if (_entries_push.empty() || _entries_push.back()->is_full()) {
	  if (_entries_pool.empty())
	    _entries_pool.emplace_back(new Entry(_size_batch,
						 _pnnet->do_compress()));

	  unique_ptr<Entry> pe = move(_entries_pool.back());
	  _entries_pool.pop_back();
	  _entries_push.push_back(move(pe)); }

	_entries_push.back()->add(pipc[id], id);

	if (_entries_push.size() == 1 && _entries_push.back()->is_full())
	  flag_do_notify = true; }

      if (flag_do_notify) _cv_entries_push.notify_one();
      continue; }

    die(ERR_INT("INTERNAL ERROR")); } }

void NNetService::wait() noexcept {
  unique_lock<mutex> lock(_m_srv);
  assert(_mmap_service.ok());
  auto p = static_cast<SharedService *>(_mmap_service());

  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = SrvType::NOP;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();
  _cv_srv.wait(lock, [&]{ return _flag_srv; });
  _flag_srv = false; }

void NNetService::nnreset(const FName &fname) noexcept {
  unique_lock<mutex> lock(_m_srv);
  assert(_mmap_service.ok() && fname.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());

  _fname = fname;
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = SrvType::NNReset;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();
  _cv_srv.wait(lock, [&]{ return _flag_srv; });
  _flag_srv = false; }

NNetService::NNetService(NNet::Impl impl, uint nnet_id, uint nipc,
			 uint size_batch, uint device_id, uint use_half,
			 uint use_wmma, uint thread_num, bool do_sleep,
			 const char *dtune)
  noexcept : _impl(impl), _flag_srv(false), _flag_quit(false), _dtune(dtune),
	     _nnet_id(nnet_id), _nipc(nipc), _size_batch(size_batch),
	     _device_id(device_id), _use_half(use_half), _use_wmma(use_wmma),
  _thread_num(thread_num), _do_sleep(do_sleep) {
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

  _th_worker_srv  = thread(&NNetService::worker_srv,  this);
  _th_worker_push = thread(&NNetService::worker_push, this);
  _th_worker_wait = thread(&NNetService::worker_wait, this); }

NNetService::NNetService(NNet::Impl impl, uint nnet_id, uint nipc,
			 uint size_batch, uint device_id, uint use_half,
			 uint use_wmma, uint thread_num, bool do_sleep,
			 const FName &fname, const char *dtune)
  noexcept : NNetService(impl, nnet_id, nipc, size_batch, device_id,
			 use_half, use_wmma, thread_num, do_sleep, dtune) {
  nnreset(fname); }

NNetService::~NNetService() noexcept {
  lock_guard<mutex> guard(_m_srv);
  assert(_mmap_service.ok());
  auto p = static_cast<SharedService *>(_mmap_service());
  assert(p && _sem_service.ok() && _sem_service_lock.ok());
  _sem_service_lock.dec_wait();
  assert(p->njob <= NNAux::maxnum_nipc);
  p->jobs[p->njob].id   = NNAux::maxnum_nipc;
  p->jobs[p->njob].type = SrvType::End;
  p->njob += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();

  {
    lock_guard<mutex> lock(_m_entries);
    _flag_quit = true; }
  _cv_entries_wait.notify_one();
  _cv_entries_push.notify_one();

  _th_worker_srv.join();
  _th_worker_push.join();
  _th_worker_wait.join();

  _sem_service_lock.close();
  _sem_service.close();
  _mmap_service.close();

  for (uint u = 0; u < _nipc; ++u) {
    _sem_ipc[u].inc();
    _sem_ipc[u].close();
    _mmap_ipc[u].close(); } }
