// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "shogibase.hpp"
#include <memory>
#include <vector>
class FName;
namespace NNAux {
  using uint   = unsigned int;
  using row_t  = std::unique_ptr<float []>;
  using wght_t = std::vector<std::pair<uint, row_t>>;
  constexpr uint maxnum_nipc    = 128U;
  constexpr uint maxnum_nnet    = 64U;
  constexpr uint width          = 9U;
  constexpr uint height         = 9U;
  constexpr uint size_plane     = width * height;
  constexpr uint nch_input      = 362U;
  constexpr uint size_input     = size_plane * nch_input;
  constexpr uint nch_out_policy = 139U;
  constexpr uint nmove          = 1024U;
  void softmax(uint n, float *p) noexcept;
  wght_t read(const FName &fwght, uint &version, uint64_t &digest) noexcept;
  unsigned short encode_nnmove(const Action &a, const Color &turn) noexcept;
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
