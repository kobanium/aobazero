// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#if defined(USE_OPENCL)
#include "nnet.hpp"
#include "opencl.hpp"
#include <memory>
#include <utility>
#include <vector>
#include <cassert>

class FName;
class ManageSend {
  static constexpr char global_memory[] = "Global Memory";
  static constexpr char pinned_memory[] = "Pinned Memory";
  static constexpr char zero_copy[]     = "Zero Copy";
  double _time;
  OCL::Memory _mem_a, _mem_b;
  const char *_method;
  void *_ptr;

public:
  ManageSend() noexcept : _method(nullptr), _ptr(nullptr) {}
  void start(const OCL::Device &dev, const OCL::Queue &queue, size_t size)
    noexcept;
  void push(const OCL::Queue &queue, size_t size, const void *ptr) noexcept;
  void end(const OCL::Queue &queue) noexcept;
  const OCL::Memory &get() const noexcept { return _mem_a; }
  std::string gen_info() const noexcept;
};

class ManageSgemmBatch {
  using uint = unsigned int;
  struct Param {
    uint nl, npm, npn, npk;
    bool operator<=(const Param &p) {
      return (nl <= p.nl && npm <= p.npm && npn <= p.npn && npk <= p.npk); } };
  OCL::Kernel _ker;
  double _time;
  size_t size_g[3], size_l[3];
  Param _param;
  uint _nbatch, _nm, _nn, _nk;
public:
  ManageSgemmBatch() noexcept : _nbatch(0) {};
  void start(const OCL::Device &dev, const OCL::Queue &queue, bool transa,
	     bool transb, uint nbatch, uint nm0, uint nn0, uint nk0) noexcept;
  void push(const OCL::Queue &queue, const OCL::Memory &mem_a,
	    const OCL::Memory &mem_b, const OCL::Memory &mem_c) noexcept;
  std::string gen_info() const noexcept;
  uint get_nm() const noexcept { assert(0 < _nbatch); return _nm; }
  uint get_nn() const noexcept { assert(0 < _nbatch); return _nn; }
  uint get_nk() const noexcept { assert(0 < _nbatch); return _nk; }
};

class NNetOCL {
  using uint   = unsigned int;
  using ushort = unsigned short;
  using row_t  = std::unique_ptr<float []>;
  struct CLResWght { OCL::Memory matU, mean, sd_inv; };
  ManageSend _mng_send;
  ManageSgemmBatch _mng_sgemm_batch_input, _mng_sgemm_batch;
  OCL::Device _cl_dev;
  OCL::Queue _queue;
  OCL::Kernel _cl_compute_matV_input;
  OCL::Kernel _cl_compute_matV;
  OCL::Kernel _cl_compute_matA_BNReLU;
  OCL::Kernel _cl_compute_matA_BNReLU_join;
  OCL::Memory _cl_bypass, _cl_output, _cl_matM, _cl_matV;
  
  std::vector<CLResWght> _cl_reswghts;
  row_t _fslot[2];

  uint _maxsize_batch, _maxsize_out, _resnet_nout;
  uint _head1_nout, _policy1_nout, _value1_nout;
  row_t _head1_weight, _head1_mean, _head1_sd_inv;

  uint _policy2_nin;
  row_t _policy2_weight, _policy2_bias;
  
  uint _value2_nin, _value2_nout, _value3_nin, _value3_nout;
  row_t _value2_weight, _value2_bias, _value3_weight, _value3_bias;

  void load(const std::vector<std::pair<uint, row_t>> &wght) noexcept;

public:
  ~NNetOCL() noexcept;
  void reset(uint maxsize_batch,
	     const std::vector<std::pair<uint, row_t>> &wght,
	     int device_id) noexcept;
  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept;
};
#endif
