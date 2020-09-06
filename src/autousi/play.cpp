// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "child.hpp"
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-srv.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "play.hpp"
#include "shogibase.hpp"
#include <chrono>
#include <queue>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <utility>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
using std::cout;
using std::deque;
using std::endl;
using std::flush;
using std::ios;
using std::lock_guard;
using std::move;
using std::mutex;
using std::ofstream;
using std::queue;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using ErrAux::die;
using namespace IOAux;
using uint = unsigned int;

constexpr double time_average_rate = 0.999;
constexpr char fmt_log[]           = "engine%03u-%03u.log";
static mutex m_seq;
static deque<SeqPRNService> seq_s;

class Device {
  enum class Type : unsigned char { Aobaz, NNService, Bad };
  class DataNNService {
    NNetService _nnet;
  public:
    DataNNService(NNet::Impl impl, uint nnet_id, const char *dtune,
		  uint size_parallel, uint size_batch, uint id, uint use_half,
		  uint thread_num)
      noexcept : _nnet(impl, nnet_id, size_parallel, size_batch, id, use_half,
		       thread_num, dtune) {
      lock_guard<mutex> lock(m_seq);
      if (seq_s.empty()) seq_s.emplace_back(); }
    void nnreset(const FName &fname) noexcept { _nnet.nnreset(fname); }
    void wait() noexcept { _nnet.wait(); } };
  unique_ptr<DataNNService> _data_nnservice;
  int _nnet_id, _device_id;
  uint _size_parallel;
  uint _size_batch;
  bool _flag_half;
  Type _type;

public:
  static constexpr Type aobaz     = Type::Aobaz;
  static constexpr Type nnservice = Type::NNService;
  static constexpr Type bad       = Type::Bad;
  explicit Device(const string &s, int nnet_id, const char *dtune) noexcept :
    _nnet_id(-1), _size_parallel(1U), _flag_half(false), _type(bad) {
    if (s.empty()) die(ERR_INT("invalid device %s", s.c_str()));
    char *endptr;
    if (s[0] == 'B' || s[0] == 'b') {
      const char *token = s.c_str() + 1;
      long int thread_num = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != ':' || thread_num < -1
	  || thread_num == LONG_MAX) die(ERR_INT("invalid device %s", token));
      token = endptr + 1;
      
      long int size_batch = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != ':' || size_batch < 1
	  || size_batch == LONG_MAX) die(ERR_INT("invalid device %s",
						 s.c_str()));
      token = endptr + 1;
      
      long int size_parallel = strtol(token, &endptr, 10);
      if (*endptr != '\0' || endptr == token || size_parallel < 1
	  || size_parallel == LONG_MAX)
	die(ERR_INT("invalid device %s", s.c_str()));
      
      _device_id     = -2;
      _nnet_id       = nnet_id;
      _type          = nnservice;
      _size_parallel = size_parallel;
      _size_batch    = size_batch;
      _data_nnservice.reset(new DataNNService(NNet::cpublas, _nnet_id, dtune,
					      _size_parallel, _size_batch,
					      _device_id, _flag_half,
					      thread_num)); }
    else if (s[0] == 'O' || s[0] == 'o') {
      const char *token = s.c_str() + 1;
      long int device_id = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != ':' || device_id < -1
	  || device_id == LONG_MAX) die(ERR_INT("invalid device %s", token));
      token = endptr + 1;
      
      long int size_batch = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != ':' || size_batch < 1
	  || size_batch == LONG_MAX) die(ERR_INT("invalid device %s",
						 s.c_str()));
      token = endptr + 1;
      
      long int size_parallel = strtol(token, &endptr, 10);
      if ((*endptr != '\0' && *endptr != 'H' && *endptr != 'h')
	  || endptr == token || size_parallel < 1 || size_parallel == LONG_MAX)
	die(ERR_INT("invalid device %s", s.c_str()));
      
      if (*endptr == '\0') _flag_half = false;
      else if ((*endptr == 'H' || *endptr == 'h') && endptr[1] == '\0')
	_flag_half = true;

      _device_id     = device_id;
      _nnet_id       = nnet_id;
      _type          = nnservice;
      _size_parallel = size_parallel;
      _size_batch    = size_batch;
      _data_nnservice.reset(new DataNNService(NNet::opencl, _nnet_id, dtune,
					      _size_parallel, _size_batch,
					      _device_id, _flag_half, 0)); }
    else {
      const char *token = s.c_str();
      _nnet_id = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != '\0' || _nnet_id < -1
	  || 65535 < _nnet_id) die(ERR_INT("invalid device %s", s.c_str()));
      _device_id = _nnet_id;
      _type = aobaz; } }
  void nnreset(const FName &wfname) noexcept {
    if (_type == nnservice) _data_nnservice->nnreset(wfname); }
  void wait() noexcept { if (_type == nnservice) _data_nnservice->wait(); }
  int get_device_id() const noexcept { return _device_id; }
  int get_nnet_id() const noexcept { return _nnet_id; }
  char get_id_option_character() const noexcept {
    return _type == nnservice ? 'e' : 'u'; }
  uint get_size_parallel() const noexcept { return _size_parallel; } };
constexpr Device::Type Device::aobaz;
constexpr Device::Type Device::nnservice;
constexpr Device::Type Device::bad;

class USIEngine : public Child {
  using uint = unsigned int;
  Node<Param::maxlen_play_learn> _node;
  steady_clock::time_point _time_last;
  double _time_average_nume, _time_average_deno, _time_average;
  FName _logname;
  ofstream _ofs;
  string _startpos, _record_main, _fingerprint;
  string _record_wght, _record_version, _record_settings;
  int64_t _id_wght;
  uint64_t _crc64_wght;
  int _device_id, _nnet_id, _version;
  uint _eid, _nmove;
  bool _flag_playing, _flag_ready, _flag_thinking;

  void out_log(const char *p) noexcept {
    assert(_ofs && p);
    _ofs << p << endl;
    if (!_ofs) die(ERR_INT("cannot write to log")); }

  void engine_out(const char *fmt, ...) noexcept {
    assert(_ofs && Child::ok() && ! Child::is_closed() && fmt);
    char buf[65536];
    va_list argList;
  
    va_start(argList, fmt);
    int nb = vsnprintf(buf, sizeof(buf), fmt, argList);
    va_end(argList);
    if (sizeof(buf) <= static_cast<size_t>(nb) + 1U)
      die(ERR_INT("buffer overrun (engine %s)", get_fp()));

    out_log(buf);
    buf[nb]     = '\n';
    buf[nb + 1] = '\0';
    if (!Child::write(buf, strlen(buf))) {
      if (errno == EPIPE) die(ERR_INT("engine %s terminates", get_fp()));
      die(ERR_CLL("write")); } }

public:
  explicit USIEngine(const FName &cname, char ch, int device_id, int nnet_id,
		     uint eid, const FNameID &wfname, uint64_t crc64,
		     uint verbose_eng, const FName &logname) noexcept :
  _time_average_nume(0.0), _time_average_deno(0.0), _time_average(0.0),
    _logname(logname),
    _fingerprint(to_string(nnet_id) + string("-") + to_string(eid)),
    _id_wght(wfname.get_id()), _crc64_wght(crc64),
    _device_id(device_id), _nnet_id(nnet_id), _version(-1), _eid(eid),
    _nmove(0),
    _flag_playing(false), _flag_ready(false), _flag_thinking(false) {
    assert(cname.ok() && 0 < cname.get_len_fname());
    assert(wfname.ok() && 0 < wfname.get_len_fname());
    assert(isalnum(ch) && -2 < nnet_id && nnet_id < 65536);
    char *argv[256];
    int argc = 0;

    unique_ptr<char []> a0(new char [cname.get_len_fname() + 1U]);
    memcpy(a0.get(), cname.get_fname(), cname.get_len_fname() + 1U);
    argv[argc++] = a0.get();

    char opt_q[] = "-q";
    if (!verbose_eng) argv[argc++] = opt_q;

    char opt_p[]       = "-p";
    char opt_p_value[] = "800";
    argv[argc++] = opt_p;
    argv[argc++] = opt_p_value;

    char opt_n[] = "-n";
    argv[argc++] = opt_n;

    char opt_m[]       = "-m";
    char opt_m_value[] = "30";
    argv[argc++] = opt_m;
    argv[argc++] = opt_m_value;

    char opt_w[] = "-w";
    unique_ptr<char []> opt_w_value(new char [wfname.get_len_fname() + 1U]);
    memcpy(opt_w_value.get(),
	   wfname.get_fname(), wfname.get_len_fname() + 1U);
    argv[argc++] = opt_w;
    argv[argc++] = opt_w_value.get();

    char opt_u[] = "-u";
    char opt_u_value[256];
    opt_u[1] = ch;
    sprintf(opt_u_value, "%i", nnet_id);
    argv[argc++] = opt_u;
    argv[argc++] = opt_u_value;

    assert(argc < 256);
    unique_ptr<char []> path(new char [cname.get_len_fname() + 1U]);
    memcpy(path.get(), cname.get_fname(), cname.get_len_fname() + 1U);
    argv[argc] = nullptr;
    Child::open(path.get(), argv);

    _logname.add_fmt_fname(fmt_log, nnet_id, eid);
    _ofs.open(_logname.get_fname(), ios::trunc);
    if (!_ofs) die(ERR_INT("cannot write to log"));
    engine_out("usi"); }

  void start_newgame() noexcept {
    _node.clear();
    _flag_playing  = true;
    _flag_thinking = false;
    _record_main   = string("PI\n+\n");
    _nmove         = 0;
    _startpos      = string("position startpos moves");

    char buf[256];
    sprintf(buf, "%16" PRIx64, _crc64_wght);
    _record_wght  = string("'w ") + to_string(_id_wght);
    _record_wght += string(" (crc64:") + string(buf) + string(")");
    engine_out("usinewgame"); }

  void engine_go() noexcept {
    _flag_thinking = true;
    engine_out("%s", _startpos.c_str());
    engine_out("go visit");
    _time_last = steady_clock::now(); }

  void engine_wght_update(const FNameID &wfname, uint64_t crc64) noexcept {
    _record_wght += string(" and later");
    _id_wght    = wfname.get_id();
    _crc64_wght = crc64;
    engine_out("setoption name USI_WeightFile value %s", wfname.get_fname()); }

  string update(char *line, queue<string> &moves_eid0) noexcept {
    assert(line);
    char *token, *saveptr;
    token = OSI::strtok(line, " ,", &saveptr);

    if (!_flag_ready && strcmp(line, "usiok") == 0) {
      _flag_ready = true;
      if (_version < 0) die(ERR_INT("No version infomation from engine %s",
				    get_fp()));
      _record_version  = string("autousi ") + to_string(Ver::major);
      _record_version += string(".") + to_string(Ver::minor);
      _record_version += string(", usi-engine ") + to_string(_version);
      engine_out("isready");
      return string(""); }

    if (!_flag_ready && strcmp(token, "id") == 0) {
      token = OSI::strtok(nullptr, " ", &saveptr);
      if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));

      if (strcmp(token, "settings") == 0) {
	token = OSI::strtok(nullptr, "", &saveptr);
	if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));
	_record_settings = string(token); }

      else if (strcmp(token, "name") == 0) {
	if (! OSI::strtok(nullptr, " ", &saveptr))
	  die(ERR_INT("Bad message from engine (%s).", get_fp()));
	  
	token = OSI::strtok(nullptr, " ", &saveptr);
	if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));
	  
	char *endptr;
	long int ver = strtol(token, &endptr, 10);
	if (endptr == token || *endptr != '\0' || ver < 0 || 65535 < ver)
	  die(ERR_INT("Bad message from engine (%s).", get_fp()));
	_version = ver; }

      return string(""); }

    if (!_flag_ready) die(ERR_INT("Bad message from engine (%s).", get_fp()));

    if (strcmp(token, "bestmove") != 0) return string("");
    if (!_flag_playing && !_flag_thinking)
      die(ERR_INT("bad usi message from engine %s", get_fp()));
    _flag_thinking = false;

    // read played move
    long int num_best = 0;
    long int num_tot  = 0;
    const char *str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
    if (!str_move_usi)
      die(ERR_INT("bad usi message from engine %s", get_fp()));

    Action actionPlay = _node.action_interpret(str_move_usi, SAux::usi);
    if (!actionPlay.ok())
      die(ERR_INT("cannot interpret candidate move %s (engine %s)\n%s",
		  str_move_usi, get_fp(),
		  static_cast<const char *>(_node.to_str())));

    if (actionPlay.is_move()) {
      steady_clock::time_point time_now = steady_clock::now();
      auto rep   = duration_cast<milliseconds>(time_now - _time_last).count();
      double dms = static_cast<double>(rep);
      _time_average_nume = _time_average_nume * time_average_rate + dms;
      _time_average_deno = _time_average_deno * time_average_rate + 1.0;
      _time_average      = _time_average_nume / _time_average_deno;
      _startpos         += " ";
      _startpos         += str_move_usi;
      _record_main      += _node.get_turn().to_str();
      _nmove            += 1U;
      _record_main      += actionPlay.to_str(SAux::csa);
      if (_eid == 0) {
	char buf[256];
	sprintf(buf, " (%5.0fms)", _time_average);
	string smove = _node.get_turn().to_str();
	smove += actionPlay.to_str(SAux::csa);
	smove += buf;
	moves_eid0.push(std::move(smove)); }
    
      const char *str_count = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_count) die(ERR_INT("cannot read count (engine %s)", get_fp()));
    
      char *endptr;
      long int num = strtol(str_count, &endptr, 10);
      if (endptr == str_count || *endptr != '\0' || num < 1
	  || num == LONG_MAX)
	die(ERR_INT("cannot interpret a visit count %s (engine %s)",
		    str_count, get_fp()));
    
      num_best = num;
      _record_main += ",'";
      _record_main += to_string(num);

      // read candidate moves
      while (true) {
	str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
	if (!str_move_usi) { _record_main += "\n"; break; }

	Action action = _node.action_interpret(str_move_usi, SAux::usi);
	if (!action.is_move())
	  die(ERR_INT("bad candidate %s (engine %s)", str_move_usi, get_fp()));
	_record_main += ",";
	_record_main += action.to_str(SAux::csa);
    
	str_count = OSI::strtok(nullptr, " ,", &saveptr);
	if (!str_count)
	  die(ERR_INT("cannot read count (engine %s)", get_fp()));

	num = strtol(str_count, &endptr, 10);
	if (endptr == str_count || *endptr != '\0'
	    || num < 1 || num == LONG_MAX)
	  die(ERR_INT("cannot interpret a visit count %s (engine %s)",
		      str_count, get_fp()));

	num_tot += num;
	_record_main += ",";
	_record_main += to_string(num); } }

    if (num_best < num_tot) die(ERR_INT("bad counts (engine %s)", get_fp()));
    _node.take_action(actionPlay);

    // force declare nyugyoku
    if (_node.get_type().is_interior() && _node.is_nyugyoku())
      _node.take_action(SAux::windecl);
    assert(_node.ok());

    // terminal test
    if (_node.get_type().is_term()) {
      string rec;
      rec += _record_wght + string(", ") + _record_version + string("\n");
      rec += string("'") + _record_settings + string("\n");
      rec += _record_main;
      rec += "%" + string(_node.get_type().to_str()) + string("\n");
      _flag_playing = false;
      return move(rec); }

    return string(""); }

  void engine_quit() noexcept { engine_out("quit"); }
  uint getline_in(char *line, uint size) noexcept {
    uint ret = Child::getline_in(line, size);
    if (ret) out_log(line);
    return ret; }
  uint getline_err(char *line, uint size) noexcept {
    uint ret = Child::getline_err(line, size);
    if (ret) out_log(line);
    return ret; }

  const char *get_fp() const noexcept { return _fingerprint.c_str(); }
  bool is_playing() const noexcept { return _flag_playing; }
  bool is_ready() const noexcept { return _flag_ready; }
  bool is_thinking() const noexcept { return _flag_thinking; }
  uint get_eid() const noexcept { return _eid; }
  uint get_nmove() const noexcept { return _nmove; }
  int get_did() const noexcept { return _device_id; }
  double get_time_average() const noexcept { return _time_average; }
};

PlayManager & PlayManager::get() noexcept {
  static PlayManager instance;
  return instance; }

PlayManager::PlayManager() noexcept : _ngen_records(0), _num_thinking(0) {}
PlayManager::~PlayManager() noexcept {}
void PlayManager::start(const char *cname, const char *dlog, const char *dtune,
			const vector<string> &devices_str, uint verbose_eng,
			const FNameID &wfname, uint64_t crc64) noexcept {
  assert(cname && dlog && dtune && wfname.ok() && _engines.empty());
  if (devices_str.empty()) die(ERR_INT("bad devices"));
  _verbose_eng = verbose_eng;
  _cname.reset_fname(cname);
  _logname.reset_fname(dlog);
  _wid = wfname.get_id();

  int nnet_id = 0;
  for (const string &s : devices_str) _devices.emplace_back(s, nnet_id++,
							    dtune);
  for (Device &d : _devices) d.nnreset(wfname);
  for (Device &d : _devices) d.wait();

  int eid = 0;
  for (Device &d : _devices) {
    uint size     = d.get_size_parallel();
    nnet_id       = d.get_nnet_id();
    int device_id = d.get_device_id();
    char ch       = d.get_id_option_character();
    for (uint u = 0; u < size; ++u)
      _engines.emplace_back(new USIEngine(_cname, ch, device_id, nnet_id,
					  eid++, wfname, crc64, _verbose_eng,
					  _logname)); } }

void PlayManager::end() noexcept {
  for (auto &e : _engines) e->engine_quit();
  while (!_engines.empty()) {
    Child::wait(1000U);
    for (auto it = _engines.begin(); it != _engines.end(); ) {
      bool flag_err = false;
      bool flag_in  = false;
      if ((*it)->has_line_err()) {
	char line[65536];
	if ((*it)->getline_err(line, sizeof(line)) == 0) flag_err = true; }
      
      if ((*it)->has_line_in()) {
	char line[65536];
	if ((*it)->getline_in(line, sizeof(line)) == 0) flag_in = true; }
      
      if (flag_err && flag_in) {
	(*it)->close();
	it = _engines.erase(it); }
      else ++it; } }

  _devices.clear();

  lock_guard<mutex> lock(m_seq);
  seq_s.clear(); }

deque<string> PlayManager::manage_play(bool has_conn, const FNameID &wfname,
				       uint64_t crc64) noexcept {
  deque<string> recs;

  Child::wait(1000U);
  for (auto &e : _engines) {
    bool flag_eof = false;
    if (e->has_line_err()) {
      char line[65536];
      if (e->getline_err(line, sizeof(line)) == 0) flag_eof = true; }
      
    if (e->has_line_in()) {
      char line[65536];
      if (e->getline_in(line, sizeof(line)) == 0) flag_eof = true;
      else {
	bool flag_thinking = e->is_thinking();
	string s = e->update(line, _moves_eid0);
	if (!s.empty()) {
	  _ngen_records += 1U;
	  recs.push_back(move(s)); }

	if (e->is_ready() && _wid == wfname.get_id()) {
	  if (has_conn && ! e->is_playing()) e->start_newgame();
	  if (e->is_playing() && ! e->is_thinking()) e->engine_go(); }

	if (! flag_thinking && e->is_thinking()) _num_thinking += 1U;
	if (flag_thinking && ! e->is_thinking()) _num_thinking -= 1U;
	assert(_num_thinking <= _engines.size()); } }

    if (flag_eof) {
      char line[65536];
      while (0 < e->getline_err(line, sizeof(line)));
      while (0 < e->getline_in (line, sizeof(line)));
      die(ERR_INT("An engine (%s) terminates.", e->get_fp())); } }

  if (_wid != wfname.get_id() && _num_thinking == 0) {
    _wid = wfname.get_id();
    cout << "\nUpdate weight" << endl;
    for (Device &d : _devices) d.nnreset(wfname);
    for (Device &d : _devices) d.wait();
    for (auto &e : _engines) {
      bool flag_thinking = e->is_thinking();
      e->engine_wght_update(wfname, crc64);
      if (e->is_ready() && _wid == wfname.get_id()) {
	if (has_conn && ! e->is_playing()) e->start_newgame();
	if (e->is_playing() && ! e->is_thinking()) e->engine_go(); }

      if (! flag_thinking && e->is_thinking()) _num_thinking += 1U;
      if (flag_thinking && ! e->is_thinking()) _num_thinking -= 1U;
      assert(_num_thinking <= _engines.size()); } }

  return recs; }

bool PlayManager::get_moves_eid0(string &move) noexcept {
  if (_moves_eid0.empty()) return false;
  move.swap(_moves_eid0.front());
  _moves_eid0.pop();
  return true; }

uint PlayManager::get_eid(uint u) const noexcept {
  assert(u < _engines.size());
  return _engines[u]->get_eid(); }

int PlayManager::get_did(uint u) const noexcept {
  assert(u < _engines.size());
  return _engines[u]->get_did(); }

uint PlayManager::get_nmove(uint u) const noexcept {
  assert(u < _engines.size());
  return _engines[u]->get_nmove(); }

double PlayManager::get_time_average(uint u) const noexcept {
  assert(u < _engines.size());
  return _engines[u]->get_time_average(); }
