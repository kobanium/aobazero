// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "child.hpp"
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ipc.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "play.hpp"
#include "shogibase.hpp"
#include <chrono>
#include <deque>
#include <fstream>
#include <memory>
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
using std::move;
using std::ofstream;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::time_point;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using ErrAux::die;
using namespace IOAux;
using uint = unsigned int;

constexpr double speed_update_rate1 = 0.05;
constexpr double speed_update_rate2 = 0.005;
constexpr uint speed_th_1to2        = 100U;
constexpr char fmt_log[]            = "engine%03u-%03u.log";

class Device {
  enum class Type : unsigned char { Aobaz, NNService, Bad };
  class DataNNService {
    NNetService _nnet;
    uint _size_batch;
  public:
    DataNNService(uint nnet_id, uint size_parallel, uint size_batch,
		  uint id, uint use_half, const FName &fname) noexcept
    : _nnet(nnet_id, size_parallel, size_parallel, id, use_half, fname, false),
      _size_batch(size_batch) {}
    void flush_on() noexcept { _nnet.flush_on(); }
  };
  unique_ptr<DataNNService> _data_nnservice;
  int _nnet_id;
  uint _size_parallel;
  Type _type;

public:
  static constexpr Type aobaz     = Type::Aobaz;
  static constexpr Type nnservice = Type::NNService;
  static constexpr Type bad       = Type::Bad;
  explicit Device(const std::string &s, int nnet_id, const FName &wfname)
    noexcept : _nnet_id(-1), _size_parallel(1U), _type(bad) {
    if (s.empty()) die(ERR_INT("invalid device %s", s.c_str()));
    char *endptr;
    if (s[0] == 'S' || s[0] == 's') {
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
      
      bool flag_half = false;;
      if (*endptr == '\0') flag_half = false;
      else if ((*endptr == 'H' || *endptr == 'h') && endptr[1] == '\0')
	flag_half = true;
      else die(ERR_INT("invalid device %s", s.c_str()));
      
      _nnet_id       = nnet_id;
      _type          = nnservice;
      _size_parallel = size_parallel;
      _data_nnservice.reset(new DataNNService(_nnet_id, size_parallel,
					      size_batch, device_id, flag_half,
					      wfname)); }
    else {
      const char *token = s.c_str();
      _nnet_id = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != '\0' || _nnet_id < -1
	  || 65535 < _nnet_id) die(ERR_INT("invalid device %s", s.c_str()));
      _type = aobaz; } }
  int get_nnet_id() const noexcept { return _nnet_id; }
  char get_id_option_character() const noexcept {
    return _type == nnservice ? 's' : 'u'; }
  uint get_size_parallel() const noexcept { return _size_parallel; }
  void flush_on() const noexcept {
    if (_type == nnservice) _data_nnservice->flush_on(); }
};
constexpr Device::Type Device::aobaz;
constexpr Device::Type Device::nnservice;
constexpr Device::Type Device::bad;

class USIEngine : public Child {
  using uint = unsigned int;
  Node<Param::maxlen_play_learn> _node;
  FName _logname;
  ofstream _ofs;
  string _startpos, _record, _record_header, _fingerprint, _settings;
  int _nnet_id, _version;
  uint _eid, _nmove;
  bool _flag_playing;

public:
  explicit USIEngine(const FName &cname, char ch, int nnet_id, uint eid,
		     const FNameID &wfname, uint64_t crc64, uint verbose_eng,
		     const FName &logname) noexcept
    : _logname(logname),
    _fingerprint(to_string(nnet_id) + string("-") + to_string(eid)),
    _nnet_id(nnet_id), _version(-1), _eid(eid), _flag_playing(false) {
    assert(cname.ok() && 0 < cname.get_len_fname());
    assert(wfname.ok() && 0 < wfname.get_len_fname());
    assert(isalnum(ch) && -2 < nnet_id && nnet_id < 65536);

    unique_ptr<char []> path(new char [cname.get_len_fname() + 1U]);
    unique_ptr<char []> a0  (new char [cname.get_len_fname() + 1U]);
    unique_ptr<char []> a7  (new char [wfname.get_len_fname() + 1U]);
    char a1[] = "-p";  char a2[] = "800";
    char a3[] = "-n";
    char a4[] = "-m";  char a5[] = "30";
    char a6[] = "-w";
    memcpy(path.get(), cname.get_fname(), cname.get_len_fname() + 1U);
    memcpy(a0.get(), cname.get_fname(), cname.get_len_fname() + 1U);
    memcpy(a7.get(), wfname.get_fname(), wfname.get_len_fname() + 1U);
    char *argv[] = { a0.get(), a1, a2, a3, a4, a5, a6, a7.get(),
		     nullptr, nullptr, nullptr, nullptr,
		     nullptr, nullptr, nullptr, nullptr };
    int argc = 8;
    char opt_q[] = "-q";
    char opt_u[] = "-u";
    char opt_u_value[256];
    opt_u[1] = ch;
    sprintf(opt_u_value, "%i", nnet_id);
    if (!verbose_eng) argv[argc++] = opt_q;
    if (0 <= nnet_id && ch == 's') {
      argv[argc++] = opt_u;
      argv[argc++] = opt_u_value; }
    Child::open(path.get(), argv);
  
    _logname.add_fmt_fname(fmt_log, nnet_id, eid);
    _ofs.open(_logname.get_fname(), ios::trunc);
    if (!_ofs) die(ERR_INT("cannot write to log"));

    char buf[256];
    snprintf(buf, sizeof(buf), "%16" PRIx64, crc64);
    _record_header  = string("'w ") + to_string(wfname.get_id());
    _record_header += string(" (crc64:") + string(buf);
    _record_header += string("), autousi ") + to_string(Ver::major);
    _record_header += string(".") + to_string(Ver::minor);

    //c.time_last    = system_clock::now();
  }

  void set_version(int version) noexcept { _version = version; }

  void set_settings(const char *p) noexcept { _settings = string(p); }

  void do_header() noexcept {
    if (_version < 0) die(ERR_INT("No version infomation from engine %s",
				  get_fp()));
    _record_header += string(", usi-engine ") + to_string(_version);
    _record_header += string("\n'") + _settings;
    _record_header += string("\nPI\n+\n"); }

  void out_log(const char *p) noexcept {
    assert(_ofs && p);
    _ofs << p << endl;
    if (!_ofs) die(ERR_INT("cannot write to log")); }

  void start_newgame() noexcept {
    _node.clear();
    _flag_playing = true;
    _record       = _record_header;
    _nmove        = 0;
    _startpos     = string("position startpos moves");
    engine_out("usinewgame");
    engine_out("%s", _startpos.c_str());
    engine_out("go visit"); }

  string update(char *line) noexcept {
    assert(line);
    char *token, *saveptr;
    token = OSI::strtok(line, " ,", &saveptr);
    if (strcmp(token, "bestmove") != 0) return string("");
    if (!_flag_playing)
      die(ERR_INT("bad usi message from engine %s", get_fp()));

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
    //time_point<system_clock> time_now = system_clock::now();
    //auto rep  = duration_cast<milliseconds>(time_now - c.time_last).count();
    //double fms = static_cast<double>(rep);
    //if (speed_th_1to2 < ++c.speed_nmove) c.speed_rate = speed_update_rate2;
    //c.speed_average += c.speed_rate * (fms - c.speed_average);
    //c.time_last = time_now;

      _startpos += " ";
      _startpos += str_move_usi;
      _record   += _node.get_turn().to_str();
      /*
      if (id == 0) {
	char buf[256];
	sprintf(buf, " (%5.0fms)", fms);
	string smove = node.get_turn().to_str();
	smove += actionPlay.to_str(SAux::csa);
	smove += buf;
	_moves_id0.push(std::move(smove)); } */
      _nmove  += 1U;
      _record += actionPlay.to_str(SAux::csa);
    
      const char *str_count = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_count) die(ERR_INT("cannot read count (engine %s)", get_fp()));
    
      char *endptr;
      long int num = strtol(str_count, &endptr, 10);
      if (endptr == str_count || *endptr != '\0' || num < 1
	  || num == LONG_MAX)
	die(ERR_INT("cannot interpret a visit count %s (engine %s)",
		    str_count, get_fp()));
    
      num_best = num;
      _record += ",'";
      _record += to_string(num);

      // read candidate moves
      while (true) {
	str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
	if (!str_move_usi) { _record += "\n"; break; }

	Action action = _node.action_interpret(str_move_usi, SAux::usi);
	if (!action.is_move())
	  die(ERR_INT("bad candidate %s (engine %s)", str_move_usi, get_fp()));
	_record += ",";
	_record += action.to_str(SAux::csa);
    
	str_count = OSI::strtok(nullptr, " ,", &saveptr);
	if (!str_count)
	  die(ERR_INT("cannot read count (engine %s)", get_fp()));

	num = strtol(str_count, &endptr, 10);
	if (endptr == str_count || *endptr != '\0'
	    || num < 1 || num == LONG_MAX)
	  die(ERR_INT("cannot interpret a visit count %s (engine %s)",
		      str_count, get_fp()));

	num_tot += num;
	_record += ",";
	_record += to_string(num); } }

    if (num_best < num_tot) die(ERR_INT("bad counts (engine %s)", get_fp()));
    _node.take_action(actionPlay);

    // force declare nyugyoku
    if (_node.get_type().is_interior() && _node.is_nyugyoku())
      _node.take_action(SAux::windecl);
    assert(_node.ok());

    // terminal test
    if (_node.get_type().is_term()) {
      _record += "%";
      _record += _node.get_type().to_str();
      _record += "\n";
      _flag_playing = false;
      return move(_record);
      /*
	if (c.get_id() == 0) {
	string s = "%";
	s += c.node.get_type().to_str();
	_moves_id0.push(s); }
      */

      //_ngen_records += 1U;
      /*
	Client::get().add_rec(_record.c_str(), _record.size());
	write_record(_record.c_str(), _record.size(),
	_dname_csa.get_fname(), _max_csa); */ }
    // go
    else {
      engine_out("%s", _startpos.c_str());
      engine_out("go visit"); } }

  void engine_out(const char *fmt, ...) noexcept {
    assert(_ofs && Child::ok() && ! Child::is_closed() && fmt);
    char buf[65536];
    va_list argList;
  
    va_start(argList, fmt);
    int nb = vsnprintf(buf, sizeof(buf), fmt, argList);
    va_end(argList);
    if (sizeof(buf) <= static_cast<size_t>(nb) + 1U)
      die(ERR_INT("buffer overrun (engine %d-%d)", _nnet_id, _eid));

    out_log(buf);
    buf[nb]     = '\n';
    buf[nb + 1] = '\0';
    if (!Child::write(buf, strlen(buf))) {
      if (errno == EPIPE) die(ERR_INT("engine %d-%d terminates",
				      _nnet_id, _eid));
      die(ERR_CLL("write")); } }

  const char *get_fp() const noexcept { return _fingerprint.c_str(); }
  const string &get_record() const noexcept { return _record; }
  bool is_playing() const noexcept { return _flag_playing; }
};

/*
static void close_flush(USIEngine &c) noexcept {
  c.close_write();
  while (true) {
    const char *line = c.getline_err_block();
    if (!line) break;
    c.ofs << line << "\n"; }
  
  while (true) {
    const char *line = c.getline_in_block();
    if (!line) break;
    c.ofs << line << "\n"; }
  
  c.ofs.close();
  if (!c.ofs) die(ERR_INT("cannot close log"));
  c.close(); }
    
static void write_record(const char *prec, size_t len, const char *dname,
			 uint max_csa) noexcept {
  if (max_csa == 0) return;
  assert(prec && dname);
  FNameID fname = grab_max_file(dname, fmt_csa_scn);
  int64_t id = fname.get_id() + 1;
  if (static_cast<int64_t>(max_csa) < id) return;
  fname.clear_fname();
  fname.add_fname(dname);
  fname.add_fmt_fname(fmt_csa, id);
  ofstream ofs(fname.get_fname(), ios::binary | ios::trunc);
  ofs.write(prec, len);
  if (!ofs) die(ERR_INT("cannot write %s", fname.get_fname())); }
*/

PlayManager & PlayManager::get() noexcept {
  static PlayManager instance;
  return instance; }

PlayManager::PlayManager() noexcept : _ngen_records(0) {}
PlayManager::~PlayManager() noexcept {}
void PlayManager::start(const char *cname, const char *dlog,
			vector<string> &&devices, uint verbose_eng) noexcept {
  assert(cname && dlog);
  if (devices.empty()) die(ERR_INT("bad devices"));
  _verbose_eng = verbose_eng;
  _devices_str = move(devices);
  _cname.reset_fname(cname);
  _logname.reset_fname(dlog); }

void PlayManager::engine_start(const FNameID &wfname, uint64_t crc64)
  noexcept {
  assert(wfname.ok() && _devices.empty() && _engines.empty());
  int nnet_id = 0;
  for (const string &s : _devices_str)
    _devices.emplace_back(s, nnet_id++, wfname);
  for (const Device &d : _devices) {
    uint size   = d.get_size_parallel();
    int nnet_id = d.get_nnet_id();
    char ch     = d.get_id_option_character();
    for (uint u = 0; u < size; ++u)
      _engines.emplace_back(new USIEngine(_cname, ch, nnet_id, u, wfname,
					  crc64, _verbose_eng, _logname)); }
  for (auto &e : _engines) e->engine_out("usi");

  uint num_done = 0;
  do {
    if (Child::wait(1000U) == 0) continue;
      
    for (auto &e : _engines) {
      bool has_line = false;
      char line[65536];
      if (e->has_line_err()) {
	if (e->getline_err(line, sizeof(line)) == 0)
	  die(ERR_INT("An engine (%s) terminates.", e->get_fp()));
	e->out_log(line); }
      
      if (e->has_line_in()) {
	if (e->getline_in(line, sizeof(line)) == 0)
	  die(ERR_INT("An engine (%s) terminates.", e->get_fp()));
	e->out_log(line);
	has_line = true; }

      if (!has_line) continue;

      if (strcmp(line, "usiok") == 0) {
	num_done += 1U;
	continue; }
	
      char *token, *saveptr, *endptr;
      token = OSI::strtok(line, " ", &saveptr);
      if (!token || strcmp(token, "id") != 0)
	die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
      token = OSI::strtok(nullptr, " ", &saveptr);
      if (!token) die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
	
      if (strcmp(token, "settings") == 0) {
	token = OSI::strtok(nullptr, "", &saveptr);
	if (!token) die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
	e->set_settings(token);
	continue; }

      if (strcmp(token, "name") == 0) {
	if (! OSI::strtok(nullptr, " ", &saveptr))
	  die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
	  
	token = OSI::strtok(nullptr, " ", &saveptr);
	if (!token) die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
	  
	long int ver = strtol(token, &endptr, 10);
	if (endptr == token || *endptr != '\0' || ver < 0 || 65535 < ver)
	  die(ERR_INT("Bad message from engine (%s).", e->get_fp()));
	e->set_version(ver);
	continue; } } }
  while (num_done < _engines.size());

  for (auto &e : _engines) {
    e->do_header();
    e->engine_out("isready");
    e->start_newgame(); } }

void PlayManager::engine_terminate() noexcept {
  for (const Device &d : _devices) d.flush_on();
  _engines.clear();
  _devices.clear(); }

deque<string> PlayManager::manage_play(bool has_conn) noexcept {
  deque<string> recs;

  Child::wait(1000U);
  for (auto &e : _engines) {
    if (e->has_line_err()) {
      char line[65536];
      if (e->getline_err(line, sizeof(line)) == 0)
	die(ERR_INT("An engine (%s) terminates.", e->get_fp()));
      e->out_log(line); }
      
    if (e->has_line_in()) {
      char line[65536];
      if (e->getline_in(line, sizeof(line)) == 0)
	die(ERR_INT("An engine (%s) terminates.", e->get_fp()));
      e->out_log(line);
      string s = e->update(line);
      if (!s.empty()) recs.push_back(move(s)); }

    if (has_conn && ! e->is_playing()) e->start_newgame(); }

  return recs; }

  /*
  for (uint u = 0; u < _nchild; ++u) {
      Client::get().add_rec(c.node.record.c_str(), c.node.record.size());
      _ngen_records += 1U;
      write_record(c.node.record.c_str(), c.node.record.size(),
		   _dname_csa.get_fname(), _max_csa);
      close_flush(c); } }
  */

void PlayManager::end() noexcept {
}

bool PlayManager::get_moves_id0(string &move) noexcept {
  if (_moves_id0.empty()) return false;
  move.swap(_moves_id0.front());
  _moves_id0.pop();
  return true; }

