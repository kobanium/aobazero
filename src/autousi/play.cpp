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
#include "client.hpp"
#include "once.hpp"
#include <chrono>
#include <queue>
#include <deque>
#include <fstream>
#include <memory>
#include <random>
#include <mutex>
#include <utility>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>
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
static uint resign_count           = 0;
static mutex m_seq;
static deque<SeqPRNService> seq_s;

const int SFEN_MAX = 26523;
const int SFEN_ADD = 2;	// 2 words, "startpos moves"
static int f_sel[SFEN_MAX][2];	// keep furibisha [0]...sente, [1]...gote, 5=nakabisha,4=siken,3=3ken 2=mukai
std::vector<std::vector<std::string>> opening_sfen;
std::mt19937 mt_rand;

void load_opening_sfen() {
	std::random_device rd;
	mt_rand.seed(rd());
	FILE *fp = fopen("aoba26523.sfen","r");
	if ( fp==NULL ) die(ERR_INT("can not open sfen"));
	int count = 0, fm_sum=0,fc_sum=0,f_ai=0,ibisha=0;
	for (;;) {
		const int size = 256*32;
		static char buf[size];
		memset(buf,0,size);
		if ( fgets( buf, size-1, fp ) == NULL ) break;
		int fm=0,fc=0;

		std::string line = buf;
		std::vector<std::string> words;
		std::string word;
		// delete \n
		line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
		std::stringstream ss(line);
		// Extract words while separating them with spaces
		while (std::getline(ss, word, ' ')) {
			words.push_back(word);
		}
		if ( words.size() != 60+SFEN_ADD ) die(ERR_INT("sfen moves err"));
//		PRT("size=%d,",words.size()); for (const std::string& w : words) PRT("%s ",w.c_str()); PRT("\n");
		opening_sfen.push_back(words);

		for (int i=0;i<30;i++) {
			if ( words.at(i+3) == "2h5h" ) fm=5;
			if ( words.at(i+3) == "2h6h" ) fm=4;
			if ( words.at(i+3) == "2h7h" ) fm=3;
			if ( words.at(i+3) == "2h8h" ) fm=2;
			if ( words.at(i+3) == "8b5b" ) fc=5;
			if ( words.at(i+3) == "8b4b" ) fc=4;
			if ( words.at(i+3) == "8b3b" ) fc=3;
			if ( words.at(i+3) == "8b2b" ) fc=2;
		}
		if ( fm ) fm_sum++;
		if ( fc ) fc_sum++;
		if ( fm && fc ) f_ai++;
		if ( fm==0 && fc==0 ) ibisha++;
		if ( count >= SFEN_MAX ) die(ERR_INT("sfen number over"));
		f_sel[count][0] = fm;
		f_sel[count][1] = fc;
		count++;
	}
	if ( count != SFEN_MAX ) die(ERR_INT("sfen number err"));
	printf("count=%d, fm_sum=%d,fc_sum=%d,ai_furi=%d,ai_ibisha=%d(%.2f %%)\n",count, fm_sum,fc_sum,f_ai,ibisha,(double)ibisha*100.0/count);
}

// Random number generation for logistics distribution
double logistic_random(double mu, double s, std::mt19937 &rng) {
    std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
    double u = uniform_dist(rng);
    return mu + s * std::log(u / (1.0 - u));
}

void get_sfen_number_moves(int *index, int *moves) {
	lock_guard<mutex> lock(m_seq);
	double mu = +30.0;	// moves param. The probability of the 30th move being chosen is the highest
	double s = 3.748;	// scale param. The probability that move 0 (initial position) will be selected is 1 in 10,000 games.
	for (;;) {
		int m = mt_rand() % SFEN_MAX;
		if ( (mt_rand() % 3)<=0 && (f_sel[m][0] + f_sel[m][1]) ) continue;	// reduce furibisha. 1/1...37%, 1/3...28%, 1/2...23%B2/3...16%
		double r = logistic_random(mu, s, mt_rand);
//		PRT("%10.5f\n",r);
		if ( r < 0 || r > 61 ) continue;
		int n = (int)r;
		if ( n < 0 || n > 60 ) die(ERR_INT("err select sfen n"));
//		if ( f_sel[m][0] + f_sel[m][1] ) furi_sum++;
		if ( opening_sfen[m].size() < (uint)n+SFEN_ADD ) die(ERR_INT("sfen moves err"));
		*index = m;
		*moves = n;
		break;
	}
}

class Device {
  enum class Type : unsigned char { Aobaz, NNService, Bad };
  class DataNNService {
    NNetService _nnet;
  public:
    DataNNService(NNet::Impl impl, uint nnet_id, const char *dtune,
		  uint size_parallel, uint size_batch, uint id, uint use_half,
		  uint use_wmma, uint thread_num, uint sleep_opencl)
      noexcept : _nnet(impl, nnet_id, size_parallel, size_batch, id, use_half,
		       use_wmma, thread_num, sleep_opencl, dtune) {
      lock_guard<mutex> lock(m_seq);
      if (seq_s.empty()) seq_s.emplace_back(); }
    void nnreset(const FName &fname) noexcept { _nnet.nnreset(fname); }
    void wait() noexcept { _nnet.wait(); } };
  unique_ptr<DataNNService> _data_nnservice;
  int _nnet_id, _device_id;
  uint _size_parallel;
  uint _size_batch;
  Type _type;

public:
  static constexpr Type aobaz     = Type::Aobaz;
  static constexpr Type nnservice = Type::NNService;
  static constexpr Type bad       = Type::Bad;
  explicit Device(const string &s, int nnet_id, uint sleep_opencl,
		  const char *dtune) noexcept :
    _nnet_id(-1), _size_parallel(1U), _type(bad) {
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
					      _device_id, 0, 0, thread_num,
					      0)); }
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
      if ((*endptr != '\0'
	   && *endptr != 'H' && *endptr != 'h'
	   && *endptr != 'W' && *endptr != 'w')
	  || endptr == token || size_parallel < 1 || size_parallel == LONG_MAX)
	die(ERR_INT("invalid device %s", s.c_str()));
      
      uint flag_half = 0;
      uint flag_wmma = 0;
      if ((*endptr == 'H' || *endptr == 'h') && endptr[1] == '\0')
	flag_half = 1;
      else if ((*endptr == 'W' || *endptr == 'w') && endptr[1] == '\0')
	flag_wmma = 1;

      _device_id     = device_id;
      _nnet_id       = nnet_id;
      _type          = nnservice;
      _size_parallel = size_parallel;
      _size_batch    = size_batch;
      _data_nnservice.reset(new DataNNService(NNet::opencl, _nnet_id, dtune,
					      _size_parallel, _size_batch,
					      _device_id, flag_half, flag_wmma,
					      0, sleep_opencl)); }
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
  string _record_wght, _record_version, _record_settings, _record_handicap;
  int64_t _id_wght;
  uint64_t _crc64_wght;
  int _device_id, _nnet_id, _version;
  uint _eid, _nmove;
  bool _flag_playing, _flag_ready, _flag_thinking, _flag_do_resign;
  int _handicap;
  int _sfen_index;
  int _sfen_moves;
  uint _silent_eng;

  void out_log(const char *p) noexcept {
    assert(_ofs && p);
    if ( _silent_eng ) return;
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
		     uint verbose_eng, uint silent_eng, const FName &logname) noexcept :
  _time_average_nume(0.0), _time_average_deno(0.0), _time_average(0.0),
    _logname(logname),
    _fingerprint(to_string(nnet_id) + string("-") + to_string(eid)),
    _id_wght(wfname.get_id()), _crc64_wght(crc64),
    _device_id(device_id), _nnet_id(nnet_id), _version(-1), _eid(eid),
    _silent_eng(silent_eng),
    _nmove(0),
    _flag_playing(false), _flag_ready(false), _flag_thinking(false),
    _flag_do_resign(false) {
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

    char opt_r[]       = "-r";
    char opt_r_value[] = "0.0";
    argv[argc++] = opt_r;
    argv[argc++] = opt_r_value;

#if 0	// normal
    char opt_p[]       = "-p";
    char opt_p_value[] = "800";
    argv[argc++] = opt_p;
    argv[argc++] = opt_p_value;
#else
    char opt_p[]       = "-p";
//  char opt_p_value[] = "3200";
//  char opt_p_value[] = "6400";
    char opt_p_value[] = "12800";
    argv[argc++] = opt_p;
    argv[argc++] = opt_p_value;

    char opt_kld[]       = "-kldgain";
//  char opt_kld_value[] = "0.0000013";
//  char opt_kld_value[] = "0.000006";
//  char opt_kld_value[] = "0.000005";
//  char opt_kld_value[] = "0.00000075";
    char opt_kld_value[] = "0.0000004";
    argv[argc++] = opt_kld;
    argv[argc++] = opt_kld_value;

    char opt_kld_i[]       = "-kldinterval";
    char opt_kld_i_value[] = "400";     // check kldgain each 400 playouts
    argv[argc++] = opt_kld_i;
    argv[argc++] = opt_kld_i_value;
#endif

    char opt_n[] = "-n";
    argv[argc++] = opt_n;

    char opt_m[]       = "-m";
//  char opt_m_value[] = "30";
    char opt_m_value[] = "10";
    argv[argc++] = opt_m;
    argv[argc++] = opt_m_value;

#if 0	// make first random 100000 games.
    char opt_nn_rand[] = "-nn_rand";
    argv[argc++] = opt_nn_rand;
    char opt_t[]       = "-t";
    char opt_t_value[] = "1";
    argv[argc++] = opt_t;
    argv[argc++] = opt_t_value;
#endif
#if 0
    char opt_mtemp[]       = "-mtemp";
    char opt_mtemp_value[] = "1.3";
    argv[argc++] = opt_mtemp;
    argv[argc++] = opt_mtemp_value;
#endif

    char opt_w[] = "-w";
    unique_ptr<char []> opt_w_value(new char [wfname.get_len_fname() + 1U]);
    memcpy(opt_w_value.get(), wfname.get_fname(), wfname.get_len_fname() + 1U);
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

  void start_newgame(const uint *phandicap_rate) noexcept {
//  std::random_device rd;
//  unsigned int handicap_rnd = rd();
//  _handicap = handicap_rnd % HANDICAP_TYPE;
    _handicap = 0;

    _node.clear(_handicap);
    _flag_playing   = true;
    _flag_thinking  = false;
    if ( (resign_count++ % 10) == 0 ) _flag_do_resign = false;
    else                              _flag_do_resign = true;
//  _flag_do_resign = false;  // 20210610

    const char *str_turn[2] = { "+", "-" };
    char buf[256];
    sprintf(buf, "%s\n%s\n",str_init_csa[_handicap], str_turn[(_handicap!=0)]);
//  _record_main    = string("PI\n+\n");
    _record_main    = string(buf);
/*
	string init_pos[HANDICAP_TYPE] = {
      "startpos moves",
      "sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",// ky
      "sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",  // ka
      "sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",  // hi
      "sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	   // 2mai
      "sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL",	   // 4mai
      "sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"	   // 6mai
    };
    _startpos = init_pos[_handicap];
    if ( _handicap > 0 ) _startpos += " w - 1 moves";
*/
    _startpos = "position";
    _nmove          = 0;

	get_sfen_number_moves(&_sfen_index, &_sfen_moves);
	for (int i=0; i< _sfen_moves + SFEN_ADD; i++) {
		string sfen_usi = opening_sfen[_sfen_index][i];
		const char *str_move_usi = sfen_usi.c_str();
		_startpos += " " + sfen_usi;
		if ( i<SFEN_ADD ) continue;
		Action actionPlay = _node.action_interpret(str_move_usi, SAux::usi);
		if (!actionPlay.ok()) die(ERR_INT("cannot interpret sfen %d move %s (engine %s)\n%s", i, str_move_usi, get_fp(), static_cast<const char *>(_node.to_str()) ));
		if (!actionPlay.is_move()) die(ERR_INT("sfen move err %s", str_move_usi));
		_nmove            += 1U;
		string new_move   = _node.get_turn().to_str();
		new_move          += actionPlay.to_str(SAux::csa);
		if ( new_move.empty() ) die(ERR_INT("sfen move empty %s", str_move_usi));
		_record_main += new_move + string("\n"); // string(",'") + new_info;
		_node.take_action(actionPlay);
	}


    sprintf(buf, "%16" PRIx64, _crc64_wght);
    _record_wght  = string("'w ") + to_string(_id_wght);
    _record_wght += string(" (crc64:") + string(buf) + string(")");
/*
    const uint *ph = Client::get().get_handicap_rate();
    if ( ph != phandicap_rate ) die(ERR_INT("handicap_rate pointer Err."));
    buf[0] = 0;
    for (int i=0; i<HANDICAP_TYPE; i++) {
      char b[10];
      if ( i > 0 ) strcat(buf,":");
      sprintf(b,"%d",ph[i]);
      strcat(buf,b);
    }
    if (0) printf("in start_newgame->%s\n",buf);
    _record_handicap = string(", handicapR ");
    _record_handicap += buf;
    engine_out("setoption name USI_HandicapRate value %s", buf);	// "30:100:150:300:700:900:1200"
*/
    sprintf(buf,"%.3f",(float)(Client::get().get_average_winrate())/1000.0);
    engine_out("setoption name USI_AverageWinrate value %s", buf);	// "0.547"
    _record_handicap = string(", average_winrate ") + buf;
    sprintf(buf,", sfen %d/%d, moves %d",_sfen_index,SFEN_MAX,_sfen_moves);
    _record_handicap += buf;

    engine_out("usinewgame");
  }

  void engine_go() noexcept {
    _flag_thinking = true;
    engine_out("%s", _startpos.c_str());
    engine_out("go visit");
    _time_last = steady_clock::now(); }

  void engine_wght_update(const FNameID &wfname, uint64_t crc64) noexcept {
//    _record_wght += string(" and later");
    _id_wght    = wfname.get_id();
    _crc64_wght = crc64;
    engine_out("setoption name USI_WeightFile value %s", wfname.get_fname()); }

  string update(char *line, queue<string> &moves_eid0, float th_resign, const uint *phandicap_rate)
    noexcept {
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

      // _record_version is used for "no-resign". do not add other info.

      engine_out("isready");
      return string("");
    }

    if (!_flag_ready && strcmp(token, "id") == 0) {
      token = OSI::strtok(nullptr, " ", &saveptr);
      if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));

      if (strcmp(token, "settings") == 0) {
		token = OSI::strtok(nullptr, "", &saveptr);
		if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));
		_record_settings = string(token);
	  } else if (strcmp(token, "name") == 0) {
		if (! OSI::strtok(nullptr, " ", &saveptr)) die(ERR_INT("Bad message from engine (%s).", get_fp()));
		token = OSI::strtok(nullptr, " ", &saveptr);
		if (!token) die(ERR_INT("Bad message from engine (%s).", get_fp()));

		char *endptr;
		long int ver = strtol(token, &endptr, 10);
		if (endptr == token || *endptr != '\0' || ver < 0 || 65535 < ver) die(ERR_INT("Bad message from engine (%s).", get_fp()));
		_version = ver;
		if (ver < Ver::usi_engine) die(ERR_INT("Please update usi engine!"));
	  }
      return string("");
    }

    if (!_flag_ready) die(ERR_INT("Bad message from engine (%s).", get_fp()));

    if (strcmp(token, "bestmove") != 0) return string("");
    if (!_flag_playing && !_flag_thinking) die(ERR_INT("bad usi message from engine %s", get_fp()));
    _flag_thinking = false;

	// bestmove 7g7f,1600,7g7f,959m,2g2f,271l,6i7h,178j,9g9f,33b,3i3h,31Z,4i5h,25h,5i6h,24W,1g1f,17Z,5i5h,14d,6i6h,8b,3i4h,8X,4g4f,7V,2h1h,6h,7i7h,4U,9i9h,3E,2h3h,3a,3g3f,2T,4i4h,2P,6g6f,2U,5g5f,2S,2h5h,1L
    // read played move
    long int num_best = 0;
    long int num_tot  = 0;
    const char *str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
    if (!str_move_usi) die(ERR_INT("bad usi message from engine %s", get_fp()));

    Action actionPlay = _node.action_interpret(str_move_usi, SAux::usi);
    if (!actionPlay.ok()) die(ERR_INT("cannot interpret candidate move %s (engine %s)\n%s", str_move_usi, get_fp(), static_cast<const char *>(_node.to_str()) ));

    string new_move, new_info;
    bool flag_resign = false;
    if (actionPlay.is_move()) {
      steady_clock::time_point time_now = steady_clock::now();
      auto rep   = duration_cast<milliseconds>(time_now - _time_last).count();
      double dms = static_cast<double>(rep);
      _time_average_nume = _time_average_nume * time_average_rate + dms;
      _time_average_deno = _time_average_deno * time_average_rate + 1.0;
      _time_average      = _time_average_nume / _time_average_deno;
      _startpos         += " ";
      _startpos         += str_move_usi;
      _nmove            += 1U;
      new_move          += _node.get_turn().to_str();
      new_move          += actionPlay.to_str(SAux::csa);
      if (_eid == 0) {
		char buf[256];
		sprintf(buf, " (%5.0fms)", _time_average);
		string smove = _node.get_turn().to_str();
		smove += actionPlay.to_str(SAux::csa);
		smove += buf;
		moves_eid0.push(std::move(smove));
	  }
      const char *str_value = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_value || str_value[0] != 'v' || str_value[1] != '=') die(ERR_INT("cannot read value (engine %s)", get_fp()));

      char *endptr;
      float value = strtof(str_value+2, &endptr);
      if (endptr == str_value+2 || *endptr != '\0' || value < 0.0f || value == HUGE_VALF) die(ERR_INT("cannot interpret value %s (engine %s)", str_value+2, get_fp()));
//    if (value < th_resign) flag_resign = true;
//    if (value < th_resign && _nmove > 30) flag_resign = true;
      if (value < th_resign && (int)_nmove > _sfen_moves + 10 ) flag_resign = true;

      str_value = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_value || str_value[0] != 'r' || str_value[1] != '=') die(ERR_INT("cannot read raw value (engine %s)", get_fp()));
      float raw_value = strtof(str_value+2, &endptr);
      if (endptr == str_value+2 || *endptr != '\0' || raw_value < 0.0f || raw_value == HUGE_VALF) die(ERR_INT("cannot interpret raw_value %s (engine %s)", str_value+2, get_fp()));

      const char *str_count = OSI::strtok(nullptr, " ,", &saveptr);
      if (!str_count) die(ERR_INT("cannot read count (engine %s)", get_fp()));

      long int num = strtol(str_count, &endptr, 10);
      if (endptr == str_count || *endptr != '\0' || num < 1 || num == LONG_MAX) die(ERR_INT("cannot interpret visit count %s (engine %s)", str_count, get_fp()));

      num_best = num;
      {
		char buf[256];
		sprintf(buf, "v=%.3f,r=%.3f,%ld", value, raw_value, num);
		new_info += buf;
      }

      // read candidate moves
      while (true) {
		str_move_usi = OSI::strtok(nullptr, " ,", &saveptr);
		if (!str_move_usi) { new_info += "\n"; break; }

		Action action = _node.action_interpret(str_move_usi, SAux::usi);
		if (!action.is_move()) die(ERR_INT("bad candidate %s (engine %s)", str_move_usi, get_fp()));
		new_info += ",";
		new_info += action.to_str(SAux::csa);

		str_count = OSI::strtok(nullptr, " ,", &saveptr);
		if (!str_count) die(ERR_INT("cannot read count (engine %s)", get_fp()));

		num = strtol(str_count, &endptr, 10);
		char c = *endptr;
		bool hasPolicy = ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
		if (endptr == str_count || hasPolicy == false || num < 1 || num == LONG_MAX) die(ERR_INT("cannot interpret a visit count %s (engine %s)", str_count, get_fp()));

		num_tot  += num;
		new_info += ",";
		new_info += to_string(num) + c;
	  }
	}

    if (num_best < num_tot) die(ERR_INT("bad counts (engine %s)", get_fp()));
    _node.take_action(actionPlay);

    // force declare nyugyoku
    if (_node.get_type().is_interior() && _node.is_nyugyoku()) _node.take_action(SAux::windecl);
    assert(_node.ok());

    // terminal test
    if (flag_resign && _flag_do_resign) {
      string rec;
      rec += _record_wght + string(", ") + _record_version + string("\n");
      rec += string("'") + _record_settings + _record_handicap + string("\n");
      rec += _record_main;
      rec += "%TORYO" + string(",'autousi,resign-th=") + to_string(th_resign);
      rec += string(",") + new_info;
      _flag_playing = false;
      return move(rec);
    }

    if (! new_move.empty()) _record_main += new_move + string(",'") + new_info;

    if (_node.get_type().is_term()) {
      string rec;
      if (_flag_do_resign) {
        rec += _record_wght + string(", ") + _record_version + string("\n");
      } else {
        rec += _record_wght + string(", ");
        rec += _record_version + string(", no-resign\n");
      }
      rec += string("'") + _record_settings + _record_handicap + string("\n");
      rec += _record_main;
      rec += "%" + string(_node.get_type().to_str()) + string("\n");
      _flag_playing = false;
      return move(rec);
    }

    return string("");
  }

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
  bool get_do_resign() const noexcept { return _flag_do_resign; }
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
			const vector<string> &devices_str, uint verbose_eng, uint silent_eng,
			uint sleep_opencl, const FNameID &wfname,
			uint64_t crc64) noexcept {
  assert(cname && dlog && dtune && wfname.ok() && _engines.empty());
  if (devices_str.empty()) die(ERR_INT("bad devices"));
  _verbose_eng = verbose_eng;
  _silent_eng  = silent_eng;
  _cname.reset_fname(cname);
  _logname.reset_fname(dlog);
  _wid = wfname.get_id();

  int nnet_id = 0;
  for (const string &s : devices_str) _devices.emplace_back(s, nnet_id++,
							    sleep_opencl,
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
					  eid++, wfname, crc64, _verbose_eng, _silent_eng,
					  _logname )); } }

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
				       uint64_t crc64, float th_resign, const uint *phandicap_rate,
				       const uint average_winrate)
  noexcept {
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
	string s = e->update(line, _moves_eid0, th_resign, phandicap_rate);
	if (!s.empty()) {
	  _ngen_records += 1U;
	  recs.push_back(move(s)); }

	if (e->is_ready() && _wid == wfname.get_id()) {
	  if (has_conn && ! e->is_playing()) e->start_newgame(phandicap_rate);
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
	if (has_conn && ! e->is_playing()) e->start_newgame(phandicap_rate);
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

bool PlayManager::get_do_resign(uint u) const noexcept {
  assert(u < _engines.size());
  return _engines[u]->get_do_resign(); }

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
