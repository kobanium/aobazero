#pragma once
#include "shogibase.hpp"
#include <algorithm>
#include <fstream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cblas.h>
using uint = unsigned int;

namespace NN {
  constexpr uint width          = 9U;
  constexpr uint height         = 9U;
  constexpr uint size_plane     = width * height;
  constexpr uint nch_input      = 362U;
  constexpr uint size_input     = size_plane * nch_input;
  constexpr uint nch_out_policy = 139U;
  unsigned short encode_nnmove(const Action &a, const Color &turn) noexcept;
}

template<uint Len>
class NodeNN : public Node<Len> {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct {
    BMap bm[Color::ok_size];
    uchar board[Sq::ok_size];
    uchar hand[Color::ok_size][Pc::hand_size];
    uint count_repeat;
  } _posi[Len];
  
  void store(float *p, uint uch, uint usq, float f = 1.0f) {
    assert(p && uch < NN::nch_input && usq < Sq::ok_size);
    p[uch * Sq::ok_size + usq] = f; };

  void store_plane(float *p, uint uch, float f = 1.0f) {
    assert(p && uch < NN::nch_input);
    std::fill_n(p + uch * Sq::ok_size, Sq::ok_size, f); };

  void set_posi() noexcept {
    uint len = Node<Len>::get_len_path();
    const Board &board = Node<Len>::get_board();
    const Color &turn  = Node<Len>::get_turn();

    for (uint uc = 0; uc < Color::ok_size; ++uc) {
      _posi[len].bm[uc] = board.get_bm_color(Color(uc));
      for (uint upc = 0; upc < Pc::hand_size; ++upc)
	_posi[len].hand[uc][upc] = board.get_num_hand(Color(uc), Pc(upc)); }
    
    for (uint nsq = 0; nsq < Sq::ok_size; ++nsq)
      _posi[len].board[nsq] = board.get_pc(Sq(nsq)).to_u();

    _posi[len].count_repeat = Node<Len>::get_count_repeat(); }
  
public:
  explicit NodeNN() noexcept { set_posi(); }
  void clear() noexcept { Node<Len>::clear(); set_posi(); }
  void take_action(const Action &a) noexcept {
    Node<Len>::take_action(a);
    if (Node<Len>::get_type().is_interior()) set_posi(); }
  
  void encode_input(float *p) noexcept {
    assert(p);
    std::fill_n(p, NN::size_input, 0.0);
    const Board &board = Node<Len>::get_board();
    const Color &turn  = Node<Len>::get_turn();
    uint len_path      = Node<Len>::get_len_path();
    
    uint ch_off = 0;
    static_assert(362U == NN::nch_input);
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
    store_plane(p, 361U, (1.0f / 512.0f) * static_cast<float>(len_path)); }
};

class IP {
  using row_t = std::unique_ptr<float []>;
  uint _nin, _nout;
  row_t _weight, _bias;
  
public:
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  uint get_nout() const noexcept { return _nout; }
  void ff(const float *fin, float *fout) const noexcept;
  void ff_relu(const float *fin, float *fout) const noexcept;
};

class Conv {
  using row_t = std::unique_ptr<float []>;
protected:
  uint _nin, _nout;
  row_t _weight, _bias;
  // _weight[_nout][_nin][kernel_h][kernel_w]
  
public:
  friend void preprocess(Conv &conv, class BNorm &bnorm) noexcept;
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  uint get_nout() const noexcept { return _nout; }
};

class Conv_1x1 : public Conv {
public:
  void ff(const float *fin, float *fout) const noexcept; };

#define F_3x3_3x3
#if defined(F_3x3_3x3)
class Conv_3x3 : public Conv {
  using uint = unsigned int;
  using row_t = std::unique_ptr<float []>;
  
  // F(3x3, 3x3) transform
  // _weight[_nout][_nin][_len_kernel^2]
  // _matrix_U[_len_tile_in][_len_tile_in][_nout][_nin]
  // _matrix_V[_len_tile_in][_len_tile_in][_nin][_ntile_h][_ntile_w]
  // _matrix_M[_len_tile_in][_len_tile_in][_nout][_ntile_h][_ntile_w]
  static constexpr uint _len_kernel   = 3U;
  static constexpr uint _size_kernel  = _len_kernel * _len_kernel;
  static constexpr uint _len_tile_out = 3U;
  static constexpr uint _len_tile_in  = 5U;
  static constexpr uint _size_tile_in = _len_tile_in * _len_tile_in;
  static constexpr uint _ntile_h      = 3U;
  static constexpr uint _ntile_w      = 3U;
  static constexpr uint _ntile        = _ntile_h * _ntile_w;
  row_t _matrix_U, _matrix_V, _matrix_M;
  
public:
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  void ff(const float *fin, float *fout) noexcept; };
#else
class Conv_3x3 : public Conv {
  using uint = unsigned int;
  using row_t = std::unique_ptr<float []>;
  static constexpr uint _len_kernel  = 3U;
  static constexpr uint _size_kernel = _len_kernel * _len_kernel;
  row_t _fcol;

public:
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  void ff(const float *fin, float *fout) noexcept; };
#endif

class BNorm {
  using row_t = std::unique_ptr<float []>;
protected:
  uint _nio;
  row_t _mean, _sd_inv;

public:
  friend void preprocess(Conv &conv, BNorm &bnorm) noexcept;
  void reset(uint nout, row_t &&mean, row_t &&sd) noexcept;
  uint get_nio() const noexcept { return _nio; }
  void ff_relu(float *fio) const noexcept;
  void ff_relu(float *fio, const float *bypass) const noexcept;
};

class FName;
class NNet {
  using row_t = std::unique_ptr<float []>;
  static constexpr float bn_factor = 1.0f / 999.982f;
  static constexpr float bn_eps    = 1e-5f;
  uint _version, _maxsize_out;
  uint64_t _digest;
  std::vector<std::pair<Conv_3x3, BNorm>> _body;
  Conv_1x1 _conv_head_plcy1, _conv_head_plcy2, _conv_head_vl1;
  BNorm _bn_head_plcy1, _bn_head_vl1;
  IP _head_vl2, _head_vl3;
  row_t fslot[3];

  void load(const FName &fwght) noexcept;

public:
  double get_elapsed() const noexcept;
  void reset(const FName &fwght) noexcept;
  float ff(const float *input, uint size_nnmove, ushort *nnmoves, float *prob)
    noexcept;
};
