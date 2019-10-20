#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ipc.hpp"
#include "param.hpp"
#include <iostream>
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
using std::map;
using std::string;
using std::thread;
using ErrAux::die;

using uint = unsigned int;
enum class Task : uint { Register, FF };

struct SharedService {
  uint id_ipc_next;
  uint nn;
  struct { uint id_ipc; Task task; } elements[NNAux::size_ipc]; };

struct SharedIPC {
  uint no_evaluator;
  FName fn_weights; };

void NNetService::worker() noexcept {
  SharedService *pservice = static_cast<SharedService *>(_mmap_service());
  SharedIPC *pipc[NNAux::size_ipc];
  for (uint u = 0; u < _nipc; ++u)
    pipc[u] = static_cast<SharedIPC *>(_mmap_ipc[u]());

  uint id_ipc;
  Task task;
  uint id_wght_next = 0;
  map<string, uint> map_str_id_wght;
  uint array_id_ipc_wght[NNAux::size_ipc];
  while (true) {
    _sem_service.dec_wait();
    _sem_service_lock.dec_wait();
    assert(0 < pservice->nn);
    pservice->nn -= 1U;
    id_ipc = pservice->elements[pservice->nn].id_ipc;
    task   = pservice->elements[pservice->nn].task;
    _sem_service_lock.inc();
    assert(id_ipc < _nipc);

    if (task == Task::Register) {
      string s(pipc[id_ipc]->fn_weights.get_fname());
      if (map_str_id_wght.find(s) == map_str_id_wght.end())
	map_str_id_wght[s] = id_wght_next++;
      uint id_wght = map_str_id_wght[s];
      array_id_ipc_wght[id_ipc] = id_wght;
      _sem_ipc[id_ipc].inc();
      std::cerr << "id_ipc: " << id_ipc
		<< " id_wght: " << id_wght
		<< " fname: " << s << std::endl; }
    else { die(ERR_INT("INTERNAL ERROR")); } } }

NNetService &NNetService::get() noexcept {
  static NNetService inst;
  return inst; }

void NNetService::start(uint n) noexcept {
  assert(n < NNAux::size_ipc);
  _nipc = n;
  _sem_service_lock.open(Param::name_sem_lock_nnet, true, 1U);
  _sem_service.open(Param::name_sem_nnet, true, 0U);
  _mmap_service.open(Param::name_mmap_nnet, true, sizeof(SharedService));
  auto p = static_cast<SharedService *>(_mmap_service());
  p->id_ipc_next = 0;
  p->nn = 0;

  for (uint u = 0; u < n; ++u) {
    char fn_sem[IOAux::maxlen_path + 256U];
    char fn_mmap[IOAux::maxlen_path + 256U];
    sprintf(fn_sem, "%s.%06u", Param::name_sem_nnet, u);
    sprintf(fn_mmap, "%s.%06u", Param::name_mmap_nnet, u);
    _sem_ipc[u].open(fn_sem, true, 0);
    _mmap_ipc[u].open(fn_mmap, true, sizeof(SharedIPC)); }

  _th_worker = thread(&NNetService::worker, this);
  _th_worker.detach(); }

void NNetService::end() noexcept {
  _sem_service_lock.close();
  _sem_service.close();
  _mmap_service.close();
  for (uint u = 0; u < _nipc; ++u) {
    _sem_ipc[u].close();
    _mmap_ipc[u].close(); } }

void NNetIPC::start(const char *str_wght) noexcept {
  assert(str_wght);
  _sem_service_lock.open(Param::name_sem_lock_nnet, false, 1U);
  _sem_service.open(Param::name_sem_nnet, false, 0U);
  _mmap_service.open(Param::name_mmap_nnet, false, sizeof(SharedService));
  SharedService *pservice = static_cast<SharedService *>(_mmap_service());
  _sem_service_lock.dec_wait();
  _id = pservice->id_ipc_next++;
  _sem_service_lock.inc();
  if (NNAux::size_ipc <= static_cast<uint>(_id))
    die(ERR_INT("too many processes"));

  char fn_sem[IOAux::maxlen_path + 256U];
  char fn_mmap[IOAux::maxlen_path + 256U];
  sprintf(fn_sem, "%s.%06u", Param::name_sem_nnet, _id);
  sprintf(fn_mmap, "%s.%06u", Param::name_mmap_nnet, _id);
  _sem_ipc.open(fn_sem, false, 0);
  _mmap_ipc.open(fn_mmap, false, sizeof(SharedIPC));
  SharedIPC *pipc = static_cast<SharedIPC *>(_mmap_ipc());

  pipc->fn_weights.reset_fname(str_wght);
  _sem_service_lock.dec_wait();
  pservice->elements[pservice->nn].id_ipc = _id;
  pservice->elements[pservice->nn].task = Task::Register;
  pservice->nn += 1U;
  _sem_service_lock.inc();
  _sem_service.inc();

  _sem_ipc.dec_wait(); }

void NNetIPC::end() noexcept {
  _sem_service_lock.close();
  _sem_service.close();
  _mmap_service.close();
  _sem_ipc.close();
  _mmap_ipc.close(); }
