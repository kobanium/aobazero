// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "nnet.hpp"
#include "opencli.hpp"
#include <string>
#include <utility>
#include <vector>
#if !defined(USE_OPENCL_AOBA)
class NNetOCL : public NNet {
  using uint  = unsigned int;
  using row_t = std::unique_ptr<float []>;
public:
  std::string reset(uint, const std::vector<std::pair<uint, row_t>> &,
		    int, bool, bool, bool, bool,
		    const char *dname_tune = nullptr) noexcept;
};
#else
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <cstdint>

class FName;
namespace NNAux {
  using uint = unsigned int;
  constexpr uint nslot = 4U;
}

class ManageDecode {
  using uint = unsigned int;
  size_t _zero_size;
  size_t _fill_size_g[3], _fill_size_l[3];
  OCL::Kernel _ker_zero_clear[NNAux::nslot];
  OCL::Kernel _ker_plane_fill[NNAux::nslot], _ker_set_one[NNAux::nslot];

public:
  void start(const OCL::Context &context,
	     const OCL::MemPinned mem_in[NNAux::nslot],
	     const OCL::Memory mem_out[NNAux::nslot], uint maxsize_batch)
    noexcept;
  void push(const OCL::Queue &queue, uint n_one, uint uslot) noexcept;
};

struct SgemmParam {
  using uint = unsigned int;
  double time;
  bool flag_read, do_half, do_wmma;
  uint nlm, nln, npm, npn, npk, ntm, ntn;
  SgemmParam() noexcept : flag_read(false) {}
  SgemmParam(bool do_half_, uint nlm_, uint nln_, uint npm_, uint npn_,
	     uint npk_) noexcept :
    time(0.0), flag_read(false), do_half(do_half_), do_wmma(false), nlm(nlm_),
    nln(nln_), npm(npm_), npn(npn_), npk(npk_) {}
  SgemmParam(bool do_half_, bool do_wmma_, uint nlm_, uint nln_, uint npm_,
	     uint npn_, uint npk_, uint ntm_, uint ntn_) noexcept :
    time(0.0), flag_read(false), do_half(do_half_), do_wmma(do_wmma_),
    nlm(nlm_), nln(nln_), npm(npm_), npn(npn_), npk(npk_), ntm(ntm_),
    ntn(ntn_) {}
  SgemmParam(bool do_half_, bool do_wmma_, const char *fname) noexcept;
    
  std::string gen_info(const char *newline = " ") const noexcept;
  bool operator<=(const SgemmParam &p) const noexcept {
    bool b = (nlm <= p.nlm && nln <= p.nln && npm <= p.npm && npn <= p.npn
	      && npk <= p.npk);
    if (do_wmma) b = (b && ntm <= p.ntm && ntn <= p.ntn);
    return b; }
  bool ok() const noexcept {
    if (!do_half && do_wmma) return true;
    bool b = (0.0 <= time && 0 < nlm && 0 < nln && 0 < npm && 0 < npn
	      && 0 < npk);
    if (do_wmma) b = (b && 0 < ntm && 0 < ntn);
    return b; }
};

class ManageComputeMatM {
  using uint = unsigned int;
  OCL::Kernel _ker[NNAux::nslot];
  size_t _size_g[3], _size_l[3];
  uint _nm, _nn, _nk;
public:
  void start(const OCL::Context &context, uint nbatch, uint nm0, uint nn0,
	     uint nk0, const SgemmParam &param) noexcept;
  void set(const OCL::Memory &mem_a, const OCL::Memory mem_b[NNAux::nslot],
	   const OCL::Memory mem_c[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
  uint get_nm() const noexcept { return _nm; }
  uint get_nn() const noexcept { return _nn; }
  uint get_nk() const noexcept { return _nk; }
};

class ManageSgemm {
  using uint = unsigned int;
  std::unique_ptr<OCL::Kernel []> _pker_sgemm;
  std::unique_ptr<class MngChgDim> _pmng_chg_dim_a;
  std::unique_ptr<class MngChgDim> _pmng_chg_dim_b;
  std::unique_ptr<class MngChgDim> _pmng_chg_dim_c;
  std::unique_ptr<class MngChgDim []> _array_mng_chg_dim_c;
  double _time;
  size_t size_g[3], size_l[3];
  SgemmParam _param;
  uint _nker, _nm, _nn, _nk, _nm0, _nn0, _nk0;
  uint _lda, _ldb, _ldc, _offa, _offb, _offc;
public:
  void start(std::string signature, int device_id, const OCL::Device &device,
	     const OCL::Context &context, uint nker, bool transa, bool transb,
	     uint nm0, uint nn0, uint nk0, uint offa, uint lda, uint offb,
	     uint ldb, uint offc, uint ldc, const char *dname_tune);
  uint get_nm() const noexcept { return _nm; }
  uint get_nn() const noexcept { return _nn; }
  uint get_nk() const noexcept { return _nk; }
  void push(const OCL::Queue &queue, uint uker) const noexcept;
  void set_a(const OCL::Memory &mem) const noexcept;
  void set_array_a(const OCL::Memory mem[]) const noexcept;
  void set_a_chg_dim(const OCL::Context &context, const OCL::Memory &mem)
    noexcept;
  void chg_dim_a_beforhand(const OCL::Context &context) noexcept;
  void set_b(const OCL::Memory &mem) const noexcept;
  void set_array_b(const OCL::Memory mem[]) const noexcept;
  void set_b_chg_dim(const OCL::Context &context, const OCL::Memory &mem)
    noexcept;
  void chg_dim_b_beforhand(const OCL::Context &context) noexcept;
  template <typename M> void set_array_c(const M mem[]) const noexcept;
  template <typename M>
  void set_array_c_chg_dim(const OCL::Context &context, const M mem[])
    noexcept;
  std::string gen_info() const noexcept { return _param.gen_info(); }
};

class ManageComputeMatV {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];

public:
  void start(bool store_half, const OCL::Context &context, uint nch, uint nb,
	     uint nn, uint nk, const OCL::Memory mem_in[NNAux::nslot],
	     const OCL::Memory mem_matV[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
};

class ManageComputeMatA {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(bool load_half, const OCL::Context &context, bool flag_join,
	     uint nch, uint nb, uint nm, uint nn, uint nn_out,
	     const OCL::Memory mem_matM[NNAux::nslot],
	     const OCL::Memory mem_bypass[NNAux::nslot],
	     const OCL::Memory &mean, const OCL::Memory &sd_inv,
	     const OCL::Memory mem_out[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint nslot) const noexcept;
};

class ManageComputeMatAV {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(bool do_half, bool do_join, bool do_fork,
	     const OCL::Context &context, uint nch, uint nb, uint nm, uint nn,
	     uint nk, const OCL::Memory mem_matM[NNAux::nslot],
	     const OCL::Memory mem_matV[NNAux::nslot],
	     const OCL::Memory &mean, const OCL::Memory &sd_inv,
	     const OCL::Memory *mem_bypass) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
};

class ManageTransformValue2 {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(const OCL::Context &context, uint nch, uint nb,
	     const OCL::Memory mem_in[NNAux::nslot], uint offin, uint nn_out,
	     const OCL::Memory mem_out[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
};

class ManageResizeBiasReLU {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(const OCL::Context &context, uint nm, uint nn, uint ldin,
	     uint ldout, const OCL::Memory &mem_bias,
	     const OCL::Memory mem_in[NNAux::nslot],
	     const OCL::Memory mem_out[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
};

class ManageComputeBNReLU {
  using uint = unsigned int;
  size_t _size_g[3], _size_l[3];
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(const OCL::Context &context, uint nch, uint nb, uint nn_in,
	     const OCL::Memory &mean, const OCL::Memory &sd_inv,
	     const OCL::Memory mem_in[NNAux::nslot],
	     const OCL::Memory mem_out[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint uslot) const noexcept;
};

class ManageComputePolicy {
  using uint = unsigned int;
  OCL::Kernel _ker[NNAux::nslot];
public:
  void start(const OCL::Context &context, uint nch_in, uint maxsize_batch,
	     const OCL::MemPinned mem_nnmoves[NNAux::nslot],
	     const OCL::Memory &mem_weight,
	     const OCL::Memory &mem_bias,
	     const OCL::Memory mem_in[NNAux::nslot],
	     const OCL::MemPinned mem_out[NNAux::nslot]) noexcept;
  void push(const OCL::Queue &queue, uint ntot_moves, uint offin,
	    uint nslot) const noexcept;
};

class NNetOCL : public NNet {
  using uint   = unsigned int;
  using ushort = unsigned short;
  using row_t  = std::unique_ptr<float []>;

  std::mutex _m_pool1_slot;
  std::condition_variable _cv_pool1_slot;
  uint _pool1_slots[NNAux::nslot];
  uint _pool1_slot_size;
  uint acquire_slot_wait() noexcept;
  void release_slot(uint uslot) noexcept;

  int64_t _elapsed_wait_ff;
  ManageDecode _mng_decode;
  ManageComputeMatV _mng_compute_matV_first;
  std::unique_ptr<ManageComputeMatM []> _pmng_compute_matM;
  std::unique_ptr<ManageComputeMatAV []> _pmng_compute_matAV;
  ManageComputeMatA _mng_compute_matA_last;
  ManageSgemm _mng_head1, _mng_value2, _mng_value3;
  ManageComputeBNReLU _mng_compute_BNReLU;
  ManageTransformValue2 _mng_transform_value2;
  ManageResizeBiasReLU _mng_resize_bias_ReLU_value3;
  ManageComputePolicy _mng_compute_policy;
  OCL::Queue _queue_a[NNAux::nslot];
  OCL::Memory _mem_head1_mean, _mem_head1_sd_inv;
  OCL::Memory _mem_policy2_wght, _mem_policy2_bias;
  OCL::Memory _mem_value2_bias;
  OCL::Memory _mem_decode[NNAux::nslot], _mem_matV_input[NNAux::nslot];
  OCL::Memory _mem_matV[NNAux::nslot], _mem_matM[NNAux::nslot];
  OCL::Memory _mem_bypass[NNAux::nslot], _mem_resout[NNAux::nslot];
  OCL::Memory _mem_head1_out[NNAux::nslot], _mem_BNReLU_out[NNAux::nslot];
  OCL::Memory _mem_transform[NNAux::nslot], _mem_value2_out[NNAux::nslot];
  OCL::Memory _mem_value3_in[NNAux::nslot];
  OCL::MemPinned _mem_in[NNAux::nslot], _mem_out[NNAux::nslot];
  std::vector<struct CLResWght> _vec_reswghts;
  uint _slots_size_batch[NNAux::nslot];
  const uint *_slots_sizes_nnmove[NNAux::nslot];
  float *_slots_probs[NNAux::nslot];
  float *_slots_values[NNAux::nslot];
  uint _maxsize_batch, _nres_block;
  row_t _value3_bias;
  bool _do_sleep;
  void push(uint uslot, size_t size_write, uint n_one, uint ntot_moves,
	    uint index_moves) noexcept;

public:
  explicit NNetOCL() noexcept;
  std::string reset(uint maxsize_batch,
		    const std::vector<std::pair<uint, row_t>> &wght,
		    int device_id, bool use_half, bool use_wmma,
		    bool flag_out, bool do_sleep,
		    const char *dname_tune = nullptr) noexcept;
  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept;
  uint push_ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	       const ushort *nnmoves, float *probs, float *values) noexcept;
  uint push_ff(const NNInBatchCompressed &nn_in_b_c, float *probs,
	       float *values) noexcept;
  void wait_ff(uint uslot) noexcept;
  bool do_compress() const noexcept { return true; }
};
#endif
