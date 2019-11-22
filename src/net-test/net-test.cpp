// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-cpu.hpp"
#include "nnet-ocl.hpp"
#include "option.hpp"
#include "param.hpp"
#include "shogibase.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <climits>
using std::cerr;
using std::copy_n;
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
using std::mutex;
using std::set;
using std::setw;
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

constexpr double epsilon = 1e-2;

static double elapsed  = 0.0;
static uint   nelapsed = 0U;

static uint   value_n       = 0;
static double value_sum_e   = 0.0;
static double value_sum_se  = 0.0;
static double value_max_e   = 0.0;

static uint   policy_n      = 0;
static double policy_sum_e  = 0.0;
static double policy_sum_se = 0.0;
static double policy_max_e  = 0.0;

static int opt_device_id   = -1;
static uint opt_batch_size = 1;
static bool opt_use_half   = false;
static string opt_str_wght;

static double absolute_error(double f1, double f2) noexcept {
  return std::fabs(f1 - f2); }

static int get_options(int argc, const char * const *argv) noexcept {
  assert(0 < argc && argv && argv[0]);
  bool flag_err = false;
  char *endptr;

  while (! flag_err) {
    int opt = Opt::get(argc, argv, "u:b:h");
    if (opt < 0) break;

    long l;
    switch (opt) {
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

    case 'h': opt_use_half = true;  break;
    default: flag_err = true; break; } }

  if (!flag_err && Opt::ind < argc) {
    opt_str_wght = string(argv[Opt::ind++]);
    return 0; }
  
  cerr << "Usage: " << Opt::cmd
       << " [-u device-id] [-b batch-size] [-h] weight" << endl;
  return -1;
}

class Entry {
  uint _no;
  uint _size_nnmove;
  double _value_answer;
  map<string, double> _policy_answers;
  map<ushort, string> _nnmove2str;

public:
  vector<float> input;
  vector<ushort> nnmoves;
  explicit Entry() noexcept {}
  explicit Entry(uint no, uint size_nnmove, double value_answer,
		 map<string, double> &&policy_answers,
		 map<ushort, string> &&nnmove2str) noexcept :
    _no(no), _size_nnmove(size_nnmove), _value_answer(value_answer),
    _policy_answers(move(policy_answers)), _nnmove2str(move(nnmove2str)) {}
  explicit Entry(const Entry &) = default;
  Entry &operator=(Entry &&entry) noexcept {
    _no             = entry._no;
    _size_nnmove    = entry._size_nnmove;
    _value_answer   = entry._value_answer;
    _policy_answers = move(entry._policy_answers);
    _nnmove2str     = move(entry._nnmove2str);
    input           = move(entry.input);
    nnmoves         = move(entry.nnmoves); }
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
  vector<Entry> data;
  unique_ptr<float []> probs;
  unique_ptr<float []> values;
};

class QueueTest {
#if defined(USE_OPENCL)
  NNetOCL _nnet;
#else
  NNetCPU _nnet;
#endif
  mutex _m;
  condition_variable _cv;
  thread _th_worker;
  deque<TestSet> _deque_ts;
  vector<Entry> _data;
  uint _npush, _nbatch;

  void worker() noexcept {
    unique_lock<mutex> lock(_m);
    TestSet ts;
    while (true) {
      _cv.wait(lock, [&]{ return 0 < _deque_ts.size(); });
      ts = move(_deque_ts.front());
      _deque_ts.pop_front();
      lock.unlock();

      if (ts.nentry == 0) return;
      _nnet.wait_ff(ts.wait_id);
      for (uint index = 0; index < ts.nentry; ++index)
	ts.data[index].compare(ts.probs.get() + index * SAux::maxsize_moves,
			       ts.values[index]);

      lock.lock();
      for (uint index = 0; index < ts.nentry; ++index)
	cout << setw(5) << ts.data[index].get_no();
      cout << " OK" << endl;
      nelapsed += 1U;
 } }

  void flush() noexcept {
    if (_npush == 0) return;

    unique_ptr<float []> _probs(new float [_nbatch * SAux::maxsize_moves]);
    unique_ptr<float []> _values(new float [_nbatch]);
    unique_ptr<float []> _input(new float [_nbatch * NNAux::size_input]);
    unique_ptr<ushort []> _nnmoves(new ushort [_nbatch * SAux::maxsize_moves]);
    unique_ptr<uint []> _sizes_nnmove(new uint [_nbatch]);
    for (uint index = 0; index < _npush; ++index) {
      const Entry &entry = _data[index];
      copy_n(entry.input.begin(), NNAux::size_input,
	     _input.get() + index * NNAux::size_input);
      copy_n(entry.nnmoves.begin(), SAux::maxsize_moves,
	     _nnmoves.get() + index * SAux::maxsize_moves);
      _sizes_nnmove[index] = entry.get_size_nnmove(); }

    uint wait_id = _nnet.push_ff(_npush, _input.get(), _sizes_nnmove.get(),
				 _nnmoves.get(), _probs.get(), _values.get());
    TestSet ts{wait_id, _npush, move(_data), move(_probs), move(_values)};
    unique_lock<mutex> lock(_m);
    _deque_ts.push_back(move(ts));
    lock.unlock();
    _cv.notify_one();

    _data.resize(_nbatch);
    _npush = 0; }
  
public:
  explicit QueueTest() noexcept : _npush(0) {}

  void start(const FName &fname, int device_id, uint nbatch, bool use_half)
    noexcept {
    _data.resize(nbatch);
    _nbatch = nbatch;
    uint version;
    uint64_t digest;
    NNAux::wght_t wght = NNAux::read(fname, version, digest);
    _th_worker = thread(&QueueTest::worker, this);

#if defined(USE_OPENCL)
    _nnet.reset(nbatch, wght, device_id, use_half);
#else
    _nnet.reset(nbatch, wght);
#endif
    (void)device_id; (void)use_half; }
  
  void push(Entry &entry) noexcept {
    assert(entry.ok());
    _data[_npush] = move(entry);
    if (++_npush == _nbatch) flush(); }

  void end() noexcept {
    flush();
    TestSet ts;
    ts.nentry = 0;
    {
      lock_guard<mutex> lock(_m);
      _deque_ts.push_back(move(ts));
    }
    _th_worker.join(); }
};

static QueueTest queue_test;

static vector<Entry> read_entries(istream &is) noexcept {
  vector<Entry> vec_entry;
  uint no = 0;

  for (uint uline = 0;;) {
    // read position startpos move...
    string string_line;
    if (! getline(is, string_line)) break;
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
    
    // read NN input
    uline += 1U;
    if (! getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "input") die(ERR_INT("bad line %u", uline));
    vector<double> input_answer;
    double di;
    while (ss >> di) input_answer.push_back(di);

    vector<float> input(NNAux::size_input);
    node.encode_input(input.data());
    if (input_answer.size() != NNAux::size_input)
      die(ERR_INT("bad input size %zu vs %u",
		  input_answer.size(), NNAux::size_input));
    
    for (uint uch = 0; uch < NNAux::nch_input; ++uch)
      for (uint usq = 0; usq < Sq::ok_size; ++usq) {
	double f1 = input_answer[uch * Sq::ok_size + usq];
	double f2 = input[uch * Sq::ok_size + usq];
	if (absolute_error(f1, f2) <= epsilon) continue;
	cerr << "input (answer): " << f1 << endl;
	cerr << "input:          " << f2 << endl;
	cerr << "absolute error: " << absolute_error(f1, f2) << endl;
	terminate(); }

    // read state value
    uline += 1U;
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "value") die(ERR_INT("bad line %u", uline));
    double value_answer;
    ss >> value_answer;

    // read action probabilities
    uline += 1U;
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
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
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    if (string_line != "END") die(ERR_INT("bad line %u", uline));
    //if (ms.size() == 0) continue;

    // push target position
    map<ushort, string> nnmove2str;
    vector<ushort> nnmoves2(SAux::maxsize_moves);
    for (uint u = 0; u < ms.size(); ++u) {
      nnmoves2[u] = NNAux::encode_nnmove(ms[u], node.get_turn());
      nnmove2str[nnmoves2[u]] = ms[u].to_str(SAux::usi); }
    
    if (node.get_turn() == white) value_answer = - value_answer;

    /*
    for (float f : input) cout << " " << f;
    cout << "\n";
    cout << ms.size() << "\n";
    for (uint u = 0; u < ms.size(); ++u) cout << " " << nnmoves2[u];
    cout << "\n";
    cout << value_answer << "\n";
    for (uint u = 0; u < ms.size(); ++u)
      cout << " " << policy_answer[nnmove2str[nnmoves2[u]]];
    cout << "\n";
    {
      static uint count = 0;
      if (++count == 1024) std::terminate(); }
    */

    vec_entry.emplace_back(++no, ms.size(), value_answer, move(policy_answer),
			   move(nnmove2str));
    vec_entry.back().input   = move(input);
    vec_entry.back().nnmoves = move(nnmoves2); }

  return vec_entry; }

int main(int argc, char **argv) {
  if (get_options(argc, argv) < 0) return 1;
  queue_test.start(FName(opt_str_wght.c_str()), opt_device_id, opt_batch_size,
		   opt_use_half);
  cout << "Reading stdin ... ";
  cout.flush();
  vector<Entry> vec_entry = read_entries(std::cin);
  cout << "done" << endl;

  steady_clock::time_point start = steady_clock::now();
  for (Entry &entry : vec_entry) queue_test.push(entry);
  queue_test.end();
  steady_clock::time_point end   = steady_clock::now();
  elapsed = duration_cast<microseconds>(end - start).count();

  if (0 < nelapsed) {
    cout << "Average Time: "
	 << 0.001 * elapsed / static_cast<double>(nelapsed)
	 << "ms" << endl; }
  
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
