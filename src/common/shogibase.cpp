// 2019 Team AobaZero
// This source code is in the public domain.
#include "shogibase.hpp"
#include "param.hpp"
#include <algorithm>
#include <functional>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <climits>
#include <cstring>
#include <cstdint>
using std::function;
using std::fill_n;
using std::min;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace SAux;

constexpr BMap::RBB BMap::tbl_bmap_rbb[81][4];
constexpr unsigned char BMap::tbl_bmap_rel[Color::ok_size][3];

FixLStr<128U> BMap::to_str() const noexcept {
  auto f = [](uint bits, uint u){
    return (bits & (1U << (26U - u))) ? "1" : "."; };
  FixLStr<128U> str;

  str += "\n"; for (uint u =  0U; u <  9U; ++u) str += f(_data[0], u);
  str += "\n"; for (uint u =  9U; u < 18U; ++u) str += f(_data[0], u);
  str += "\n"; for (uint u = 18U; u < 27U; ++u) str += f(_data[0], u);
  str += "\n"; for (uint u =  0U; u <  9U; ++u) str += f(_data[1], u);
  str += "\n"; for (uint u =  9U; u < 18U; ++u) str += f(_data[1], u);
  str += "\n"; for (uint u = 18U; u < 27U; ++u) str += f(_data[1], u);
  str += "\n"; for (uint u =  0U; u <  9U; ++u) str += f(_data[2], u);
  str += "\n"; for (uint u =  9U; u < 18U; ++u) str += f(_data[2], u);
  str += "\n"; for (uint u = 18U; u < 27U; ++u) str += f(_data[2], u);
  str += "\n"; return str; }

const char * const Color::tbl_color_CSA[ok_size] = { "+", "-" };
constexpr unsigned char Pc::tbl_promote[ok_size];
constexpr unsigned char Pc::tbl_unpromote[ok_size];
constexpr unsigned char Pc::tbl_slider[14];
constexpr unsigned char Pc::tbl_point[14];
const char * const Pc::tbl_pc_name[mode_size][ok_size] = {
  { "FU", "KY", "KE", "GI", "KI", "KA", "HI",
    "OU", "TO", "NY", "NK", "NG", "UM", "RY" },
  { "P",  "L",  "N",  "S",  "G",  "B",  "R",
    "K", "+P", "+L", "+N", "+S", "+B", "+R" } };

Pc::Pc(int ch1, int ch2, SAux::Mode mode) noexcept {
  for (_u = 0; _u < Pc::ok_size; ++_u) {
    const char *p = Pc(_u).to_str(mode);
    if (ch1 == p[0] && ch2 == p[1]) break; } }

constexpr BMap Sq::tbl_sq_obstacle[81][81];
constexpr BMap Sq::tbl_sq_u2bmap[ok_size][ray_size];
constexpr uchar Sq::tbl_sq_rel[2][81];
constexpr uchar Sq::tbl_sq_adv[2][81];
constexpr uchar Sq::tbl_sq_ray[81][81];
constexpr uchar Sq::tbl_sq_dir[81][81];
constexpr uchar Sq::tbl_sq_distance[81][81];

const char * const Sq::tbl_sq_name[mode_size][ok_size] = {
  { "91", "81", "71", "61", "51", "41", "31", "21", "11",
    "92", "82", "72", "62", "52", "42", "32", "22", "12",
    "93", "83", "73", "63", "53", "43", "33", "23", "13",
    "94", "84", "74", "64", "54", "44", "34", "24", "14",
    "95", "85", "75", "65", "55", "45", "35", "25", "15",
    "96", "86", "76", "66", "56", "46", "36", "26", "16",
    "97", "87", "77", "67", "57", "47", "37", "27", "17",
    "98", "88", "78", "68", "58", "48", "38", "28", "18",
    "99", "89", "79", "69", "59", "49", "39", "29", "19" },
  { "9a", "8a", "7a", "6a", "5a", "4a", "3a", "2a", "1a",
    "9b", "8b", "7b", "6b", "5b", "4b", "3b", "2b", "1b",
    "9c", "8c", "7c", "6c", "5c", "4c", "3c", "2c", "1c",
    "9d", "8d", "7d", "6d", "5d", "4d", "3d", "2d", "1d",
    "9e", "8e", "7e", "6e", "5e", "4e", "3e", "2e", "1e",
    "9f", "8f", "7f", "6f", "5f", "4f", "3f", "2f", "1f",
    "9g", "8g", "7g", "6g", "5g", "4g", "3g", "2g", "1g",
    "9h", "8h", "7h", "6h", "5h", "4h", "3h", "2h", "1h",
    "9i", "8i", "7i", "6i", "5i", "4i", "3i", "2i", "1i" } };
  
constexpr Sq::Attacks Sq::tbl_sq_atk[Color::ok_size][81];
constexpr BMap Sq::tbl_sq_atk_king[81];

Sq::Sq(int ch1, int ch2, Mode mode) noexcept {
  int file, rank, i;

  i = ch1 - '1';
  if (0 <= i && i <= 8) file = 8 - i;
  else { _u = 81U; return; }

  i = ch2 - ((mode == usi) ? 'a' : '1');
  if (0 <= i && i <= 8) rank = i;
  else { _u = 81U; return; }

  _u = static_cast<uchar>(rank * 9 + file); }

constexpr unsigned char Pc::num_array[hand_size];
constexpr Action::Type Action::normal;
constexpr Action::Type Action::promotion;

FixLStr<7U> Action::to_str(SAux::Mode mode) const noexcept {
  assert(ok());
  FixLStr<7U> str;
  if (mode == csa) {
    if      (_type == resign)  str += resigned.to_str();
    else if (_type == windecl) str += windclrd.to_str();
    else {
      assert(is_move());
      Pc pc(_pc);
      if (_type == promotion) pc = pc.to_proPc();
      str += (_type == drop) ? "00" : _from.to_str(csa);
      str += _to.to_str(csa);
      str += pc.to_str(csa); } }
  else {
    assert(mode == usi);
    if      (_type == resign)  str += "resign";
    else if (_type == windecl) str += "win";
    else {
      assert(is_move());
      if (_type == drop) {
	str += _pc.to_str(usi);
	str += "*";
	str += _to.to_str(usi); }
      else {
	str += _from.to_str(usi);
	str += _to.to_str(usi);
	if (_type == promotion) str += "+"; } } }
  return str; }

bool Action::ok() const noexcept {
  if (is_notmove()) {
    if (_from.ok() || _to.ok() || _pc.ok() || _cap.ok()) return false; }
  else if (_type == normal) {
    if (!_pc.ok() || !_from.ok() || !_to.ok()) return false;
    if (_cap == king || _from == _to) return false; }
  else if (_type == promotion) {
    if (!_pc.can_promote() || !_from.ok() || !_to.ok()) return false;
    if (_cap == king || _from == _to ) return false; }
  else if (_type == drop) {
    if (_from.ok() || _cap.ok()) return false;
    if (!_to.ok() || !_pc.hand_ok()) return false; }
  else return false;
    
  return true; }

constexpr uint64_t
ZKey::tbl_zkey_hand[Color::ok_size][Pc::hand_size][Pc::npawn];
constexpr uint64_t ZKey::tbl_zkey_sq[Color::ok_size][Pc::ok_size][Sq::ok_size];

ZKey Board::make_zkey(const Color &turn) const noexcept {
  assert(turn.ok());
  ZKey zkey(0);
  if (turn == white) zkey.xor_turn();
  for (uint uc = 0; uc < Color::ok_size; ++uc)
    for (uint upc = 0; upc < Pc::hand_size; ++upc)
      for (uint u = 0; u < _hand[uc][upc]; ++u)
	zkey.xor_hand(Color(uc), Pc(upc), u);

  for (uint usq = 0; usq < Sq::ok_size; ++usq) {
    const Sq sq(usq);
    if (!get_c(sq).ok()) continue;
    zkey.xor_sq(get_c(sq), get_pc(sq), sq); }
  return zkey; }

FixLStr<512U> Board::to_str(const Color &turn) const noexcept {
  FixLStr<512U> str;
  for (int rank = 0; rank < 9; ++rank) {
    str += "P";
    str += rank;
    for (int file = 0; file < 9; ++file) {
      Sq sq(rank, file);
      if (!get_c(sq).ok()) str += " * ";
      else {
	str += get_c(sq).to_str();
	str += get_pc(sq).to_str(csa); } }
    str += "\n"; }
  
  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    str += "Hand";
    str += Color(uc).to_str();
    str += ":";
    for (uint upc = 0; upc < Pc::hand_size; ++upc) {
      str += " ";
      str += Pc(upc).to_str(csa);
      str += _hand[uc][upc]; }
    str += "\n"; }

  if (turn.ok()) str += turn.to_str();
  str += "\n";
  return str; }

bool Board::ok(const Color &turn) const noexcept {
  uint count[Color::ok_size][Pc::ok_size] = { 0 };
  uchar pawn_file[Color::ok_size][SAux::file_size] = { 0 };

  if (turn.ok() && _zkey != make_zkey(turn)) return false;
  for (uint uc = 0; uc < Color::ok_size; ++uc)
    for (uint upc = 0; upc < Pc::unpromo_size; ++upc)
      if (!_bm_sub_mobil[uc][upc].ok()) return false;
  for (uint uc = 0; uc < Color::ok_size; ++uc)
    if (!_bm_color[uc].ok()) return false;
  for (uint ray = 0; ray < SAux::ray_size; ++ray)
    if (!_bm_all[ray].ok()) return false;

  if (!_bm_color[ublack].is_disjoint(_bm_color[uwhite])) return false;
  if ((_bm_color[ublack] | _bm_color[uwhite])
      != _bm_all[ray_rank]) return false;
  
  // piece count on the board
  for (uint usq = 0; usq < Sq::ok_size; ++usq) {
    Color c  = _board[usq].c;
    Pc    pc = _board[usq].pc;
    Sq    sq = Sq(usq);
    uint uc  = c.to_u();
    if (c.ok() && !pc.ok()) return false;
    if (!c.ok() && pc.ok()) return false;
    if (!c.ok()) {
      for (uint r = 0; r < ray_size; ++r)
	if (!_bm_all[r].is_disjoint(sq.to_bmap(r))) return false;
      for (uint uc2 = 0; uc2 < 2U; ++uc2)
	for (uint u = 0; u < Pc::unpromo_size; ++u)
	  if (!_bm_sub_mobil[uc2][u].is_disjoint(sq.to_bmap())) return false;
      continue; }
    if (!can_exist(c, sq, pc)) return false;

    if (pc.is_unpromo()) {
      assert(pc.to_u() < Pc::unpromo_size);
      if (_bm_sub_mobil[uc][pc.to_u()].is_disjoint(sq.to_bmap()))
	return false;
      if (pc == pawn) {
	if (_bm_pawn_atk[uc].is_disjoint(sq.to_atk_pawn(c)))
	  return false;
	if (_pawn_file[uc][sq.to_file()] == 0) return false;
	if (pawn_file[uc][sq.to_file()] == 1U) return false;
	pawn_file[uc][sq.to_file()] = 1U; }
      else if (pc == king && _sq_kings[uc] != sq) return false; }
    else if (pc == horse) {
      if (_bm_horse[uc].is_disjoint(sq.to_bmap())) return false;
      if (_bm_sub_mobil[uc][ubishop].is_disjoint(sq.to_bmap())) return false;
      if (_bm_sub_mobil[uc][uking].is_disjoint(sq.to_bmap()))	return false; }
    else if (pc == dragon) {
      if (_bm_dragon[uc].is_disjoint(sq.to_bmap())) return false;
      if (_bm_sub_mobil[uc][urook].is_disjoint(sq.to_bmap())) return false;
      if (_bm_sub_mobil[uc][uking].is_disjoint(sq.to_bmap())) return false; }
    else {
      if (_bm_sub_mobil[uc][ugold].is_disjoint(sq.to_bmap())) return false; }
    
    for (uint r = 0; r < ray_size; ++r)
      if (_bm_all[r].is_disjoint(sq.to_bmap(r))) return false;
    count[uc][pc.to_u()] += 1U; }

  // test piece count
  if (_bm_pawn_atk[ublack].size() != count[ublack][upawn]) return false;
  if (_bm_pawn_atk[uwhite].size() != count[uwhite][upawn]) return false;
  if (_bm_horse[ublack].size()    != count[ublack][uhorse]) return false;
  if (_bm_horse[uwhite].size()    != count[uwhite][uhorse]) return false;
  if (_bm_dragon[ublack].size()   != count[ublack][udragon]) return false;
  if (_bm_dragon[uwhite].size()   != count[uwhite][udragon]) return false;

  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    if (1U < count[uc][uking]) return false;
    uint num = 0;
    for (uint upc = 0; upc < Pc::hand_size; ++upc) {
      count[uc][upc] += _hand[uc][upc];
      num            += _hand[uc][upc]; }
    if (num != _hand_color[uc]) return false; }
  
  for (uint upc = 0; upc < Pc::ok_size; ++upc)
    count[ublack][upc] += count[uwhite][upc];
  
  for (const Pc &pc : array_pc_canpromo)
    count[ublack][pc.to_u()] += count[ublack][pc.to_proPc().to_u()];
  
  for (uint upc = 0; upc < Pc::hand_size; ++upc)
    if (count[black.to_u()][upc] != Pc::num_array[upc]) return false;

  // test check
  if (turn.ok() && is_incheck(turn.to_opp())) return false;
  return true; }

bool Board::is_nyugyoku(const Color &turn, const uint handicap) const noexcept {
  assert(turn.ok() && _sq_kings[turn.to_u()].ok());
  if (sq94 <= _sq_kings[turn.to_u()].rel(turn)) return false;

  uint32_t bits_all = _bm_color[turn.to_u()].get_rel(turn, 0);
  uint nall = count_popu(bits_all);
  if (nall < 11U) return false;
  
  constexpr uint bonus[Color::ok_size] = { 0, 1 };
  uint32_t bits_big = _bm_sub_mobil[turn.to_u()][ubishop].get_rel(turn, 0);
  bits_big |= _bm_sub_mobil[turn.to_u()][urook].get_rel(turn, 0);
  bits_big &= bits_all;
  
  uint nbig = count_popu(bits_big);
  nbig += _hand[turn.to_u()][bishop.to_u()] + _hand[turn.to_u()][rook.to_u()];
  nall += _hand_color[turn.to_u()];
  // sente 28 ok, gote(uwate) 27 ok.
  // the removed pieces are counted towards uwate(White's) total. https://www.shogi.or.jp/faq/rules/
  int add = 0;
  if ( turn.to_u() == 1 ) {
    if ( handicap == 1 ) { add =  1; }	// ky
    if ( handicap == 2 ) { add =  5; }	// ka
    if ( handicap == 3 ) { add =  5; }	// hi
    if ( handicap == 4 ) { add = 10; }	// 2mai
    if ( handicap == 5 ) { add = 12; }	// 4mai
    if ( handicap == 6 ) { add = 14; }	// 6mai
  }
  if (nall + 4U * nbig + bonus[turn.to_u()] + add < 29U) return false;	// nall includes king.
  if (is_incheck(turn)) return false;
  return true; }

bool Board::action_ok_easy(const Color &turn, const Action &a) const noexcept {
  if (!turn.ok() && !a.ok()) return false;
  if (a.is_resign()) return true;
  if (a.is_windecl()) return true;
  
  Sq from  = a.get_from();
  Sq to    = a.get_to();
  Pc pc    = a.get_pc();
  Pc cap   = a.get_cap();
  Color tx = turn.to_opp();
  if (a.is_drop()) {
    assert(turn.ok());
    if (!to.ok() || !pc.hand_ok()) return false;
    if (!can_exist(turn, to, pc)) return false;
    if (_board[to.to_u()] != Value()) return false;
    if (_hand[turn.to_u()][pc.to_u()] == 0) return false;
    if (pc == pawn && _pawn_file[turn.to_u()][to.to_file()]) return false; }
  else {
    if (!a.is_normal() && !a.is_promotion()) return false;
    if (_board[from.to_u()] != Value(turn, pc)) return false;
    if (_board[to.to_u()].pc == king) {}
    if (cap.ok()) {
      if (_board[to.to_u()] != Value(tx, cap)) return false; }
    else if (_board[to.to_u()] != Value()) return false;
    if (a.is_normal()) {
      if (!can_exist(turn, to, pc)) return false; }
    else if (! can_promote(turn, from, to)) return false;
    if (pc.is_slider()
	&& ! from.to_obstacle(to).is_disjoint(_bm_all[ray_rank]))
      return false; }
  return true; }

bool Board::action_ok_full(const Color &turn, const Action &a, const uint handicap) noexcept {
  if (!action_ok_easy(turn, a)) return false;
  if (a.is_resign()) return true;
  if (a.is_windecl()) return is_nyugyoku(turn, handicap);


  assert(a.is_move());
  update(turn, a, false);
  bool b_incheck = is_incheck(turn);
  undo(turn, a, false);
  if (b_incheck) return false;
  if (is_mate_by_drop_pawn(turn, a)) return false;
  return true; }

bool Board::is_mate_by_drop_pawn(const Color &turn, const Action &action)
  noexcept {
  assert(turn.ok() && action.ok());
  if (!action.is_drop() || action.get_pc() != pawn) return false;

  Color tx = turn.to_opp();
  assert(turn.ok() && tx.ok());
  Sq sqk = _sq_kings[tx.to_u()];
  if (!sqk.ok()) return false;

  Sq to = action.get_to();
  assert(to.ok());
  if (sqk.rel(tx).to_u() != to.rel(tx).to_u() + 9U) return false;
  
  BMap bm;
  bm  = _bm_sub_mobil[tx.to_u()][uknight] & to.to_atk_knight(turn);
  bm |= _bm_sub_mobil[tx.to_u()][usilver] & to.to_atk_silver(turn);
  bm |= _bm_sub_mobil[tx.to_u()][ugold]   & to.to_atk_gold(turn);
  bm |= _bm_sub_mobil[tx.to_u()][ubishop] & to_atk_bishop(Color(), to);
  bm |= _bm_sub_mobil[tx.to_u()][urook]   & to_atk_rook(Color(), to);
  bm |= ( _bm_sub_mobil[tx.to_u()][uking].andnot(sqk.to_bmap())
	  & to.to_atk_king() );
  if (!bm.empty()) {
    Sq from(bm.pop_f());
    assert(from.ok());
    do {
      assert(_board[from.to_u()].c == tx);
      if (!is_pinned(tx, from, to)) return false;
      from = Sq(bm.pop_f());
    } while (from.ok()); }

  bool is_mate = true;
  toggle_ray_penetrate(to);
  bm = sqk.to_atk_king().andnot(_bm_color[tx.to_u()]);
  for (Sq sq(bm.pop_f()); sq.ok(); sq = Sq(bm.pop_f()))
    if (! is_attacked(tx, sq)) { is_mate = false;  break; }
  toggle_ray_penetrate(to);
  return is_mate; }

const BMap & Board::to_atk(const Sq &sq, uint ray) const noexcept {
  assert(sq.ok() && ray < ray_size);
#include "tbl_board.inc"
  uint bits = _bm_all[ray].to_bits(sq.to_u(), ray);
  return tbl_board_atk_slide[sq.to_u()][ray][bits]; }

BMap Board::to_attacker(const Color &c, const Sq &sq) const noexcept {
  assert(c.ok() && sq.ok());
  uint ucx      = c.to_opp().to_u();
  BMap atk_rook = to_atk_rook(Color(), sq);
  BMap atk_king = sq.to_atk_king();
  BMap bm = (_bm_sub_mobil[ucx][upawn] & sq.to_atk_pawn(c));
  bm |= _bm_sub_mobil[ucx][ulance]  & sq.to_atk_lance_j(c) & atk_rook;
  bm |= _bm_sub_mobil[ucx][uknight] & sq.to_atk_knight(c);
  bm |= _bm_sub_mobil[ucx][usilver] & sq.to_atk_silver(c);
  bm |= _bm_sub_mobil[ucx][ugold]   & sq.to_atk_gold(c);
  bm |= _bm_sub_mobil[ucx][uking]   & atk_king;    
  bm |= _bm_sub_mobil[ucx][ubishop] & to_atk_bishop(Color(), sq);
  bm |= _bm_sub_mobil[ucx][urook]   & atk_rook;
  return bm; }

bool Board::is_pinned(const Color &ck, const Sq &from, const Sq &to)
  const noexcept {
  assert(ck.ok() && from.ok() && to.ok());
  if (!_sq_kings[ck.to_u()].ok()) return false;

  uint ray = _sq_kings[ck.to_u()].to_ray(from);
  if (! ray_ok(ray)) return false;
  if (ray == _sq_kings[ck.to_u()].to_ray(to)) return false;
  
  const BMap & bm_ray  = to_atk(from, ray);
  const BMap & bm_king = _sq_kings[ck.to_u()].to_bmap();
  if (bm_ray.is_disjoint(bm_king)) return false;
  
  BMap bm_sub_mobil;
  Color cx = ck.to_opp();
  if (ray == ray_rank) bm_sub_mobil = _bm_sub_mobil[cx.to_u()][urook];
  else if (ray == ray_file) {
    bm_sub_mobil  = _bm_sub_mobil[cx.to_u()][ulance] & from.to_atk_lance_j(ck);
    bm_sub_mobil |= _bm_sub_mobil[cx.to_u()][urook]; }
  else bm_sub_mobil = _bm_sub_mobil[cx.to_u()][ubishop];
  
  return ! bm_ray.is_disjoint(bm_sub_mobil); }

void Board::toggle_ray_penetrate(const Sq &sq) noexcept {
  assert(sq.ok());
  for (uint u = 0; u < ray_size; ++u) _bm_all[u] ^= sq.to_bmap(u); }

void Board::place_sq(const Color &c, const Pc &pc, const Sq &sq, bool do_aux)
  noexcept {
  assert(c.ok() && pc.ok() && sq.ok() && !get_c(sq).ok());
  if (pc.is_unpromo()) {
    assert(pc.to_u() < Pc::unpromo_size);
    if (pc == pawn) {
      assert(can_exist(c, sq, pawn));
      assert(_pawn_file[c.to_u()][sq.to_file()] == 0);
      _bm_pawn_atk[c.to_u()] ^= sq.to_atk_pawn(c);
      _pawn_file[c.to_u()][sq.to_file()] = 1U; }
    else if (pc == king) _sq_kings[c.to_u()] = sq;
    _bm_sub_mobil[c.to_u()][pc.to_u()] ^= sq.to_bmap(); }
  else if (pc == horse) {
    _bm_horse[c.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][bishop.to_u()] ^= sq.to_bmap(); }
  else if (pc == dragon) {
    _bm_dragon[c.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][rook.to_u()] ^= sq.to_bmap(); }
  else { _bm_sub_mobil[c.to_u()][gold.to_u()] ^= sq.to_bmap(); }
  
  _bm_color[c.to_u()] ^= sq.to_bmap(0);
  for (uint r = 0; r < ray_size; ++r) _bm_all[r] ^= sq.to_bmap(r);
  _board[sq.to_u()] = Value(c, pc);
  if (do_aux) _zkey.xor_sq(c, pc, sq); }

void Board::remove_sq(const Sq &sq, bool do_aux) noexcept {
  assert(sq.ok());
  Color c = get_c(sq);
  Pc pc   = get_pc(sq);
  assert(c.ok() && pc.ok());
  if (pc.is_unpromo()) {
    assert(pc.to_u() < Pc::unpromo_size);
    if (pc == pawn) {
      assert(_pawn_file[c.to_u()][sq.to_file()] == 1U);
      _bm_pawn_atk[c.to_u()] ^= sq.to_atk_pawn(c);
      _pawn_file[c.to_u()][sq.to_file()] = 0; }
    _bm_sub_mobil[c.to_u()][pc.to_u()] ^= sq.to_bmap(); }
  else if (pc == horse) {
    _bm_horse[c.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][bishop.to_u()] ^= sq.to_bmap(); }
  else if (pc == dragon) {
    _bm_dragon[c.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_sub_mobil[c.to_u()][rook.to_u()] ^= sq.to_bmap(); }
  else { _bm_sub_mobil[c.to_u()][gold.to_u()] ^= sq.to_bmap(); }
  
  _bm_color[c.to_u()] ^= sq.to_bmap(0);
  for (uint r = 0; r < ray_size; ++r) _bm_all[r] ^= sq.to_bmap(r);
  if (do_aux) _zkey.xor_sq(c, pc, sq);
  _board[sq.to_u()] = Value(); }

void Board::place_hand(const Color &c, const Pc &pc, bool do_aux) noexcept {
  assert(c.ok() && pc.hand_ok());
  assert(_hand[c.to_u()][pc.to_u()] < Pc::num_array[pc.to_u()]);
  if (do_aux) {
    _zkey.xor_hand(c, pc, _hand[c.to_u()][pc.to_u()]);
    _hand_color[c.to_u()] += 1U; }
  _hand[c.to_u()][pc.to_u()] += 1U; }

void Board::remove_hand(const Color &c, const Pc &pc, bool do_aux) noexcept {
  assert(c.ok() && pc.hand_ok() && 0 < _hand[c.to_u()][pc.to_u()]);
  _hand[c.to_u()][pc.to_u()] -= 1U;
  if (do_aux) {
    assert(0 < _hand_color[c.to_u()]);
    _zkey.xor_hand(c, pc, _hand[c.to_u()][pc.to_u()]);
    _hand_color[c.to_u()] -= 1U; } }

void Board::update(const Color &turn, const Action &a, bool do_aux) noexcept {
  assert(turn.ok() && a.is_move());
  Sq to = a.get_to();
  Pc pc = a.get_pc();
  if (a.is_drop()) {
    remove_hand(turn, pc, do_aux);
    place_sq(turn, pc, to, do_aux); }
  else {
    Sq from = a.get_from();
    Pc cap  = a.get_cap();
    assert(cap != king);
    assert(get_value_wbt(from) == Value(turn, pc));
    if (cap.ok()) {
      place_hand(turn, cap.to_unproPc(), do_aux);
      remove_sq(to, do_aux); }
    if (a.is_promotion()) {
      assert(pc.can_promote());
      pc = pc.to_proPc(); }
    remove_sq(from, do_aux);
    place_sq(turn, pc, to, do_aux); }
  next_turn(do_aux); }

void Board::undo(const Color &turn, const Action &a, bool do_aux) noexcept {
  assert(turn.ok() && a.is_move());
  next_turn(do_aux);
  Sq to = a.get_to();
  Pc pc = a.get_pc();
  if (a.is_drop()) {
    remove_sq(to, do_aux);
    place_hand(turn, pc, do_aux); }
  else {
    Sq from = a.get_from();
    Pc cap  = a.get_cap();
    assert(cap != king);
    assert(get_value_wbt(to)
	   == Value(turn, (a.is_promotion() ? pc.to_proPc() : pc)));
    remove_sq(to, do_aux);
    place_sq(turn, pc, from, do_aux);
    if (cap.ok()) {
      place_sq(turn.to_opp(), cap, to, do_aux);
      remove_hand(turn, cap.to_unproPc(), do_aux); } } }

void Board::clear() noexcept {
  _sq_kings[ublack] = _sq_kings[uwhite] = Sq();
  _zkey = ZKey(0);
  fill_n(&_bm_sub_mobil[0][0], sizeof(_bm_sub_mobil) / sizeof(BMap), BMap());
  fill_n(_bm_pawn_atk, sizeof(_bm_pawn_atk) / sizeof(BMap), BMap());
  fill_n(_bm_horse, sizeof(_bm_horse) / sizeof(BMap), BMap());
  fill_n(_bm_dragon, sizeof(_bm_dragon) / sizeof(BMap), BMap());
  fill_n(_bm_all, sizeof(_bm_all) / sizeof(BMap), BMap());
  fill_n(_bm_color, sizeof(_bm_color) / sizeof(BMap), BMap());
  fill_n(_board, Sq::ok_size, Value());
  memset(_hand, 0, sizeof(_hand));
  memset(_pawn_file, 0, sizeof(_pawn_file));
  memset(_hand_color, 0, sizeof(_hand_color)); }

const char * const NodeType::tbl_nodetype_CSA[NodeType::ok_size] = {
  "", "TORYO", "KACHI", "SENNICHITE", "-ILLEGAL_ACTION",
  "+ILLEGAL_ACTION", "CHUDAN" };

template<uint N>
bool Node<N>::ok() const noexcept {
  if (!_turn.ok() || !_type.ok() || !_board.ok(_turn)) return false;
  if (N < _len_path) return false;
  if (_len_path == N && _type == interior) return false;
  return true; }

template<uint N>
void Node<N>::clear(int num_d) noexcept {
  _board.clear();
  _count_repeat = 0;
  _turn         = black;
  _node_handicap = num_d;
  assert(0 <= num_d && num_d < HANDICAP_TYPE);
  if ( num_d > 0 ) _turn = white;
  auto place = [&](const Color &c, const Pc &pc,
		   const Sq &sq){ _board.place_sq(c, pc,  sq.rel(c)); };
  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    Color c(uc);
    for (uint u = sq97.to_u(); u < sq98.to_u(); ++u) place(c, pawn, Sq(u));

    place(c, silver, sq79);  place(c, silver, sq39);
    place(c, gold,   sq69);  place(c, gold,   sq49);
    place(c, king,   sq59);
    if ( num_d == 0 || uc == 0 ) {
      place(c, knight, sq89);  place(c, knight, sq29);
      place(c, lance,  sq99);  place(c, lance,  sq19);
      place(c, bishop, sq88);  place(c, rook,   sq28);
      continue;
    }
    if ( num_d == 6 ) continue;
    place(c, knight, sq89);  place(c, knight, sq29);
    if ( num_d == 5 ) continue;
    if ( num_d == 1 ) {
                               place(c, lance,  sq19);
      place(c, bishop, sq88);  place(c, rook,   sq28);
      continue;
    }
    place(c, lance,  sq99);  place(c, lance,  sq19);
    if ( num_d == 4 ) continue;
    if ( num_d == 2 ) {
      place(c, rook,   sq28);
    }
    if ( num_d == 3 ) {
      place(c, bishop, sq88);
    }
  }
  
  _path[0]        = _board.get_zkey();
  _len_incheck[0] = 0;
  _len_incheck[1] = _board.is_incheck(_turn) ? 1U : 0;
  _len_path       = 0;
  _type           = interior;
  assert(ok()); }

template<uint N>
void Node<N>::take_action(const Action &a) noexcept {
  assert(a.ok() && _len_path < N && _type == interior);
  assert(_board.action_ok_full(_turn, a, _node_handicap));
  if (a.is_resign())  { _type = resigned; return; }
  if (a.is_windecl()) { _type = windclrd; return; }
  if (_len_path + 1U == N) { _type = maxlen_term; return; }
    
  Color t0 = _turn;
  _board.update(t0, a, true);
  _len_path += 1;
  _path[_len_path] = _board.get_zkey();
  _turn = _turn.to_opp();
  
  Color t1 = _turn;
  if (_board.is_incheck(t1))
    _len_incheck[_len_path + 1U] = _len_incheck[_len_path - 1U] + 1U;
  else _len_incheck[_len_path + 1U] = 0;

  _count_repeat = 0;
  for (uint len = _len_path; 2U <= len;) {
    len -= 2U;
    if (_path[len] != _path[_len_path]) continue;
    _count_repeat += 1U;
    if (_count_repeat < 3U) continue;
      
    uint nmove = (_len_path - len) / 2U;
    if (nmove <= _len_incheck[_len_path + 1U]) _type = illegal_win[t1.to_u()];
    else if (nmove <= _len_incheck[_len_path]) _type = illegal_win[t0.to_u()];
    else _type = repeated;
    return; }
    
  assert(ok()); }
  
template<uint N>
Action Node<N>::action_interpret(const char *cstr, SAux::Mode mode) noexcept {
  assert(ok() && cstr);
  if (_type.is_term()) return Action();
  Action action;
  
  if (mode == usi) {
    if      (strcmp(cstr, "resign") == 0) action = resign;
    else if (strcmp(cstr, "win")    == 0) action = windecl;
    else {
      if (isupper(cstr[0]) && cstr[1] == '*' && cstr[2] != '\0'
	  && cstr[3] != '\0' && cstr[4] == '\0') {
	Pc pc(cstr[0], '\0', usi);
	Sq to(cstr[2], cstr[3], usi);
	action = Action(to, pc); }
      else {
	if (cstr[0] == '\0' || cstr[1] == '\0'
	    || cstr[2] == '\0' || cstr[3] == '\0') return Action();
	Sq from(cstr[0], cstr[1], usi);
	Sq to(cstr[2], cstr[3], usi);
	if (!from.ok() || !to.ok()) return Action();
	Pc pc  = _board.get_pc(from);
	Pc cap = _board.get_pc(to);
	if (cstr[4] == '\0') action = Action(from, to, pc, cap,
					     Action::normal);
	else {
	  if (cstr[4] != '+' || cstr[5] != '\0') return Action();
	  action = Action(from, to, pc, cap, Action::promotion); } } } }
  else {
    assert(mode == csa);
    if      (strcmp(cstr, resigned.to_str()) == 0) action = resign;
    else if (strcmp(cstr, windclrd.to_str()) == 0) action = windecl;
    else {
      if (cstr[0] == '0' && cstr[1] == '0' && cstr[2] != '\0' && cstr[3] != '\0'
	  && cstr[4] != '\0' && cstr[5] != '\0' && cstr[6] == '\0') {
	Sq to(cstr[2], cstr[3], csa);
	Pc pc(cstr[4], cstr[5], csa);
	action = Action(to, pc); }
      else {
	if (cstr[0] == '\0' || cstr[1] == '\0'
	    || cstr[2] == '\0' || cstr[3] == '\0'
	    || cstr[4] == '\0' || cstr[5] == '\0') return Action();
	Sq from(cstr[0], cstr[1], csa);
	Sq to(cstr[2], cstr[3], csa);
	Pc pc1(cstr[4], cstr[5], csa);
	if (!from.ok() || !to.ok()) return Action();
	
	Pc pc  = _board.get_pc(from);
	Pc cap = _board.get_pc(to);
	if (pc == pc1) action = Action(from, to, pc, cap, Action::normal);
	else if (pc.to_proPc() == pc1)
	  action = Action(from, to, pc, cap, Action::promotion);
	else return Action(); } } }

  if (!_board.action_ok_full(_turn, action, _node_handicap)) return Action();
  return action; }

template class Node<Param::maxlen_play_learn>;
template class Node<Param::maxlen_play>;

template<uint N> template <uint UPCMobil>
void MoveSet<N>::add(const Color &turn, const Sq &from, const Sq &to,
		  const Pc &pc, const Pc &cap) noexcept {
  if (can_promote(turn, Pc(UPCMobil), from, to)) {
    assert(_uend < SAux::maxsize_moves);
    _moves[_uend++] = Action(from, to, pc, cap, Action::promotion); }

  if (can_exist(turn, to, Pc(UPCMobil))) {
    assert(_uend < SAux::maxsize_moves);
    _moves[_uend++] = Action(from, to, pc, cap, Action::normal); } }

template<uint N>
void MoveSet<N>::gen_pawn(const Board &board, const BMap &bm_target,
		       const Color &turn) noexcept {
  assert(bm_target.ok() && turn.ok());
  const Color tx = turn.to_opp();
  BMap bm = board.get_bm_pawn_atk(turn) & bm_target;
  for (Sq to(bm.pop_f()); to.ok(); to = Sq(bm.pop_f())) {
    Sq from(to.adv(tx));
    Pc cap(board.get_pc(to));
    if (board.is_pinned(turn, from, to)) continue;
    add<upawn>(turn, from, to, pawn, cap); } }

template<uint N>
void MoveSet<N>::gen_king(Node<N> &node, const Color &turn) noexcept {
  assert(turn.ok());
  const Board & board     = node.get_board();
  const BMap & bm_friends = board.get_bm_color(turn);
  const Sq sqk            = board.get_sq_king(turn);
  node.get_board().toggle_ray_penetrate(sqk);
  BMap bm = sqk.to_atk_king().andnot(bm_friends);
  for (Sq to(bm.pop_f()); to.ok(); to = Sq(bm.pop_f())) {
    if (board.is_attacked(turn, to)) continue;
    Pc cap(board.get_pc(to));
    assert(_uend + 1U < SAux::maxsize_moves);
    add<uking>(turn, sqk, to, king, cap); }
  node.get_board().toggle_ray_penetrate(sqk); }

template<uint N> template <uint UPCMobil>
void MoveSet<N>::gen_nsg(const Board &board, const Color &turn,
			 const BMap &bm_target,
			 function<BMap(const Sq &, const Color &)> fatk)
  noexcept {
  assert(turn.ok());
  BMap bm_from = board.get_bm_sub_mobil(turn, Pc(UPCMobil));
  for (Sq from(bm_from.pop_f()); from.ok(); from = Sq(bm_from.pop_f())) {
    Pc pc      = board.get_pc(from);
    BMap bm_to = fatk(from, turn) & bm_target;
    assert(pc.ok());
    for (Sq to(bm_to.pop_f()); to.ok(); to = Sq(bm_to.pop_f())) {
      Pc cap(board.get_pc(to));
      if (board.is_pinned(turn, from, to)) continue;
      add<UPCMobil>(turn, from, to, pc, cap); } } }

template<uint N> template <uint UPCMobil>
void MoveSet<N>::gen_slider(const Board &board, const Color &turn,
			    BMap bm_from, const BMap &bm_target,
			    function<BMap(const Board &, const Color &,
					  const Sq &)> fatk)
  noexcept {
  assert(turn.ok() && bm_from.ok() && bm_target.ok());
  for (Sq from(bm_from.pop_f()); from.ok(); from = Sq(bm_from.pop_f())) {
    Pc pc      = board.get_pc(from);
    BMap bm_to = fatk(board, turn, from) & bm_target;
    assert(pc.ok());
    for (Sq to(bm_to.pop_f()); to.ok(); to = Sq(bm_to.pop_f())) {
      Pc cap(board.get_pc(to));
      if (board.is_pinned(turn, from, to)) continue;
      add<UPCMobil>(turn, from, to, pc, cap); } } }


template<uint N>
void MoveSet<N>::gen_drop(Board &board, const Color &turn, BMap bm_target)
  noexcept {
  assert(turn.ok());
  Pc drops[Pc::hand_size];
  uint ndrop = 0;

  for (uint upc = 0; upc < Pc::hand_size; ++upc)
    if (board.get_num_hand(turn, Pc(upc))) drops[ndrop++] = Pc(upc);
  if (ndrop == 0) return;

  for (Sq to(bm_target.pop_f()); to.ok(); to = Sq(bm_target.pop_f()))
    for (uint u = 0; u < ndrop; ++u) {
      if (drops[u] == pawn) {
	if (!can_exist(turn, to, pawn)) continue;
	if (board.have_pawn(turn, to.to_file())) continue;
	if (board.is_mate_by_drop_pawn(turn, Action(to, pawn))) continue; }
      else if (drops[u] == lance) {
	if (!can_exist(turn, to, lance)) continue; }
      else if (drops[u] == knight) {
	if (!can_exist(turn, to, knight)) continue; }
      
      assert(_uend < SAux::maxsize_moves);
      _moves[_uend++] = Action(to, drops[u]); } }

template<uint N>
void MoveSet<N>::gen_all_evation(Node<N> &node) noexcept {
  const Color &turn = node.get_turn();
  Board &board      = node.get_board();
  const Sq &sqk     = board.get_sq_king(turn);
  gen_king(node, turn);

  BMap bm_attacker = board.to_attacker(turn, sqk);
  uint nchecker    = bm_attacker.size();
  if (nchecker == 2) return;

  assert(nchecker == 1);
  Sq sq_checker(bm_attacker.pop_f());
  const BMap &bm_obstacle = sqk.to_obstacle(sq_checker);
  const BMap bm_target    = (bm_obstacle | sq_checker.to_bmap());
  gen_pawn(board, bm_target, turn);
  gen_nsg<uknight>(board, turn, bm_target, &Sq::to_atk_knight);
  gen_nsg<usilver>(board, turn, bm_target, &Sq::to_atk_silver);
  gen_nsg<ugold>  (board, turn, bm_target, &Sq::to_atk_gold);
  gen_slider<ulance>(board, turn, board.get_bm_sub_mobil(turn, lance),
		     bm_target, &Board::to_atk_lance);
  gen_slider<ubishop>(board, turn, board.get_bm_bishop(turn), bm_target,
		      &Board::to_atk_bishop);
  gen_slider<uhorse>(board, turn, board.get_bm_horse(turn), bm_target,
		      &Board::to_atk_horse);
  gen_slider<urook>(board, turn, board.get_bm_rook(turn), bm_target,
		    &Board::to_atk_rook);
  gen_slider<udragon>(board, turn, board.get_bm_dragon(turn), bm_target,
		      &Board::to_atk_dragon);
  gen_drop(board, turn, bm_obstacle); }

template<uint N>
void MoveSet<N>::gen_all_no_evation(Node<N> &node) noexcept {
  const Color turn        = node.get_turn();
  Board &board            = node.get_board();
  const BMap  &bm_friends = board.get_bm_color(turn);
  const BMap  bm_target   = ~bm_friends;

  gen_king(node, turn);
  gen_pawn(board, bm_target, turn);
  gen_nsg<uknight>(board, turn, bm_target, &Sq::to_atk_knight);
  gen_nsg<usilver>(board, turn, bm_target, &Sq::to_atk_silver);
  gen_nsg<ugold>  (board, turn, bm_target, &Sq::to_atk_gold);
  gen_slider<ulance>(board, turn, board.get_bm_sub_mobil(turn, lance),
		     bm_target, &Board::to_atk_lance);
  gen_slider<ubishop>(board, turn, board.get_bm_bishop(turn), bm_target,
		      &Board::to_atk_bishop);
  gen_slider<uhorse>(board, turn, board.get_bm_horse(turn), bm_target,
		      &Board::to_atk_horse);
  gen_slider<urook>(board, turn, board.get_bm_rook(turn), bm_target,
		    &Board::to_atk_rook);
  gen_slider<udragon>(board, turn, board.get_bm_dragon(turn), bm_target,
		      &Board::to_atk_dragon);
  gen_drop(board, turn, ~board.get_bm_all()); }

template<uint N>
void MoveSet<N>::gen_all(Node<N> &node) noexcept {
  assert(node.ok() && node.get_type().is_interior());
  _uend = 0;
  if (node.is_incheck()) gen_all_evation(node);
  else                   gen_all_no_evation(node); }

template<uint N>
bool MoveSet<N>::ok() const noexcept {
  if (SAux::maxsize_moves <= _uend) return false;
  for (uint u = 0; u < _uend; ++u) if (!_moves[u].ok()) return false;
  return true; }

template class MoveSet<Param::maxlen_play>;
