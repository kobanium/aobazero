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
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <lzma.h>
using std::cout;
using std::cerr;
using std::endl;
using std::fill_n;
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

static double elapsed = 0.0;
static uint nelapsed  = 0U;

static void do_test(istream &is) noexcept;
static void compare(const vector<string> &path, const vector<double> &input,
		    double value, const map<string, double> &policy) noexcept;

static uint   value_n          = 0;
static double value_sum_e      = 0.0;
static double value_sum_se     = 0.0;
static double value_max_e      = 0.0;

static uint   policy_n          = 0;
static double policy_sum_e      = 0.0;
static double policy_sum_se     = 0.0;
static double policy_max_e      = 0.0;

static NNet nnet;

int main(int argc, char **argv) {
  if (argc != 2) {
    die(ERR_INT("Usage: %s weight", argv[0]));
    return 1; }
  
  nnet.reset(FName(argv[1]));
  do_test(std::cin);

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

  for (uint uline = 0, udata = 0;;) {
    // read position startpos move...
    string string_line;
    if (! getline(is, string_line)) break;
    udata += 1U;
    uline += 1U;
    
    stringstream ss(string_line);
    string token1, token2, token3;
    ss >> token1 >> token2 >> token3;
    if (token1 != "position" || token2 != "startpos" || token3 != "moves")
      die(ERR_INT("bad line %u", uline));

    vector<string> path;
    while (ss >> token1) path.emplace_back(move(token1));
    
    // input
    uline += 1U;
    if (! getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "input") die(ERR_INT("bad line %u", uline));
    vector<double> input;
    double f;
    while (ss >> f) input.push_back(f);

    // value
    uline += 1U;
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "value") die(ERR_INT("bad line %u", uline));
    double value;
    ss >> value;

    // policy
    uline += 1U;
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "policy") die(ERR_INT("bad line %u", uline));

    map<string, double> policy;
    while (ss >> token1 >> f) policy.emplace(move(token1), f);

    // END
    uline += 1U;
    if (!getline(is, string_line)) die(ERR_INT("bad line %u", uline));
    if (string_line != "END") die(ERR_INT("bad line %u", uline));

    compare(path, input, value, policy);
    cout << setw(5) << udata << " OK" << endl; } }

static double absolute_error(double f1, double f2) noexcept {
  return std::fabs(f1 - f2); }

static void compare(const vector<string> &path, const vector<double> &input1,
		    double value1, const map<string, double> &policy1)
  noexcept {
  constexpr double epsilon = 1e-4;
  NodeNN<Param::maxlen_play> node;
  for (auto &s : path) {
    Action a = node.action_interpret(s.c_str(), SAux::usi);
    if (! a.is_move()) die(ERR_INT("bad move"));
    node.take_action(a); }
  
  // test input
  unique_ptr<float []> input2(new float [NN::size_input]);
  node.encode_input(input2.get());
  
  if (input1.size() != NN::size_input)
    die(ERR_INT("bad input size %zu vs %u", input1.size(), NN::size_input));

  for (uint uch = 0; uch < NN::nch_input; ++uch)
    for (uint usq = 0; usq < Sq::ok_size; ++usq) {
      double f1 = input1[uch * Sq::ok_size + usq];
      double f2 = input2[uch * Sq::ok_size + usq];
      if (absolute_error(f1, f2) <= epsilon) continue;
      cerr << "input1:         " << f1 << endl;
      cerr << "input2:         " << f2 << endl;
      cerr << "absolute error: " << absolute_error(f1, f2) << endl;
      terminate(); }

  MoveSet<Param::maxlen_play> ms;
  ms.gen_all(node);
  assert(ms.ok());

  // test genmove
  set<string> set1, set2;
  for (auto &f : policy1)              set1.insert(f.first);
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
  
  if (ms.size() == 0) return;
  
  ushort nnmoves2[SAux::maxsize_moves];
  for (uint u = 0; u < ms.size(); ++u)
    nnmoves2[u] = NN::encode_nnmove(ms[u], node.get_turn());
  
  float prob2[SAux::maxsize_moves];
  system_clock::time_point start = system_clock::now();
  double value2 = nnet.ff(input2.get(), ms.size(), nnmoves2, prob2);
  system_clock::time_point end = system_clock::now();
  elapsed += duration_cast<microseconds>(end - start).count();
  nelapsed += 1U;
  if (node.get_turn() == white) value2 = -value2;
  
  map<string, double> policy2;
  for (uint u = 0; u < ms.size(); ++u)
    policy2.emplace(ms[u].to_str(SAux::usi), prob2[u]);
  
  // test output
  double value_e    = absolute_error(value1, value2);
  value_n          += 1U;
  value_sum_e      += value_e;
  value_sum_se     += value_e * value_e;
  value_max_e       = max(value_max_e, value_e);
  if (2.0 * epsilon < value_e) {
    cerr << "value1:         " << value1 << endl;
    cerr << "value2:         " << value2 << endl;
    cerr << "absolute error: " << value_e << endl;
    terminate(); }
  
  auto &&it1 = policy1.cbegin();
  auto &&it2 = policy2.cbegin();
  while (it1 != policy1.cend() && it2 != policy2.cend()) {
    double prob1 = (it1++)->second;
    double prob2 = (it2++)->second;
    double policy_e     = absolute_error(prob1, prob2);
    policy_n          += 1U;
    policy_sum_e      += policy_e;
    policy_sum_se     += policy_e * policy_e;
    policy_max_e       = max(policy_max_e, policy_e);
    if (epsilon < policy_e) {
      cerr << "prob1:          " << prob1 << endl;
      cerr << "prob2:          " << prob2 << endl;
      cerr << "absolute error: " << policy_e << endl;
      terminate(); } } }
