// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  pragma warning( disable : 4127)
#endif
#include "err.hpp"
#include "option.hpp"
#include "osi.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
using std::ifstream;
using std::out_of_range;
using std::numeric_limits;
using std::vector;
using std::max;
using std::map;
using std::mutex;
using std::string;
using std::stringstream;
using std::cout;
using std::cerr;
using std::endl;
using ushort = unsigned short;
using uint   = unsigned int;

void Config::read(const char *fname, map<string, string> &m) {
  assert(fname);
  ifstream ifs(fname);
  if (!ifs) throw ERR_INT("cannot open a file %s", fname);
    
  char line[1024];
  char *token, *saveptr;
  string s1, s2;
  bool bFirst;
  while (ifs.getline(line, sizeof(line))) {
    token = OSI::strtok(line, " \t,=:", &saveptr);
    if (token == NULL || token[0] == '#') continue;
    s1 = string(token);

    s2 = string("");
    bFirst = true;
    while (true) {
      token = OSI::strtok(NULL, " \t", &saveptr);
      if (token == NULL || token[0] == '#') break;
      if (bFirst) { bFirst = false; s2 += string(token); continue; }
      s2 += " ";
      s2 += string(token); }

    try { m.at(s1) = s2; }
    catch (const out_of_range &) { throw ERR_INT("invalid option %s",
						 s1.c_str()); } } }

template<typename T>
T Config::get(const map<string, string> &m, const char *p, bool (*ok)(T)) {
  assert(p);
  
  char *endptr;
  const char *token = m.at(p).c_str();
  if (std::is_floating_point<T>::value == true) {
    long double v = strtold(token, &endptr);
    if (endptr == token || *endptr != '\0' || v == HUGE_VALL
	|| numeric_limits<T>::max() < v || v < numeric_limits<T>::lowest()
	|| !ok(static_cast<T>(v)))
      throw ERR_INT("value of option %s invalid", p);
    return static_cast<T>(v); }

  long long int v = strtoll(token, &endptr, 10);
  if (endptr == token || *endptr != '\0' || v == LLONG_MAX || v == LLONG_MIN
      || numeric_limits<T>::max() < v || v < numeric_limits<T>::min()
      || !ok(static_cast<T>(v)))
    throw ERR_INT("value of option %s invalid", p);
  return static_cast<T>(v); }

const char *Config::get_cstr(const map<string, string> &m, const char *p,
			     size_t maxlen) {
  assert(p);
  const string &s = m.at(string(p));
  
  if (maxlen < s.size() + 1) throw ERR_INT("value of option %s too long", p);
  return s.c_str(); }

vector<string>
Config::get_vecstr(const map<string, string> &m, const char *p) {
  assert(p);
  vector<string> vec;
  stringstream ss( m.at(string(p)) );
  string s;
  while (ss >> s) vec.push_back(s);
  return vec; }

template<typename T>
vector<T> Config::getv(const map<string, string> &m, const char *p,
		       bool (*ok)(T)) {
  assert(p);
  vector<T> ret;
  char *endptr;
  const char *token( m.at(p).c_str() );

  while (true) {
    long long int v = strtoll(token, &endptr, 10);

    if (endptr == token || (*endptr != '\0' && *endptr != ' ')
	|| v == LLONG_MAX || v == LLONG_MIN
	|| numeric_limits<T>::max() < v || v < numeric_limits<T>::min()
	|| !ok(static_cast<T>(v)))
      throw ERR_INT("value of option %s invalid", p);

    ret.push_back( static_cast<T>(v) );
    if (*endptr == '\0') break;
    token = endptr + 1; }
  
  if (ret.empty()) throw ERR_INT("value of option %s invalid", p);
  return ret; }

template float Config::get<float>(const map<string, string> &, const char *,
				  bool (*)(float));
template ushort Config::get<ushort>(const map<string, string> &, const char *,
				    bool (*)(ushort));
template uint Config::get<uint>(const map<string, string> &, const char *,
				bool (*)(uint));
template int64_t Config::get<int64_t>(const map<string, string> &, const char *,
				      bool (*)(int64_t));
template vector<int> Config::getv<int>(const map<string, string> &,
				       const char *, bool (*)(int));

/* a getopt clone based on Daniel J. Bernstein (1991)*/
using namespace Opt;
const char *Opt::arg = nullptr;
const char *Opt::cmd = nullptr;
int Opt::ind = 1;
int Opt::err = 1;
int Opt::opt = 0;

static int pos = 0;

int Opt::get(int argc, const char * const *argv, const char *opts) noexcept {
  if (!cmd) {
    if (argv[0]) {
      cmd = argv[0];
      for (const char *p = cmd; *p != '\0'; ++p)
	if (*p == '/') cmd = p + 1; }
    else cmd = ""; }
 
  arg = nullptr;
  if (!opts || !argv || !argv[0] || argc <= ind || !argv[ind]) return -1;
  
  while (pos && argv[ind][pos] == '\0') {
    ind += 1;
    pos = 0;
    if (argc <= ind || !argv[ind]) return -1; }
 
  if (pos == 0) {
    if (argv[ind][0] != '-') return -1;
    pos += 1;
    int c = argv[ind][1];
    if (c == '-' || c == '\0') {
      if (c == '-') ind += 1;
      pos = 0;
      return -1; } }
 
  int c = argv[ind][pos];
  pos += 1;
  while (*opts != '\0') {
    
    if (c != *opts) {
      opts += 1;
      if (*opts == ':') opts += 1;
      continue; }

    if (opts[1] == ':') {
      arg = argv[ind] + pos;
      ind += 1;
      pos = 0;

      if (*arg == '\0' && (argc <= ind || !argv[ind])) {
	opt = c;
	if (err) cerr << cmd << ": option requires an argument "
		      << static_cast<char>(c) << endl;
	return '?'; }

      if (*arg == '\0') arg = argv[ind++]; }
    
    return c; }
  
  opt = c;
  if (err) cerr << cmd << ": illegal option " << static_cast<char>(c) << endl;
  return '?'; }
