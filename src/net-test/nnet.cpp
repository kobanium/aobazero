// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet.hpp"
#include "osi.hpp"
#include "xzi.hpp"
#include <fstream>
#include <queue>
#include <vector>
#include <cassert>
#include <climits>
#include <cmath>
using std::ifstream;
using std::ios;
using std::pair;
using std::queue;
using std::unique_ptr;
using std::vector;
using row_t = unique_ptr<float []>;
using namespace ErrAux;

constexpr char msg_bad_xz_fmt[] = "bad xz format";
constexpr char msg_bad_wght[]   = "bad weight format";
constexpr uint len_wght_line    = 1024U * 1024U * 64U;

static pair<uint, row_t> read_row(char *line) noexcept {
  assert(line);
  char *saveptr, *endptr;
  const char *token = OSI::strtok(line, " ", &saveptr);
  queue<float> queue_float;
  while (token) {
    float f = strtof(token, &endptr);
    if (endptr == token || *endptr != '\0' || f == -HUGE_VALF
	|| f == HUGE_VALF) die(ERR_INT(msg_bad_wght));
    queue_float.push(f);
    token = OSI::strtok(nullptr, " ", &saveptr); }
  
  pair<uint, row_t> ret(static_cast<uint>(queue_float.size()),
			unique_ptr<float []>(new float [queue_float.size()]));
  uint u = 0;
  while(!queue_float.empty()) {
    ret.second[u++] = queue_float.front();
    queue_float.pop(); }
  return ret; }

class NotXZ {};
static vector<pair<uint, row_t>>
read_xz(const FName &fwght, uint &version, uint64_t &digest) {
  ifstream ifs(fwght.get_fname(), ios::binary);
  if (!ifs) die(ERR_INT("cannot open %s", fwght.get_fname()));
  
  unique_ptr<char []> line_ptr(new char [len_wght_line]);
  char *line = line_ptr.get();
  XZDecode<ifstream, PtrLen<char>> xzd;
  xzd.init();
  
  // read version
  PtrLen<char> pl_line(line, 0);
  bool bRet = xzd.getline(&ifs, &pl_line, len_wght_line, "\n");
  if (!bRet) throw NotXZ();
  if (pl_line.len == 0) die(ERR_INT(msg_bad_wght));
  if (pl_line.len == len_wght_line) die(ERR_INT("line too long"));
  pl_line.p[pl_line.len] = '\0';
  char *endptr;
  long int v = strtol(line, &endptr, 10);
  if (endptr == line || *endptr != '\0' || v == LONG_MAX || v == LONG_MIN)
    die(ERR_INT(msg_bad_wght));
  version = static_cast<uint>(v);
  
  // read weights and detect dimensions
  vector<pair<uint, row_t>> vec;
  for (uint counter = 0;; ++counter) {
    if (counter == 1024U) die(ERR_INT(msg_bad_wght));
    pl_line.clear();
    bRet = xzd.getline(&ifs, &pl_line, len_wght_line, "\n");
    if (!bRet) die(ERR_INT(msg_bad_wght));
    if (len_wght_line == pl_line.len) die(ERR_INT("line too long"));
    if (pl_line.len == 0) break;
    pl_line.p[pl_line.len] = '\0';
    pair<uint, row_t> row = read_row(line);
    vec.emplace_back(move(row)); }
  digest = xzd.get_crc64();
  return vec; }

static vector<pair<uint, row_t>>
read_txt(const FName &fwght, uint &version, uint64_t &digest) noexcept {
  ifstream ifs(fwght.get_fname());
  if (!ifs) die(ERR_INT("cannot open %s", fwght.get_fname()));
  
  unique_ptr<char []> line_ptr(new char [len_wght_line]);
  char *line = line_ptr.get();
  
  // read version
  ifs.getline(line, len_wght_line, '\n');
  if (ifs.eof()) die(ERR_INT(msg_bad_wght));
  if (!ifs) die(ERR_INT("line too long"));
  digest = XZAux::crc64(line, 0);
  digest = XZAux::crc64("\n", digest);
  
  char *endptr;
  long int v = strtol(line, &endptr, 10);
  if (endptr == line || *endptr != '\0' || v == LONG_MAX || v == LONG_MIN)
    die(ERR_INT(msg_bad_wght));
  version = static_cast<uint>(v);
  
  // read weights and detect dimensions
  vector<pair<uint, row_t>> vec;
  for (uint counter = 0;; ++counter) {
    if (counter == 1024U) die(ERR_INT(msg_bad_wght));
    
    ifs.getline(line, len_wght_line);
    if (ifs.eof()) break;
    if (!ifs) die(ERR_INT("line too long"));
    digest = XZAux::crc64(line, digest);
    digest = XZAux::crc64("\n", digest);
    pair<uint, row_t> row = read_row(line);
    vec.emplace_back(move(row)); }
  return vec; }

vector<pair<uint, row_t>>
NNAux::read(const FName &fwght, uint &version, uint64_t &digest) noexcept {
  wght_t ret;
  try             { ret = read_xz (fwght, version, digest); }
  catch (NotXZ &) { ret = read_txt(fwght, version, digest); }
  return ret; }
