#include "nnet.hpp"
#include "nnet-shogi.hpp"
#include "param.hpp"
#include <algorithm>
#include <cassert>
using std::fill_n;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
static_assert(SAux::file_size == NNAux::width,
	      "SAux::file_size == NNAux::width");
static_assert(SAux::rank_size == NNAux::height,
	      "SAux::rank_size == NNAux::height");
static_assert(SAux::maxsize_moves == NNAux::nmove,
	      "SAux::maxsize_size == NNAux::nmove");

static void store(float *p, uint uch, uint usq, float f = 1.0f) noexcept {
  assert(p && uch < NNAux::nch_input && usq < Sq::ok_size);
  p[uch * Sq::ok_size + usq] = f; };

static void store_plane(float *p, uint uch, float f = 1.0f) noexcept {
  assert(p && uch < NNAux::nch_input);
  fill_n(p + uch * Sq::ok_size, Sq::ok_size, f); };

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

template<unsigned int Len>
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

template<unsigned int Len>
void NodeNN<Len>::take_action(const Action &a) noexcept {
  Node<Len>::take_action(a);
  if (Node<Len>::get_type().is_interior()) set_posi(); }

template<unsigned int Len>
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
