// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "client.hpp"
#include "err.hpp"
#include "iobase.hpp"
#include "osi.hpp"
#include "pipe.hpp"
#include "shogibase.hpp"
#include "version.hpp"
#include <chrono>
#include <fstream>
#include <memory>
#include <utility>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdio>
using std::cout;
using std::endl;
using std::flush;
using std::ios;
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
constexpr uint speed_th_1to2       = 100U;
constexpr uint max_nchild          = 32;
constexpr char fmt_log[]     = "engine%03u.log";
constexpr char fmt_csa[]     = "rec%012" PRIi64 ".csa";
constexpr char fmt_csa_scn[] = "rec%16[^.].csa";

class NodeRec : public Node {
public:
  string startpos, record;
  void clear() noexcept {
    Node::clear();
    record.clear();
    startpos = string("position startpos moves"); } };

class USIEngine : public OSI::Pipe {
  shared_ptr<const WghtFile> _wght;
  uint _id;
  int _device;
  
public:
  time_point<system_clock> time_last;
  double speed_average, speed_rate;
  uint speed_nmove, nmove;
  NodeRec node;
  FName flog;
  ofstream ofs;

  void set_id(uint id) noexcept { _id = id; }
  void set_device(int device) noexcept { _device = device; }
  void update_wght() noexcept { _wght = Client::get().get_wght(); }
  
  int get_id() const noexcept { return _id; }
  int get_device() const noexcept { return _device; }
  const shared_ptr<const WghtFile> & get_wght() const noexcept {
    return _wght; } };

static void write_record(const char *prec, size_t len,
			 const char *dname, uint max_csa) noexcept {
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
    
static int read_id(USIEngine &c, int &version, string &settings) noexcept {
  assert(c.ok());
  version  = -1;
  settings = string("");
  while (true) {
    char *line = c.getline_in_block();
    if (!line) {
      close_flush(c);
      die(ERR_INT("faild to execute the usi engine (%d)", c.get_id())); }

    c.ofs << line << endl;
    if (!c.ofs) die(ERR_INT("cannot write to log"));
    if (strcmp(line, "usiok") == 0) break;
    
    char *token, *saveptr;
    token = OSI::strtok(line, " ", &saveptr);
    if (!token || strcmp(token, "id") != 0) return -1;
    token = OSI::strtok(nullptr, " ", &saveptr);
    if (!token) return -1;

    if (strcmp(token, "settings") == 0) {
      token = OSI::strtok(nullptr, "", &saveptr);
      if (!token) return -1;
      settings = string(token); }
    
    else if (strcmp(token, "name") == 0) {
      if (!OSI::strtok(nullptr, " ", &saveptr)) return -1;
      token = OSI::strtok(nullptr, " ", &saveptr);
      if (!token) return -1;
    
      char *endptr;
      long int ver = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != '\0' || ver < 0 || 65535 < ver)
	return -1;
      version = ver; } }

  return 0; }

static void engine_out(USIEngine &c, const char *fmt, ...) noexcept {
  assert(fmt);
  char buf[65536];
  va_list argList;
  
  va_start(argList, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, argList);
  va_end(argList);
  if (sizeof(buf) <= static_cast<size_t>(nb) + 1U) {
    close_flush(c);
    die(ERR_INT("buffer overrun (engine no. %d)", c.get_id())); }
  buf[nb]     = '\n';
  buf[nb + 1] = '\0';
  c.ofs << buf << flush;
  if (!c.ofs) die(ERR_INT("cannot write to log"));
  if (!c.write(buf, strlen(buf))) {
    close_flush(c);
    if (errno == EPIPE) die(ERR_INT("engine no. %d terminates", c.get_id()));
    die(ERR_CLL("write")); } }

static void engine_start(USIEngine &c, const FName &cname)
  noexcept {
  c.nmove = 0;
  c.node.clear();
  c.update_wght();

  unique_ptr<char []> path(new char [cname.get_len_fname() + 1U]);
  unique_ptr<char []> a0  (new char [cname.get_len_fname() + 1U]);
  unique_ptr<char []> a8  (new char [c.get_wght()->get_len_fname() + 1U]);
  char a1[] = "-p";  char a2[] = "800";
  char a3[] = "-q";
  char a4[] = "-n";
  char a5[] = "-m";  char a6[] = "30";
  char a7[] = "-w";
  memcpy(path.get(), cname.get_fname(), cname.get_len_fname() + 1U);
  memcpy(a0.get(), cname.get_fname(), cname.get_len_fname() + 1U);
  memcpy(a8.get(), c.get_wght()->get_fname(),
	 c.get_wght()->get_len_fname() + 1U);
  if (c.get_device() < 0) {
    char *argv[] = { a0.get(), a1, a2, a3, a4, a5, a6, a7, a8.get(),
		     nullptr };
    c.open(path.get(), argv); }
  else {
    char a9[] = "-u";  char a10[16];
    sprintf(a10, "%i", c.get_device());
    char *argv[] = { a0.get(), a9, a10, a1, a2, a3, a4, a5, a6, a7,
		     a8.get(), nullptr };
    c.open(path.get(), argv); }
  
  c.ofs.open(c.flog.get_fname(), ios::trunc);
  if (!c.ofs) die(ERR_INT("cannot write to log"));
  
  int eng_ver;
  string eng_settings;
  engine_out(c, "usi");
  if (read_id(c, eng_ver, eng_settings) < 0) {
    close_flush(c); die(ERR_INT("Bad usi command")); }
  if (eng_ver < 0) {
    close_flush(c); die(ERR_INT("Bad version of usi engine")); }
  if (eng_settings.empty()) {
    close_flush(c); die(ERR_INT("Bad settings of usi engine")); }
  if (eng_ver < Client::get().get_ver_engine()) {
    close_flush(c); die(ERR_INT("Please update USI engine.")); }

  char buf[17];
  snprintf(buf, sizeof(buf), "%16" PRIx64, c.get_wght()->get_crc64());
  engine_out(c, "isready");
  engine_out(c, "%s", c.node.startpos.c_str());
  c.time_last    = system_clock::now();
  engine_out(c, "go visit");
  c.node.record  = string("'w ") + to_string(c.get_wght()->get_id());
  c.node.record += string(" (crc64:") + string(buf);
  c.node.record += string("), autousi ") + to_string(Ver::major);
  c.node.record += string(".") + to_string(Ver::minor);
  c.node.record += string(", usi-engine ") + to_string(eng_ver);
  c.node.record += string("\n'") + eng_settings;
  c.node.record	+= string("\nPI\n+\n"); }

Pipe & Pipe::get() noexcept {
  static Pipe instance;
  return instance; }

Pipe::Pipe() noexcept : _ngen_records(0) {}
Pipe::~Pipe() noexcept {}
void Pipe::start(const char *cname, const char *dlog,
		 const vector<int> &devices,  const char *cstr_csa,
		 uint max_csa) noexcept {
  assert(cname && cstr_csa && dlog);
  if (devices.empty() || max_nchild < devices.size())
    die(ERR_INT("bad devices"));
  _cname.reset_fname(cname);
  _dname_csa.reset_fname(cstr_csa);
  _nchild      = static_cast<uint>(devices.size());
  _max_csa     = max_csa;
  _children.reset(new USIEngine [devices.size()]);
  for (uint u = 0; u < _nchild; ++u) {
    USIEngine & c = _children[u];
    c.speed_average = 1000.0;
    c.speed_nmove   = 0;
    c.speed_rate    = speed_update_rate1;
    c.flog          = FName(dlog);
    c.flog.add_fmt_fname(fmt_log, u);
    c.set_id(u);
    c.set_device(devices[u]); } }

bool Pipe::play_update(char *line, class USIEngine &c) noexcept {
  NodeRec &node = c.node;
  int id = c.get_id();
  assert(line && 0 <= id);
  
  char *token, *saveptr;
  token = OSI::strtok(line, " ,", &saveptr);
  if (strcmp(token, "bestmove") != 0) return false;
  
  // read played move
  long int num_best = 0;
  long int num_tot  = 0;
  const char *str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
  if (!str_move_usi) {
    close_flush(c);
    die(ERR_INT("bad usi format")); }

  Action actionPlay = node.action_interpret(str_move_usi, SAux::usi);
  if (!actionPlay.ok()) {
    close_flush(c);
    die(ERR_INT("cannot interpret candidate move %s (engine no. %d)\n%s",
		str_move_usi, id,
		static_cast<const char *>(node.to_str()))); }

  if (actionPlay.is_move()) {
    time_point<system_clock> time_now = system_clock::now();
    auto rep  = duration_cast<milliseconds>(time_now - c.time_last).count();
    double fms = static_cast<double>(rep);
    if (speed_th_1to2 < ++c.speed_nmove) c.speed_rate = speed_update_rate2;
    c.speed_average += c.speed_rate * (fms - c.speed_average);
    c.time_last = time_now;

    node.startpos += " ";
    node.startpos += str_move_usi;
    node.record   += node.get_turn().to_str();
    if (id == 0) {
      char buf[256];
      sprintf(buf, " (%6.0fms)", fms);
      string smove = node.get_turn().to_str();
      smove += actionPlay.to_str(SAux::csa);
      smove += buf;
      _moves_id0.push(std::move(smove)); }
    c.nmove     += 1U;
    node.record += actionPlay.to_str(SAux::csa);
    
    const char *str_count = OSI::strtok(nullptr, " ,", &saveptr);
    if (!str_count) {
      close_flush(c);
      die(ERR_INT("cannot read count (engine no. %d)", id)); }
    
    char *endptr;
    long int num = strtol(str_count, &endptr, 10);
    if (endptr == str_count || *endptr != '\0' || num < 1 || num == LONG_MAX) {
      close_flush(c);
      die(ERR_INT("cannot interpret a visit count %s (engine no. %d)",
		  str_count, id)); }
    
    num_best = num;
    node.record += ",'";
    node.record += to_string(num);

    // read candidate moves
    while (true) {
      str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_move_usi) { node.record += "\n"; break; }

      Action action = node.action_interpret(str_move_usi, SAux::usi);
      if (!action.is_move()) {
	close_flush(c);
	die(ERR_INT("bad candidate %s (engine no. %d)",	str_move_usi, id)); }
      node.record += ",";
      node.record += action.to_str(SAux::csa);
    
      str_count = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_count) {
	close_flush(c);
	die(ERR_INT("cannot read count (engine no. %d)", id)); }

      num = strtol(str_count, &endptr, 10);
      if (endptr == str_count || *endptr != '\0'
	  || num < 1 || num == LONG_MAX) {
	close_flush(c);
	die(ERR_INT("cannot interpret a visit count %s (engine no. %d)",
		    str_count, id)); }

      node.record += ",";
      num_tot     += num;
      node.record += to_string(num); } }

  if (num_best < num_tot) {
    close_flush(c);
    die(ERR_INT("bad counts (engine no. %d)", id)); }
  node.take_action(actionPlay);
  if (node.get_type().is_interior() && node.is_nyugyoku())
    node.take_action(SAux::windecl);
  assert(node.ok());
  return true; }

void Pipe::wait() noexcept {
  bool has_conn = Client::get().has_conn();
  _selector.reset();
  for (uint u = 0; u < _nchild; ++u) {
    USIEngine &c = _children[u];
    if (c.is_closed() && has_conn) engine_start(c, _cname);
    _selector.add(c); }

  _selector.wait(0, 500U);
  for (uint u = 0; u < _nchild; ++u) {
    USIEngine &c = _children[u];
    char *line;
    bool eof = false;
    if (_selector.try_getline_err(c, &line) && line) {
      c.ofs << line << endl;
      if (!c.ofs) die(ERR_INT("cannot write to log (engine no. %d)",
			      c.get_id())); }

    if (_selector.try_getline_in(c, &line)) {
      if (!line) eof = true;
      else {
	c.ofs << line << endl;
	if (!c.ofs) die(ERR_INT("cannot write to log (engine no. %d)",
				c.get_id()));
	
	if (play_update(line, c)) {
	  if (c.node.get_type().is_term()) engine_out(c, "quit");
	  else {
	    engine_out(c, "%s", c.node.startpos.c_str());
	    engine_out(c, "go visit"); } } } }
    
    if (eof) {
      NodeType type = c.node.get_type();
      if (!type.is_term()) {
	close_flush(c);
	die(ERR_INT("Engine no. %d terminates.", c.get_id())); }

      assert(c.node.get_type().is_term());
      c.node.record += "%";
      c.node.record += c.node.get_type().to_str();
      c.node.record += "\n";
      if (c.get_id() == 0) {
	  string s = "%";
	  s += c.node.get_type().to_str();
	  _moves_id0.push(s); }
	
      Client::get().add_rec(c.node.record.c_str(), c.node.record.size());
      _ngen_records += 1U;
      write_record(c.node.record.c_str(), c.node.record.size(),
		   _dname_csa.get_fname(), _max_csa);
      close_flush(c); } } }

void Pipe::end() noexcept {
  for (uint u = 0; u < _nchild; ++u) {
    USIEngine & c = _children[u];
    if (c.is_closed()) continue;
    close_flush(c); } }

bool Pipe::get_moves_id0(string &move) noexcept {
  if (_moves_id0.empty()) return false;
  move.swap(_moves_id0.front());
  _moves_id0.pop();
  return true; }

bool Pipe::is_closed(uint u) const noexcept {
  assert(u < _nchild);
  return _children[u].is_closed(); }

uint Pipe::get_pid(uint u) const noexcept {
  assert(u < _nchild);
  return _children[u].get_pid(); }

uint Pipe::get_nmove(uint u) const noexcept {
  assert(u < _nchild);
  return _children[u].nmove; }

double Pipe::get_speed_average(uint u) const noexcept {
  assert(u < _nchild);
  return _children[u].speed_average; }
