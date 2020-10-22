// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-cpu.hpp"
#include "nnet-ocl.hpp"
#include "option.hpp"
#include "param.hpp"
#include "shogibase.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
using std::cerr;
using std::cout;
using std::condition_variable;
using std::deque;
using std::endl;
using std::getline;
using std::istream;
using std::lock_guard;
using std::map;
using std::max;
using std::move;
using std::mt19937;
using std::mutex;
using std::set;
using std::setw;
using std::shuffle;
using std::string;
using std::stringstream;
using std::terminate;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;
using namespace SAux;

constexpr double epsilon = 5e-2;

static uint   value_n       = 0;
static double value_sum_e   = 0.0;
static double value_sum_se  = 0.0;
static double value_max_e   = 0.0;

static uint   policy_n      = 0;
static double policy_sum_e  = 0.0;
static double policy_sum_se = 0.0;
static double policy_max_e  = 0.0;

static int opt_repeat_num   =  1;
static int opt_thread_num   = -1;
static int opt_device_id    = -1;
static uint opt_batch_size  =  1;
static bool opt_use_half    = false;
static bool opt_use_wmma    = false;
static bool opt_mode_cpu    = false;
static bool opt_mode_opencl = true;
static bool opt_verbose     = false;
static string opt_str_wght;
static string opt_str_dname_tune;
static const char *opt_cstr_dname_tune = nullptr;

static string gen_usage(const char *cmd) noexcept {
  stringstream ss;
  ss << "Usage: \n"
     << cmd << " [-i opencl] [-r num] [-u num] [-b size] [-hv] weight\n"
     << cmd << " -i cpublas [-r num] [-t num] [-b size] [-v] weight" << R"(
  -i code  uses OpenCL if "code" is opencl, uses BLAS for CPU if "code" is
           cpublas.
  -d dir   let "opencl" code save performance tuning results to directory path
           "dir".
  -u num   let "opencl" code use a device of ID "num". Default is -1.
  -h       let "opencl" code make use of half precision floating points.
  -w       let "opencl" code make use of wmma instructions for half-precision
           use.
  -t num   let "cpublas" code make use of "num" threads. Default is -1.
  -b size  specifies batch size. Default is 1.
  -r num   evaluates each state "num" times. Default is 1.
  -v       turns on verbose mode.
)";
  
  return ss.str(); }

static double absolute_error(double f1, double f2) noexcept {
  return std::fabs(f1 - f2); }

static int get_options(int argc, const char * const *argv) noexcept {
  assert(0 < argc && argv && argv[0]);
  bool flag_err = false;
  char *endptr;

  while (! flag_err) {
    int opt = Opt::get(argc, argv, "b:i:t:u:r:d:vhw");
    if (opt < 0) break;

    long l;
    switch (opt) {
    case 'i':
      if      (strcmp(Opt::arg, "cpublas") == 0) {
	opt_mode_cpu    = true;
	opt_mode_opencl = false; }
      else if (strcmp(Opt::arg, "opencl")  == 0) {
	opt_mode_cpu    = false;
	opt_mode_opencl = true; }
      else flag_err = true;
      break;

    case 'd':
      opt_str_dname_tune = string(Opt::arg);
      opt_cstr_dname_tune = opt_str_dname_tune.c_str();
      break;
      
    case 'r': 
      l = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0' || l < 1 || l == LONG_MAX)
	flag_err = true;
      opt_repeat_num = static_cast<int>(l);
      break;

    case 't': 
      l = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0' || l < -1 || l == LONG_MAX)
	flag_err = true;
      opt_thread_num = static_cast<int>(l);
      break;

    case 'u': 
      l = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0' || l < -1 || l == LONG_MAX)
	flag_err = true;
      opt_device_id = static_cast<int>(l);
      break;

    case 'b': 
      l = strtol(Opt::arg, &endptr, 10);
      if (endptr == Opt::arg || *endptr != '\0' || l < 1 || l == LONG_MAX)
	flag_err = true;
      opt_batch_size = static_cast<uint>(l);
      break;

    case 'h': opt_use_half = true; break;
    case 'w': opt_use_wmma = true; break;
    case 'v': opt_verbose  = true; break;
    default: flag_err = true; break; } }

  if (!flag_err && Opt::ind < argc) {
    opt_str_wght = string(argv[Opt::ind++]);
    return 0; }
  
  cerr << gen_usage(Opt::cmd) << std::flush;
  return -1; }

class Entry {
  uint _no;
  uint _size_nnmove;
  double _value_answer;
  map<string, double> _policy_answers;
  map<ushort, string> _nnmove2str;
  float _features[NNAux::size_plane * NNAux::nch_input];
  float _compressed_features[NNAux::maxsize_compressed_features];
  uint _n_one;

public:
  vector<ushort> nnmoves;
  explicit Entry() noexcept : _size_nnmove(0) {}
  explicit Entry(bool do_compress, uint no, uint size_nnmove,
		 double value_answer, map<string, double> &&policy_answers,
		 map<ushort, string> &&nnmove2str,
		 const float *features) noexcept :
    _no(no), _size_nnmove(size_nnmove), _value_answer(value_answer),
    _policy_answers(move(policy_answers)), _nnmove2str(move(nnmove2str)) {
    if (do_compress)
      _n_one = NNAux::compress_features(_compressed_features, features);
    else memcpy(_features, features, sizeof(_features)); }
  Entry(const Entry &e) = default;
  Entry &operator=(const Entry &e) = default;
  Entry(Entry &&e) = default;
  Entry &operator=(Entry &&e) = default;

  const float *get_features() const noexcept { return _features; }
  const float *get_compressed_features() const noexcept {
    return _compressed_features; }
  uint get_n_one() const noexcept { return _n_one; }
  uint get_size_nnmove() const noexcept { return _size_nnmove; }
  uint get_no() const noexcept { return _no; }
  bool ok() const noexcept { return _size_nnmove == _policy_answers.size(); }
  void compare(const float *probs, float value) const noexcept {
    assert(probs);
    
    // test output
    double value_e = absolute_error(value, _value_answer);
    value_n      += 1U;
    value_sum_e  += value_e;
    value_sum_se += value_e * value_e;
    value_max_e   = max(value_max_e, value_e);
    if (2.0 * epsilon < value_e) {
      cerr << "value1:         " << value << endl;
      cerr << "value2:         " << _value_answer << endl;
      cerr << "absolute error: " << value_e << endl;
      terminate(); }
    
    map<string, double> policy;
    for (uint u = 0; u < _size_nnmove; ++u) {
      const string &str = _nnmove2str.at( nnmoves[u] );
      policy[str] = probs[u]; }
    
    auto &&it1 = _policy_answers.cbegin();
    auto &&it2 = policy.cbegin();
    while (it1 != _policy_answers.cend() && it2 != it1) {
      double prob1     = (it1++)->second;
      double prob2     = (it2++)->second;
      double policy_e  = absolute_error(prob1, prob2);
      policy_n        += 1U;
      policy_sum_e    += policy_e;
      policy_sum_se   += policy_e * policy_e;
      policy_max_e     = max(policy_max_e, policy_e);
      if (epsilon < policy_e) {
	cerr << "prob1:          " << prob1 << endl;
	cerr << "prob2:          " << prob2 << endl;
	cerr << "absolute error: " << policy_e << endl;
	terminate(); } } }
};

struct TestSet {
  uint wait_id;
  uint nentry;
  unique_ptr<NNInBatchBase> nn_in_b;
  unique_ptr<const Entry *[]> ppentry;
  unique_ptr<float []> probs;
  unique_ptr<float []> values;
  bool do_compress;
  explicit TestSet(uint nb_, bool do_compress_) noexcept : nentry(0),
    ppentry(new const Entry *[nb_]),
    probs(new float [nb_ * SAux::maxsize_moves]),
    values(new float [nb_]), do_compress(do_compress_) {
    if (do_compress) nn_in_b.reset(new NNInBatchCompressed(nb_));
    else             nn_in_b.reset(new NNInBatch(nb_)); };
};

class QueueTest {
  unique_ptr<NNet> _pnnet;
  mutex _m;
  bool _flag_quit;
  condition_variable _cv;
  thread _th_worker_wait_test;
  thread _th_init;
  deque<unique_ptr<TestSet>> _deque_wait;
  deque<unique_ptr<TestSet>> _deque_pool;
  unique_ptr<TestSet> _pts_push;
  uint _nb;
  int64_t _neval;

  void worker_wait_test() noexcept {
    unique_ptr<TestSet> pts;
    while (true) {
      unique_lock<mutex> lock(_m);
      _cv.wait(lock, [&]{ return (_flag_quit || 0 < _deque_wait.size()); });
      if (_deque_wait.size() == 0 && _flag_quit) return;

      pts = move(_deque_wait.front());
      _deque_wait.pop_front();
      lock.unlock();
      _cv.notify_one();

      _pnnet->wait_ff(pts->wait_id);
      for (uint ub = 0; ub < pts->nentry; ++ub)
	pts->ppentry[ub]->compare(pts->probs.get() + ub * SAux::maxsize_moves,
				  pts->values[ub]);
      if (opt_verbose) {
	for (uint ub = 0; ub < pts->nentry; ++ub)
	  cout << setw(5) << pts->ppentry[ub]->get_no();
	cout << " OK" << endl; }
      _neval += 1U;

      lock.lock();
      _deque_pool.push_back(move(pts));
      lock.unlock(); } }

  void flush() noexcept {
    if (_pts_push->nentry == 0) return;
    if (_pts_push->do_compress) {
      auto p = static_cast<NNInBatchCompressed *>(_pts_push->nn_in_b.get());
      _pts_push->wait_id
	= _pnnet->push_ff(*p, _pts_push->probs.get(),
			  _pts_push->values.get()); }
    else {
      auto p = static_cast<NNInBatch *>(_pts_push->nn_in_b.get());
      _pts_push->wait_id
	= _pnnet->push_ff(p->get_ub(), p->get_features(),
			  p->get_sizes_nnmove(), p->get_nnmoves(),
			  _pts_push->probs.get(), _pts_push->values.get()); }
    unique_lock<mutex> lock(_m);
    _cv.wait(lock, [&]{ return _deque_wait.size() < 16U; });
    _deque_wait.push_back(move(_pts_push));
    if (_deque_pool.empty())
      _deque_pool.emplace_back(new TestSet(_nb, do_compress()));
    _pts_push = move(_deque_pool.front());
    _deque_pool.pop_front();
    lock.unlock();
    _cv.notify_one();

    _pts_push->nentry = 0;
    _pts_push->nn_in_b->erase(); }
  
public:
  explicit QueueTest(const FName &fname, int device_id, uint nb, bool use_half,
		     bool use_wmma, int thread_num) noexcept
    : _flag_quit(false), _nb(nb), _neval(0) {
    _th_worker_wait_test = thread(&QueueTest::worker_wait_test, this);
    if (opt_mode_opencl) {
      _pnnet.reset(new NNetOCL);
      NNetOCL *p = static_cast<NNetOCL *>(_pnnet.get());
      _th_init = thread([=]{ p->reset(nb, NNAux::read(fname), device_id,
				      use_half, use_wmma, true, false,
				      opt_cstr_dname_tune); }); }
    else if (opt_mode_cpu) {
      _pnnet.reset(new NNetCPU);
      NNetCPU *p = static_cast<NNetCPU *>(_pnnet.get());
      _th_init = thread([=]{ p->reset(nb, NNAux::read(fname),
				      thread_num); }); }
    else die(ERR_INT("Internal Error"));

    _pts_push.reset(new TestSet(nb, do_compress())); }

  void wait_init() noexcept { _th_init.join(); }
  bool do_compress() const noexcept { return _pnnet->do_compress(); }

  void push(const Entry *pe) noexcept {
    assert(pe && pe->ok());
    _pts_push->ppentry[_pts_push->nentry++] = pe;
    if (do_compress())
      static_cast<NNInBatchCompressed *>(_pts_push->nn_in_b.get())
	->add(pe->get_n_one(), pe->get_compressed_features(),
	      pe->get_size_nnmove(), pe->nnmoves.data());
    else
      static_cast<NNInBatch *>(_pts_push->nn_in_b.get())
	->add(pe->get_features(), pe->get_size_nnmove(),
	      pe->nnmoves.data());
    
    assert(_pts_push->nentry == _pts_push->nn_in_b->get_ub());
    if (_pts_push->nentry == _nb) flush(); }
  
  void end() noexcept {
    flush();
    {
      lock_guard<mutex> lock(_m);
      _flag_quit = true;
    }
    _cv.notify_one();
    _th_worker_wait_test.join(); }

  int64_t get_neval() const noexcept { return _neval; }
};

static vector<Entry> read_entries(istream *pis, bool do_compress) noexcept {
  vector<Entry> vec_entry;
  uint no = 0;

  for (uint uline = 0;;) {
    // read position startpos move...
    string string_line;
    if (! getline(*pis, string_line)) break;
    uline += 1U;
    
    stringstream ss(string_line);
    string token1, token2, token3;
    ss >> token1 >> token2 >> token3;
    if (token1 != "position" || token2 != "startpos" || token3 != "moves")
      die(ERR_INT("bad line %u", uline));

    NodeNN<Param::maxlen_play> node;
    vector<string> path;
    while (ss >> token1) {
      Action a = node.action_interpret(token1.c_str(), SAux::usi);
      if (! a.is_move()) die(ERR_INT("bad move"));
      node.take_action(a);
      path.emplace_back(move(token1)); }
    
    // read features
    uline += 1U;
    if (! getline(*pis, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "input") die(ERR_INT("bad line %u", uline));
    vector<double> features_answer;
    double di;
    while (ss >> di) features_answer.push_back(di);

    vector<float> input(NNAux::size_plane * NNAux::nch_input);
    node.encode_features(input.data());
    if (features_answer.size() != NNAux::size_plane * NNAux::nch_input)
      die(ERR_INT("bad input feature size %zu vs %u",
		  features_answer.size(),
		  NNAux::size_plane * NNAux::nch_input));
    
    for (uint uch = 0; uch < NNAux::nch_input; ++uch)
      for (uint usq = 0; usq < Sq::ok_size; ++usq) {
	double f1 = features_answer[uch * Sq::ok_size + usq];
	double f2 = input[uch * Sq::ok_size + usq];
	if (absolute_error(f1, f2) <= epsilon) continue;
	cerr << "input (answer): " << f1 << endl;
	cerr << "input:          " << f2 << endl;
	cerr << "absolute error: " << absolute_error(f1, f2) << endl;
	terminate(); }

    // read state value
    uline += 1U;
    if (!getline(*pis, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "value") die(ERR_INT("bad line %u", uline));
    double value_answer;
    ss >> value_answer;

    // read action probabilities
    uline += 1U;
    if (!getline(*pis, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "policy") die(ERR_INT("bad line %u", uline));

    map<string, double> policy_answer;
    while (ss >> token1 >> di) policy_answer.emplace(move(token1), di);

    // test genmove
    MoveSet<Param::maxlen_play> ms;
    ms.gen_all(node);
    assert(ms.ok());
    
    set<string> set1, set2;
    for (auto &f : policy_answer)        set1.insert(f.first);
    for (uint u = 0; u < ms.size(); ++u) set2.emplace(ms[u].to_str(SAux::usi));
    if (set1 != set2) {
      cerr << "position startpos moves";
      for (auto &s : path) cerr << " " << s;
      cerr << "\n" << node.to_str();
      
      auto &&it1 = set1.cbegin();
      auto &&it2 = set2.cbegin();
      while (it1 != set1.cend() || it2 != set2.cend()) {
	if (it1 == set1.cend()) cerr << "      ";
	else cerr << std::setw(6) << std::left << *it1++;
	
	if (it2 != set2.cend()) cerr << std::setw(6) << std::left << *it2++;
	cerr << endl; }
      die(ERR_INT("bad move generation")); }
    
    // read END
    uline += 1U;
    if (!getline(*pis, string_line)) die(ERR_INT("bad line %u", uline));
    if (string_line != "END") die(ERR_INT("bad line %u", uline));

    // push target position
    map<ushort, string> nnmove2str;
    vector<ushort> nnmoves2(SAux::maxsize_moves);
    for (uint u = 0; u < ms.size(); ++u) {
      nnmoves2[u] = NNAux::encode_nnmove(ms[u], node.get_turn());
      nnmove2str[nnmoves2[u]] = ms[u].to_str(SAux::usi); }
    
    if (node.get_turn() == white) value_answer = - value_answer;

    vec_entry.emplace_back(do_compress, ++no, ms.size(), value_answer,
			   move(policy_answer), move(nnmove2str),
			   input.data());
    vec_entry.back().nnmoves = move(nnmoves2); }

  return vec_entry; }

int main(int argc, char **argv) {
  if (get_options(argc, argv) < 0) return 1;

  QueueTest queue_test(FName(opt_str_wght.c_str()), opt_device_id,
		       opt_batch_size, opt_use_half, opt_use_wmma,
		       opt_thread_num);
  cout << "Start reading entries from stdin ..." << endl;
  vector<Entry> vec_entry = read_entries(&std::cin, queue_test.do_compress());
  cout << "Finish reading " << vec_entry.size() << " entries" << endl;
  queue_test.wait_init();

  vector<const Entry *> vec_pe(opt_repeat_num * vec_entry.size());
  uint index = 0;
  for (const Entry &e : vec_entry)
    for (int i = 0; i < opt_repeat_num; ++i) vec_pe[index++] = &e;
  mt19937 mt(7);
  shuffle(vec_pe.begin(), vec_pe.end(), mt);

  cout << "Testrun start" << endl;
  steady_clock::time_point start = steady_clock::now();
  for (const Entry *pe : vec_pe) queue_test.push(pe);
  queue_test.end();
  steady_clock::time_point end   = steady_clock::now();
  int64_t elapsed  = duration_cast<microseconds>(end - start).count();
  int64_t nelapsed = queue_test.get_neval() * 1000U;
  if (0 < nelapsed)
    cout << "Average Time: "
	 << static_cast<double>(elapsed) / static_cast<double>(nelapsed)
	 << "ms" << endl;
  
  if (0 < value_n) {
    double factor = 1.0 / static_cast<double>(value_n);
    cout << "Value (ABSOLUTE, " << value_n << " samples)\n";
    cout << "  - MaxE: " << value_max_e << "\n";
    cout << "  - ME:   " << value_sum_e * factor << "\n";
    cout << "  - RMSE: " << std::sqrt(value_sum_se * factor) << "\n\n"; }
    
  if (0 < policy_n) {
    double factor = 1.0 / static_cast<double>(policy_n);
    cout << "Policy (ABSOLUTE, " << policy_n << " samples)\n";
    cout << "  - MaxE: " << policy_max_e << "\n";
    cout << "  - ME:   " << policy_sum_e * factor << "\n";
    cout << "  - RMSE: " << std::sqrt(policy_sum_se * factor) << "\n\n"; }
  
  return 0; }
