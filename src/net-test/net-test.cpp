// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "shogibase.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
using std::cout;
using std::endl;
using std::fill_n;
using std::string;
using std::max;
using std::min;
using std::pair;
using std::terminate;
using std::vector;
using policy_t = vector<pair<string, float>>;
using namespace ErrAux;
using namespace SAux;

static void do_test(std::istream &ifs) noexcept;
static void compare(const vector<string> &path, const vector<float> &inputs,
		    float value, const policy_t &policy, uint udata) noexcept;

int main(int argc, char **argv) {
  if (argc == 1) {
    do_test(std::cin);
    return 0; }
  
  while (*++argv) {
    std::ifstream ifs(*argv);
    if (!ifs) die(ERR_INT("cannot open %s", *argv));
    do_test(ifs); }
  return 0; }
  
static void do_test(std::istream &ifs) noexcept {

  for (uint uline = 0, udata = 0;;) {

    // read position startpos moves...
    std::string string_line;
    if (! std::getline(ifs, string_line)) break;
    udata += 1U;
    uline += 1U;
    std::cout << udata << std::endl;
    
    std::stringstream ss(string_line);
    std::string token1, token2, token3;
    ss >> token1 >> token2 >> token3;
    if (token1 != "position" || token2 != "startpos" || token3 != "moves")
      die(ERR_INT("bad line %u", uline));

    vector<string> path;
    while (ss >> token1) path.push_back(std::move(token1));
    
    // input...
    uline += 1U;
    if (! std::getline(ifs, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "input") die(ERR_INT("bad line %u", uline));
    
    vector<float> inputs;
    float f;
    while (ss >> f) inputs.push_back(f);

    // value...
    uline += 1U;
    if (! std::getline(ifs, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "value") die(ERR_INT("bad line %u", uline));

    float value;
    ss >> value;

    // policy...
    uline += 1U;
    if (! std::getline(ifs, string_line)) die(ERR_INT("bad line %u", uline));
    ss.clear();
    ss.str(string_line);
    ss >> token1;
    if (token1 != "policy") die(ERR_INT("bad line %u", uline));

    policy_t policy;
    while (ss >> token1 >> f) policy.emplace_back(std::move(token1), f);

    // END
    uline += 1U;
    if (! std::getline(ifs, string_line)) die(ERR_INT("bad line %u", uline));
    if (string_line != "END") die(ERR_INT("bad line %u", uline));

    compare(path, inputs, value, policy, udata); } }

namespace NN {
  constexpr uint nch      = 362U;
  constexpr uint ninput   = nch * Sq::ok_size;
  constexpr float epsilon = 0.001f;
}

static bool is_approx_equal(float f1, float f2) noexcept {
  return fabsf(f1 - f2) <= max(fabsf(f1), fabsf(f2)) * NN::epsilon; }

static void encode_features(const Node &node, float *p) noexcept {
  assert(node.ok() && p);
  fill_n(p, NN::ninput, 0.0);
  const Board &board = node.get_board();
  const Color &turn  = node.get_turn();
  
  auto store = [p](uint ch, uint usq, float f = 1.0f){
    p[ch * Sq::ok_size + usq] = f; };

  auto store_plane = [p](uint ch, float f = 1.0f){
    fill_n(p + ch * Sq::ok_size, Sq::ok_size, f); };

  uint ch_off = 0;
  for (Color c : {turn, turn.to_opp()}) {
    BMap bm = board.get_bm_color(c);
    for (Sq sq(bm.pop_f()); sq.ok(); sq = Sq(bm.pop_f())) {
      Pc pc = board.get_pc(sq);
      store(ch_off + pc.to_u(), sq.rel(turn).to_u()); }
    ch_off += Pc::ok_size; }
  
  for (Color c : {turn, turn.to_opp()}) {
    for (uint upc = 0; upc < Pc::hand_size; ++upc) {
      uint num = board.get_num_hand(c, Pc(upc));
      if (num) {
	float f = ( static_cast<float>(num)
		    / static_cast<float>(Pc::num_array[upc]) );
	store_plane(ch_off + upc, f); } }
    ch_off += Pc::hand_size; }

  uint nrep = node.get_count_repeat();
  for (uint u = 0; u < nrep; ++u) store_plane(ch_off + u);
  ch_off += 3U;

  if (turn == white) store_plane(360U);
  uint len = min(node.get_len_path(), 512U);
  store_plane(361U, static_cast<float>(len) * (1.0f / 512.0f)); }

#include <set>
#include <iomanip>
static void compare(const vector<string> &path, const vector<float> &inputs1,
		    float value, const policy_t &policy, uint udata) noexcept {
  std::unique_ptr<float []> inputs2(new float [NN::ninput]);
  Node node;
  for (auto &s : path) {
    Action a = node.action_interpret(s.c_str(), SAux::usi);
    if (! a.is_move()) die(ERR_INT("bad move"));
    node.take_action(a); }

  encode_features(node, inputs2.get());
  
  MoveSet ms;
  ms.gen_all(node);

  // test genmove
  std::set<string> set1, set2;
  for (auto &f : policy) set1.emplace(f.first);
  for (uint u = 0; u < ms.size(); ++u) set2.emplace(ms[u].to_str(SAux::usi));

  if (set1 != set2) {
    cout << "position startpos moves";
    for (auto &s : path) cout << " " << s;
    cout << "\n" << node.to_str();

    auto &&it1 = set1.cbegin();
    auto &&it2 = set2.cbegin();
    while (it1 != set1.cend() || it2 != set2.cend()) {
      if (it1 == set1.cend()) cout << "      ";
      else cout << std::setw(6) << std::left << *it1++;
    
      if (it2 != set2.cend()) cout << std::setw(6) << std::left << *it2++;
      cout << endl; }
    
    terminate(); }

  // test input
  if (inputs1.size() != NN::ninput) {
    cout << "bad input size " << inputs1.size() << endl;
    terminate(); }

  for (uint uch = 0; uch < 28U + 14U + 3U /*NN::nch*/; ++uch)
    for (uint usq = 0; usq < Sq::ok_size; ++usq)
      if (!is_approx_equal(inputs1[uch*Sq::ok_size + usq],
			   inputs2[uch*Sq::ok_size + usq])) {
	cout << "bad input " << uch << " " << usq << endl;
	cout << inputs1[uch*Sq::ok_size + usq] << " "
	     << inputs2[uch*Sq::ok_size + usq] << endl;
	terminate(); }
}
