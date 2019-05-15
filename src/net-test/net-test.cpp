// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "shogibase.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
using std::vector;
using std::string;
using std::pair;
using std::cout;
using std::endl;
using policy_t = vector<pair<string, float>>;
using namespace ErrAux;

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

#include <set>
#include <iomanip>
static void compare(const vector<string> &path, const vector<float> &inputs,
		    float value, const policy_t &policy, uint udata) noexcept {
  Node node;
  for (auto &s : path) {
    Action a = node.action_interpret(s.c_str(), SAux::usi);
    if (! a.is_move()) die(ERR_INT("bad move"));
    node.take_action(a); }

  MoveSet ms;
  ms.gen_all(node);
  
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
    std::terminate(); } }
