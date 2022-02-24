// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "flstr.hpp"
#include <functional>
#include <iostream>
#include <iomanip>
#include <memory>
#include <cassert>
#include <cstdint>
#if defined(USE_SSE4)
#  include <emmintrin.h>
#  include <smmintrin.h>
#endif

namespace SAux {
  using uint = unsigned int;
  enum class Mode { CSA_MODE, USI_MODE };
  constexpr Mode csa = Mode::CSA_MODE, usi = Mode::USI_MODE;
  constexpr unsigned int mode_size     = 2U;
  constexpr unsigned int maxsize_moves = 1024U;
  constexpr unsigned int file_size = 9U, rank_size = 9U;
  constexpr unsigned int ray_rank  = 0U, ray_file  = 1U;
  constexpr unsigned int ray_diag1 = 2U, ray_diag2 = 3U, ray_size = 4U;
  constexpr bool ray_ok(uint u) { return u < ray_size; }
  constexpr bool file_ok(int i) { return 0 <= i && i < 9; }
  constexpr bool rank_ok(int i) { return 0 <= i && i < 9; }

#if defined(__GNUC__) && defined(__x86_64__)
  inline unsigned int count_popu(uint32_t bits) noexcept {
    return __builtin_popcount(bits); }

  inline bool count_lz(uint *out, uint bits) noexcept {
    assert(out);
    if (!bits) return false;
    *out = (__builtin_clz(bits) ^ 0x1fU);
    return true; }
#elif defined(_MSC_VER) && defined(_M_X64)
#  include <intrin.h>
  inline unsigned int count_popu(uint32_t bits) noexcept {
    return __popcnt(bits); }

  inline bool count_lz(uint *out, uint32_t bits) noexcept {
    assert(out);
    return _BitScanReverse(reinterpret_cast<unsigned long *>(out), bits); }
#else
  inline unsigned int count_popu(uint32_t bits) noexcept {
    unsigned int popu = 0;
    for (; bits; bits &= bits - 1U) popu += 1U;
    return popu; }

  inline bool count_lz(uint *out, uint32_t bits) noexcept {
    assert(out);
    constexpr unsigned char tbl[16] = { 0U, 1U, 2U, 2U, 3U, 3U, 3U, 3U,
                                        4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U };
    if (!bits) return false;
    uint32_t u;
    uint nbase = 0;
    u = bits >> 16; if (u) { nbase += 16U; bits = u; }
    u = bits >>  8; if (u) { nbase +=  8U; bits = u; }
    u = bits >>  4; if (u) { nbase +=  4U; bits = u; }
    *out = nbase - 1U + static_cast<uint>(tbl[bits]);
    return true; }
#endif
}

// 0:B 1:W 2:B
class Color {
  using uint  = unsigned int;
  using uchar = unsigned char;
  static const char * const tbl_color_CSA[2];
  uchar _u;
  
public:
  static constexpr uint all_size = 3U, ok_size  = 2U;
  explicit constexpr Color() noexcept : _u(2U) {}
  explicit constexpr Color(uint u) noexcept
  : _u(u < 2U ? static_cast<uchar>(u) : 2U) {}
  constexpr Color(const Color &c) noexcept : _u(c._u) {}
  constexpr bool operator==(const Color &c) const noexcept {
    return _u == c._u; }
  constexpr bool operator!=(const Color &c) const noexcept {
    return _u != c._u; }
  constexpr bool ok() const noexcept { return _u < 2U; }
  constexpr uint to_u() const noexcept { return _u; }
  const char *to_str() const noexcept {
    assert(ok()); return tbl_color_CSA[_u]; }
  constexpr Color to_opp() const noexcept { return Color(_u ^ 0x01U); }
};

namespace SAux {
  constexpr uint ublack(0U), uwhite(1U);
  constexpr Color black(0U), white(1U); }

class BMap {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct RBB { uchar i, s; };
  static constexpr uchar tbl_bmap_rel[Color::ok_size][3] = { {0U, 1U, 2U},
							     {2U, 1U, 0U} };
#include "tbl_bmap.inc"
#if defined(USE_SSE4)
  union { __m128i _u128; uint32_t _data[4]; };
#else
  uint32_t _data[3];
#endif

public:
  explicit constexpr BMap() noexcept : BMap(0,0,0) {}
  FixLStr<128U> to_str() const noexcept;
  constexpr uint32_t get(uint u) const noexcept { return _data[u]; }
  constexpr uint32_t get_rel(const Color &c, uint u) const noexcept {
    return get(tbl_bmap_rel[c.to_u()][u]); }
  constexpr uint to_bits(uint usq, uint ray) const noexcept {
    return ( _data[tbl_bmap_rbb[usq][ray].i]
	     >> tbl_bmap_rbb[usq][ray].s ) & 0x7fU; }
  uint size() const noexcept {
    return (SAux::count_popu(_data[0])
	    + SAux::count_popu(_data[1]) + SAux::count_popu(_data[2])); }

  uint pop_f() noexcept {
    uint count;
    if (SAux::count_lz(&count, _data[0])) {
      _data[0] ^= (1U << count); return 26U - count; }
    if (SAux::count_lz(&count, _data[1])) {
      _data[1] ^= (1U << count); return 53U - count; }
    if (SAux::count_lz(&count, _data[2])) {
      _data[2] ^= (1U << count); return 80U - count; }
    return 81U; }

#if defined(USE_SSE4)
  explicit constexpr BMap(uint32_t u0, uint32_t u1,
			  uint32_t u2) noexcept : _data{u0, u1, u2, 0} {}
  explicit constexpr BMap(const __m128i &m) noexcept : _u128(m) {}
  void clear() noexcept { _u128 = _mm_setzero_si128(); }
  void operator^=(const BMap &b) noexcept {
    _u128 = _mm_xor_si128(_u128, b._u128); }
  void operator|=(const BMap &b) noexcept {
    _u128 = _mm_or_si128(_u128, b._u128); }
  void operator&=(const BMap &b) noexcept {
    _u128 = _mm_and_si128(_u128, b._u128); }
  bool ok() const noexcept {
    return is_disjoint(BMap(_mm_setr_epi32(0xf8000000U, 0xf8000000U,
					   0xf8000000U, 0))); }
  BMap operator&(const BMap &b) const noexcept {
    return BMap(_mm_and_si128(_u128, b._u128)); }
  BMap operator|(const BMap &b) const noexcept {
    return BMap(_mm_or_si128(_u128, b._u128)); }
  BMap operator~() const noexcept {
    __m128i tmp = _mm_setr_epi32(0x07ffffffU, 0x07ffffffU, 0x07ffffffU, 0);
    return BMap(_mm_andnot_si128(_u128, tmp)); }
				 
  BMap andnot(const BMap &b) const noexcept {
    return BMap(_mm_andnot_si128(b._u128, _u128)); }
  /*
  uint32_t get_mm(uint u) const noexcept {
    assert(u < 3U);
    return _mm_extract_epi32(_u128, u); }
  */
  bool operator==(const BMap &b) const noexcept {
    __m128i tmp = _mm_xor_si128(_u128, b._u128);
    return _mm_test_all_zeros(tmp, tmp); }
  bool operator!=(const BMap &b) const noexcept {
    __m128i tmp = _mm_xor_si128(_u128, b._u128);
    return ! _mm_test_all_zeros(tmp, tmp); }
  bool empty() const noexcept { return _mm_test_all_zeros(_u128, _u128); }
  bool is_disjoint(const BMap &b) const noexcept {
    return _mm_test_all_zeros(_u128, b._u128); }
#else
  explicit constexpr BMap(uint32_t u0, uint32_t u1, uint32_t u2) noexcept
  : _data{u0, u1, u2} {}
  void clear() noexcept { _data[0] = _data[1] = _data[2] = 0; }
  void operator^=(const BMap &b) noexcept {
    _data[0] ^= b._data[0]; _data[1] ^= b._data[1]; _data[2] ^= b._data[2]; }
  void operator|=(const BMap &b) noexcept {
    _data[0] |= b._data[0]; _data[1] |= b._data[1]; _data[2] |= b._data[2]; }
  void operator&=(const BMap &b) noexcept {
    _data[0] &= b._data[0]; _data[1] &= b._data[1]; _data[2] &= b._data[2]; }
  /*constexpr uint32_t get_mm(uint u) const noexcept { return _data[u]; }*/
  bool ok() const noexcept {
    return is_disjoint(BMap(0xf8000000U, 0xf8000000U, 0xf8000000U)); }
  constexpr BMap operator&(const BMap &b) const noexcept {
    return BMap(_data[0] & b._data[0], _data[1] & b._data[1],
		_data[2] & b._data[2]); }
  constexpr BMap operator|(const BMap &b) const noexcept {
    return BMap(_data[0] | b._data[0], _data[1] | b._data[1],
		_data[2] | b._data[2]); }
  constexpr BMap operator~() const noexcept {
    return BMap(~_data[0] & 0x07ffffffU,
		~_data[1] & 0x07ffffffU,
		~_data[2] & 0x07ffffffU	); }
  constexpr BMap andnot(const BMap &b) const noexcept {
    return BMap(_data[0] & ~b._data[0], _data[1] & ~b._data[1],
		_data[2] & ~b._data[2]); }
  constexpr bool operator==(const BMap &b) const noexcept {
    return ( _data[0] == b._data[0] && _data[1] == b._data[1]
	     && _data[2] == b._data[2] ); }
  constexpr bool operator!=(const BMap &b) const noexcept {
    return ( _data[0] != b._data[0] || _data[1] != b._data[1]
	     || _data[2] != b._data[2] ); }
  constexpr bool empty() const noexcept {
    return _data[0] == 0 && _data[1] == 0 && _data[2] == 0; }
  constexpr bool is_disjoint(const BMap &b) const noexcept {
    return ( (_data[0] & b._data[0]) == 0 && (_data[1] & b._data[1]) == 0
	     && (_data[2] & b._data[2]) == 0 ); }
#endif
};

// 0-80:sq 81:bad
class Sq {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct Attacks { BMap silver, gold, knight, lance_j, pawn; };
#include "tbl_sq.inc"
  static const char * const tbl_sq_name[SAux::mode_size][81];
  uchar _u;

public:
  static constexpr uint all_size = 82U, ok_size = 81U;
  explicit constexpr Sq() noexcept : _u(81U) {}
  explicit constexpr Sq(uint u) noexcept : _u(static_cast<uchar>(u)) {}
  explicit constexpr Sq(int u) noexcept : Sq(static_cast<uint>(u)) {}
  constexpr Sq(const Sq &sq) noexcept : _u(sq._u) {}
  explicit constexpr Sq(int r, int f) noexcept :
  _u( (0 <= r && r < 9 && 0 <= f && f < 9)
      ? static_cast<uchar>(r*9 + f) : 81U ) {}
  explicit Sq(int ch1, int ch2, SAux::Mode mode) noexcept;
  
  constexpr bool operator==(const Sq &c) const noexcept { return _u == c._u; }
  constexpr bool operator!=(const Sq &c) const noexcept { return _u != c._u; }
  constexpr bool operator<=(const Sq &c) const noexcept { return _u <= c._u; }
  constexpr bool operator>=(const Sq &c) const noexcept { return _u >= c._u; }

  constexpr bool ok() const noexcept { return _u < 81U; }
  constexpr uint to_u() const noexcept { return _u; }
  constexpr Sq rel(const Color &c) const noexcept {
    return Sq(tbl_sq_rel[c.to_u()][_u]); }
  constexpr Sq adv(const Color &c) const noexcept {
    return Sq(tbl_sq_adv[c.to_u()][_u]); }
  constexpr int to_file() const noexcept {
    return static_cast<int>(_u % 9U); }
  constexpr int to_rank() const noexcept {
    return static_cast<int>(_u / 9U); }
  const char *to_str(const SAux::Mode &mode = SAux::csa) const noexcept {
    assert(ok()); return tbl_sq_name[static_cast<int>(mode)][_u]; }
  constexpr uint to_ray(const Sq &sq) const noexcept {
    return tbl_sq_ray[_u][sq.to_u()]; }
  constexpr uchar to_dir(const Sq &sq) const noexcept {
    return tbl_sq_dir[_u][sq.to_u()]; }
  constexpr uint to_distance(const Sq &sq) const noexcept {
    return tbl_sq_distance[_u][sq.to_u()]; }
  constexpr const BMap & to_obstacle(const Sq &sq) const noexcept {
    return tbl_sq_obstacle[_u][sq.to_u()]; }
  constexpr const BMap & to_bmap() const noexcept {
    return tbl_sq_u2bmap[_u][SAux::ray_rank]; }
  constexpr const BMap & to_bmap(uint ray) const noexcept {
    return tbl_sq_u2bmap[_u][ray]; }
  constexpr const BMap & to_atk_king() const noexcept {
    return tbl_sq_atk_king[_u]; }
  constexpr const BMap & to_atk_silver(const Color &c) const noexcept {
    return tbl_sq_atk[c.to_u()][_u].silver; }
  constexpr const BMap & to_atk_gold(const Color &c) const noexcept {
    return tbl_sq_atk[c.to_u()][_u].gold; }
  constexpr const BMap & to_atk_knight(const Color &c) const noexcept {
    return tbl_sq_atk[c.to_u()][_u].knight; }
  constexpr const BMap & to_atk_lance_j(const Color &c) const noexcept {
    return tbl_sq_atk[c.to_u()][_u].lance_j; }
  constexpr const BMap & to_atk_pawn(const Color &c) const noexcept {
    return tbl_sq_atk[c.to_u()][_u].pawn; }
};

namespace SAux {
  constexpr Sq sq91( 0), sq81( 1), sq71( 2), sq61( 3), sq51( 4);
  constexpr Sq sq41( 5), sq31( 6), sq21( 7), sq11( 8);
  constexpr Sq sq92( 9), sq82(10), sq72(11), sq62(12), sq52(13);
  constexpr Sq sq42(14), sq32(15), sq22(16), sq12(17);
  constexpr Sq sq93(18), sq83(19), sq73(20), sq63(21), sq53(22);
  constexpr Sq sq43(23), sq33(24), sq23(25), sq13(26);
  constexpr Sq sq94(27), sq84(28), sq74(29), sq64(30), sq54(31);
  constexpr Sq sq44(32), sq34(33), sq24(34), sq14(35);
  constexpr Sq sq95(36), sq85(37), sq75(38), sq65(39), sq55(40);
  constexpr Sq sq45(41), sq35(42), sq25(43), sq15(44);
  constexpr Sq sq96(45), sq86(46), sq76(47), sq66(48), sq56(49);
  constexpr Sq sq46(50), sq36(51), sq26(52), sq16(53);
  constexpr Sq sq97(54), sq87(55), sq77(56), sq67(57), sq57(58);
  constexpr Sq sq47(59), sq37(60), sq27(61), sq17(62);
  constexpr Sq sq98(63), sq88(64), sq78(65), sq68(66), sq58(67);
  constexpr Sq sq48(68), sq38(69), sq28(70), sq18(71);
  constexpr Sq sq99(72), sq89(73), sq79(74), sq69(75), sq59(76);
  constexpr Sq sq49(77), sq39(78), sq29(79), sq19(80); }

// 0:pawn   1:lance   2:knight 3:silver     4:gold        5:bishop
// 6:rook   7:king    8:tokin  9:pro_lance 10:pro_knight 11:pro_silver
//12:horse 13:dragon 14:bad
class Pc {
  using uint  = unsigned int;
  using uchar = unsigned char;
  static constexpr uchar tbl_promote[14] = {8U, 9U, 10U, 11U, 14U, 12U, 13U,
					    14U, 14U, 14U, 14U, 14U, 14U, 14U};
  static constexpr uchar tbl_unpromote[14] = {0U, 1U, 2U, 3U, 4U, 5U, 6U,
					      7U, 0U, 1U, 2U, 3U, 5U, 6U};
  static constexpr uchar tbl_slider[14] = {0U, 1U, 0U, 0U, 0U, 1U, 1U,
					   0U, 0U, 0U, 0U, 0U, 1U, 1U};
  static constexpr uchar tbl_point[14] = {1U, 1U, 1U, 1U, 1U, 5U, 5U,
					  0U, 1U, 1U, 1U, 1U, 5U, 5U};
  static const char * const tbl_pc_name[SAux::mode_size][14];
  uchar _u;

public:
  static constexpr uint all_size = 15U, ok_size = 14U, hand_size = 7U;
  static constexpr uint unpromo_size = 8U, npawn = 18U, npc = 40U;
  static constexpr uchar num_array[hand_size] = {18U, 4U, 4U, 4U, 4U, 2U, 2U};
  explicit constexpr Pc() noexcept : _u(14U) {}
  explicit constexpr Pc(uint u) noexcept :
  _u(u < 14U ? static_cast<uchar>(u) : 14U) {}
  constexpr Pc(const Pc &pc) noexcept : _u(pc._u) {}
  explicit Pc(int ch1, int ch2, SAux::Mode mode = SAux::csa) noexcept;
  
  constexpr bool operator==(const Pc &pc) const noexcept {
    return _u == pc._u; }
  constexpr bool operator!=(const Pc &pc) const noexcept {
    return _u != pc._u; }
  constexpr bool ok() const noexcept { return _u < 14U; }
  constexpr bool isnot_promo() const noexcept { return _u < 8U; }
  constexpr bool hand_ok() const noexcept { return _u < 7U; }
  constexpr bool is_pawn_lance() const noexcept { return _u < 2U; }
  constexpr bool is_promo() const noexcept { return 8U <= _u && _u < 14U; }
  constexpr bool is_unpromo() const noexcept { return _u < 8U; }
  constexpr bool is_slider() const noexcept { return tbl_slider[_u]; }
  constexpr bool can_promote() const noexcept { return _u < 7U && _u != 4U; }
  constexpr bool capt_ok() const noexcept { return _u < 14U && _u != 7U; }
  constexpr uchar to_u() const noexcept { return _u; }
  const char *to_str(const SAux::Mode &mode = SAux::csa) const noexcept {
    assert(ok()); return tbl_pc_name[static_cast<int>(mode)][_u]; }
  constexpr Pc to_proPc() const noexcept { return Pc(tbl_promote[_u]); }
  constexpr Pc to_unproPc() const noexcept { return Pc(tbl_unpromote[_u]); }
  constexpr uchar to_point() const noexcept { return tbl_point[_u]; }
};

namespace SAux {
  constexpr uint upawn(0U), ulance(1U), uknight(2U), usilver(3U), ugold(4U);
  constexpr uint ubishop(5U), urook(6U), uking(7U), utokin(8U);
  constexpr uint uhorse(12U), udragon(13U);
  constexpr Pc pawn(0U), lance(1U), knight(2U), silver(3U), gold(4U);
  constexpr Pc bishop(5U), rook(6U), king(7U), tokin(8U), pro_lance(9U);
  constexpr Pc pro_knight(10U), pro_silver(11U), horse(12U), dragon(13U);
  constexpr Pc array_pc_canpromo[] = { pawn, lance, knight, silver,
				       bishop, rook };
  constexpr bool can_exist(const Color &c, const Sq &sq, const Pc &pc)
    noexcept {
    return (pc.is_pawn_lance() ? (sq.rel(c) <= sq11 ? false : true)
	    : (pc == knight ? (sq.rel(c) <= sq12 ? false : true) : true)); }
  constexpr bool can_promote(const Color &c, const Sq &from, const Sq &to)
    noexcept { return from.rel(c) <= sq13 || to.rel(c) <= sq13; }
  constexpr bool can_promote(const Color &c, const Pc &pc, const Sq &from,
			     const Sq &to) noexcept {
    return pc.can_promote() && can_promote(c, from, to); }
}

class ZKey {
  using uint = unsigned int;
#include "tbl_zkey.inc"
  uint64_t _u64;
  
public:
  explicit ZKey() noexcept {}
  explicit constexpr ZKey(uint64_t u64) noexcept : _u64(u64) {}
  constexpr bool operator==(const ZKey &zkey) const noexcept {
    return _u64 == zkey._u64; }
  constexpr bool operator!=(const ZKey &zkey) const noexcept {
    return _u64 != zkey._u64; }

  constexpr uint64_t get() const noexcept { return _u64; }
  void xor_sq(const Color &c, const Pc &pc, const Sq &sq) noexcept {
    assert(c.ok() && pc.ok() && sq.ok() && SAux::can_exist(c, sq, pc));
    _u64 ^= tbl_zkey_sq[c.to_u()][pc.to_u()][sq.to_u()]; }

  void xor_hand(const Color &c, const Pc &pc, const uint &u) noexcept {
    assert(c.ok() && pc.ok() && u < Pc::num_array[pc.to_u()]);
    _u64 ^= tbl_zkey_hand[c.to_u()][pc.to_u()][u]; }

  void xor_turn() noexcept { _u64 ^= zkey_turn; }
};

class Action {
  using uchar = unsigned char;
  enum class Type : uchar { Normal, Promotion, Drop, Resign, WinDecl, Bad };
  Sq _from, _to;
  Pc _pc, _cap;
  Type _type;
  
public:
  static constexpr Type normal  = Type::Normal, promotion = Type::Promotion;
  static constexpr Type drop    = Type::Drop,   resign    = Type::Resign;
  static constexpr Type windecl = Type::WinDecl;
  explicit constexpr Action() noexcept :
  _from(Sq()), _to(Sq()), _pc(Pc()), _cap(Pc()), _type(Type::Bad) {}
  explicit constexpr Action(const Sq &from, const Sq &to, const Pc &pc,
			    const Pc &cap, const Type &type)
  noexcept : _from(from), _to(to), _pc(pc), _cap(cap), _type(type) {}
  explicit constexpr Action(const Sq &to, const Pc &pc) noexcept :
  _from(Sq()), _to(to), _pc(pc), _cap(Pc()), _type(Type::Drop) {}
  explicit constexpr Action(const Type &type) noexcept :
  _from(Sq()), _to(Sq()), _pc(Pc()), _cap(Pc()), _type(type) {}

  constexpr bool operator==(const Action &a) const noexcept {
    return ( _from == a._from && _to == a._to && _pc == a._pc
	     && _cap == a._cap && _type == a._type ); }
  constexpr bool operator!=(const Action &a) const noexcept {
    return ! operator==(a); }
  constexpr bool is_normal() const noexcept { return _type == Type::Normal; }
  constexpr bool is_promotion() const noexcept {
    return _type == Type::Promotion; }
  constexpr bool is_drop() const noexcept { return _type == Type::Drop; }
  constexpr bool is_resign() const noexcept { return _type == Type::Resign; }
  constexpr bool is_windecl() const noexcept { return _type == Type::WinDecl; }
  constexpr bool is_move() const noexcept {
    return static_cast<uchar>(_type) <= static_cast<uchar>(Type::Drop); }
  constexpr bool is_notmove() const noexcept {
    return (_type == Type::WinDecl || _type == Type::Resign); }
  constexpr Type get_type() const noexcept { return _type; }
  constexpr Sq get_from() const noexcept { return _from; }
  constexpr Sq get_to() const noexcept { return _to; }
  constexpr Pc get_pc() const noexcept { return _pc; }
  constexpr Pc get_cap() const noexcept { return _cap; }
  bool ok() const noexcept;

  FixLStr<7U> to_str(SAux::Mode mode = SAux::csa) const noexcept;
};

namespace SAux {
  constexpr Action resign(Action::resign), windecl(Action::windecl);
}

class Board {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct Value {
    Color c; Pc pc;
    explicit constexpr Value() noexcept : c(Color()), pc(Pc()) {}
    explicit constexpr Value(const Color &c_, const Pc &pc_) noexcept
    : c(c_), pc(pc_) {}
    constexpr bool operator==(const Value &v) const noexcept {
      return c == v.c && pc == v.pc; }
    constexpr bool operator!=(const Value &v) const noexcept {
      return c != v.c || pc != v.pc; } };
  ZKey _zkey;
  BMap _bm_pawn_atk[Color::ok_size];
  BMap _bm_sub_mobil[Color::ok_size][Pc::unpromo_size];
  BMap _bm_horse[Color::ok_size];
  BMap _bm_dragon[Color::ok_size];
  BMap _bm_color[Color::ok_size];
  BMap _bm_all[SAux::ray_size];
  Sq _sq_kings[Color::ok_size];
  Value _board[Sq::ok_size];
  uchar _hand[Color::ok_size][Pc::hand_size];
  uchar _pawn_file[Color::ok_size][SAux::file_size];
  uchar _hand_color[Color::ok_size];
  ZKey make_zkey(const Color &turn) const noexcept;
  const BMap & to_atk(const Sq &sq, uint ray) const noexcept;
  Value get_value_wbt(const Sq &sq) const noexcept {
    return sq.ok() ? _board[sq.to_u()] : Value(); }

public:
  explicit Board() noexcept { clear(); }

  FixLStr<512U> to_str(const Color &turn) const noexcept;
  bool ok(const Color &turn) const noexcept;
  bool action_ok_easy(const Color &turn, const Action &a) const noexcept;
  bool is_nyugyoku(const Color &turn, const uint hadicap) const noexcept;
  BMap to_attacker(const Color &c, const Sq &sq) const noexcept;
  bool is_pinned(const Color &ck, const Sq &from, const Sq &to) const noexcept;
  ZKey get_zkey() const noexcept { return _zkey; }
  Pc get_pc(const Sq &sq) const noexcept {
    assert(sq.ok()); return _board[sq.to_u()].pc; }
  Color get_c(const Sq &sq) const noexcept {
    assert(sq.ok()); return _board[sq.to_u()].c; }
  bool is_attacked(const Color &c, const Sq &sq) const noexcept {
    assert(c.ok() && sq.ok()); return ! to_attacker(c, sq).empty(); }
  bool is_incheck(const Color &c) const noexcept {
    assert(c.ok());
    return _sq_kings[c.to_u()].ok() && is_attacked(c, _sq_kings[c.to_u()]); }
  Sq get_sq_king(const Color &c) const noexcept {
    assert(c.ok()); return _sq_kings[c.to_u()]; }
  BMap to_atk_lance(const Color &c, const Sq &sq) const noexcept {
    return sq.to_atk_lance_j(c) & to_atk(sq, SAux::ray_file); }
  BMap to_atk_bishop(const Color &, const Sq &sq) const noexcept {
    return to_atk(sq, SAux::ray_diag1) | to_atk(sq, SAux::ray_diag2); }
  BMap to_atk_horse(const Color &, const Sq &sq) const noexcept {
    return to_atk_bishop(Color(), sq) | sq.to_atk_king(); }
  BMap to_atk_rook(const Color &, const Sq &sq) const noexcept {
    return to_atk(sq, SAux::ray_rank) | to_atk(sq, SAux::ray_file); }
  BMap to_atk_dragon(const Color &, const Sq &sq) const noexcept {
    return to_atk_rook(Color(), sq) | sq.to_atk_king(); }
  const BMap & get_bm_pawn_atk(const Color &c) const noexcept {
    assert(c.ok()); return _bm_pawn_atk[c.to_u()]; }
  const BMap & get_bm_all() const noexcept { return _bm_all[0]; }
  const BMap & get_bm_color(const Color &c) const noexcept {
    assert(c.ok()); return _bm_color[c.to_u()]; }
  const BMap & get_bm_sub_mobil(const Color &c, const Pc &pc) const noexcept {
    assert(c.ok() && pc.is_unpromo());
    return _bm_sub_mobil[c.to_u()][pc.to_u()]; }
  BMap get_bm_rook(const Color &c) const noexcept {
    assert(c.ok()); uint uc = c.to_u();
    return _bm_sub_mobil[uc][SAux::urook].andnot(_bm_dragon[uc]); }
  BMap get_bm_bishop(const Color &c) const noexcept {
    assert(c.ok()); uint uc = c.to_u();
    return _bm_sub_mobil[uc][SAux::ubishop].andnot(_bm_horse[uc]); }
  const BMap & get_bm_horse(const Color &c) const noexcept {
    assert(c.ok()); return _bm_horse[c.to_u()]; }
  const BMap & get_bm_dragon(const Color &c) const noexcept {
    assert(c.ok()); return _bm_dragon[c.to_u()]; }
  uchar get_num_hand(const Color &c, const Pc &pc) const noexcept {
    assert(c.ok() && pc.hand_ok()); return _hand[c.to_u()][pc.to_u()]; }
  bool have_pawn(const Color &c, int file) const noexcept {
    return 0 < _pawn_file[c.to_u()][file]; }
  
  void clear() noexcept;
  void toggle_ray_penetrate(const Sq &sq) noexcept;
  bool action_ok_full(const Color &turn, const Action &a, const uint handicap) noexcept;
  void place_sq(const Color &c, const Pc &pc, const Sq &sq,
		bool do_aux = true) noexcept;
  void remove_sq(const Sq &sq, bool do_aux = true) noexcept;
  void place_hand(const Color &c, const Pc &pc, bool do_aux = true) noexcept;
  void remove_hand(const Color &c, const Pc &pc, bool do_aux = true) noexcept;
  void update(const Color &turn, const Action &a, bool do_aux) noexcept;
  void undo(const Color &turn, const Action &a, bool do_aux) noexcept;
  bool is_mate_by_drop_pawn(const Color &turn, const Action &action) noexcept;
  void set_zkey(const ZKey &zkey) noexcept { _zkey = zkey; }
  void next_turn(bool do_aux = true) noexcept { if (do_aux) _zkey.xor_turn(); }
};

// 0:interior 1:resigned 2:windecl 3:repetition 4:illegal_bwin
// 5:illegal_wwin 6:maxlen_term 7:bad
class NodeType {
  using uint  = unsigned int;
  using uchar = unsigned char;
  static const char * const tbl_nodetype_CSA[7];
  uchar _u;

public:
  static constexpr uint ok_size = 7U;
  explicit constexpr NodeType() noexcept : _u(7U) {}
  explicit constexpr NodeType(uint u) noexcept : _u(static_cast<uchar>(u)) {}

  constexpr bool operator==(const NodeType &t) const noexcept {
    return _u == t._u; }
  constexpr bool operator!=(const NodeType &t) const noexcept {
    return _u != t._u; }
  constexpr bool ok() const noexcept { return _u < 7U; }
  constexpr uint to_u() const noexcept { return _u; }
  constexpr bool is_term() const noexcept { return 0 < _u && _u < 7U; }
  constexpr bool is_interior() const noexcept { return _u == 0; }
  const char *to_str() const noexcept { return tbl_nodetype_CSA[_u]; }
};

namespace SAux {
  constexpr NodeType interior(0), resigned(1), windclrd(2), repeated(3);
  constexpr NodeType illegal_bwin(4), illegal_wwin(5), maxlen_term(6);
  constexpr NodeType illegal_win[2] = { illegal_bwin, illegal_wwin };
}

template<unsigned int N>
class Node {
  using uint   = unsigned int;
  using ushort = unsigned short;
  using uchar  = unsigned char;
  ZKey _path[N];
  Board _board;
  ushort _len_incheck[N + 1U];
  ushort _len_path;
  uchar _count_repeat;
  Color _turn;
  NodeType _type;
  uint _node_handicap;

public:
  explicit Node() noexcept { clear(); }
  const NodeType &get_type() const noexcept { return _type; }
  bool is_incheck() const noexcept { return _len_incheck[_len_path + 1U]; }
  bool is_nyugyoku() const noexcept { return _board.is_nyugyoku(_turn, _node_handicap); }
  uint get_len_path() const noexcept { return _len_path; }
  Color get_turn() const noexcept { return _turn; }
  uint get_count_repeat() const noexcept { return _count_repeat; }
  uint get_node_handicap() const noexcept { return _node_handicap; }
  const Board &get_board() const noexcept { return _board; }
  FixLStr<512U> to_str() const noexcept { return _board.to_str(_turn); }
  bool ok() const noexcept;
  
  void clear(int num_d = 0) noexcept;
  Board &get_board() noexcept { return _board; }
  void take_action(const Action &a) noexcept;
  Action action_interpret(const char *cstr,
			  SAux::Mode mode = SAux::csa) noexcept;
};

template<unsigned int N>
class MoveSet {
  using uint   = unsigned int;
  using ushort = unsigned short;
  Action _moves[SAux::maxsize_moves];
  ushort _uend;
  
  template <uint UPCMobil>
  void add(const Color &turn, const Sq &from, const Sq &to, const Pc &pc,
	   const Pc &cap) noexcept;
  void gen_pawn(const Board &board, const BMap &bm_target, const Color &turn)
    noexcept;
  void gen_king(Node<N> &node, const Color &turn) noexcept;
  template <uint UPCMobil>
  void gen_nsg(const Board &board, const Color &turn, const BMap &bm_target,
	       std::function<BMap(const Sq &, const Color &)> fatk) noexcept;
  template <uint UPCMobil>
  void gen_slider(const Board &board, const Color &turn, BMap bm_from,
		  const BMap &bm_target,
		  std::function<BMap(const Board &, const Color &,
				     const Sq &)> fatk) noexcept;
  void gen_drop(Board &board, const Color &turn, BMap bm_target) noexcept;
  void gen_all_evation(Node<N> &node) noexcept;
  void gen_all_no_evation(Node<N> &node) noexcept;
  
public:
  explicit MoveSet() noexcept {}
  void gen_all(Node<N> &node) noexcept;

  const Action & operator[](uint u) const noexcept {
    assert(u < _uend); return _moves[u]; }
  uint size() const noexcept { return _uend; }
  bool ok() const noexcept;
};
