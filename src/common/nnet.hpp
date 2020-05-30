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
  using row_t  = std::unique_ptr<float []>;
  using wght_t = std::vector<std::pair<uint, row_t>>;
  constexpr uint maxnum_nipc = 256U;
  constexpr uint maxnum_nnet = 64U;
  constexpr uint width       = 9U;
  constexpr uint height      = 9U;
  constexpr uint size_plane  = width * height;
  constexpr uint nmove       = 1024U;
  void softmax(uint n, float *p) noexcept;
  wght_t read(const FName &fwght, uint &version, uint64_t &digest) noexcept;

  constexpr uint nch_input      = 362U;
  constexpr uint nch_input_fill = 17U * 8U + 2U;
  constexpr uint nch_out_policy = 139U;
  constexpr uint size_input     = size_plane * nch_input;
  constexpr uint ceil_multi(uint u, uint mul) noexcept {
    return ((u + mul - 1U) / mul) * mul; }
  unsigned short encode_nnmove(const Action &a, const Color &turn) noexcept;
  constexpr uint fill_block_size(uint nb) noexcept {
    return NNAux::ceil_multi(nb * NNAux::nch_input_fill, 32U); }
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
  void encode_input(float *p) const noexcept;
};

class NNet {
  using uint = unsigned int;
public:
  enum class Impl : uint { CPUBLAS, OpenCL, End };
  static constexpr Impl cpublas = Impl::CPUBLAS, opencl  = Impl::OpenCL;
  virtual ~NNet() noexcept {}
  virtual uint push_ff(uint size_batch, const float *input,
		       const uint *sizes_nnmove, const ushort *nnmoves,
		       float *probs, float *values) noexcept = 0;
  virtual void wait_ff(uint) noexcept = 0;
};

enum class SrvType : uint { Register, FeedForward, FlushON, FlushOFF,
    NNReset, End };

struct SharedService {
  uint id_ipc_next;
  FName fn_weights;
  uint njob;
  struct { uint id; SrvType type; } jobs[NNAux::maxnum_nipc + 16U];
};

struct SharedIPC {
  uint nnet_id;
  float input[NNAux::size_input];
  uint size_nnmove;
  ushort nnmoves[SAux::maxsize_moves];
  float probs[SAux::maxsize_moves];
  float value; };
