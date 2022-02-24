// 2019 Team AobaZero
// This source code is in the public domain.
#include "datakeep.hpp"
#include "err.hpp"
#include "listen.hpp"
#include "logging.hpp"
#include "option.hpp"
#include "osi.hpp"
#include "param.hpp"
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
using std::cout;
using std::current_exception;
using std::endl;
using std::exception_ptr;
using std::rethrow_exception;
using std::string;
using std::unique_ptr;
using std::ifstream;
using std::set_terminate;
using std::map;
using std::exception;
using uint = unsigned int;
using namespace IOAux;
using ErrAux::die;

static constexpr char fname_quit[] = "quit";
unique_ptr<Logger> logger;

static void init() noexcept {
  map<string, string> m = {{"DirWeight",         "./weight"},
			   {"DirArchives",       "./archive"},
			   {"DirPool",           "./pool"},
			   {"WeightPolling",     "60"},
			   {"PortPlayer",        "20001"},
			   {"ClusterData",       "1000"},
			   {"BackLog",           "64"},
			   {"TimeoutSelect",     "5"},
			   {"TimeoutPlayer",     "3600"},
			   {"MaxAccept",         "2000"},
			   {"SizeQueue",         "256"},
			   {"MaxRecv",           "524288"},
			   {"MaxSend",           "65536"},
			   {"MaxSizeCSA",        "2097152"},
			   {"Log2LenRedundant",  "18"},
			   {"MinLenPlay",        "16"},
			   {"MinAveChildren",    "3"},
			   {"MaxConnPerAddrSec", "64"},
			   {"MaxConnPerAddrMin", "256"},
			   {"CutConnPerAddrMin", "1024"},
			   {"MaxComPerAddr",     "104857600"},
			   {"WeightBlock",       "1048576"},
			   {"DirLog",            "./log"},
			   {"LenLogArchive",     "67108864"},
			   {"ResignEMADeno",     "2000"},
			   {"ResignEMAInit",     "0.10"},
			   {"ResignMRate",       "0.05"}};
  try { Config::read("server.cfg", m); } catch (exception &e) { die(e); }
  const char *dir_wght = Config::get_cstr(m, "DirWeight",   maxlen_path);
  const char *dir_arch = Config::get_cstr(m, "DirArchives", maxlen_path);
  const char *dir_pool = Config::get_cstr(m, "DirPool",     maxlen_path);
  const char *dir_log  = Config::get_cstr(m, "DirLog",      maxlen_path);
  uint resign_ema_deno = Config::get<uint>(m, "ResignEMADeno",
					   [](uint u){ return 0U < u; });
  float resign_ema_init
    = Config::get<float>(m, "ResignEMAInit",
			 [](float v){ return 0.0 <= v && v <= 1.0; });
  float resign_mrate
    = Config::get<float>(m, "ResignMRate",
			 [](float v){ return 0.0 <= v && v <= 1.0; });

  uint port_p      = Config::get<ushort>(m, "PortPlayer");
  uint size_queue  = Config::get<uint>  (m, "SizeQueue");
  uint wght_poll   = Config::get<uint>  (m, "WeightPolling");
  uint backlog     = Config::get<uint>  (m, "BackLog");
  uint selectTO    = Config::get<uint>  (m, "TimeoutSelect");
  uint playerTO    = Config::get<uint>  (m, "TimeoutPlayer");
  uint max_accept  = Config::get<uint>  (m, "MaxAccept");
  uint max_recv    = Config::get<uint>  (m, "MaxRecv");
  uint len_block   = Config::get<uint>  (m, "WeightBlock");
  uint len_logarch = Config::get<uint>  (m, "LenLogArchive");
  uint maxlen_com  = Config::get<uint>  (m, "MaxComPerAddr",
					 [](uint u){ return 1023U < u; });
  uint maxconn_sec = Config::get<uint>  (m, "MaxConnPerAddrSec",
					 [](uint u){ return 1U < u; });
  uint maxconn_min = Config::get<uint>  (m, "MaxConnPerAddrMin",
					 [](uint u){ return 1U < u; });
  uint cutconn_min = Config::get<uint>  (m, "CutConnPerAddrMin",
					 [](uint u){ return 1U < u; });
  uint maxlen_csa  = Config::get<uint>  (m, "MaxSizeCSA",
					 [](uint u){ return 63U < u; });
  uint max_send    = Config::get<uint>  (m, "MaxSend",
					 [](uint u){ return 15U < u; });
  uint minlen_play  = Config::get<uint> (m, "MinLenPlay");
  uint minave_child = Config::get<uint> (m, "MinAveChildren");
  uint log2_redun = Config::get<uint>(m, "Log2LenRedundant",
				      [](uint u){ return 1U < u && u < 30U; });
  
  logger.reset(new Logger(dir_log, "server", len_logarch));
  logger->out(nullptr, "start server %d.%d (usi engine %d)",
	      Ver::major, Ver::minor, Ver::usi_engine);
  WghtKeep::get().start(logger.get(), dir_wght, wght_poll);
  RecKeep::get().start(logger.get(), dir_arch, dir_pool, size_queue,
		       maxlen_csa, max_recv, log2_redun - 1U, minlen_play,
		       minave_child, resign_ema_deno, resign_ema_init,
		       resign_mrate);
  Listen::get().start(logger.get(), port_p, backlog, selectTO, playerTO,
		      max_accept, max_recv, max_send, len_block, maxconn_sec,
		      maxconn_min, cutconn_min, maxlen_com); }

static void on_terminate() {
  exception_ptr p = current_exception();
  try { if (p) rethrow_exception(p); }
  catch (const exception &e) { cout << e.what() << endl; }
  abort(); }

int main() {
  OSI::prevent_multirun(FName(Param::name_server));
  set_terminate(on_terminate);
  if (remove(fname_quit) < 0 && errno != ENOENT) die(ERR_CLL("remove"));

  init();
  while (true) {
    ifstream ifs(fname_quit);
    if (ifs) break;
    
    Listen::get().wait(); }

  RecKeep::get().end();
  WghtKeep::get().end();
  Listen::get().end();
  return 0; }
