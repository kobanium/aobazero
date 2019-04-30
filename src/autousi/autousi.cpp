// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "client.hpp"
#include "pipe.hpp"
#include "option.hpp"
#include <atomic>
#include <exception>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <fstream>
#include <vector>
using std::atomic;
using std::cerr;
using std::cout;
using std::current_exception;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::ifstream;
using std::map;
using std::random_device;
using std::rethrow_exception;
using std::string;
using std::set_terminate;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using namespace IOAux;
using ErrAux::die;
using uint   = unsigned int;
using ushort = unsigned short int;

constexpr uint max_sleep = 3000U; // in msec
volatile atomic<int> flag_signal(0);

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
			   {"PrintSpeed",    "0"},
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
  uint size_queue     = Config::get<uint>  (m, "SizeSendQueue", is_posi);
  uint recvTO         = Config::get<uint>  (m, "RecvTO",        is_posi);
  uint sendTO         = Config::get<uint>  (m, "SendTO",        is_posi);
  uint recv_bufsiz    = Config::get<uint>  (m, "RecvBufSiz",    is_posi);
  uint send_bufsiz    = Config::get<uint>  (m, "SendBufSiz",    is_posi);
  uint max_retry      = Config::get<uint>  (m, "MaxRetry",      is_posi);
  uint max_csa        = Config::get<uint>  (m, "MaxCSA");
  uint print_speed    = Config::get<uint>  (m, "PrintSpeed");
  uint print_csa      = Config::get<uint>  (m, "PrintCSA");
  uint keep_wght      = Config::get<uint>  (m, "KeepWeight");
  uint port           = Config::get<ushort>(m, "Port");
  vector<int> devices = Config::getv<int>  (m, "Device");

  Client::get().start(cstr_dwght, cstr_addr, port, recvTO, recv_bufsiz, sendTO,
		      send_bufsiz, max_retry, size_queue, keep_wght);
  OSI::handle_signal(on_signal);
  Pipe::get().start(cstr_cname, cstr_dlog, devices, cstr_csa, max_csa,
		    print_csa, print_speed);
  cout << "self-play start" << endl; }

int main() {
  OSI::prevent_multirun(FName("/tmp/autousi.jBQoNA7kEd.lock"));
  sleep_for(milliseconds(random_device()() % max_sleep));
  set_terminate(on_terminate);
  init();
  while (!flag_signal) Pipe::get().wait();
  
  cout << "signal " << flag_signal << " caught" << endl;
  Client::get().end();
  Pipe::get().end();
  return 0;
}
