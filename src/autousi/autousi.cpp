// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "client.hpp"
#include "pipe.hpp"
#include "option.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <iomanip>
#include <map>
#include <random>
#include <string>
#include <fstream>
#include <vector>
#include <cinttypes>
#include <cstdio>
using std::atomic;
using std::cerr;
using std::cout;
using std::current_exception;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::fill_n;
using std::ifstream;
using std::map;
using std::min;
using std::random_device;
using std::rethrow_exception;
using std::string;
using std::set_terminate;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::time_point;
using std::chrono::system_clock;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using namespace IOAux;
using ErrAux::die;
using uint   = unsigned int;
using ushort = unsigned short int;

constexpr uint max_sleep = 3U; // in sec
atomic<int> flag_signal(0);
static uint print_status, print_csa;
static vector<int> devices;

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
  map<string, string> m = {{"WeightSave",    "./weight_save"},
			   {"CmdPath",       "./autousi"},
			   {"DirLog",        "./log"},
			   {"DirCSA",        "./csa"},
			   {"Device",        "-1"},
			   {"SizeSendQueue", "64"},
			   {"RecvTO",        "3"},
			   {"SendTO",        "3"},
			   {"RecvBufSiz",    "8192"},
			   {"SendBufSiz",    "8192"},
			   {"MaxRetry",      "7"},
			   {"MaxCSA",        "0"},
			   {"PrintStatus",  "0"},
			   {"PrintCSA",      "0"},
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
  uint max_csa      = Config::get<uint>  (m, "MaxCSA");
  uint keep_wght    = Config::get<uint>  (m, "KeepWeight");
  uint port         = Config::get<ushort>(m, "Port");
  devices           = Config::getv<int>  (m, "Device");
  print_status      = Config::get<uint>  (m, "PrintStatus");
  print_csa         = Config::get<uint>  (m, "PrintCSA");
  Client::get().start(cstr_dwght, cstr_addr, port, recvTO, recv_bufsiz, sendTO,
		      send_bufsiz, max_retry, size_queue, keep_wght);
  OSI::handle_signal(on_signal);
  Pipe::get().start(cstr_cname, cstr_dlog, devices, cstr_csa, max_csa);
  cout << "self-play start" << endl; }

static void output() noexcept {
  static bool first = true;
  static bool print_csa_do_nl = false;
  static uint print_csa_num   = 0;
  static time_point<system_clock> time_last = system_clock::now();

  // print moves of child id #0
  string s;
  while (Pipe::get().get_moves_id0(s)) {
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
  
  time_point<system_clock> time_now = system_clock::now();
  if (time_now < time_last + seconds(print_status)) return;
  
  static uint prev_ntot     = 0;
  static uint prev_nsend    = 0;
  static uint prev_ndiscard = 0;
  uint ntot     = Pipe::get().get_ngen_records();
  uint nsend    = Client::get().get_nsend();
  uint ndiscard = Client::get().get_ndiscard();
  if ( prev_ntot == ntot && prev_nsend == nsend && prev_ndiscard == ndiscard ) return;
  prev_ntot     = ntot;
  prev_nsend    = nsend;
  prev_ndiscard = ndiscard;

  first           = true;
  print_csa_do_nl = false;
  print_csa_num   = 0;
  time_last       = time_now;
  puts("");
  puts("+------+-----+--------+---< Aobaz Status >------------------------+");
  puts("|  PID | Dev | Average|               Moves                       |");
  puts("+------+-----+--------+-------------------------------------------+");
  for (uint u = 0; u < devices.size(); ++u) {
    char spid[64];
    sprintf(spid,"  N/A ");
    if ( ! Pipe::get().is_closed(u) ) sprintf(spid,"%6d",Pipe::get().get_pid(u));
    char buf[64];
    fill_n(buf, sizeof(buf), '#');
    uint len = std::min(Pipe::get().get_nmove(u) / 5,
			static_cast<uint>(sizeof(buf)) - 1U);
    buf[len] = '\0';
    printf("|%s|%4d |%6.0fms|%3d:%-39s|\n",
	   spid, devices[u],
	   Pipe::get().get_speed_average(u),
	   Pipe::get().get_nmove(u), buf); }
  puts("+------+-----+--------+-------------------------------------------+");

  printf("- Send Status: Sent %d, Lost %d, Waiting %d\n",
	 nsend, ndiscard, ntot - nsend - ndiscard);

  int64_t wght_id        = Client::get().get_wght_id();
  bool    is_downloading = Client::get().is_downloading();
  const char *buf_time   = Client::get().get_buf_wght_time();
  printf("- Recv Status: Weights' ID %" PRIi64 ", ", wght_id);

  if (is_downloading) puts("NOW DOWNLOADING NEW WEIGHTS\n");
  else                printf("Last Check %s\n\n", buf_time); }

int main() {
  OSI::prevent_multirun(FName("/tmp/autousi.jBQoNA7kEd.lock"));
  sleep_for(seconds(random_device()() % max_sleep));
  set_terminate(on_terminate);
  
  init();
  while (!flag_signal) {
    output();
    Pipe::get().wait(); }
  
  cout << "\nsignal " << flag_signal << " caught" << endl;
  Client::get().end();
  Pipe::get().end();
  return 0;
}
