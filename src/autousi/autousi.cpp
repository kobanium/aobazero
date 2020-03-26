// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "client.hpp"
#include "param.hpp"
#include "play.hpp"
#include "option.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <cinttypes>
#include <cstdio>
using std::atomic;
using std::cerr;
using std::cout;
using std::current_exception;
using std::deque;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::fill_n;
using std::ifstream;
using std::ios;
using std::map;
using std::min;
using std::move;
using std::ofstream;
using std::random_device;
using std::rethrow_exception;
using std::string;
using std::set_terminate;
using std::vector;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::hours;
using std::this_thread::sleep_for;
using namespace IOAux;
using ErrAux::die;
using uint   = unsigned int;
using ushort = unsigned short int;

constexpr uint max_sleep     = 3U; // in sec
constexpr char fmt_csa_scn[] = "rec%16[^.].csa";
constexpr char fmt_csa[]     = "rec%012" PRIi64 ".csa";
atomic<int> flag_signal(0);
static uint print_status, print_csa;
static steady_clock::time_point time_start;
static FName opt_dname_csa;
static uint opt_max_csa;

static bool is_posi(uint u) { return 0 < u; }

static void on_terminate() {
  exception_ptr p = current_exception();
  try { if (p) rethrow_exception(p); }
  catch (const exception &e) { cout << e.what() << endl; }

  try { WghtFile::cleanup(); }
  catch (exception &e) {
    cerr << ERR_INT("cleanup() failed").what() << endl;
    cerr << e.what() << endl; }
  
  abort(); }

static void on_signal(int signum) { flag_signal = signum; }

static void init() noexcept {
  vector<string> devices;
  map<string, string> m = {{"WeightSave",    "./weight_save"},
			   {"CmdPath",       "./autousi"},
			   {"DirLog",        "./log"},
			   {"DirCSA",        "./csa"},
			   {"Device",        "S-1:3:7"},
			   {"SizeSendQueue", "64"},
			   {"RecvTO",        "3"},
			   {"SendTO",        "3"},
			   {"RecvBufSiz",    "8192"},
			   {"SendBufSiz",    "8192"},
			   {"MaxRetry",      "7"},
			   {"MaxCSA",        "0"},
			   {"PrintStatus",   "0"},
			   {"PrintCSA",      "0"},
			   {"VerboseEngine", "0"},
			   {"KeepWeight",    "0"},
			   {"Addr",          "127.0.0.1"},
			   {"Port",          "20000"}};
  try { Config::read("autousi.cfg", m); } catch (exception &e) { die(e); }
  const char *cstr_dwght = Config::get_cstr(m, "WeightSave", maxlen_path);
  const char *cstr_cname = Config::get_cstr(m, "CmdPath",    maxlen_path);
  const char *cstr_dlog  = Config::get_cstr(m, "DirLog",     maxlen_path);
  const char *cstr_csa   = Config::get_cstr(m, "DirCSA",     maxlen_path);
  const char *cstr_addr  = Config::get_cstr(m, "Addr",       64);
  uint size_queue   = Config::get<uint>  (m, "SizeSendQueue", is_posi);
  uint recvTO       = Config::get<uint>  (m, "RecvTO",        is_posi);
  uint sendTO       = Config::get<uint>  (m, "SendTO",        is_posi);
  uint recv_bufsiz  = Config::get<uint>  (m, "RecvBufSiz",    is_posi);
  uint send_bufsiz  = Config::get<uint>  (m, "SendBufSiz",    is_posi);
  uint max_retry    = Config::get<uint>  (m, "MaxRetry",      is_posi);
  opt_max_csa       = Config::get<uint>  (m, "MaxCSA");
  uint keep_wght    = Config::get<uint>  (m, "KeepWeight");
  uint verbose_eng  = Config::get<uint>  (m, "VerboseEngine");
  uint port         = Config::get<ushort>(m, "Port");
  devices           = Config::get_vecstr (m, "Device");
  print_status      = Config::get<uint>  (m, "PrintStatus");
  print_csa         = Config::get<uint>  (m, "PrintCSA");

  opt_dname_csa.reset_fname(cstr_csa);
  Client::get().start(cstr_dwght, cstr_addr, port, recvTO, recv_bufsiz, sendTO,
		      send_bufsiz, max_retry, size_queue, keep_wght);
  OSI::handle_signal(on_signal);
  PlayManager::get().start(cstr_cname, cstr_dlog, devices, verbose_eng);
  time_start = steady_clock::now(); }

static void output() noexcept {
  static bool first = true;
  static bool print_csa_do_nl = false;
  static uint print_csa_num   = 0;
  static steady_clock::time_point time_last = steady_clock::now();

  // print moves of child id #0
  string s;
  while (PlayManager::get().get_moves_eid0(s)) {
    if (print_csa == 0) continue;
    if (print_csa_do_nl) { print_csa_do_nl = false; puts(""); }
    else if (!first) fputs(", ", stdout);
    first = false;
    fputs(s.c_str(), stdout);
    if (s.at(0) == '%' || (++print_csa_num % print_csa) == 0) {
      if (s.at(0) == '%') puts("");
      print_csa_num   = 0;
      print_csa_do_nl = true; }
    fflush(stdout); }
  
  // print status
  if (print_status == 0) return;
  
  steady_clock::time_point time_now = steady_clock::now();
  if (time_now < time_last + seconds(print_status)) return;
  
  static uint prev_ntot     = 0;
  static uint prev_nsend    = 0;
  static uint prev_ndiscard = 0;
  uint ntot     = PlayManager::get().get_ngen_records();
  uint nsend    = Client::get().get_nsend();
  uint ndiscard = Client::get().get_ndiscard();
  if (prev_ntot == ntot && prev_nsend == nsend && prev_ndiscard == ndiscard)
    return;
  prev_ntot     = ntot;
  prev_nsend    = nsend;
  prev_ndiscard = ndiscard;

  first           = true;
  print_csa_do_nl = false;
  print_csa_num   = 0;
  time_last       = time_now;
  puts("");
  puts("+------+-----+--------+---< Aobaz Status >-------------------------+");
  puts("|  ID  | Dev | Average|               Moves                        |");
  puts("+------+-----+--------+--------------------------------------------+");
  for (uint u = 0; u < PlayManager::get().get_nengine(); ++u) {
    const int BUF_SIZE = 64;
    char spid[BUF_SIZE] = "  N/A ";
    snprintf(spid, BUF_SIZE, "%6u", PlayManager::get().get_eid(u));
    char buf[BUF_SIZE];
    fill_n(buf, sizeof(buf), '#');
    uint len = std::min(PlayManager::get().get_nmove(u) / 5,
			static_cast<uint>(sizeof(buf)) - 1U);
    buf[len] = '\0';
    printf("|%s|%4d |%6.0fms|%3d:%-40s|\n",
	   spid, PlayManager::get().get_did(u),
	   PlayManager::get().get_time_average(u),
	   PlayManager::get().get_nmove(u), buf); }
  puts("+------+-----+--------+--------------------------------------------+");

  printf("- Send Status: Sent %d, Lost %d, Waiting %d\n",
	 nsend, ndiscard, ntot - nsend - ndiscard);

  int64_t wght_id        = Client::get().get_wght()->get_id();
  bool    is_downloading = Client::get().is_downloading();
  const char *buf_time   = Client::get().get_buf_wght_time();
  printf("- Recv Status: Weights' ID %" PRIi64 ", ", wght_id);

  if (is_downloading) puts("NOW DOWNLOADING NEW WEIGHTS\n");
  else                printf("Last Check %s\n", buf_time);
  auto rep = duration_cast<seconds>(time_now - time_start).count();
  double sec  = static_cast<double>(rep) + 1e-6;
  double hour = sec / 3600.0;
  double day  = sec /(3600.0*24.0);
  printf("- %.1f sent/hour, %.1f sent/day, "
	 "Running for %.1f hours (%.1f days).\n\n",
	 nsend / hour, nsend / day, hour, day);
}

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

int main() {
  OSI::prevent_multirun(FName(Param::name_autousi));
  sleep_for(seconds(random_device()() % max_sleep));
  set_terminate(on_terminate);
  
  init();
  std::shared_ptr<const WghtFile> wght = Client::get().get_wght();
  PlayManager::get().engine_start(wght->get_fname(), wght->get_crc64());

  cout << "self-play start" << endl;
  while (! flag_signal) {
    output();

    deque<string> recs
      = PlayManager::get().manage_play(Client::get().has_conn());
    for (const string &rec : recs) {
      Client::get().add_rec(rec.c_str(), rec.size());
      write_record(rec.c_str(), rec.size(),
		   opt_dname_csa.get_fname(), opt_max_csa); }

    if (wght->get_id() == Client::get().get_wght()->get_id()) continue;
    wght = Client::get().get_wght();
    PlayManager::get().engine_terminate();
    PlayManager::get().engine_start(wght->get_fname(), wght->get_crc64()); }
  
  cout << "\nsignal " << flag_signal << " caught" << endl;

  PlayManager::get().engine_terminate();
  Client::get().end();
  PlayManager::get().end();
  return 0;
}
