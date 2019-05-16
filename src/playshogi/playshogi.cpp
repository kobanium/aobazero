// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "option.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "shogibase.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
using std::cerr;
using std::cout;
using std::current_exception;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::flush;
using std::rethrow_exception;
using std::set_terminate;
using std::string;
using ErrAux::die;
using uint = unsigned int;

class USIEngine : public OSI::Pipe {
  uint _id;
  const string & _cmd;
public:
  USIEngine(uint id, const string &cmd)  noexcept : _id(id), _cmd(cmd) {}
  bool ok() const noexcept { return OSI::Pipe::ok() && _id < 2; }
  uint get_id() const noexcept { return _id; }
  const string & get_cmd() const noexcept { return _cmd; }
};

static void play_update(USIEngine *players[],
			Node<Param::maxlen_play> & node, int index, char *line,
			string & startpos) noexcept;
static void play_shogi(USIEngine & player_black, USIEngine & player_white,
		       Node<Param::maxlen_play> & node) noexcept;
static void addup_result(const Node<Param::maxlen_play> & node,
			 const Color & turn_player0, int nplay,
			 uint result[][NodeType::ok_size][3])
  noexcept;
static void result_out(Color turn, uint result[][NodeType::ok_size][3])
  noexcept;
static void start_engine(USIEngine & c) noexcept;
static int get_options(int argc, const char * const *argv) noexcept;
static void child_out(USIEngine &c, const char *fmt, ...) noexcept;
static void log_out(USIEngine &c, const char *fmt, ...) noexcept;
static void close_flush(USIEngine &c) noexcept;
static bool flag_f = false;
static bool flag_r = false;
static bool flag_u = false;
static bool flag_s = false;
static long int num_m = 1;
static FName shell("/bin/sh");
static string cmd0, cmd1;

static void on_terminate() {
  exception_ptr p = current_exception();
  try { if (p) rethrow_exception(p); }
  catch (const exception &e) { cout << e.what() << endl; }
  abort(); }

int main(int argc, char **argv) {
  set_terminate(on_terminate);
  if (get_options(argc, argv) < 0) return 1;
  
  USIEngine player0(0, cmd0), player1(1, cmd1);
  bool is_first(true);
  Color turn_player0 = SAux::black;
  uint result[Color::ok_size][NodeType::ok_size][3] = {{{0}}};
    
  start_engine(player0);
  start_engine(player1);
  for (int nplay = 0; nplay < num_m; ++nplay) {
    cout << "'\n'no.=" << nplay << ", player0="
	 << ((turn_player0 == SAux::black) ? "black" : "white") << endl;
    
    if (is_first) {
      is_first = false;
      if (flag_r) cout << "PI\n+" << endl; }
    else if (flag_r) cout << "/\nPI\n+" << endl;

    Node<Param::maxlen_play> node;
    if (turn_player0 == SAux::black) play_shogi(player0, player1, node);
    else                             play_shogi(player1, player0, node);
    
    const NodeType & type = node.get_type();
    assert(type.is_term());
    if (flag_r && type.is_term()) cout << "%" << type.to_str() << endl;
    
    addup_result(node, turn_player0, nplay, result);
    if (!flag_f) turn_player0 = turn_player0.to_opp(); }

  child_out(player0, "quit");
  child_out(player1, "quit");
  player0.close();
  player1.close();
  return 0; }

enum { win = 0, draw = 1, lose = 2 };

static void addup_result(const Node<Param::maxlen_play> & node,
			 const Color & turn_player0, int nplay,
			 uint result[][NodeType::ok_size][3])
  noexcept {
  assert(node.ok() && node.get_type().is_term() && turn_player0.ok());
  const NodeType & type = node.get_type();

  int abso = -1;
  if (type == SAux::resigned) {
    if (turn_player0 == node.get_turn()) abso = lose;
    else                                 abso = win; }
  
  else if (type == SAux::windclrd) {
    if (turn_player0 == node.get_turn()) abso = win;
    else                                 abso = lose; }
  
  else if (type == SAux::repeated) abso = draw;

  else if (type == SAux::illegal_bwin) {
    if (turn_player0 == SAux::black) abso = win;
    else                             abso = lose; }

  else if (type == SAux::illegal_wwin) {
    if (turn_player0 == SAux::black) abso = lose;
    else                             abso = win; }
  
  else if (type == SAux::maxlen_term) abso = draw;
  
  else die(ERR_INT("INTERNAL ERROR"));
  assert(abso != -1);
  
  result[turn_player0.to_u()][type.to_u()][abso] += 1U;
  uint tot[1][NodeType::ok_size][3];
  for (uint ut = 0; ut < NodeType::ok_size; ++ut)
    for (uint ur = 0; ur < 3; ++ur) {
      tot[0][ut][ur] = ( result[SAux::black.to_u()][ut][ur]
		      + result[SAux::white.to_u()][ut][ur] ); }

  if (flag_s) {
    cout << "'Results of player0 when it plays black:\n";
    result_out(SAux::black, result);
    cout << "'\n";
    cout << "'Results of player0 when it plays white:\n";
    result_out(SAux::white, result);
    cout << "'\n"; }
  
  cout << "'Results of player0:\n";
  result_out(SAux::black, tot); }

static void result_out(Color turn,
		       uint result[][NodeType::ok_size][3]) noexcept {
  
  int nwin = ( result[turn.to_u()][SAux::resigned.to_u()][win]
	       + result[turn.to_u()][SAux::windclrd.to_u()][win]
	       + result[turn.to_u()][SAux::illegal_bwin.to_u()][win]
	       + result[turn.to_u()][SAux::illegal_wwin.to_u()][win] );
  
  int ndraw = ( result[turn.to_u()][SAux::repeated.to_u()][draw]
		+ result[turn.to_u()][SAux::maxlen_term.to_u()][draw] );
  
  int nlose = ( result[turn.to_u()][SAux::resigned.to_u()][lose]
		+ result[turn.to_u()][SAux::windclrd.to_u()][lose]
		+ result[turn.to_u()][SAux::illegal_bwin.to_u()][lose]
		+ result[turn.to_u()][SAux::illegal_wwin.to_u()][lose] );
	    
  int ntot = nwin + nlose + ndraw;
  
  cout << "'- Win " << nwin << " (resign "
       << result[turn.to_u()][SAux::resigned.to_u()][win]
       << ", windecl "
       << result[turn.to_u()][SAux::windclrd.to_u()][win]
       << ", pcheck "
       << ( result[turn.to_u()][SAux::illegal_bwin.to_u()][win]
	    + result[turn.to_u()][SAux::illegal_wwin.to_u()][win] )
       << ")\n";
  
  cout << "'- Draw " << ndraw << " (rep "
       << result[turn.to_u()][SAux::repeated.to_u()][draw]
       << ")\n";
  
  cout << "'- Lose: " << nlose << " (resign "
       << result[turn.to_u()][SAux::resigned.to_u()][lose]
       << ", windecl "
       << result[turn.to_u()][SAux::windclrd.to_u()][lose]
       << ", pcheck "
       << ( result[turn.to_u()][SAux::illegal_bwin.to_u()][lose]
	    + result[turn.to_u()][SAux::illegal_wwin.to_u()][lose] )
       << ")\n";

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
    double se    = sqrt( deno * corr * ( dwin * dwin * pwin
					 + ddraw * ddraw * pdraw
					 + dlose * dlose * plose ) );
    cout << "'point: " << mean << " pm " << 1.96 * se << "\n"; } }

static void play_shogi(USIEngine & player_black, USIEngine & player_white,
		       Node<Param::maxlen_play> & node) noexcept {
  assert(player_black.ok() && player_white.ok() && node.ok());
  OSI::Selector selector;
  string startpos("position startpos moves");
  USIEngine *players[2] = { & player_black, & player_white };
  
  child_out(player_black, startpos.c_str());
  child_out(player_white, startpos.c_str());
  child_out(player_black, "go");
  while (true) {
    selector.reset();
    selector.add(player_black);
    selector.add(player_white);
    selector.wait(0, 500U);
    for (int index = 0; index < 2; ++index) {
      USIEngine &c = *players[index];
      bool eof     = false;
      char *line;
      if (selector.try_getline_err(c, &line)) {
	if (line) log_out(c, "%s", line);
	else eof = true; }

      if (selector.try_getline_in(c, &line)) {
	if (line) {
	  log_out(c, "%s", line);
	  play_update(players, node, index, line, startpos); }
	else eof = true; }

      if (eof) {
	close_flush(player_black);
	close_flush(player_white);
	die(ERR_INT("Player %d terminates.\n%s", players[index]->get_id(),
		    static_cast<const char *>(node.to_str()))); } }

    if (node.get_type().is_term()) break; }

  return; }

static void play_update(USIEngine *players[], Node<Param::maxlen_play> & node,
			int index, char *line, string & startpos) noexcept {
  assert(players[0] && players[0]->ok());
  assert(players[1] && players[1]->ok());
  assert(node.ok() && (index == 0 || index == 1) && line);
  
  char *token = strtok(line, " ");
  if (strcmp(token, "bestmove")) return;
  token = strtok(nullptr, " ,\t");
  
  Action action = node.action_interpret(token, SAux::usi);
  if (!action.ok()) {
    close_flush(*players[0]);
    close_flush(*players[1]);
    die(ERR_INT("cannot interpret move %s (Player %d)\n%s",
		token, players[index]->get_id(),
		static_cast<const char *>(node.to_str()))); }
  
  if (flag_r && action.is_move())
    cout << node.get_turn().to_str() << action.to_str(SAux::csa) << endl;

  node.take_action(action);
  if (! node.get_type().is_term()) {
    startpos += " ";
    startpos += string(token);
    child_out(*players[0], startpos.c_str());
    child_out(*players[1], startpos.c_str());
    index = 1 - index;
    child_out(*players[index], "go"); } }

static void start_engine(USIEngine & c) noexcept {
  assert(c.ok());
  char a0[shell.get_len_fname() + 1];
  char a1[] = "-c";
  char a2[c.get_cmd().size() + 1];
  memcpy(a0, shell.get_fname(), sizeof(a0));
  memcpy(a2, c.get_cmd().c_str(), sizeof(a2));
  char *argv[] = { a0, a1, a2, nullptr };

  c.open(shell.get_fname(), argv);
  child_out(c, "usi");
  while (true) {
    const char *line = c.getline_in_block();
    if (!line) {
      close_flush(c);
      die(ERR_INT("Player %d terminates.", c.get_id())); }
    log_out(c, "%s", line);
    if (strcmp(line, "usiok") == 0) break;
  }

  child_out(c, "isready");
  while (true) {
    const char *line = c.getline_in_block();
    if (!line) {
      close_flush(c);
      die(ERR_INT("Player %d terminates.", c.get_id())); }
    log_out(c, "%s", line);
    if (strcmp(line, "readyok") == 0) break; } }

static void child_out(USIEngine &c, const char *fmt, ...) noexcept {
  assert(c.ok() && fmt);
  char buf[65536];
  va_list list;
  
  va_start(list, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, list);
  va_end(list);
  if (sizeof(buf) <= static_cast<size_t>(nb) + 1) {
    close_flush(c);
    die(ERR_INT("buffer overrun (engine no. %d)", c.get_id())); }
  buf[nb]     = '\n';
  buf[nb + 1] = '\0';
  
  if (flag_u) cout << c.get_id() << " <- " <<  buf << flush;
  if (!c.write(buf, strlen(buf))) {
    close_flush(c);
    if (errno == EPIPE) die(ERR_INT("engine no. %d terminates", c.get_id()));
    die(ERR_CLL("write")); } }

static void log_out(USIEngine &c, const char *fmt, ...) noexcept {
  assert(c.ok() && fmt);
  if (!flag_u) return;
  char buf[65536];
  va_list list;
  
  va_start(list, fmt);
  int nb = vsnprintf(buf, sizeof(buf), fmt, list);
  va_end(list);
  if (sizeof(buf) <= static_cast<size_t>(nb) + 1) {
    close_flush(c);
    die(ERR_INT("buffer overrun (engine no. %d)", c.get_id())); }
  buf[nb]     = '\n';
  buf[nb + 1] = '\0';
  cout << c.get_id() << " -> " << buf << flush; }

static void close_flush(USIEngine &c) noexcept {
  assert(c.ok());
  c.close_write();
  while (true) {
    const char *line = c.getline_err_block();
    if (!line) break;
    cout << line << "\n"; }
    
  while (true) {
    const char *line = c.getline_in_block();
    if (!line) break;
    cout << line << "\n"; }

  cout << flush;
  c.close(); }

static int get_options(int argc, const char * const *argv) noexcept {
  assert(0 < argc && argv && argv[0]);
  bool flag_err = false;
  char *endptr;
  
  while (! flag_err) {
    int opt = Opt::get(argc, argv, "0:1:c:m:frsu");
    if (opt < 0) break;
    
    switch (opt) {
    case '0': cmd0 = string(Opt::arg); break;
    case '1': cmd1 = string(Opt::arg); break;
    case 'f': flag_f = true; break;
    case 'r': flag_r = true; break;
    case 's': flag_s = true; break;
    case 'u': flag_u = true; break;
    case 'c': shell.reset_fname(Opt::arg); break;
    case 'm':
      num_m = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0'
	  || num_m == LONG_MAX || num_m < 1) flag_err = true;
      break;
    default: flag_err = true; break; } }

  if (!flag_err && 0 < cmd0.size() && 0 < cmd1.size()) {
    cout << "'Player0:   " << shell.get_fname()
	 << " -c \"" << cmd0 << "\"\n";
    cout << "'Player1:   " << shell.get_fname()
	 << " -c \"" << cmd1 << "\"\n";
    cout << "'Gameplays: " << num_m << "\n";
    cout << "'Fix color? " << (flag_f ? "Yes\n" : "No\n");
    cout << "'Out USI?   " << (flag_u ? "Yes\n" : "No\n");
    return 0; }

  cerr << "Usage: " << Opt::cmd << " [OPTION] -0 \"CMD0\" -1 \"CMD1\"\n";
  cerr << 
    "Generate gameplays between two USI shogi engines\n\n"
    "Mandatory options:\n"
    "  -0 \"CMD0\" Start player0 as '/bin/sh -c \"CMD0\"'.\n"
    "  -1 \"CMD1\" Start player1 as '/bin/sh -c \"CMD1\"'.\n\n"
    "Other options:\n"
    "  -m NUM    Generate NUM gameplays. NUM must be a positive integer.\n"
    "            Default value is 1.\n"
    "  -f        Always assign the color of player0 black. If this is not\n"
    "            specified, then black and white are assigned alternatively.\n"
    "  -r        Print CSA records.\n"
    "  -s        Print results in detail.\n"
    "  -u        Print verbose USI messages.\n"
    "  -c SHELL  Use SHELL, e.g., /bin/csh, instead of /bin/sh.\n\n"
    "Example:\n"
    "  " << Opt::cmd <<
    " -0 \"~/aobaz -w ~/w0.txt\" -1 \"~/aobaz -w ~/w1.txt\"\n"
    "            Generate a gameplay between 'w0.txt' (black) and 'w1.txt'\n"
    "            (white)\n";
  
  return -1; }
