// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "xzi.hpp"
#include <algorithm>
#include <fstream>
#include <queue>
#include <vector>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
using std::fill_n;
using std::ifstream;
using std::ios;
using std::make_tuple;
using std::pair;
using std::queue;
using std::tuple;
using std::unique_ptr;
using std::vector;
using row_t  = unique_ptr<float []>;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;
static_assert(SAux::file_size == NNAux::width,
	      "SAux::file_size == NNAux::width");
static_assert(SAux::rank_size == NNAux::height,
	      "SAux::rank_size == NNAux::height");

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

static void store(float *p, uint uch, uint usq, float f = 1.0f) noexcept {
  assert(p && uch < NNAux::nch_input && usq < Sq::ok_size);
  p[uch * Sq::ok_size + usq] = f; };

static void store_plane(float *p, uint uch, float f = 1.0f) noexcept {
  assert(p && uch < NNAux::nch_input);
  fill_n(p + uch * Sq::ok_size, Sq::ok_size, f); };

NNInput::NNInput(uint nb) noexcept : _ub(0), _nb(nb),
  _input(new float [nb * NNAux::size_input]), _sizes_nnmove(new uint [nb]),
  _nnmoves(new ushort [nb * SAux::maxsize_moves]) {}

void NNInput::add(const float *input, uint size_nnmove, const ushort *nnmoves)
  noexcept {
  assert(input && size_nnmove < SAux::maxsize_moves && nnmoves && _ub < _nb);
  memcpy(_input.get() + _ub * NNAux::size_input, input,
	   NNAux::size_input * sizeof(float));
  memcpy(_nnmoves.get() + _ub * SAux::maxsize_moves, nnmoves,
	 SAux::maxsize_moves * sizeof(ushort));
  _sizes_nnmove[_ub] = size_nnmove;
  _ub += 1U; }

void NNAux::softmax(uint n, float *p) noexcept {
  if (n == 0) return;
  assert(p);
  float fmax = *std::max_element(p, p + n);
  float fsum = 0.0f;
  for (uint u = 0; u < n; ++u) {
    p[u] = std::exp(p[u] - fmax);
    fsum += p[u]; }
  
  assert(0.0 < fsum);
  float factor = 1.0f / fsum;
  for (uint u = 0; u < n; ++u) p[u] *= factor; }

vector<pair<uint, row_t>>
NNAux::read(const FName &fwght, uint &version, uint64_t &digest) noexcept {
  wght_t ret;
  try             { ret = read_xz (fwght, version, digest); }
  catch (NotXZ &) { ret = read_txt(fwght, version, digest); }
  return ret; }

ushort NNAux::encode_nnmove(const Action &a, const Color &turn) noexcept {
  assert(a.is_move() && turn.ok());
  Sq to = a.get_to().rel(turn);
  Pc pc = a.get_pc();
  if (a.is_drop()) {
    uint ch   = 2U * (64U + 2U) + pc.to_u();
    ushort us = static_cast<ushort>(ch * Sq::ok_size + to.to_u());
    assert(10692U <= us && us < 11259U);
    return us; }

  uint ch = 0;
  if (a.is_promotion()) ch += 64U + 2U;

  Sq from   = a.get_from().rel(turn);
  uchar dir = from.to_dir(to);
  assert(dir < 10U);
  if      (dir <  8U) ch += dir * 8U + from.to_distance(to) - 1U;
  else if (dir == 8U) ch += 64U;
  else if (dir == 9U) ch += 64U + 1U;

  ushort us = static_cast<ushort>(ch * Sq::ok_size + from.to_u());
  assert(us < 10692U);
  return us; }

tuple<uint, uint, uint, uint>
NNAux::pack_batch(uint nb0, uint nb, const float *in, const uint *sizes_nnmove,
		  const ushort *nnmoves, void *out) noexcept {
  assert(nb0 <= nb && in && sizes_nnmove && nnmoves && out);
  float *pvalue = static_cast<float *>(out);
  uint  *pindex = static_cast<uint *>(out) + NNAux::fill_block_size(nb);
  uint index;
  for (uint ub = 0; ub < nb; ++ub) {
    for (uint uposi = 0; uposi < 8U; ++uposi)
      for (uint ufplane = 28; ufplane < 45U; ++ufplane) {
	uint ch = 45U * uposi + ufplane;
	index   = (ub * NNAux::nch_input + ch) * NNAux::size_plane;
	*pindex++ = (ch * nb + ub) * NNAux::size_plane;
	*pvalue++ = (ub < nb0) ? in[index] : 0.0f; }
    index     = (ub * NNAux::nch_input + 360U) * NNAux::size_plane;
    *pindex++ = (360U * nb + ub) * NNAux::size_plane;
    *pvalue++ = (ub < nb0) ? in[index] : 0.0f;
    index     = (ub * NNAux::nch_input + 361U) * NNAux::size_plane;
    *pindex++ = (361U * nb + ub) * NNAux::size_plane;
    *pvalue++ = (ub < nb0) ? in[index] : 0.0f; }

  pindex = static_cast<uint *>(out) + NNAux::fill_block_size(nb)*2U;
  uint n_one = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint uposi = 0; uposi < 8U; ++uposi)
      for (uint ufplane = 0; ufplane < 28U; ++ufplane) {
	uint ch  = 45U * uposi + ufplane;
	uint bch = (ub * NNAux::nch_input + ch) * NNAux::size_plane;
	uint chb = (ch * nb + ub) * NNAux::size_plane;
	for (uint u = 0; u < NNAux::size_plane; ++u) {
	  if (in[bch + u] < 0.5f) continue;
	  pindex[n_one++] = chb + u; } }

  uint ntot_moves = 0;
  uint index_moves = (NNAux::fill_block_size(nb)*2U
		      + NNAux::ceil_multi(n_one, 32U));
  pindex = static_cast<uint *>(out) + index_moves;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint unn = 0; unn < sizes_nnmove[ub]; ++unn) {
      uint nnmove = nnmoves[ub * SAux::maxsize_moves + unn];
      ntot_moves += 1U;
      assert(nnmove < NNAux::nch_out_policy * NNAux::size_plane);
      *pindex++ = ub * NNAux::nch_out_policy * NNAux::size_plane + nnmove; }
  
  uint size_write = (index_moves + ntot_moves) * sizeof(uint);
  return make_tuple(size_write, n_one, ntot_moves, index_moves); }

template<uint Len>
void NodeNN<Len>::set_posi() noexcept {
  uint len = Node<Len>::get_len_path();
  const Board &board = Node<Len>::get_board();

  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    _posi[len].bm[uc] = board.get_bm_color(Color(uc));
    for (uint upc = 0; upc < Pc::hand_size; ++upc)
      _posi[len].hand[uc][upc] = board.get_num_hand(Color(uc), Pc(upc)); }
    
  for (uint nsq = 0; nsq < Sq::ok_size; ++nsq)
    _posi[len].board[nsq] = board.get_pc(Sq(nsq)).to_u();
  
  _posi[len].count_repeat = Node<Len>::get_count_repeat(); }

template<uint Len>
void NodeNN<Len>::take_action(const Action &a) noexcept {
  Node<Len>::take_action(a);
  if (Node<Len>::get_type().is_interior()) set_posi(); }

template<uint Len>
void NodeNN<Len>::encode_input(float *p) const noexcept {
  assert(p);
  fill_n(p, NNAux::size_input, 0.0f);
  const Color &turn = Node<Len>::get_turn();
  
  uint ch_off = 0;
  for (uint uposi = 0; uposi < 8U; ++uposi) {
    if (Node<Len>::get_len_path() < uposi) break;
    uint len_path = Node<Len>::get_len_path() - uposi;
    
    // board piece position
    for (Color c : {turn, turn.to_opp()}) {
      BMap bm = _posi[len_path].bm[c.to_u()];
      for (Sq sq(bm.pop_f()); sq.ok(); sq = Sq(bm.pop_f())) {
	uint upc = _posi[len_path].board[sq.to_u()];
	store(p, ch_off + upc, sq.rel(turn).to_u()); }
      ch_off += Pc::ok_size; }
    
    // hand piece position
    for (Color c : {turn, turn.to_opp()}) {
      for (uint upc = 0; upc < Pc::hand_size; ++upc) {
	uint num = _posi[len_path].hand[c.to_u()][upc];
	if (num == 0) continue;
	float f = ( static_cast<float>(num)
		    / static_cast<float>(Pc::num_array[upc]) );
	store_plane(p, ch_off + upc, f); }
      ch_off += Pc::hand_size; }
    
    // repeat count
    uint nrep = _posi[len_path].count_repeat;
    for (uint u = 0; u < nrep; ++u) store_plane(p, ch_off + u);
    ch_off += 3U; }
  
  // player to move
  if (turn == SAux::white) store_plane(p, 360U);
  
  // path length from root of shogi
  uint len_path = Node<Len>::get_len_path();
  store_plane(p, 361U, (1.0f / 512.0f) * static_cast<float>(len_path)); }

template class NodeNN<Param::maxlen_play>;

/*
  channel 
  0  - 13 black position
  14 - 27 white position
  28 - 34 black hand (fill)
  35 - 41 white hand (fill)
  42 - 44 rep        (fill)

  360     0: black, 1: white (fill)
  361     length             (fill)
 */
