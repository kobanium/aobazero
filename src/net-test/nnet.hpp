// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <cstdint>
class FName;
namespace NNAux {
  using row_t = std::unique_ptr<float []>;
  using uint = unsigned int;
  class NotXZ {};
  constexpr uint width          = 9U;
  constexpr uint height         = 9U;
  constexpr uint size_plane     = width * height;
  constexpr uint nch_input      = 362U;
  constexpr uint size_input     = size_plane * nch_input;
  constexpr uint nch_out_policy = 139U;
  constexpr uint nmove          = 1024U;
  std::vector<std::pair<uint, row_t>>
  read_xz(const FName &fwght, uint &version, uint64_t &digest);
  std::vector<std::pair<uint, row_t>>
  read_txt(const FName &fwght, uint &version, uint64_t &digest) noexcept;
}
