// 2019 Team AobaZero
// This source code is in the public domain.
#include "shogibase.hpp"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <climits>
#include <cstring>
#include <cstdint>
using std::fill_n;
using std::min;
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
constexpr unsigned char Sq::tbl_sq_rel[2][81];
constexpr unsigned char Sq::tbl_sq_ray[81][81];
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

constexpr uint64_t ZKey::tbl_zkey_hand[Color::ok_size][Pc::hand_size][Pc::npawn];
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
      if (!_bm_mobil[uc][upc].ok()) return false;
  for (uint uc = 0; uc < Color::ok_size; ++uc)
    if (!_bm_color[uc].ok()) return false;
  for (uint ray = 0; ray < SAux::ray_size; ++ray)
    if (!_bm_all[ray].ok()) return false;

  if (!_bm_color[black.to_u()].is_0(_bm_color[white.to_u()])) return false;
  if ((_bm_color[black.to_u()] | _bm_color[white.to_u()])
      != _bm_all[ray_rank]) return false;
  
  // piece count on the board
  for (uint usq = 0; usq < Sq::ok_size; ++usq) {
    Color c  = _board[usq].c;
    Pc    pc = _board[usq].pc;
    Sq    sq = Sq(usq);
    if (c.ok() && !pc.ok()) return false;
    if (!c.ok() && pc.ok()) return false;
    if (!c.ok()) {
      for (uint r = 0; r < ray_size; ++r)
	if (!_bm_all[r].is_0(sq.to_bmap(r))) return false;
      for (uint uc = 0; uc < 2U; ++uc)
	for (uint u = 0; u < Pc::unpromo_size; ++u)
	  if (!_bm_mobil[uc][u].is_0(sq.to_bmap())) return false;
      continue; }
    if (!can_exist(c, sq, pc)) return false;

    if (pc.is_unpromo()) {
      assert(pc.to_u() < Pc::unpromo_size);
      if (_bm_mobil[c.to_u()][pc.to_u()].is_0(sq.to_bmap())) return false;
      if (pc == pawn) {
	if (_pawn_file[c.to_u()][sq.to_file()] == 0) return false;
	if (pawn_file[c.to_u()][sq.to_file()] == 1U) return false;
	pawn_file[c.to_u()][sq.to_file()] = 1U; }
      else if (pc == king && _sq_kings[c.to_u()] != sq) return false; }
    else if (pc == horse) {
      if (_bm_mobil[c.to_u()][bishop.to_u()].is_0(sq.to_bmap())) return false;
      if (_bm_mobil[c.to_u()][king.to_u()].is_0(sq.to_bmap())) return false; }
    else if (pc == dragon) {
      if (_bm_mobil[c.to_u()][rook.to_u()].is_0(sq.to_bmap())) return false;
      if (_bm_mobil[c.to_u()][king.to_u()].is_0(sq.to_bmap())) return false; }
    else {
      if (_bm_mobil[c.to_u()][gold.to_u()].is_0(sq.to_bmap())) return false; }
    
    for (uint r = 0; r < ray_size; ++r)
      if (_bm_all[r].is_0(sq.to_bmap(r))) return false;
    count[c.to_u()][pc.to_u()] += 1U; }

  // test piece count
  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    if (1U < count[uc][king.to_u()]) return false;
    uint num = 0;
    for (uint upc = 0; upc < Pc::hand_size; ++upc) {
      count[uc][upc] += _hand[uc][upc];
      num            += _hand[uc][upc]; }
    if (num != _hand_color[uc]) return false; }
  
  for (uint upc = 0; upc < Pc::ok_size; ++upc)
    count[black.to_u()][upc] += count[white.to_u()][upc];
  
  for (const Pc &pc : array_pc_canpromo)
    count[black.to_u()][pc.to_u()] += count[black.to_u()][pc.to_proPc().to_u()];
  
  for (uint upc = 0; upc < Pc::hand_size; ++upc)
    if (count[black.to_u()][upc] != Pc::num_array[upc]) return false;

  // test check
  if (turn.ok() && is_incheck(turn.to_opp())) return false;
  return true; }

bool Board::is_nyugyoku(const Color &turn) const noexcept {
  assert(turn.ok() && _sq_kings[turn.to_u()].ok());
  if (sq94 <= _sq_kings[turn.to_u()].rel(turn)) return false;

  uint32_t bits_all = _bm_color[turn.to_u()].get_rel(turn, 0);
  uint nall = count_popu(bits_all);
  if (nall < 11U) return false;
  
  constexpr uint bonus[Color::ok_size] = { 0, 1 };
  uint32_t bits_big = _bm_mobil[turn.to_u()][bishop.to_u()].get_rel(turn, 0);
  bits_big |= _bm_mobil[turn.to_u()][rook.to_u()].get_rel(turn, 0);
  bits_big &= bits_all;
  
  uint nbig = count_popu(bits_big);
  nbig += _hand[turn.to_u()][bishop.to_u()] + _hand[turn.to_u()][rook.to_u()];
  nall += _hand_color[turn.to_u()];
  if (nall + 4U * nbig + bonus[turn.to_u()] < 29U) return false;
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
	&& ! from.to_obstacle(to).is_0(_bm_all[ray_rank]))
      return false; }
  return true; }

bool Board::action_ok_full(const Color &turn, const Action &a) noexcept {
  if (!action_ok_easy(turn, a)) return false;
  if (a.is_resign()) return true;
  if (a.is_windecl()) return is_nyugyoku(turn);

  bool is_ok = true;
  assert(a.is_move());
  update(turn, a, false);
  if (is_incheck(turn) || is_mate_by_drop_pawn(turn, a, false)) is_ok = false;
  undo(turn, a, false);
  return is_ok; }

bool Board::is_mate_by_drop_pawn(const Color &turn, const Action &a,
				 bool before) noexcept {
  assert(turn.ok() && a.ok());
  if (!a.is_drop() || a.get_pc() != pawn) return false;

  Color tx = turn.to_opp();
  assert(turn.ok() && tx.ok());
  Sq sq_king = _sq_kings[tx.to_u()];
  if (!sq_king.ok()) return false;

  Sq to = a.get_to();
  assert(to.ok());
  if (sq_king.rel(tx).to_u() != to.rel(tx).to_u() + 9U) return false;
  
  BMap bm;
  bm  = _bm_mobil[tx.to_u()][knight.to_u()] & to.to_atk_knight(turn);
  bm |= _bm_mobil[tx.to_u()][silver.to_u()] & to.to_atk_silver(turn);
  bm |= _bm_mobil[tx.to_u()][gold.to_u()]   & to.to_atk_gold(turn);
  bm |= _bm_mobil[tx.to_u()][bishop.to_u()] & to_atk_bishop(to);
  bm |= _bm_mobil[tx.to_u()][rook.to_u()]   & to_atk_rook(to);
  bm |= ( _bm_mobil[tx.to_u()][king.to_u()].andnot(sq_king.to_bmap())
	  & to.to_atk_king() );
  if (!bm.is_0()) {
    Sq from(bm.pop_front());
    assert(from.ok());
    do {
      assert(_board[from.to_u()].c == tx);
      if (!is_pinned(tx, from, to)) return false;
      from = Sq(bm.pop_front());
    } while (from.ok()); }

  bool is_mate = true;
  if (before)
    for (uint u = 0; u < ray_size; ++u) _bm_all[u] ^= to.to_bmap(u);

  bm = sq_king.to_atk_king().andnot(_bm_color[tx.to_u()]);
  for (Sq sq(bm.pop_front()); sq.ok(); sq = Sq(bm.pop_front())) {
    if (! is_attacked(tx, sq)) { is_mate = false;  break; } }

  if (before)
    for (uint u = 0; u < ray_size; ++u) _bm_all[u] ^= to.to_bmap(u);
  return is_mate; }

const BMap & Board::to_atk(const Sq &sq, uint ray) const noexcept {
  assert(sq.ok() && ray < ray_size);
#include "tbl_board.inc"
  uint bits = _bm_all[ray].to_bits(sq.to_u(), ray);
  return tbl_board_atk_slide[sq.to_u()][ray][bits]; }

BMap Board::to_attacker(const Color &c, const Sq &sq) const noexcept {
  assert(c.ok() && sq.ok());
  uint ucx      = c.to_opp().to_u();
  BMap atk_rook = to_atk_rook(sq);
  BMap atk_king = sq.to_atk_king();
  BMap bm;
  bm |= _bm_mobil[ucx][pawn.to_u()]   & sq.to_atk_lance_j(c) & atk_king;
  bm |= _bm_mobil[ucx][lance.to_u()]  & sq.to_atk_lance_j(c) & atk_rook;
  bm |= _bm_mobil[ucx][knight.to_u()] & sq.to_atk_knight(c);
  bm |= _bm_mobil[ucx][silver.to_u()] & sq.to_atk_silver(c);
  bm |= _bm_mobil[ucx][gold.to_u()]   & sq.to_atk_gold(c);
  bm |= _bm_mobil[ucx][king.to_u()]   & atk_king;    
  bm |= _bm_mobil[ucx][bishop.to_u()] & to_atk_bishop(sq);
  bm |= _bm_mobil[ucx][rook.to_u()]   & atk_rook;
  return bm; }

bool Board::is_pinned(const Color &ck, const Sq &from, const Sq &to)
  const noexcept {
  assert(ck.ok() && from.ok());
  if (!_sq_kings[ck.to_u()].ok()) return false;

  uint ray = _sq_kings[ck.to_u()].to_ray(from);
  if (ray == ray_size) return false;
  if (ray == _sq_kings[ck.to_u()].to_ray(to)) return false;
  
  const BMap & bm_ray  = to_atk(from, ray);
  const BMap & bm_king = _sq_kings[ck.to_u()].to_bmap();
  if (bm_ray.is_0(bm_king)) return false;
  
  BMap bm_mobil;
  Color cx = ck.to_opp();
  if (ray == ray_rank) bm_mobil = _bm_mobil[cx.to_u()][rook.to_u()];
  else if (ray == ray_file) {
    bm_mobil  = _bm_mobil[cx.to_u()][lance.to_u()] & from.to_atk_lance_j(ck);
    bm_mobil |= _bm_mobil[cx.to_u()][rook.to_u()]; }
  else bm_mobil = _bm_mobil[cx.to_u()][bishop.to_u()];
  
  return ! bm_ray.is_0(bm_mobil); }

void Board::place_sq(const Color &c, const Pc &pc, const Sq &sq, bool do_aux)
  noexcept {
  assert(c.ok() && pc.ok() && sq.ok() && !get_c(sq).ok());
  if (pc.is_unpromo()) {
    assert(pc.to_u() < Pc::unpromo_size);
    if (pc == pawn) {
      assert(can_exist(c, sq, pawn));
      assert(_pawn_file[c.to_u()][sq.to_file()] == 0);
      _pawn_file[c.to_u()][sq.to_file()] = 1U; }
    else if (pc == king) _sq_kings[c.to_u()] = sq;
    _bm_mobil[c.to_u()][pc.to_u()] ^= sq.to_bmap(); }
  else if (pc == horse) {
    _bm_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_mobil[c.to_u()][bishop.to_u()] ^= sq.to_bmap(); }
  else if (pc == dragon) {
    _bm_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_mobil[c.to_u()][rook.to_u()] ^= sq.to_bmap(); }
  else { _bm_mobil[c.to_u()][gold.to_u()] ^= sq.to_bmap(); }
  
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
      _pawn_file[c.to_u()][sq.to_file()] = 0; }
    _bm_mobil[c.to_u()][pc.to_u()] ^= sq.to_bmap(); }
  else if (pc == horse) {
    _bm_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_mobil[c.to_u()][bishop.to_u()] ^= sq.to_bmap(); }
  else if (pc == dragon) {
    _bm_mobil[c.to_u()][king.to_u()] ^= sq.to_bmap();
    _bm_mobil[c.to_u()][rook.to_u()] ^= sq.to_bmap(); }
  else { _bm_mobil[c.to_u()][gold.to_u()] ^= sq.to_bmap(); }
  
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
  _sq_kings[0] = _sq_kings[1] = Sq();
  _zkey  = ZKey(0);
  fill_n(&_bm_mobil[0][0], sizeof(_bm_mobil) / sizeof(BMap), BMap());
  fill_n(_bm_all, sizeof(_bm_all) / sizeof(BMap), BMap());
  fill_n(_bm_color, sizeof(_bm_color) / sizeof(BMap), BMap());
  fill_n(_board, Sq::ok_size, Value());
  memset(_hand, 0, sizeof(_hand));
  memset(_pawn_file, 0, sizeof(_pawn_file));
  memset(_hand_color, 0, sizeof(_hand_color)); }

const char * const NodeType::tbl_nodetype_CSA[NodeType::ok_size] = {
  "", "TORYO", "KACHI", "SENNICHITE", "-ILLEGAL_ACTION",
  "+ILLEGAL_ACTION", "CHUDAN" };

bool Node::ok() const noexcept {
  if (!_turn.ok() || !_type.ok() || !_board.ok(_turn)) return false;
  if (maxlen_path < _len_path) return false;
  if (_len_path == maxlen_path && _type == interior) return false;
  return true; }

void Node::clear() noexcept {
  _board.clear();
  _turn = black;
  auto place = [&](const Color &c, const Pc &pc,
		   const Sq &sq){ _board.place_sq(c, pc,  sq.rel(c)); };
  for (uint uc = 0; uc < Color::ok_size; ++uc) {
    Color c(uc);
    for (uint u = sq97.to_u(); u < sq98.to_u(); ++u) place(c, pawn, Sq(u));
    place(c, bishop, sq88);  place(c, rook,   sq28);
    place(c, lance,  sq99);  place(c, lance,  sq19);
    place(c, knight, sq89);  place(c, knight, sq29);
    place(c, silver, sq79);  place(c, silver, sq39);
    place(c, gold,   sq69);  place(c, gold,   sq49);
    place(c, king,   sq59); }
  
  _path[0]        = _board.get_zkey();
  _len_incheck[0] = 0;
  _len_incheck[1] = _board.is_incheck(_turn) ? 1U : 0;
  _len_path       = 0;
  _type           = interior;
  assert(ok()); }

void Node::take_action(const Action &a) noexcept {
  assert(a.ok() && _len_path < maxlen_path && _type == interior);
  assert(_board.action_ok_full(_turn, a));
  if (a.is_resign())  { _type = resigned; return; }
  if (a.is_windecl()) { _type = windclrd; return; }
  if (_len_path + 1U == maxlen_path) { _type = maxlen_term; return; }
    
  Color t0 = _turn;
  _board.update(t0, a, true);
  _len_path += 1;
  _path[_len_path] = _board.get_zkey();
  _turn = _turn.to_opp();
  
  Color t1 = _turn;
  if (_board.is_incheck(t1))
    _len_incheck[_len_path + 1U] = _len_incheck[_len_path - 1U] + 1U;
  else _len_incheck[_len_path + 1U] = 0;
    
  for (uint count = 0, len = _len_path; 2U <= len;) {
    len -= 2U;
    if (_path[len] != _path[_len_path]) continue;
    if (count < 2U) { count += 1; continue; }
      
    uint nmove = (_len_path - len) / 2U;
    if (nmove <= _len_incheck[_len_path + 1U]) _type = illegal_win[t1.to_u()];
    else if (nmove <= _len_incheck[_len_path]) _type = illegal_win[t0.to_u()];
    else _type = repeated;
    return; }
    
  assert(ok()); }
  
Action Node::action_interpret(const char *cstr, SAux::Mode mode) noexcept {
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
	if (cstr[4] == '\0') action = Action(from, to, pc, cap, Action::normal);
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

  if (!_board.action_ok_full(_turn, action)) return Action();
  return action; }
