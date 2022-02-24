// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "shogibase.hpp"
#include <memory>
#include <tuple>
#include <vector>
class FName;
namespace NNAux {
  using uint   = unsigned int;
  using ushort = unsigned short;
  using row_t  = std::unique_ptr<float []>;
  using wght_t = std::vector<std::pair<uint, row_t>>;
  constexpr uint maxnum_nipc = 256U;
  constexpr uint maxnum_nnet = 64U;
  constexpr uint width       = 9U;
  constexpr uint height      = 9U;
  constexpr uint size_plane  = width * height;
  void softmax(uint n, float *p) noexcept;
  wght_t read(const FName &fwght, uint &version, uint64_t &digest) noexcept;
  wght_t read(const FName &fwght) noexcept;

  constexpr uint nch_input      = 362U;
  constexpr uint nch_input_fill = 17U * 8U + 2U;
#ifdef USE_POLICY2187
  constexpr uint nch_out_policy =  27U;
  constexpr uint maxn_one       = 64U * 8U * 2;
#else
  constexpr uint nch_out_policy = 139U;
  constexpr uint maxn_one       = 64U * 8U;
#endif
  constexpr uint maxsize_compressed_features = (size_plane * nch_input) / 16U;
  constexpr uint ceil_multi(uint u, uint mul) noexcept {
    return ((u + mul - 1U) / mul) * mul; }
  unsigned short encode_nnmove(const Action &a, const Color &turn) noexcept;
  constexpr uint fill_block_size(uint nb) noexcept {
    return NNAux::ceil_multi(nb * NNAux::nch_input_fill, 32U); }
  uint compress_features(void *out, const float *in) noexcept;
  void decompress_features(float *out, uint n_one, const void *in) noexcept;
  std::tuple<uint, uint, uint, uint>
  pack_batch(uint nb0, uint nb, const float *in, const uint *sizes_nnmove,
	     const ushort *nnmoves, void *out) noexcept;
}

template<unsigned int Len>
class NodeNN : public Node<Len> {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct {
    BMap bm[Color::ok_size];
    uchar board[Sq::ok_size];
    uchar hand[Color::ok_size][Pc::hand_size];
    uint count_repeat; } _posi[Len];
  void set_posi() noexcept;
  
public:
  explicit NodeNN() noexcept { set_posi(); }
  void clear() noexcept { Node<Len>::clear(); set_posi(); }
  void take_action(const Action &a) noexcept;
  void encode_features(float *p) const noexcept;
};

class NNInBatchBase {
  using uint = unsigned int;
  std::unique_ptr<uint []> _sizes_nnmove;
  uint _ub, _nb;
public:
  explicit NNInBatchBase(uint nb) noexcept :
  _sizes_nnmove(new uint [nb]), _ub(0), _nb(nb) {}
  virtual ~NNInBatchBase() noexcept {}
  virtual void erase() noexcept { _ub = 0; }
  virtual bool ok() const noexcept { return (_ub <= _nb && _sizes_nnmove); }
  void add(uint size_nnmove) noexcept { _sizes_nnmove[_ub++] = size_nnmove; }
  const uint *get_sizes_nnmove() const noexcept { return _sizes_nnmove.get(); }
  uint get_ub() const noexcept { return _ub; }
  uint get_nb() const noexcept { return _nb; }
};

class NNInBatch : public NNInBatchBase {
  using uint   = unsigned int;
  using ushort = unsigned short;
  std::unique_ptr<float []> _features;
  std::unique_ptr<ushort []> _nnmoves;
public:
  explicit NNInBatch(uint nb) noexcept;
  void add(const float *features, uint size_nnmove, const ushort *nnmoves)
    noexcept;
  const float *get_features() const noexcept { return _features.get(); }
  const ushort *get_nnmoves() const noexcept { return _nnmoves.get(); }
  bool ok() const noexcept {
    return (NNInBatchBase::ok() && _features && _nnmoves); }
};

class NNInBatchCompressed : public NNInBatchBase {
  using uint   = unsigned int;
  using ushort = unsigned short;
  std::unique_ptr<float []> _fills;
  std::unique_ptr<uint []> _ones;
  std::unique_ptr<uint []> _nnmoves;
  uint _n_one, _ntot_moves;
public:
  explicit NNInBatchCompressed(uint nb) noexcept;
  void add(uint n_one, const void *compressed_features, uint size_nnmove,
	   const ushort *nnmoves) noexcept;
  void erase() noexcept { NNInBatchBase::erase(); _n_one = _ntot_moves = 0U; }
  std::tuple<uint, uint, uint, uint>
  compute_pack_batch(void *out) const noexcept;
  bool ok() const noexcept {
    return (NNInBatchBase::ok() && _fills && _ones && _nnmoves); }
};

class NNet {
  using uint   = unsigned int;
  using ushort = unsigned short;
public:
  enum class Impl : uint { CPUBLAS, OpenCL, End };
  static constexpr Impl cpublas = Impl::CPUBLAS, opencl  = Impl::OpenCL;
  virtual ~NNet() noexcept {}
  virtual uint push_ff(uint size_batch, const float *input,
		       const uint *sizes_nnmove, const ushort *nnmoves,
		       float *probs, float *values) noexcept;
  virtual uint push_ff(const NNInBatchCompressed &nn_in_b_c, float *probs,
		       float *values) noexcept;
  virtual void wait_ff(uint) noexcept;
  virtual bool do_compress() const noexcept { return false; }
};

enum class SrvType : unsigned int { Register, FeedForward, FlushON, FlushOFF,
				    NNReset, NOP, End };

struct SharedService {
  using uint = unsigned int;
  uint id_ipc_next;
  FName fn_weights;
  uint njob;
  bool do_compress;
  struct { uint id; SrvType type; } jobs[NNAux::maxnum_nipc + 16U];
};

struct SharedIPC {
  using uint = unsigned int;
  uint nnet_id, n_one;
  float compressed_features[NNAux::maxsize_compressed_features];
  float features[NNAux::size_plane * NNAux::nch_input]; // ~100KB
  unsigned int size_nnmove;
  unsigned short nnmove[SAux::maxsize_moves];
  float probs[SAux::maxsize_moves];
  float value; };
