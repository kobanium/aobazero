// 2019 Team AobaZero
// This source code is in the public domain.
#include "child.hpp"
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-srv.hpp"
#include "option.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "shogibase.hpp"
#include <atomic>
#include <deque>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
using std::atomic;
using std::cerr;
using std::cout;
using std::current_exception;
using std::deque;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::flush;
using std::map;
using std::rethrow_exception;
using std::set_terminate;
using std::string;
using std::stringstream;
using std::to_string;
using std::terminate;
using std::unique_ptr;
using std::vector;
using ErrAux::die;
using uint = unsigned int;

class USIEngine : public Child {
  using uint = unsigned int;
  const string _cmd;
  uint _game_id, _player_id;

public:
  USIEngine(uint game_id, uint player_id, const string &cmd) noexcept
  : _cmd(cmd), _game_id(game_id), _player_id(player_id) {}
  bool ok() const noexcept { return Child::ok() && _player_id < 2U; }
  uint get_game_id() const noexcept { return _game_id; }
  uint get_player_id() const noexcept { return _player_id; }
  const string &get_cmd() const noexcept { return _cmd; }
};

struct Game {
  Node<Param::maxlen_play_learn> node;
  USIEngine engine0, engine1;
  string startpos, record;
  uint nplay;
  Color turn0;
  Game(uint game_id, const string &cmd0, const string &cmd1) noexcept
  : engine0(game_id, 0U, cmd0), engine1(game_id, 1U, cmd1) {}
};

struct RowResult {
  NodeType type_term;
  Color turn;
  uint len;
  Color turn0;
  string record; };

static void procedure_io(USIEngine &myself, USIEngine &opponent,
			 Node<Param::maxlen_play_learn> &node,
			 string &startpos, string &record) noexcept;
static void node_update(USIEngine &myself, USIEngine &opponent,
			Node<Param::maxlen_play_learn> &node, string &startpos,
			string &record, char *line) noexcept;
static string addup_result(const NodeType &type_term, const Color &turn,
			   uint len, const Color &turn0,
			   uint result[][NodeType::ok_size][3]) noexcept;
static string result_out(Color turn, uint result[][NodeType::ok_size][3],
			 double &se) noexcept;
static void start_engine(USIEngine & c) noexcept;
static void start_newgame(Game &game, uint nplay, const Color &turn0) noexcept;
static int get_options(int argc, const char * const *argv) noexcept;
static void child_out(USIEngine &c, const char *fmt, ...) noexcept;
static void log_out(USIEngine &c, const char *fmt, ...) noexcept;
static void close_flush(USIEngine &c) noexcept;
static void file_out(const char *fmt, ...) noexcept;

constexpr char str_book[]   = "records2016_10818.sfen";
constexpr uint size_book    = 10831U;
static string book_file;
static void load_book_file(string file=str_book) noexcept;

const char *str_go_visit[] = { "go", "go visit" };
static int go_visit = 0;
static bool flag_f          = false;
static bool flag_r          = false;
static bool flag_u          = false;
static bool flag_s          = false;
static bool flag_b          = false;
static long int num_m       = 1;
static long int num_P       = 1;
static long int num_N       = 0;
static long int num_d       = 0;
static int num_B[2]         = {  1,  1};
static int num_U[2]         = { -1, -1};
static int num_H[2]         = {  0,  0};
static int num_T[2]         = { -1, -1};
static NNet::Impl impl_I[2] = { NNet::opencl, NNet::opencl };
static FName fname_W[2];

static FName shell("/bin/sh");
static string cmd0, cmd1;
static vector <string> book;
static string file_out_name;
static atomic<int> flag_signal(0);
static deque<NNetService> nnets;
static deque<SeqPRNService> seq_s;

static void on_terminate() {
  exception_ptr p = current_exception();
  try { if (p) rethrow_exception(p); }
  catch (const exception &e) { cout << e.what() << endl; }

  try { OSI::Semaphore::cleanup(); }
  catch (exception &e) { cerr << e.what() << endl; }
  
  try { OSI::MMap::cleanup(); }
  catch (exception &e) { cerr << e.what() << endl; }

  abort(); }

static void on_signal(int signum) { flag_signal = signum; }

int main(int argc, char **argv) {
  OSI::DirLock dlock(".");
  set_terminate(on_terminate);

  if (get_options(argc, argv) < 0) return 1;
  if (flag_b) load_book_file();
  if ( !book_file.empty() ) load_book_file(book_file);

  if (num_N == 2) {
    seq_s.emplace_back();
    nnets.emplace_back(impl_I[0], 0U, num_P, num_B[0], num_U[0], 0, num_H[0],
		       num_T[0], false, fname_W[0], "");
    nnets.emplace_back(impl_I[1], 1U, num_P, num_B[1], num_U[1], 0, num_H[1],
		       num_T[1], false, fname_W[1], ""); }
  else if (num_N == 1) {
    seq_s.emplace_back();
    nnets.emplace_back(impl_I[0], 0U, num_P * 2, num_B[0], num_U[0], 0,
		       num_H[0], num_T[0], false, fname_W[0], ""); }

  static vector<unique_ptr<Game>> games;
  for (uint u = 0; u < static_cast<uint>(num_P); ++u)
    games.emplace_back(new Game(u, cmd0, cmd1));

  //bool is_first = true;
  Color turn0 = SAux::black;
  uint nplay  = 0;
  uint latest = 0;
  uint result[Color::ok_size][NodeType::ok_size][3] = {{{0}}};
  bool is_first = true;
  if (num_d > 0) turn0 = SAux::white;

  OSI::handle_signal(on_signal);
  for (auto &ptr : games) {
    start_engine(ptr->engine0);
    start_engine(ptr->engine1);
    start_newgame(*ptr, nplay++, turn0);
    if (! flag_f) turn0 = turn0.to_opp(); }

  // main loop
  map<uint, RowResult> row_results;
  while (latest < num_m) {
    if (flag_signal) break;
    if (Child::wait(1000U) == 0) continue;

    for (auto &ptr : games) {
      procedure_io(ptr->engine0, ptr->engine1, ptr->node, ptr->startpos,
		   ptr->record);
      procedure_io(ptr->engine1, ptr->engine0, ptr->node, ptr->startpos,
		   ptr->record);
      if (! ptr->node.get_type().is_term()) continue;

      cout << "'Play #" << ptr->nplay << " ends." << endl;

      ptr->record += "\n%" + string(ptr->node.get_type().to_str()) + "\n";
      row_results[ptr->nplay] = { ptr->node.get_type(), ptr->node.get_turn(),
				  ptr->node.get_len_path(), ptr->turn0,
				  ptr->record };
      while (latest < num_m) {
	auto it = row_results.find(latest);
	if (it == row_results.end()) break;
	
	cout << "'Count result #" << latest << endl;
	string str = addup_result(it->second.type_term,
				  it->second.turn, it->second.len,
				  it->second.turn0, result);
	if (is_first) is_first = false;
	else          str     += "/\n";

	if (flag_r) str += it->second.record;
	cout << str;
	row_results.erase(it);
	latest += 1U; }
      
      start_newgame(*ptr, nplay++, turn0);
      if (! flag_f) turn0 = turn0.to_opp(); } }

  if (flag_signal) { cout << "cought signal " << flag_signal << endl; }
  for (auto &ptr : games) {
    close_flush(ptr->engine0);
    close_flush(ptr->engine1); }

  while (!nnets.empty()) nnets.pop_back();
  while (!seq_s.empty()) seq_s.pop_back();
  return 0; }

enum { win = 0, draw = 1, lose = 2 };

int sum_result(Color turn, uint result[][NodeType::ok_size][3], int abso ) {
  int n = result[turn.to_u()][SAux::repeated.to_u()   ][abso]
        + result[turn.to_u()][SAux::maxlen_term.to_u()][abso];
  if ( abso!=draw ) {
    n = result[turn.to_u()][SAux::resigned.to_u()    ][abso]
      + result[turn.to_u()][SAux::windclrd.to_u()    ][abso]
      + result[turn.to_u()][SAux::illegal_bwin.to_u()][abso]
      + result[turn.to_u()][SAux::illegal_wwin.to_u()][abso]; }
  return n; }

static string addup_result(const NodeType &type_term, const Color &turn,
			   uint len, const Color &turn0,
			   uint result[][NodeType::ok_size][3]) noexcept {
  assert(type_term.ok() && turn.ok() && turn0.ok());
  string str_ret;
  int abso = -1;

  if (type_term == SAux::resigned) {
    if (turn0 == turn) abso = lose;
    else               abso = win; }
  
  else if (type_term == SAux::windclrd) {
    if (turn0 == turn) abso = win;
    else               abso = lose; }
  
  else if (type_term == SAux::repeated) abso = draw;

  else if (type_term == SAux::illegal_bwin) {
    if (turn0 == SAux::black) abso = win;
    else                      abso = lose; }

  else if (type_term == SAux::illegal_wwin) {
    if (turn0 == SAux::black) abso = lose;
    else                      abso = win; }
  
  else if (type_term == SAux::maxlen_term) abso = draw;
  
  else die(ERR_INT("INTERNAL ERROR"));
  assert(abso != -1);
  
  result[turn0.to_u()][type_term.to_u()][abso] += 1U;
  uint tot[1][NodeType::ok_size][3];
  for (uint ut = 0; ut < NodeType::ok_size; ++ut)
    for (uint ur = 0; ur < 3; ++ur) {
      tot[0][ut][ur] = ( result[SAux::black.to_u()][ut][ur]
		      + result[SAux::white.to_u()][ut][ur] ); }

  double se;
  if (flag_s) {
    str_ret += "'Results of player0 when it plays sente:\n";
    str_ret += result_out(SAux::black, result, se);
    str_ret += "'\n";
    str_ret += "'Results of player0 when it plays gote :\n";
    str_ret += result_out(SAux::white, result, se);
    str_ret += "'\n"; }
  
  str_ret += "'Results of player0:\n";
  str_ret += result_out(SAux::black, tot, se);

  if ( 1 ) {
    int nwin  = sum_result( SAux::black, tot, win);
    int ndraw = sum_result( SAux::black, tot, draw);
    int nlose = sum_result( SAux::black, tot, lose);
    int ntot  = nwin + nlose + ndraw;
    int bs_win  = sum_result( SAux::black, result, win);
    int bs_lose = sum_result( SAux::black, result, lose);
    int ws_win  = sum_result( SAux::white, result, win);
    int ws_lose = sum_result( SAux::white, result, lose);
    int s_win  = bs_win  + ws_lose;
    int s_lose = bs_lose + ws_win;
    int rep   = tot[SAux::black.to_u()][SAux::repeated.to_u()][draw];
    int dcl_w = tot[SAux::black.to_u()][SAux::windclrd.to_u()][win];
    int dcl_l = tot[SAux::black.to_u()][SAux::windclrd.to_u()][lose];
    float s_rate = 0;
    if (s_win + s_lose) s_rate = (float)s_win / (s_win + s_lose);

    double wr = ((static_cast<double>(nwin) + static_cast<double>(ndraw) / 2.0)
		 / static_cast<double>(ntot));
    double elo = 0.0;
    if ( wr != 0 && wr != 1.0 ) elo = -400.0 * log10(1.0 / wr - 1.0);
    //  if (1e-5 < wr) elo = -400.0 * log10(1.0 / wr - 1.0);

    // 95% confidence interval
    // double ci = 1.96*sqrt(wr*(1.0 - wr)/static_cast<double>(ntot));
    double ci = 1.96 * se;
    std::time_t tt = std::time(nullptr);
    struct std::tm *tb = std::localtime(&tt);
    char str[1024];
    sprintf(str, "'%4d-%02d-%02d %02d:%02d:%02d %d-%d-%d %d "
	    "(%d-%d-%d)(s=%d-%d,%5.3f), m=%3d, wr=%5.3f(%5.3f)(%4d)\n",
	    tb->tm_year+1900, tb->tm_mon+1, tb->tm_mday, tb->tm_hour,
	    tb->tm_min,tb->tm_sec, nwin, ndraw, nlose, ntot, dcl_w, rep,
	    dcl_l, s_win,s_lose,s_rate, len, wr, ci, static_cast<int>(elo) );
    str_ret += str;
    file_out("%s",str); }

  return str_ret; }

static string result_out(Color turn, uint result[][NodeType::ok_size][3],
			 double &se) noexcept {
  se = 0.0;
  stringstream ss;
  int nwin  = sum_result( turn, result, win);
  int ndraw = sum_result( turn, result, draw);
  int nlose = sum_result( turn, result, lose);
  int ntot  = nwin + nlose + ndraw;
  
  ss << "'- Win : " << nwin << " (res "
     << result[turn.to_u()][SAux::resigned.to_u()][win]
     << ", dcl "
     << result[turn.to_u()][SAux::windclrd.to_u()][win]
     << ", pck "
     << (result[turn.to_u()][SAux::illegal_bwin.to_u()][win]
	 + result[turn.to_u()][SAux::illegal_wwin.to_u()][win])
     << ")\n";
  
  ss << "'- Draw: " << ndraw << " (rep "
     << result[turn.to_u()][SAux::repeated.to_u()][draw]
     << ")\n";
  
  ss << "'- Lose: " << nlose << " (res "
     << result[turn.to_u()][SAux::resigned.to_u()][lose]
     << ", dcl "
     << result[turn.to_u()][SAux::windclrd.to_u()][lose]
     << ", pck "
     << (result[turn.to_u()][SAux::illegal_bwin.to_u()][lose]
	 + result[turn.to_u()][SAux::illegal_wwin.to_u()][lose])
     << ")\n";

  double ret_se = 0;
  // win 1 point, draw 0.5 point, lose 0 point
  if (2 <= ntot) {
    double deno  = 1.0 / static_cast<double>(ntot);
    double pwin  = deno * static_cast<double>(nwin);
    double pdraw = deno * static_cast<double>(ndraw);
    double plose = deno * static_cast<double>(nlose);
    double mean  = pwin + 0.5 * pdraw;
    
    double corr  = static_cast<double>(ntot) / static_cast<double>(ntot - 1);
    double dwin  = 1.0 - mean;
    double ddraw = 0.5 - mean;
    double dlose = 0.0 - mean;
    se = sqrt( deno * corr * ( dwin * dwin * pwin
			       + ddraw * ddraw * pdraw
			       + dlose * dlose * plose ) );
    ss << "'point: " << mean << " pm " << 1.96 * se << "\n"; }

  return ss.str(); }

static void procedure_io(USIEngine &myself, USIEngine &opponent,
			 Node<Param::maxlen_play_learn> &node,
			 string &startpos, string &record) noexcept {
  assert(myself.ok() && opponent.ok() && node.ok());
  bool eof = false;
  char line[65536];

  if (myself.has_line_err()) {
    uint ret = myself.getline_err(line, sizeof(line));
    if (ret == 0) eof = true;
    else log_out(myself, "%s", line); }

  if (myself.has_line_in()) {
    uint ret = myself.getline_in(line, sizeof(line));
    if (ret == 0) eof = true;
    else {
      log_out(myself, "%s", line);
      node_update(myself, opponent, node, startpos, record, line); } }

  if (eof) {
    while (0 < myself.getline_err(line, sizeof(line)))
      log_out(myself, "%s", line);
    while (0 < myself.getline_in(line, sizeof(line)))
      log_out(myself, "%s", line);
    die(ERR_INT("Player %u of game %u terminates.\n%s",
		myself.get_player_id(), myself.get_game_id(),
		static_cast<const char *>(node.to_str()))); } }

static void node_update(USIEngine &myself, USIEngine &opponent,
			Node<Param::maxlen_play_learn> &node, string &startpos,
			string &record, char *line) noexcept {
  assert(myself.ok() && opponent.ok() && node.ok() && line);  
  char *token = strtok(line, " ");
  if (token == nullptr) return;
  if (strcmp(token, "bestmove")) return;
  token = strtok(nullptr, " ,\t");

  Action action = node.action_interpret(token, SAux::usi);
  if (!action.ok())
    die(ERR_INT("Bad move %s (Player %u of game %u)\n%s", token,
		myself.get_player_id(), myself.get_game_id(),
		static_cast<const char *>(node.to_str())));

  if (action.is_move()) {
    if ( go_visit ) {
      record += "\n";
    } else {
      if ((node.get_len_path() % 8U) == 0) record += "\n";
      else record += ",";
    }
    record += node.get_turn().to_str();
    record += action.to_str(SAux::csa);
 }

  if ( action.is_move() && go_visit ) {
    string new_info;
    const char *str_value = strtok(nullptr, " ,");
    if (!str_value || str_value[0] != 'v' || str_value[1] != '=') die(ERR_INT("cannot read value (engine)"));
    char *endptr;
    float value = strtof(str_value+2, &endptr);
    if (endptr == str_value+2 || *endptr != '\0' || value < 0.0f || value == HUGE_VALF) die(ERR_INT("cannot interpret value %s (engine)", str_value+2));
    const char *str_count = strtok(nullptr, " ,");
    if (!str_count) die(ERR_INT("cannot read count (engine)"));
    long int num = strtol(str_count, &endptr, 10);
    if (endptr == str_count || *endptr != '\0' || num < 1 || num == LONG_MAX) die(ERR_INT("cannot interpret visit count %s (engine)", str_count));
    int num_all = num;
    {
      char buf[256];
      sprintf(buf, "v=%.3f,%ld", value, num);
      new_info += buf;
    }
    // read candidate moves
    int num_tot = 0;
    while (true) {
      char *str_move_usi = strtok(nullptr, " ,");
      if (!str_move_usi) {
//      new_info += "\n";
        break;
      }

      Action action = node.action_interpret(str_move_usi, SAux::usi);
      if (!action.is_move()) die(ERR_INT("bad candidate %s (engine)", str_move_usi));
      new_info += ",";
      new_info += action.to_str(SAux::csa);

      str_count = strtok(nullptr, " ,");
      if (!str_count) die(ERR_INT("cannot read count (engine)"));

      num = strtol(str_count, &endptr, 10);
      if (endptr == str_count || *endptr != '\0' || num < 1 || num == LONG_MAX) die(ERR_INT("cannot interpret a visit count %s (engine)", str_count));

      num_tot  += num;
      new_info += ",";
      new_info += to_string(num);
    }
    if (num_all < num_tot) die(ERR_INT("bad counts (engine)"));

    record += ",'" + new_info;
  }

  node.take_action(action);

  // Declare win if possible
  if (node.get_type().is_interior() && node.is_nyugyoku())
    node.take_action(SAux::windecl);

  if (node.get_type().is_term()) return;

  startpos += string(" ") + string(token);
  child_out(myself,   startpos.c_str());
  child_out(opponent, startpos.c_str());
  child_out(opponent, str_go_visit[go_visit]); }

static void start_newgame(Game &game, uint nplay, const Color &turn0) noexcept {
  bool fTurn0Black = (turn0 == SAux::black);
  cout << "'Play #" << nplay << " starts from player"
       << (fTurn0Black ? "0." : "1.") << endl;

  bool fSenteName = fTurn0Black;
  if ( num_d ) fSenteName = true;
  game.record  = "";
  game.record += "N+player" + string(fSenteName ? "0\n" : "1\n");
  game.record += "N-player" + string(fSenteName ? "1\n" : "0\n");

  game.nplay = nplay;
  game.turn0 = turn0;
  game.node.clear(num_d);

  if ( num_d == 0 ) {
    game.record += "PI\n+";
  } else {
    string s = static_cast<const char *>(game.node.to_str());
    for (int i=0; i<9; i++) s.at(30*i+1) = s.at(30*i+1)+1;
    int n = s.find("Hand");
    game.record += s.substr(0, n);
    game.record += "-";
  }

  child_out(game.engine0, "usinewgame");
  child_out(game.engine1, "usinewgame");


  if ( !book_file.empty() ) {
    const int KOMAOCHI_BOOK_SIZE = 800;
    const int KOMAOCHI_BOOK_MOVES = 16;
    int n = nplay;
    if ( num_d==0 && !flag_f ) n = n / 2;
    int book_i = n % KOMAOCHI_BOOK_SIZE;
    game.startpos = "position " + book[book_i];
    int del = 6;
    if ( game.startpos.find("startpos") != string::npos ) del = 2;
    std::stringstream ss(book[book_i]);
    for (int i = 0; i < KOMAOCHI_BOOK_MOVES + del; ++i) {
      string token;
      ss >> token;
      if (i < del) continue;
      Action action = game.node.action_interpret(token.c_str(), SAux::usi);
      if (! action.ok()) die(ERR_INT("Bad book move %s", token.c_str()));

      if (action.is_move()) {
        if ((game.node.get_len_path() % 8U) == 0) game.record += "\n";
        else game.record += ",";
        game.record += game.node.get_turn().to_str();
        game.record += action.to_str(SAux::csa);
      }
      game.node.take_action(action);
    }
  } else if (num_d>0) { // hadicap game. drop ky,ka,hi,2mai,4mai,6mai. also change shogibase.cpp node.clear(min_d)
	string init_pos[HANDICAP_TYPE-1] = {
      "position sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",// ky
      "position sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	// ka
      "position sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	// hi
      "position sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	// 2mai
      "position sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	// 4mai
      "position sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"		// 6mai
    };
    if (num_d>=HANDICAP_TYPE || !flag_f) die(ERR_INT("Handicap game Err. Must be with 'f'"));
    game.startpos = init_pos[num_d-1] + " w - 1 moves";
  } else if (flag_f || ! flag_b) {
    game.startpos = "position startpos moves";
  } else {
    game.startpos = "position " + book[nplay / 2U];
    std::stringstream ss(book[nplay / 2U]);
    for (int i = 0; i < 24 + 2; ++i) {
      string token;
      ss >> token;
      if (i < 2) continue;
      Action action = game.node.action_interpret(token.c_str(), SAux::usi);
      if (! action.ok()) die(ERR_INT("Bad book move %s", token.c_str()));

      if (action.is_move()) {
        if ((game.node.get_len_path() % 8U) == 0) game.record += "\n";
        else game.record += ",";
        game.record += game.node.get_turn().to_str();
        game.record += action.to_str(SAux::csa);
      }
      game.node.take_action(action);
    }
  }
  child_out(game.engine0, game.startpos.c_str());
  child_out(game.engine1, game.startpos.c_str());

  if (fTurn0Black) child_out(game.engine0, str_go_visit[go_visit]);
  else             child_out(game.engine1, str_go_visit[go_visit]);
}

static void start_engine(USIEngine &c) noexcept {
  assert(c.ok());
  char line[65536];
  char a0[shell.get_len_fname() + 1];
  char a1[] = "-c";
  char a2[c.get_cmd().size() + 1];
  memcpy(a0, shell.get_fname(), sizeof(a0));
  memcpy(a2, c.get_cmd().c_str(), sizeof(a2));
  char *argv[] = { a0, a1, a2, nullptr };
  c.open(shell.get_fname(), argv);

  class EoF {};
  try {
    child_out(c, "usi");
    while (true) {
      if (c.getline_in(line, sizeof(line)) == 0) throw EoF();
      log_out(c, "%s", line);
      if (strcmp(line, "usiok") == 0) break; }

    child_out(c, "isready");
    while (true) {
      if (c.getline_in(line, sizeof(line)) == 0) throw EoF();
      log_out(c, "%s", line);
      if (strcmp(line, "readyok") == 0) break; } }
  catch (const EoF &e) {
    while (0 < c.getline_err(line, sizeof(line))) log_out(c, "%s", line);
    while (0 < c.getline_in (line, sizeof(line))) log_out(c, "%s", line);
    die(ERR_INT("Player %u of game %u terminates.",
		c.get_player_id(), c.get_game_id())); }
  catch (...) { terminate(); } }

static void child_out(USIEngine &c, const char *fmt, ...) noexcept {
  assert(c.ok() && fmt);
  bool flag_error = false;
  char buf[65536];
  va_list list;
  
  va_start(list, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, list);
  va_end(list);
  if (sizeof(buf) <= static_cast<size_t>(nb) + 1U) {
    flag_error = true;
    buf[sizeof(buf) - 3U] = '=';
    buf[sizeof(buf) - 2U] = '\n';
    buf[sizeof(buf) - 1U] = '\0'; }
  else {
    buf[nb]     = '\n';
    buf[nb + 1] = '\0'; }
  
  if (flag_u)
    cout << "OUT (" << c.get_game_id() << "-" << c.get_player_id()
	 << "): " <<  buf << flush;

  if (flag_error)
    die(ERR_INT("buffer overrun (player %u of game %u)",
		c.get_player_id(), c.get_game_id()));

  if (!c.write(buf, strlen(buf))) {
    if (errno == EPIPE) die(ERR_INT("Player %u of game %u terminates.",
				    c.get_player_id(), c.get_game_id()));
    die(ERR_CLL("write")); } }

static void log_out(USIEngine &c, const char *fmt, ...) noexcept {
  assert(c.ok() && fmt);
  if (!flag_u) return;
  bool flag_error = false;
  char buf[65536];
  va_list list;
  
  va_start(list, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, list);
  va_end(list);
  if (sizeof(buf) <= static_cast<size_t>(nb) + 1U) {
    flag_error = true;
    buf[sizeof(buf) - 3U] = '=';
    buf[sizeof(buf) - 2U] = '\n';
    buf[sizeof(buf) - 1U] = '\0'; }
  else {
    buf[nb]     = '\n';
    buf[nb + 1] = '\0'; }

  cout << "IN (" << c.get_game_id() << "-" << c.get_player_id() << "): "
       << buf << flush;
  
  if (flag_error)
    die(ERR_INT("buffer overrun (player %u of game %u)",
		c.get_player_id(), c.get_game_id())); }

static void close_flush(USIEngine &c) noexcept {
  assert(c.ok());
  char line[65536];

  child_out(c, "quit");
  while (true) {
    if (c.getline_err(line, sizeof(line)) == 0) break;
    log_out(c, "%s", line); }
    
  while (true) {
    if (c.getline_in(line, sizeof(line)) == 0) break;
    log_out(c, "%s", line); }

  c.close(); }

static void file_out(const char *fmt, ...) noexcept {
  assert(fmt);
  FILE *fp = fopen(file_out_name.c_str(), "a");
  if (fp == NULL) die(ERR_CLL("fopen"));

  char buf[65536];
  va_list list;
  va_start(list, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, list);
  va_end(list);
  fprintf(fp, "%s", buf);
  fclose(fp); }

static int get_options(int argc, const char * const *argv) noexcept {
  assert(0 < argc && argv && argv[0]);
  char buf[1024];
  bool flag_err = false;
  char *endptr;
  uint num;

  while (! flag_err) {
    int opt = Opt::get(argc, argv, "0:1:c:m:d:P:I:B:U:H:W:T:o:frsubv");
    if (opt < 0) break;

    switch (opt) {
    case '0': cmd0 = string(Opt::arg); break;
    case '1': cmd1 = string(Opt::arg); break;
    case 'f': flag_f = true; break;
    case 'r': flag_r = true; break;
    case 's': flag_s = true; break;
    case 'u': flag_u = true; break;
    case 'b': flag_b = true; break;
    case 'v': go_visit = 1; break;
    case 'c': shell.reset_fname(Opt::arg); break;
    case 'd':
      num_d = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0'
	  || num_d >= HANDICAP_TYPE || num_d < 0) flag_err = true;
      break;
    case 'o':
      book_file = string(Opt::arg);
      break;
    case 'm':
      num_m = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0'
	  || num_m == LONG_MAX || num_m < 1) flag_err = true;
      break;
    case 'P':
      num_P = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0'
	  || num_P == LONG_MAX || num_P < 1) flag_err = true;
      break;
    case 'I':
      num = 0;
      strncpy(buf, Opt::arg, sizeof(buf));
      buf[sizeof(buf) - 1U] = '\0';
      for (char *str = buf;; str = nullptr) {
	const char *token = OSI::strtok(str, ":", &endptr);
	if (!token) break;
	if (2U <= num) { flag_err = true; break; }
	if      (token[0] == 'B') impl_I[num] = NNet::cpublas;
	else if (token[0] == 'O') impl_I[num] = NNet::opencl;
	else                      flag_err = true;
	if (2U < ++num) { flag_err = true; break; } }
      break;
    case 'W':
      strncpy(buf, Opt::arg, sizeof(buf));
      buf[sizeof(buf) - 1U] = '\0';
      for (char *str = buf;; str = nullptr) {
	const char *token = OSI::strtok(str, ":", &endptr);
	if (!token) break;
	if (2U <= num_N) { flag_err = true; break; }
	fname_W[num_N++].reset_fname(token); }
      if (num_N == 0) flag_err = true;
      break;
    case 'B':
      num = 0;
      for (const char *str = Opt::arg;; str = endptr + 1) {
	num_B[num] = strtol(str, &endptr, 10);
	if (endptr == str || (*endptr != '\0' && *endptr != ':')
	    || num_B[num] < 1 || INT_MAX <= num_B[num]) flag_err = true;
	if (*endptr != ':') break;
	if (2U <= ++num) { flag_err = true; break; } }
      break;
    case 'U':
      num = 0;
      for (const char *str = Opt::arg;; str = endptr + 1) {
	num_U[num] = strtol(str, &endptr, 10);
	if (endptr == str || (*endptr != '\0' && *endptr != ':')
	    || num_U[num] < -1 || INT_MAX <= num_U[num]) flag_err = true;
	if (*endptr != ':') break;
	if (2U <= ++num) { flag_err = true; break; } }
      break;
    case 'H':
      num = 0;
      for (const char *str = Opt::arg;; str = endptr + 1) {
	num_H[num] = strtol(str, &endptr, 10);
	if (endptr == str || (*endptr != '\0' && *endptr != ':')
	    || num_H[num] < 0 || 1 < num_H[num]) flag_err = true;
	if (*endptr != ':') break;
	if (2U <= ++num) { flag_err = true; break; } }
      break;
    case 'T':
      num = 0;
      for (const char *str = Opt::arg;; str = endptr + 1) {
	num_T[num] = strtol(str, &endptr, 10);
	if (endptr == str || (*endptr != '\0' && *endptr != ':')
	    || num_T[num] < -1 || INT_MAX <= num_T[num]) flag_err = true;
	if (*endptr != ':') break;
	if (2U <= ++num) { flag_err = true; break; } }
      break;
    default: flag_err = true; break; } }

  if (!flag_err && 0 < cmd0.size() && 0 < cmd1.size()) {
    cout << "'-------------------------------------------------------------\n";
    cout << "'Player0:    " << shell.get_fname() << " -c \"" << cmd0 << "\"\n";
    cout << "'Player1:    " << shell.get_fname() << " -c \"" << cmd1 << "\"\n";
    cout << "'Gameplays:  " << num_m << "\n";
    cout << "'Fix color?  " << (flag_f ? "Yes\n" : "No\n");
    cout << "'Out USI?    " << (flag_u ? "Yes\n" : "No\n");
    cout << "'Book?       " << (flag_b ? "Yes\n" : "No\n");
    cout << "'Parallel:   " << num_P << "\n";
    cout << "'Handicap:   " << num_d << "\n";
    cout << "'Book file:  " << book_file << "\n";
    for (int i = 0; i < num_N; ++i) {
      cout << "'NNet" << i << ":\n";
      cout << "'- Implimentation " << (int)impl_I[i] << "\n";
      cout << "'- Weight:        " << fname_W[i].get_fname() << "\n";
      cout << "'- BatchSize:     " << num_B[i] << "\n";
      cout << "'- Device:        " << num_U[i] << "\n";
      cout << "'- Half:          " << num_H[i] << "\n";
      cout << "'- Thread:        " << num_T[i] << "\n"; }
    cout.flush();
    std::time_t tt = std::time(nullptr);
    struct std::tm *tb = std::localtime(&tt);
    char str[256];
    sprintf(str, "%4d%02d%02d_%02d%02d%02d",
	    tb->tm_year+1900, tb->tm_mon+1, tb->tm_mday, tb->tm_hour,
	    tb->tm_min, tb->tm_sec);
    file_out_name = str;
    auto c0s = cmd0.find("w0000");
    auto c0e = cmd0.rfind(".txt");
    if ( c0s > 0 && c0e > 0 && c0s+13 == c0e )
      file_out_name += "_w" + cmd0.substr(c0s+9, 4);

    auto c1s = cmd1.find("NodesLimit value ");
    if ( c1s != std::string::npos ) file_out_name += "_" + cmd1.substr(c1s+17);
    auto fs = file_out_name.find(" ");
    if ( fs  != std::string::npos ) file_out_name = file_out_name.substr(0,fs);
    file_out_name += ".txt";
    file_out("Player0: %s\n", cmd0.c_str());
    file_out("Player1: %s\n", cmd1.c_str());
    cout << "'" << file_out_name << "\n";
    return 0; }

  cerr << "\nUsage: " << Opt::cmd << " [OPTION] -0 \"CMD0\" -1 \"CMD1\"\n";
  cerr << R"(
      Generate gameplays between two USI shogi engines

Mandatory options:
  -0 "CMD0" Start player0 as '/bin/sh -c "CMD0"'.
  -1 "CMD1" Start player1 as '/bin/sh -c "CMD1"'.

Other options:
  -m NUM   Generate NUM gameplays. NUM must be a positive integer. The default
           value is 1.
  -f       Always assign player0 to Sente (black). If this is not specified,
           then Sente and Gote (white) are assigned alternatively.
  -r       Print CSA records.
  -s       Print results in detail.
  -u       Print verbose USI messages.
  -b       Use positions recorded in records2016_10818.sfen (a collection of
           24 moves from the no-handicap initial position).
  -o STR   Use positions recorded file. Especially for hadicap opening book.
  -v       use 'go visit' to get aobak 'v=' and searched moves and nodes.
  -d NUM   Handicap Game. NUM = 1(ky), 2(ka), 3(hi), 4(2mai), 5(4mai), 6(6mai)
  -c SHELL Use SHELL, e.g., /bin/csh, instead of /bin/sh.
  -P NUM   Generate NUM gameplays simultaneously. The default is 1.
  -I STR   Specifies nnet implementation. STR can conatin two characters
           separated by ':'. Character 'B' means CPU BLAS implementation, and
           'O' means OpenCL implimentation. The default is 'O'.
  -B STR   Specifies batch sizes of nnet computation. STR can contain two
           sizes separated by ':'. The default size is 1.
  -W STR   Specifies weight paths for nnet computation. STR can contain two
           file names separated by ':'.
  -U STR   Specifies device IDs of OpenCL nnet computation. STR can contain
           two IDs separated by ':'. Each ID must be different from the other.
  -H STR   OpenCL uses half precision floating-point values. STR can contain
           two binary values separated by ':'. The value should be 1 (use
           half and NVIDIA wmma instructions if possible), or 0 (do not use
           half). The default is 0.
  -T STR   Specifies the number of threads for CPU BLAS computation. STR can
           contain two numbers separated by ':'. The default is -1 (means an
           upper bound of the number).
Example:
  )" << Opt::cmd << R"( -0 "~/aobaz -w ~/w0.txt" -1 "~/aobaz -w ~/w1.txt"
           Generate a gameplay between 'w0.txt' (black) and 'w1.txt' (white)

)";

  return -1; }

static void load_book_file(string file) noexcept {

  std::ifstream ifs(file);
  if (!ifs) die(ERR_INT("Cannot open %s", file));

  string str;
  while (std::getline(ifs, str)) {
    int n = str.size();
    if ( n > 0 && (str.at(n-1)=='\r' || str.at(n-1)=='\n') ) str.pop_back();	// delete \r \n
    n = str.size();
    if ( n > 0 && (str.at(n-1)=='\r' || str.at(n-1)=='\n') ) str.pop_back();
//  str.pop_back(); // delete last " "
    book.push_back(std::move(str));
  }
  if ( file == str_book ) {
	if ( book.size() != size_book ) die(ERR_INT("Bad book size %d", book.size()));
    if (flag_f) die(ERR_INT("Option -f with -b is not supported."));
  } else {
    if (flag_b) die(ERR_INT("Option -b with -o is not supported."));
    if (!flag_f && num_d!=0 ) die(ERR_INT("-o must be with -f."));
  }

  std::random_device seed_gen;
  auto seed = seed_gen();
  std::mt19937 engine(seed);
  std::shuffle(book.begin(), book.end(), engine);
  cout << "'Read " << book.size() << " lines from " << file << " seed=" << seed << std::endl;
}
