// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "nnet.hpp"
#include <memory>
#include <utility>
#include <vector>
#if ! defined(USE_OPENBLAS) && ! defined(USE_MKL)
class NNetCPU : public NNet {
  using uint  = unsigned int;
  using row_t = std::unique_ptr<float []>;
public:
  void reset(uint, const std::vector<std::pair<uint, row_t>> &, int) noexcept;
};
#else
class NNetCPU : public NNet {
  using ushort = unsigned short;
  using uint   = unsigned int;
  using row_t  = std::unique_ptr<float []>;
  struct ResWght { row_t matU, mean, sd_inv; };
  row_t _fslot[3];

  int _thread_num;
  uint _maxsize_batch, _maxsize_out;
  uint _conv3x3_nin_max, _conv3x3_nout_max;
  uint _resnet_nout;
  row_t _matM, _matV;
  std::vector<ResWght> _reswghts;
  uint _head1_nout, _policy1_nout, _value1_nout;
  row_t _head1_weight, _head1_mean, _head1_sd_inv;

  uint _policy2_nin;
  row_t _policy2_weight, _policy2_bias;
  
  uint _value2_nin, _value2_nout, _value3_nin, _value3_nout;
  row_t _value2_weight, _value2_bias, _value3_weight, _value3_bias;
  
  void load(const std::vector<std::pair<uint, row_t>> &wght) noexcept;

public:
  void reset(uint maxsize_batch,
	     const std::vector<std::pair<uint, row_t>> &wght,
	     int thread_num) noexcept;
  uint push_ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	       const ushort *nnmoves, float *probs, float *values) noexcept;
  void wait_ff(uint) noexcept {};
  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept {
    push_ff(size_batch, input, sizes_nnmove, nnmoves, probs, values); }
};
#endif
