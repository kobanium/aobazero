// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#if defined(USE_OPENCL)
#include "nnet.hpp"
#include "opencl.hpp"
#include <memory>
#include <utility>
#include <vector>

class FName;
class NNetOCL {
  using uint   = unsigned int;
  using ushort = unsigned short;
  using row_t  = std::unique_ptr<float []>;
  struct CLResWght { OCL::Memory matU, mean, sd_inv; };
  OCL::Device _cl_dev;
  OCL::Kernel _cl_compute_matV_input;
  OCL::Kernel _cl_compute_matM_input;
  OCL::Kernel _cl_compute_matV;
  OCL::Kernel _cl_compute_matM;
  OCL::Kernel _cl_compute_matM_1x3;
  OCL::Kernel _cl_compute_matA_BNReLU;
  OCL::Kernel _cl_compute_matA_BNReLU_join;
  OCL::Memory _cl_input, _cl_bypass, _cl_output, _cl_matM, _cl_matV;
  std::vector<CLResWght> _cl_reswghts;
  row_t _fslot[2];

  uint _maxsize_batch, _maxsize_out;
  uint _conv3x3_nin_max, _conv3x3_nout_max;
  uint _resnet_nout;
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
	     int device_id) noexcept;
  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept;
};
#endif
