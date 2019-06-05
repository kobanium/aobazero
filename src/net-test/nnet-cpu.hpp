// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "nnet.hpp"
#include <memory>
#include <utility>
#include <vector>

class IP {
  using row_t = std::unique_ptr<float []>;
  using uint = unsigned int;
  uint _nin, _nout;
  row_t _weight, _bias;
  
public:
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  bool ok() const noexcept {
    return (0 < _nin && 0 < _nout && _weight && _bias); }
  uint get_nout() const noexcept { return _nout; }
  void ff(uint size_batch, const float *fin, float *fout) const noexcept;
  void ff_relu(uint size_batch, const float *fin, float *fout) const noexcept;
};

class Conv {
  using row_t = std::unique_ptr<float []>;
  using uint = unsigned int;
protected:
  uint _nin, _nout;
  row_t _weight, _bias;
  // _weight[_nout][_nin][kernel_h][kernel_w]
  
public:
  friend void preprocess(Conv &conv, class BNorm &bnorm) noexcept;
  void reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept;
  bool ok() const noexcept {
    return (0 < _nin && 0 < _nout && _weight && _bias); }
  uint get_nout() const noexcept { return _nout; }
};

class Conv_1x1 : public Conv {
  using uint = unsigned int;
public:
  void ff(uint size_batch, const float *fin, float *fout) const noexcept; };

#define F_3x3_3x3
#if defined(F_3x3_3x3)
// F(3x3, 3x3) transform
class Conv_3x3 : public Conv {
  using uint = unsigned int;
  using row_t = std::unique_ptr<float []>;
  
  // _weight[_nout][_nin][_size_kernel]
  // _matrix_U[_size_tile_in][_nout][_nin]
  // _matrix_V[_size_tile_in][_nin][size_batch][_ntile]
  // _matrix_M[_size_tile_in][_nout][size_batch][_ntile]
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
  void reset(uint maxsize_batch, uint nin, uint nout,
	     row_t &&weight, row_t &&bias) noexcept;
  void ff(uint size_batch, const float *fin, float *fout) noexcept; };
#else
// Im2col transform
class Conv_3x3 : public Conv {
  using uint = unsigned int;
  using row_t = std::unique_ptr<float []>;
  static constexpr uint _len_kernel  = 3U;
  static constexpr uint _size_kernel = _len_kernel * _len_kernel;
  row_t _fcol;

public:
  void reset(uint maxsize_batch, uint nin, uint nout,
	     row_t &&weight, row_t &&bias) noexcept;
  void ff(uint size_batch, const float *fin, float *fout) noexcept; };
#endif

class BNorm {
  using row_t = std::unique_ptr<float []>;
  using uint = unsigned int;
protected:
  uint _nio;
  row_t _mean, _sd_inv;

public:
  friend void preprocess(Conv &conv, BNorm &bnorm) noexcept;
  void reset(uint nout, row_t &&mean, row_t &&sd) noexcept;
  bool ok() const noexcept { return (0 < _nio && _mean && _sd_inv); }
  uint get_nio() const noexcept { return _nio; }
  void ff_relu(uint size_batch, float *fio) const noexcept;
  void ff_relu(uint size_batch, float *fio, const float *bypass)
    const noexcept;
};

class FName;
class NNet {
  using uint = unsigned int;
  using ushort = unsigned short;
  using row_t = std::unique_ptr<float []>;
  static constexpr float bn_factor = 1.0f / 999.982f;
  static constexpr float bn_eps    = 1e-5f;
  uint _version, _maxsize_out, _maxsize_batch;
  uint64_t _digest;
  std::vector<std::pair<Conv_3x3, BNorm>> _body;
  Conv_1x1 _conv_head_plcy1, _conv_head_plcy2, _conv_head_vl1;
  BNorm _bn_head_plcy1, _bn_head_vl1;
  IP _head_vl2, _head_vl3;
  row_t _fslot[3];

  void load(const FName &fwght) noexcept;

public:
  void reset(uint maxsize_batch, const FName &fwght) noexcept;
  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept;
};
