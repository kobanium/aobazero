// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet.hpp"
#include "param.hpp"
#include "shogibase.hpp"
#include "xzi.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
using std::copy_n;
using std::cout;
using std::cerr;
using std::endl;
using std::getline;
using std::istream;
using std::map;
using std::max;
using std::move;
using std::set;
using std::setw;
using std::string;
using std::stringstream;
using std::terminate;
using std::unique_ptr;
using std::vector;
using std::chrono::system_clock;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;
using namespace SAux;

static_assert(file_size == NN::width,  "file_size == NN::width");
static_assert(rank_size == NN::height, "rank_size == NN::height");
constexpr double epsilon = 1e-4;

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

static double absolute_error(double f1, double f2) noexcept {
  return std::fabs(f1 - f2); }

template <uint N>
class QueueTest {
  static_assert(0 < N, "0 < N");
  unique_ptr<float []> _input;
  unique_ptr<ushort []> _nnmoves;
  unique_ptr<float []> _probs;
  float _values[N];
  uint _sizes_nnmove[N];
  uint _npush;
  uint _ntest;
  NNet _nnet;
  map<ushort, string> _tbl_nnmove2str[N];
  map<string, double> _policy_answers[N];
  float _value_answers[N];

  void test(uint index) const noexcept {
    assert(index < N);
    
    // test output
    double value_e  = absolute_error(_values[index], _value_answers[index]);
    value_n        += 1U;
    value_sum_e    += value_e;
    value_sum_se   += value_e * value_e;
    value_max_e     = max(value_max_e, value_e);
    if (2.0 * epsilon < value_e) {
      cerr << "value1:         " << _values[index] << endl;
      cerr << "value2:         " << _value_answers[index] << endl;
      cerr << "absolute error: " << value_e << endl;
      terminate(); }
  
    map<string, double> policy;
    for (uint u = 0; u < _sizes_nnmove[index]; ++u) {
      ushort us = _nnmoves[index * SAux::maxsize_moves + u];
      const string &str = _tbl_nnmove2str[index].at(us);
      policy[str] = _probs[index * SAux::maxsize_moves + u]; }
	   
    assert(_sizes_nnmove[index] == _policy_answers[index].size());
    auto &&it1 = _policy_answers[index].cbegin();
    auto &&it2 = policy.cbegin();
    while (it1 != _policy_answers[index].cend()
	   && it2 != _policy_answers[index].cend()) {
      double prob1       = (it1++)->second;
      double prob2       = (it2++)->second;
      double policy_e    = absolute_error(prob1, prob2);
      policy_n          += 1U;
      policy_sum_e      += policy_e;
      policy_sum_se     += policy_e * policy_e;
      policy_max_e       = max(policy_max_e, policy_e);
      if (epsilon < policy_e) {
	cerr << "prob1:          " << prob1 << endl;
	cerr << "prob2:          " << prob2 << endl;
	cerr << "absolute error: " << policy_e << endl;
	terminate(); } } }
  
public:
  explicit QueueTest() noexcept
  : _input(new float [N * NN::size_input]),
    _nnmoves(new ushort [N * SAux::maxsize_moves]),
    _probs(new float [N * SAux::maxsize_moves]), _npush(0), _ntest(0) {}

  void reset(const FName &fname) noexcept { _nnet.reset(fname); }
  
  void flush() noexcept {
    system_clock::time_point start = system_clock::now();

    for (uint u = 0; u < _npush; ++u)
      _nnet.ff(1, &( _input[u * NN::size_input] ), _sizes_nnmove + u,
	       &( _nnmoves[u * SAux::maxsize_moves] ),
	       &( _probs[u * SAux::maxsize_moves] ), _values + u);
    
    system_clock::time_point end = system_clock::now();
    elapsed  += duration_cast<microseconds>(end - start).count();
    nelapsed += 1U;

    for (uint index = 0; index < _npush; ++index) test(index);
    for (uint index = 0; index < _npush; ++index) {
      _ntest += 1U; cout << setw(5) << _ntest; }
    if (_npush) {
      cout << " OK" << endl;
      _npush = 0; } }
  
  void push(const float *input, uint size_nnmove, const ushort *nnmoves,
	    float value, const map<string, double> &policy_answers,
	    const map<ushort, string> &nnmove2str) noexcept {
    copy_n(input, NN::size_input,
	   &( _input[_npush * NN::size_input] ));
    copy_n(nnmoves, SAux::maxsize_moves,
	   &( _nnmoves[_npush * SAux::maxsize_moves] ));
    _sizes_nnmove[_npush]   = size_nnmove;
    _value_answers[_npush]  = value;
    _policy_answers[_npush] = policy_answers;
    _tbl_nnmove2str[_npush] = nnmove2str;
    if (++_npush == N) flush(); }
};

static void do_test(istream &is) noexcept;

static QueueTest<4> queue_test;

int main(int argc, char **argv) {
  if (argc != 2) {
    die(ERR_INT("Usage: %s weight", argv[0]));
    return 1; }
  
  queue_test.reset(FName(argv[1]));
  do_test(std::cin);
  queue_test.flush();

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

static void do_test(istream &is) noexcept {

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
    double f;
    while (ss >> f) input_answer.push_back(f);

    unique_ptr<float []> input(new float [NN::size_input]);
    node.encode_input(input.get());
    if (input_answer.size() != NN::size_input)
      die(ERR_INT("bad input size %zu vs %u",
		  input_answer.size(), NN::size_input));
    
    for (uint uch = 0; uch < NN::nch_input; ++uch)
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
    while (ss >> token1 >> f) policy_answer.emplace(move(token1), f);

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
    if (ms.size() == 0) return;

    // push target position
    map<ushort, string> nnmove2str;
    ushort nnmoves2[SAux::maxsize_moves];
    for (uint u = 0; u < ms.size(); ++u) {
      nnmoves2[u] = NN::encode_nnmove(ms[u], node.get_turn());
      nnmove2str[nnmoves2[u]] = ms[u].to_str(SAux::usi); }
    
    if (node.get_turn() == white) value_answer = - value_answer;
    
    queue_test.push(input.get(), ms.size(), nnmoves2, value_answer,
		    policy_answer, nnmove2str); } }
