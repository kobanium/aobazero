// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(USE_OPENCL)
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ocl.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <cassert>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstring>
#if defined(USE_MKL)
#  include <mkl.h>
#elif defined(USE_OPENBLAS)
#  include <cblas.h>
#else
#  error "NO BLAS"
#endif
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::this_thread::sleep_for;
using std::copy_n;
using std::fill_n;
using std::forward;
using std::max;
using std::max_element;
using std::move;
using std::pair;
using std::string;
using std::to_string;
using std::deque;
using std::swap;
using std::unique_ptr;
using std::vector;
using row_t  = unique_ptr<float []>;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;

static double elapsed_sum = 0.0;
static uint nelapsed      = 0;

constexpr char msg_bad_wght_dim[] = "bad weight dimension";
constexpr uint tune_sample_size = 16U;
constexpr uint len_kernel      = 3U;
constexpr uint size_kernel     = len_kernel * len_kernel;
constexpr uint len_tile_out    = 3U;
constexpr uint len_tile_in     = 5U;
constexpr uint size_tile_in    = len_tile_in * len_tile_in;
constexpr uint ntile_h         = 3U;
constexpr uint ntile_w         = 3U;
constexpr uint ntile           = ntile_h * ntile_w;
constexpr uint size_plane_in   = size_tile_in * ntile;
constexpr uint pad             = 1U;
constexpr float bn_factor      = 1.0f / 999.982f;
constexpr float bn_eps         = 1e-5f;
/*
  _cl_dev.finish();
  steady_clock::time_point start = steady_clock::now();
  _cl_dev.finish();
  steady_clock::time_point end = steady_clock::now();
  double elapsed
    = static_cast<double>(duration_cast<microseconds>(end - start).count());
  elapsed_sum += elapsed;
  nelapsed    += 1U;
  std::cout << std::endl;
  std::cout << elapsed << std::endl;
  std::cout << elapsed_sum / static_cast<double>(nelapsed) << std::endl;
  std::cout << std::endl;
*/
const string code_common =
  "#define NCH_INPUT "    + to_string(NNAux::nch_input)  + "\n"
  "#define SIZE_PLANE "   + to_string(NNAux::size_plane) + "\n"
  "#define HEIGHT "       + to_string(NNAux::height)     + "\n"
  "#define WIDTH "        + to_string(NNAux::width)      + "\n"
  "#define LEN_TILE_IN "  + to_string(len_tile_in)       + "\n"
  "#define SIZE_TILE_IN " + to_string(size_tile_in)      + "\n"
  "#define LEN_TILE_OUT " + to_string(len_tile_out)      + "\n"
  "#define NTILE_H "      + to_string(ntile_h)           + "\n"
  "#define NTILE_W "      + to_string(ntile_w)           + "\n"
  "#define NTILE "        + to_string(ntile)             + "\n"
  "#define PAD "          + to_string(pad)               + R"(
#define uint unsigned int
float x1(float x) { return x; }
float x2(float x) { return x + x; }
float x3(float x) { return x + x + x; }
float x4(float x) { x += x; return x + x; }
float x6(float x) { x += x; return x + x + x; }
float x9(float x) { float y = x + x; y += y; return y + y + x; }
)";

const string code_compute_matV = R"(
void compute_matV_child(uint nk, uint nn, uint ch, uint ub, uint uh, uint uw,
                        __global const float *fin, __global float *matV) {
  int y0 = uh * LEN_TILE_OUT - PAD;
  int x0 = uw * LEN_TILE_OUT - PAD;
  float md[LEN_TILE_IN][LEN_TILE_IN];
  for (int y = 0; y < LEN_TILE_IN; ++y)
    for (int x = 0; x < LEN_TILE_IN; ++x)
      if (0 <= y0 + y && y0 + y < HEIGHT && 0 <= x0 + x && x0 + x < WIDTH)
        md[y][x] = fin[(y0 + y) * WIDTH + x0 + x];
      else md[y][x] = 0.0f;

  uint uca = nk * nn * LEN_TILE_IN;
  uint ucb = nk * nn;
  uint ucc = ch * nn + ub * NTILE + uh * NTILE_W + uw;
  matV[ucc + uca*0U + ucb*0U]
    = (+ x4(md[0][0]) - x2(md[0][1]) - x4(md[0][2]) + x2(md[0][3])
       - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
       - x4(md[2][0]) + x2(md[2][1]) + x4(md[2][2]) - x2(md[2][3])
       + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*1U + ucb*0U]
    = (- x4(md[1][0]) + x2(md[1][1]) + x4(md[1][2]) - x2(md[1][3])
       - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
       + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*2U + ucb*0U]
    = (+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
       - x6(md[2][0]) + x3(md[2][1]) + x6(md[2][2]) - x3(md[2][3])
       + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*3U + ucb*0U]
    = (- x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
       + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*4U + ucb*0U]
    = (+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
       - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
       - x4(md[3][0]) + x2(md[3][1]) + x4(md[3][2]) - x2(md[3][3])
       + x2(md[4][0]) - x1(md[4][1]) - x2(md[4][2]) + x1(md[4][3]));
  matV[ucc + uca*0U + ucb*1U]
    = (- x4(md[0][1]) - x2(md[0][2]) + x2(md[0][3])
       + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
       + x4(md[2][1]) + x2(md[2][2]) - x2(md[2][3])
       - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*1U + ucb*1U]
    = (+ x4(md[1][1]) + x2(md[1][2]) - x2(md[1][3])
       + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
       - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*2U + ucb*1U]
    = (- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
       + x6(md[2][1]) + x3(md[2][2]) - x3(md[2][3])
       - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*3U + ucb*1U]
    = (+ x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
       - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*4U + ucb*1U]
    = (- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
       + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
       + x4(md[3][1]) + x2(md[3][2]) - x2(md[3][3])
       - x2(md[4][1]) - x1(md[4][2]) + x1(md[4][3]));
  matV[ucc + uca*0U + ucb*2U]
    = (+ x4(md[0][1]) - x6(md[0][2]) + x2(md[0][3])
       - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
       - x4(md[2][1]) + x6(md[2][2]) - x2(md[2][3])
       + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*1U + ucb*2U]
    = (- x4(md[1][1]) + x6(md[1][2]) - x2(md[1][3])
       - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
       + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*2U + ucb*2U]
    = (+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
       - x6(md[2][1]) + x9(md[2][2]) - x3(md[2][3])
       + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*3U + ucb*2U]
    = (- x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
       + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
  matV[ucc + uca*4U + ucb*2U]
    = (+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
       - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
       - x4(md[3][1]) + x6(md[3][2]) - x2(md[3][3])
       + x2(md[4][1]) - x3(md[4][2]) + x1(md[4][3]));
  matV[ucc + uca*0U + ucb*3U]
    = (- x2(md[0][1]) + x2(md[0][3]) + x1(md[1][1]) - x1(md[1][3])
       + x2(md[2][1]) - x2(md[2][3]) - x1(md[3][1]) + x1(md[3][3]));
  matV[ucc + uca*1U + ucb*3U]
    = (+ x2(md[1][1]) - x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
       - x1(md[3][1]) + x1(md[3][3]));
  matV[ucc + uca*2U + ucb*3U]
    = (- x2(md[1][1]) + x2(md[1][3]) + x3(md[2][1]) - x3(md[2][3])
       - x1(md[3][1]) + x1(md[3][3]));
  matV[ucc + uca*3U + ucb*3U]
    = (+ x1(md[1][1]) - x1(md[1][3]) - x1(md[3][1]) + x1(md[3][3]));
  matV[ucc + uca*4U + ucb*3U]
    = (- x2(md[1][1]) + x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
       + x2(md[3][1]) - x2(md[3][3]) - x1(md[4][1]) + x1(md[4][3]));
  matV[ucc + uca*0U + ucb*4U]
    = (+ x4(md[0][1]) - x2(md[0][2]) - x4(md[0][3]) + x2(md[0][4])
       - x2(md[1][1]) + x1(md[1][2]) + x2(md[1][3]) - x1(md[1][4])
       - x4(md[2][1]) + x2(md[2][2]) + x4(md[2][3]) - x2(md[2][4])
       + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]));
  matV[ucc + uca*1U + ucb*4U]
    = (- x4(md[1][1]) + x2(md[1][2]) + x4(md[1][3]) - x2(md[1][4])
       - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
       + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]));
  matV[ucc + uca*2U + ucb*4U]
    = (+ x4(md[1][1]) - x6(md[2][1]) + x2(md[3][1])
       - x2(md[1][2]) + x3(md[2][2]) - x1(md[3][2])
       - x4(md[1][3]) + x6(md[2][3]) - x2(md[3][3])
       + x2(md[1][4]) - x3(md[2][4]) + x1(md[3][4]));
  matV[ucc + uca*3U + ucb*4U]
    = (- x2(md[1][1]) + x2(md[3][1]) + x1(md[1][2]) - x1(md[3][2])
       + x2(md[1][3]) - x2(md[3][3]) - x1(md[1][4]) + x1(md[3][4]));
  matV[ucc + uca*4U + ucb*4U]
    = (+ x4(md[1][1]) - x2(md[1][2]) - x4(md[1][3]) + x2(md[1][4])
       - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
       - x4(md[3][1]) + x2(md[3][2]) + x4(md[3][3]) - x2(md[3][4])
       + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4])); }

__kernel void compute_matV_input(uint nb, uint nk, uint nn,
                                 __global const float *fin,
                                 __global float *matV) {
  uint gid   = get_global_id(0);
  uint loop  = gid / NTILE;
  uint utile = gid % NTILE;
  uint ub    = loop / NCH_INPUT;
  uint ch    = loop % NCH_INPUT;
  uint chb   = ch * nb + ub;
  uint uh    = utile / NTILE_W;
  uint uw    = utile % NTILE_W;
  fin       += loop * SIZE_PLANE;
  compute_matV_child(nk, nn, ch, ub, uh, uw, fin, matV); }

__kernel void compute_matV(uint nb, uint nk, uint nn,
                           __global const float *fin,
                           __global float *matV) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / nb;
  uint ub    = chb % nb;
  uint uh    = utile / NTILE_W;
  uint uw    = utile % NTILE_W;
  fin       += chb * SIZE_PLANE;
  compute_matV_child(nk, nn, ch, ub, uh, uw, fin, matV); }
)";

const string code_compute_matA = R"(
float compute_BNReLU(float sd_inv, float mean, float x) {
  return max(0.0f, sd_inv * (x - mean)); }

__kernel void compute_matA_BNReLU(uint nb, uint offc, uint ldc,
                                  __global const float *matM,
                                  __global const float *mean_array,
                                  __global const float *sd_inv_array,
                                  __global float *fout) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / nb;
  uint ub    = chb % nb;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * offc
                              + ch * ldc + ub * NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint ucd     = chb * SIZE_PLANE + (uh * WIDTH + uw) * LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  fout[ucd + 0U * WIDTH + 0U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  fout[ucd + 0U * WIDTH + 1U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[0][1] - mm[0][2] + x2(mm[0][3])
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2] + x2(mm[2][3])
                     + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  fout[ucd + 0U * WIDTH + 2U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  fout[ucd + 1U * WIDTH + 0U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
                     + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));
  fout[ucd + 1U * WIDTH + 1U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     - mm[2][1] + mm[2][2]
                     + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));
  fout[ucd + 1U * WIDTH + 2U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
                     + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));
  fout[ucd + 2U * WIDTH + 0U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
                     + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  fout[ucd + 2U * WIDTH + 1U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2]
                     + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
                     + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  fout[ucd + 2U * WIDTH + 2U]
    = compute_BNReLU(sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
                     + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }

float compute_BNReLU_join(float join, float sd_inv, float mean, float x) {
  return max(0.0f, join + sd_inv * (x - mean)); }

__kernel void compute_matA_BNReLU_join(uint nb, uint offc, uint ldc,
                                       __global const float *matM,
                                       __global const float *mean_array,
                                       __global const float *sd_inv_array,
                                       __global float *fout) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / nb;
  uint ub    = chb % nb;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * offc
                              + ch * ldc + ub * NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint ucd     = chb * SIZE_PLANE + (uh * WIDTH + uw) * LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  fout[ucd + 0U * WIDTH + 0U]
    = compute_BNReLU_join(fout[ucd + 0U * WIDTH + 0U], sd_inv, mean,
                     + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  fout[ucd + 0U * WIDTH + 1U]
    = compute_BNReLU_join(fout[ucd + 0U * WIDTH + 1U], sd_inv, mean,
                     + mm[0][1] - mm[0][2] + x2(mm[0][3])
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2] + x2(mm[2][3])
                     + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  fout[ucd + 0U * WIDTH + 2U]
    = compute_BNReLU_join(fout[ucd + 0U * WIDTH + 2U], sd_inv, mean,
                     + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  fout[ucd + 1U * WIDTH + 0U]
    = compute_BNReLU_join(fout[ucd + 1U * WIDTH + 0U], sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
                     + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));
  fout[ucd + 1U * WIDTH + 1U]
    = compute_BNReLU_join(fout[ucd + 1U * WIDTH + 1U], sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     - mm[2][1] + mm[2][2]
                     + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));
  fout[ucd + 1U * WIDTH + 2U]
    = compute_BNReLU_join(fout[ucd + 1U * WIDTH + 2U], sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
                     + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));
  fout[ucd + 2U * WIDTH + 0U]
    = compute_BNReLU_join(fout[ucd + 2U * WIDTH + 0U], sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
                     + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  fout[ucd + 2U * WIDTH + 1U]
    = compute_BNReLU_join(fout[ucd + 2U * WIDTH + 1U], sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2]
                     + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
                     + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  fout[ucd + 2U * WIDTH + 2U]
    = compute_BNReLU_join(fout[ucd + 2U * WIDTH + 2U], sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
                     + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }
)";

const string code_sgemm_batch = R"(
__kernel __attribute__((reqd_work_group_size(SGEMM_NL, SGEMM_NL, 1U)))
void sgemm_batch(uint nm, uint nn, uint nk,
                 __global const float *gA, uint offa,
                 __global const float *gB, uint offb,
                 __global float *gC, uint offc) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint uln = get_local_id(0);
  uint ngk = nk / (SGEMM_NL * SGEMM_NPK);
  gA += ub*offa + ugm*SGEMM_NL*SGEMM_NPM;
  gB += ub*offb + ugn*SGEMM_NL*SGEMM_NPN;
  gC += ub*offc + ugm*SGEMM_NL*SGEMM_NPM*nn + ugn*SGEMM_NL*SGEMM_NPN;

  __local float lA[SGEMM_NL*SGEMM_NPK][SGEMM_NL*SGEMM_NPM];
  __local float lB[SGEMM_NL*SGEMM_NPK][SGEMM_NL*SGEMM_NPN];
  float pC[SGEMM_NPM][SGEMM_NPN];
  float pB[SGEMM_NPN];
  float pA;

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn) pC[upm][upn] = 0.0f;

  uint ulA1 = (ulm*SGEMM_NL + uln) % (SGEMM_NL*SGEMM_NPM);
  uint ulA2 = ((ulm*SGEMM_NL + uln) / (SGEMM_NL*SGEMM_NPM))
              * SGEMM_NPM*SGEMM_NPK;
  uint ulB1 = (ulm*SGEMM_NL + uln) % (SGEMM_NL*SGEMM_NPN);
  uint ulB2 = ((ulm*SGEMM_NL + uln) / (SGEMM_NL*SGEMM_NPN))
              * SGEMM_NPN*SGEMM_NPK;

  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NPM*SGEMM_NPK; ++u)
      lA[ulA2 + u][ulA1] = gA[(ulA2 + u)*nm + ulA1];
    for (uint u = 0; u < SGEMM_NPN*SGEMM_NPK; ++u)
      lB[ulB2 + u][ulB1] = gB[(ulB2 + u)*nn + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NPK; ++ulpk) {
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = lB[ulpk][uln*SGEMM_NPN + upn];
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = lA[ulpk][ulm*SGEMM_NPM + upm];
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn]; } }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NL*SGEMM_NPK*nm;
    gB += SGEMM_NL*SGEMM_NPK*nn; }

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      gC[(ulm*SGEMM_NPM + upm)*nn + uln*SGEMM_NPN + upn] = pC[upm][upn]; }
)";

const string gen_code_sgemm_batch(uint nl, uint npm, uint npn, uint npk)
  noexcept {
  string str;
  str += "#define SGEMM_NL  " + to_string(nl)  + "\n";
  str += "#define SGEMM_NPM " + to_string(npm) + "\n";
  str += "#define SGEMM_NPN " + to_string(npn) + "\n";
  str += "#define SGEMM_NPK " + to_string(npk) + "\n";
  str += code_sgemm_batch;
  return str; }

static uint ceil_multi(uint u, uint mul) noexcept {
  assert(1U <= mul && u <= UINT_MAX - mul + 1U);
  return ((u + mul - 1U) / mul) * mul; }

static uint ceil_power2(uint u) noexcept {
  uint u0;
  for (u0 = 1U; u0 < u; u0 *= 2U) assert(u0 <= UINT_MAX / 2U);
  return u0; }

static void softmax(uint n, float *p) noexcept {
  assert(0 < n && p);
  float fmax = *std::max_element(p, p + n);
  float fsum = 0.0f;
  for (uint u = 0; u < n; ++u) {
    p[u] = std::exp(p[u] - fmax);
    fsum += p[u]; }
  
  float factor = 1.0f / fsum;
  for (uint u = 0; u < n; ++u) p[u] *= factor; }

static void get_device(uint udev, OCL::Device &device_udev) noexcept {
  uint id = 0;
  vector<OCL::Platform> platforms = OCL::gen_platforms();
  for (auto &platform : platforms) {
    vector<OCL::Device> devices = platform.gen_devices_all();
    for (auto &device : devices) {
      if (id++ != udev) continue;
      device_udev = move(device);
      return; } }
  device_udev = OCL::Device();
  return; }

static void get_best_device(OCL::Device &device_best) noexcept {
  double value_best = -1.0;
  uint id = 0;
  vector<OCL::Platform> platforms = OCL::gen_platforms();
  for (auto &platform : platforms) {
    vector<OCL::Device> devices = platform.gen_devices_all();
    for (auto &device : devices) {
      double value = static_cast<double>(device.gen_max_compute_units());
      value *= static_cast<double>(device.gen_max_clock_frequency());
      if (device.ok() || value_best < value) {
	value_best  = value;
	device_best = move(device); }
      id += 1U; } } }

static double measure_send_global(const OCL::Queue &queue, size_t size) {
  assert(dev.ok() && 0 < size);
  double elapsed = 0.0;
  unique_ptr<uchar> data(new uchar [size]);
  fill_n(data.get(), size, '\x55');
  OCL::Memory mem_a = queue.gen_mem_r(size);
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(1000U));
    steady_clock::time_point start = steady_clock::now();
    queue.push_write(mem_a, size, data.get());
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    elapsed += static_cast<double>
      (duration_cast<microseconds>(end - start).count()); }
  return elapsed / static_cast<double>(tune_sample_size); }

static double measure_send_pinned(const OCL::Queue &queue, size_t size) {
  assert(dev.ok() && 0 < size);
  double elapsed = 0.0;
  unique_ptr<uchar> data(new uchar [size]);
  fill_n(data.get(), size, '\x55');
  OCL::Memory mem_a = queue.gen_mem_r(size);
  OCL::Memory mem_b = queue.gen_mem_map_r(size);
  void *ptr = queue.push_map_w(mem_b, size);
  queue.finish();
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(1000U));
    steady_clock::time_point start = steady_clock::now();
    copy_n(data.get(), size, static_cast<uchar *>(ptr));
    queue.push_write(mem_b, size, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    elapsed += static_cast<double>
      (duration_cast<microseconds>(end - start).count()); }
  queue.push_unmap(mem_b, ptr);
  queue.finish();
  return elapsed / static_cast<double>(tune_sample_size); }

static double measure_send_zcopy(const OCL::Queue &queue, size_t size) {
  assert(dev.ok() && 0 < size);
  double elapsed = 0.0;
  unique_ptr<uchar> data(new uchar [size]);
  fill_n(data.get(), size, '\x55');
  OCL::Memory mem_a = queue.gen_mem_map_r(size);
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(1000U));
    steady_clock::time_point start = steady_clock::now();
    void *ptr = queue.push_map_w(mem_a, size);
    queue.finish();
    copy_n(data.get(), size, static_cast<uchar *>(ptr));
    queue.push_unmap(mem_a, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    elapsed += static_cast<double>
      (duration_cast<microseconds>(end - start).count()); }
  return elapsed / static_cast<double>(tune_sample_size); }

constexpr char ManageSend::global_memory[];
constexpr char ManageSend::pinned_memory[];
constexpr char ManageSend::zero_copy[];
void ManageSend::start(const OCL::Device &dev, const OCL::Queue &queue,
		       size_t size) noexcept {
  assert(dev.ok() && queue.ok() && 0 < size);
  double elapsed_global = DBL_MAX;
  double elapsed_pinned = DBL_MAX;
  double elapsed_zcopy  = DBL_MAX;
  {
    OCL::Queue qtmp = dev.gen_queue();
    try { elapsed_global = measure_send_global(qtmp, size); } catch (...) {}
    try { elapsed_pinned = measure_send_pinned(qtmp, size); } catch (...) {}
    try { elapsed_zcopy  = measure_send_zcopy (qtmp, size); } catch (...) {} }

  if (elapsed_zcopy < elapsed_global && elapsed_zcopy < elapsed_pinned) {
    _time   = elapsed_zcopy;
    _method = zero_copy; }
  else if (elapsed_pinned < elapsed_global) {
    _time   = elapsed_pinned;
    _method = pinned_memory; }
  else {
    _time   = elapsed_global;
    _method = global_memory; }
  if (_time == DBL_MAX) die(ERR_INT("ManageSend() failed."));

  if (_method == pinned_memory) {
    _mem_a = queue.gen_mem_r(size);
    _mem_b = queue.gen_mem_map_r(size);
    _ptr   = queue.push_map_w(_mem_b, size); }
  else if (_method == zero_copy) _mem_a = queue.gen_mem_map_r(size);
  else _mem_a = queue.gen_mem_r(size); }

void ManageSend::end(const OCL::Queue &queue) noexcept {
  assert(_method && queue.ok());
  if (_method == pinned_memory) queue.push_unmap(_mem_b, _ptr); }

void ManageSend::push(const OCL::Queue &queue, size_t size, const void *ptr)
  noexcept {
  assert(_method && queue.ok() && 0 < size && ptr);
  if (_method == global_memory) queue.push_write(_mem_a, size, ptr);
  else if (_method == pinned_memory) {
    memcpy(_ptr, ptr, size);
    queue.push_write(_mem_a, size, _ptr); }
  else {
    _ptr = queue.push_map_w(_mem_a, size);
    queue.finish();
    memcpy(_ptr, ptr, size);
    queue.push_unmap(_mem_a, _ptr);
    queue.finish(); } }

string ManageSend::gen_info() const noexcept {
  string s(_method);
  s += " (";
  s += to_string(static_cast<uint>(_time));
  s += "us)";
  return s; }

static double measure_sgemm_batch(const OCL::Queue &queue,
				  uint nl, uint npm, uint npn, uint npk,
				  uint nbatch, uint nm0, uint nn0, uint nk0) {
  assert(queue.ok() && 0 < nl && 0 < npm && 0 < npn && 0 < npk);
  assert(0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0);
  uint nm = ceil_multi(nm0, nl * npm);
  uint nn = ceil_multi(nn0, nl * npn);
  uint nk = ceil_multi(nk0, nl * npk);
  uint offa = nm * nk;
  uint offb = nk * nn;
  uint offc = nm * nn;
  unique_ptr<float []> host_a(new float [nbatch * nm * nk]);
  unique_ptr<float []> host_b(new float [nbatch * nk * nn]);
  unique_ptr<float []> host_c(new float [nbatch * nm * nn]);
  string str        = gen_code_sgemm_batch(nl, npm, npn, npk);
  OCL::Program pg   = queue.gen_program(str.c_str());
  OCL::Kernel ker   = pg.gen_kernel("sgemm_batch");
  OCL::Memory mem_a = queue.gen_mem_r(nbatch * nm * nk * sizeof(float));
  OCL::Memory mem_b = queue.gen_mem_r(nbatch * nk * nn * sizeof(float));
  OCL::Memory mem_c = queue.gen_mem_rw(nbatch * nm * nn * sizeof(float));
  fill_n(host_a.get(), nbatch * nm * nk, 0.1f);
  fill_n(host_b.get(), nbatch * nk * nn, 0.01f);
  queue.push_write(mem_a, nbatch * nm * nk, host_a.get());
  queue.push_write(mem_b, nbatch * nk * nn, host_b.get());
  ker.set_arg(0, sizeof(nm), &nm);
  ker.set_arg(1, sizeof(nn), &nn);
  ker.set_arg(2, sizeof(nk), &nk);
  ker.set_arg(4, sizeof(offa), &offa);
  ker.set_arg(6, sizeof(offb), &offb);
  ker.set_arg(8, sizeof(offc), &offc);
  const size_t size_g[3] = { nn / npn, nm / npm, nbatch };
  const size_t size_l[3] = { nl, nl, 1U };
  double elapsed = 0.0;
  queue.finish();
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(1000U));
    steady_clock::time_point start = steady_clock::now();
    ker.set_arg(3, mem_a);
    ker.set_arg(5, mem_b);
    ker.set_arg(7, mem_c);
    queue.push_ndrange_kernel(ker, 3U, size_g, size_l);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    elapsed += static_cast<double>
      (duration_cast<microseconds>(end - start).count());
    queue.push_read(mem_c, nbatch * nm * nn, host_c.get());
    queue.finish(); }
  return elapsed / static_cast<double>(tune_sample_size); }

void ManageSgemmBatch::start(const OCL::Device &dev, const OCL::Queue &queue,
			     bool, bool, uint nbatch, uint nm0,
			     uint nn0, uint nk0) noexcept {
  assert(dev.ok() && queue.ok() && 0 < nbatch && 0 < nm0 && 0 < nn0
	 && 0 < nk0);
  deque<Param> params;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);
  for (uint nl = 8U; nl <= 32; nl *= 2U) {
    if (mmax < nl || nmax < nl || kmax < nl) continue;
    for (uint npm = 1U; npm <= nl; npm *= 2U) {
      if (mmax < nl * npm) continue;
      for (uint npn = 1U; npn <= nl; npn *= 2U) {
	if (nmax < nl * npn) continue;
	for (uint npk = 1U; npk <= nl; npk *= 2U) {
	  if (kmax < nl * npk) continue;
	  params.push_back({nl, npm, npn, npk}); } } } }

  OCL::Queue qtmp = dev.gen_queue();
  _time = DBL_MAX;
  while (! params.empty()) {
    Param param = params.front();
    params.pop_front();
    
    double elapsed = DBL_MAX;
    bool flag_error = false;
    try {
      elapsed = measure_sgemm_batch(qtmp, param.nl, param.npm, param.npn,
				    param.npk, nbatch, nm0, nn0, nk0); }
    catch (...) { flag_error = true; }
    
    if (! flag_error) {
      if (elapsed < _time) { _time = elapsed; _param = param; }
      continue; }
    
    for (auto it = params.begin(); it != params.end(); ) {
      if (param <= *it) it = params.erase(it);
      else ++it; } }
  
  if (_time == DBL_MAX) die(ERR_INT("ManageSgemmBatch() failed."));
  string str = gen_code_sgemm_batch(_param.nl, _param.npm,
				    _param.npn, _param.npk);
  OCL::Program pg = queue.gen_program(str.c_str());
  uint nm = ceil_multi(nm0, _param.nl * _param.npm);
  uint nn = ceil_multi(nn0, _param.nl * _param.npn);
  uint nk = ceil_multi(nk0, _param.nl * _param.npk);
  uint offa = nm * nk;
  uint offb = nk * nn;
  uint offc = nm * nn;
  _ker = pg.gen_kernel("sgemm_batch");
  _ker.set_arg(0, sizeof(nm), &nm);
  _ker.set_arg(1, sizeof(nn), &nn);
  _ker.set_arg(2, sizeof(nk), &nk);
  _ker.set_arg(4, sizeof(offa), &offa);
  _ker.set_arg(6, sizeof(offb), &offb);
  _ker.set_arg(8, sizeof(offc), &offc);
  _nbatch = nbatch;
  _nm = nm;
  _nn = nn;
  _nk = nk;
  size_g[0] = nn / _param.npn;
  size_g[1] = nm / _param.npm;
  size_g[2] = nbatch;
  size_l[0] = size_l[1] = _param.nl;
  size_l[2] = 1U; }

string ManageSgemmBatch::gen_info() const noexcept {
  assert(0 < _nbatch);
  string s;
  s += string("NL:")  + to_string(_param.nl)  + string(" ");
  s += string("NPM:") + to_string(_param.npm) + string(" ");
  s += string("NPN:") + to_string(_param.npn) + string(" ");
  s += string("NPK:") + to_string(_param.npk) + string(" (");
  s += to_string(static_cast<uint>(_time)) + string("us)");
  return s; }

void ManageSgemmBatch::push(const OCL::Queue &queue, const OCL::Memory &mem_a,
			    const OCL::Memory &mem_b,
			    const OCL::Memory &mem_c) noexcept {
  assert(queue.ok() && mem_a.ok() && mem_b.ok() && mem_c.ok());
  _ker.set_arg(3, mem_a);
  _ker.set_arg(5, mem_b);
  _ker.set_arg(7, mem_c);
  queue.push_ndrange_kernel(_ker, 3U, size_g, size_l); }

NNetOCL::~NNetOCL() noexcept { _mng_send.end(_queue); }

void NNetOCL::reset(uint maxsize_batch, const vector<pair<uint, row_t>> &wght,
		    int device_id) noexcept {
  assert(0 < maxsize_batch);

#if defined(USE_MKL)
  mkl_set_num_threads_local(1);
#else
  openblas_set_num_threads(1);
#endif
  uint size_input = (sizeof(float) * maxsize_batch
		     * NNAux::nch_input * NNAux::size_plane);
  
  if (0 <= device_id) {
    get_device(device_id, _cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("bad device ID %u", device_id)); }
  else {
    get_best_device(_cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("no device found")); }
  std::cout << "- Device ID: " << device_id << "\n";
  std::cout << _cl_dev.gen_info();
  std::cout << std::endl;
  
  _queue         = _cl_dev.gen_queue();
  _resnet_nout   = wght[1].first;
  _maxsize_batch = maxsize_batch;
  _mng_send.start(_cl_dev, _queue, size_input);
  _mng_sgemm_batch_input.start(_cl_dev, _queue, true, false, size_tile_in,
			       _resnet_nout, maxsize_batch * ntile,
			       NNAux::nch_input);
  _mng_sgemm_batch.start(_cl_dev, _queue, true, false, size_tile_in,
			 _resnet_nout, maxsize_batch * ntile,
			 _resnet_nout);
  load(wght);
  for (auto &f : _fslot) f.reset(new float [maxsize_batch * _maxsize_out]);

  std::cout << "  Send:              " << _mng_send.gen_info() << "\n";
  std::cout << "  SGEMM-Batch-Input: "
	    << _mng_sgemm_batch_input.gen_info() << "\n";
  std::cout << "  SGEMM-Batch:       " << _mng_sgemm_batch.gen_info() << "\n";
  std::cout << std::endl;

  uint mmax = max(_mng_sgemm_batch_input.get_nm(), _mng_sgemm_batch.get_nm());
  uint nmax = max(_mng_sgemm_batch_input.get_nn(), _mng_sgemm_batch.get_nn());
  uint kmax = max(_mng_sgemm_batch_input.get_nk(), _mng_sgemm_batch.get_nk());
  uint sizeV = size_tile_in * kmax * nmax;
  uint sizeM = size_tile_in * mmax * nmax;
  
  _cl_matV   = _queue.gen_mem_rw(sizeof(float) * sizeV);
  _cl_matM   = _queue.gen_mem_rw(sizeof(float) * sizeM);
  _cl_bypass = _queue.gen_mem_rw(sizeof(float) * maxsize_batch * _maxsize_out);
  _cl_output = _queue.gen_mem_rw(sizeof(float) * maxsize_batch * _maxsize_out);
  row_t matV(new float [sizeV]);
  row_t matM(new float [sizeM]);
  fill_n(matV.get(), sizeV, 0.0f);
  fill_n(matM.get(), sizeM, 0.0f);
  _queue.push_write(_cl_matV, sizeof(float) * sizeV, matV.get());
  _queue.push_write(_cl_matM, sizeof(float) * sizeM, matM.get());
  _queue.finish();
  
  string code0 = "#define NCH_RESNET " + to_string(_resnet_nout) + "\n";
  string code  = (code0 + code_common
		  + code_compute_matV + code_compute_matA);
  
  OCL::Program pg = _queue.gen_program(code.c_str());
  _cl_compute_matV_input = pg.gen_kernel("compute_matV_input");
  assert(_cl_compute_matV_input.ok());
  std::cout << "  - Kernel: compute_matV_input\n";
  std::cout << _cl_compute_matV_input.gen_info();
  
  _cl_compute_matV = pg.gen_kernel("compute_matV");
  assert(_cl_compute_matV.ok());
  std::cout << "  - Kernel: compute_matV\n";
  std::cout << _cl_compute_matV.gen_info();
  
  _cl_compute_matA_BNReLU = pg.gen_kernel("compute_matA_BNReLU");
  assert(_cl_compute_matA_BNReLU.ok());
  std::cout << "  - Kernel: _cl_compute_matA_BNReLU\n";
  std::cout << _cl_compute_matA_BNReLU.gen_info();
  
  _cl_compute_matA_BNReLU_join = pg.gen_kernel("compute_matA_BNReLU_join");
  assert(_cl_compute_matA_BNReLU_join.ok());
  std::cout << "  - Kernel: _cl_compute_matA_BNReLU_join\n";
  std::cout << _cl_compute_matA_BNReLU_join.gen_info();
  std::cout.flush(); }

// in:  weight[nout][nin][size_kernel]
// ret: matrix_U[size_tile_in][nout][nin]
static row_t gen_matU(uint nout, uint nin, const float *weight,
		      uint nm, uint nk) noexcept {
  assert(0 < nout && 0 < nin && weight);
  constexpr float matG[5][3] = { +1.0f / 2.0f, +0.0f,        +0.0f,
				 -1.0f / 2.0f, -1.0f / 2.0f, -1.0f / 2.0f,
				 -1.0f / 6.0f, +1.0f / 6.0f, -1.0f / 6.0f,
				 +1.0f / 6.0f, +1.0f / 3.0f, +2.0f / 3.0f,
				 +0.0f,        +0.0f,        +1.0f };
  size_t size = size_tile_in * nm * nk;
  row_t matU(new float [size]);
  fill_n(matU.get(), size, 0.0f);
  for (uint ch_out = 0; ch_out < nout; ++ch_out)
    for (uint ch_in = 0; ch_in < nin; ++ch_in) {
      const float *mg = weight + (ch_out * nin + ch_in) * size_kernel;
      for (uint uh = 0; uh < len_tile_in; ++uh)
	for (uint uw = 0; uw < len_tile_in; ++uw) {
	  float fsum = 0.0;
	  for (uint u1 = 0; u1 < len_kernel; ++u1)
	    for (uint u2 = 0; u2 < len_kernel; ++u2)
	      fsum += matG[uh][u1] * matG[uw][u2] * mg[u1 * len_kernel + u2];
	  matU[(uh * len_tile_in + uw) * nm * nk
	       + ch_in * nm
	       + ch_out] = fsum; } }
  return matU; }

static row_t gen_mean(uint nch, const float *bias, const float *mean)
  noexcept {
  row_t row(new float [nch]);
  for (uint ch = 0; ch < nch; ++ch) row[ch] = mean[ch] * bn_factor - bias[ch];
  return row; }

static row_t gen_sd_inv(uint nch, const float *sd) noexcept {
  row_t row(new float [nch]);
  for (uint ch = 0; ch < nch; ++ch)
    row[ch] = 1.0f / std::sqrt(sd[ch] * bn_factor + bn_eps);
  return row; }

static row_t gen_head1_weight(uint nout1, uint nout2, uint nin,
			      const float *w1, const float *w2) noexcept {
  row_t row(new float [(nout1 + nout2) * nin * NNAux::size_plane]);
  copy_n(w1, nout1 * nin, row.get());
  copy_n(w2, nout2 * nin, row.get() + nout1 * nin);
  return row; }

static row_t gen_head1_mean(uint nch1, uint nch2,
			    const float *bias1, const float *mean1,
			    const float *bias2, const float *mean2) noexcept {
  row_t row(new float [nch1 + nch2]);
  for (uint ch = 0; ch < nch1; ++ch)
    row[ch] = mean1[ch] * bn_factor - bias1[ch];
  for (uint ch = 0; ch < nch2; ++ch)
    row[nch1 + ch] = mean2[ch] * bn_factor - bias2[ch];
  return row; }

static row_t gen_head1_sd_inv(uint nch1, uint nch2, const float *sd1,
			      const float *sd2) noexcept {
  row_t row(new float [nch1 + nch2]);
  for (uint ch = 0; ch < nch1; ++ch)
    row[ch] = 1.0f / std::sqrt(sd1[ch] * bn_factor + bn_eps);
  for (uint ch = 0; ch < nch2; ++ch)
    row[nch1 + ch] = 1.0f / std::sqrt(sd2[ch] * bn_factor + bn_eps);
  return row; }

void NNetOCL::load(const vector<pair<uint, row_t>> &wght) noexcept {
  constexpr uint nrow_input = 4U;
  constexpr uint nrow_head  = 14U;
  uint nrow = static_cast<uint>(wght.size());
  if (nrow < nrow_input + nrow_head) die(ERR_INT(msg_bad_wght_dim));
  
  uint nrow_res = nrow - nrow_input - nrow_head;
  if (nrow_res % 8U) die(ERR_INT(msg_bad_wght_dim));
  
  // load body part
  // for index in 0 ... nrow_res / 4
  // (4*index + 0) weight [nout][nin][size_plane]
  // (4*index + 1) bias [nout]
  // (4*index + 2) mean value [nout]
  // (4*index + 3) standard deviation [nout]
  _maxsize_out = _resnet_nout * NNAux::size_plane;
  uint nin = NNAux::nch_input;
  uint nm  = _mng_sgemm_batch_input.get_nm();
  uint nk  = _mng_sgemm_batch_input.get_nk();
  uint index = 0;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (wght[index].first != _resnet_nout * nin * size_kernel
	|| wght[index + 1U].first != _resnet_nout
	|| wght[index + 2U].first != _resnet_nout
	|| wght[index + 3U].first != _resnet_nout)
      die(ERR_INT(msg_bad_wght_dim));
    uint sizeU = size_tile_in * nm * nk;
    row_t matU = gen_matU(_resnet_nout, nin, wght[index].second.get(), nm, nk);
    OCL::Memory cl_matU   = _queue.gen_mem_rw(sizeof(float) * sizeU);
    OCL::Memory cl_mean   = _queue.gen_mem_rw(sizeof(float) * _resnet_nout);
    OCL::Memory cl_sd_inv = _queue.gen_mem_rw(sizeof(float) * _resnet_nout);
    row_t mean = gen_mean(_resnet_nout, wght[index + 1U].second.get(),
			    wght[index + 2U].second.get());
    row_t sd_inv = gen_sd_inv(_resnet_nout, wght[index + 3U].second.get());
    _queue.push_write(cl_matU,   sizeof(float) * sizeU, matU.get());
    _queue.push_write(cl_mean,   sizeof(float) * _resnet_nout, mean.get());
    _queue.push_write(cl_sd_inv, sizeof(float) * _resnet_nout, sd_inv.get());
    _cl_reswghts.push_back({move(cl_matU), move(cl_mean), move(cl_sd_inv)});
    _queue.finish();
    nin = _resnet_nout;
    nm  = _mng_sgemm_batch.get_nm();
    nk  = _mng_sgemm_batch.get_nk();
    index += 4U; }

  // load head1 part (conv 1x1)
  // index + 0: weight (policy part)
  // index + 1: bias (policy part)
  // index + 2: mean value (policy part)
  // index + 3: standard deviation (policy part)
  // index + 6: weight (value part)
  // index + 7: bias (value part)
  // index + 8: mean value (value part)
  // index + 9: standard deviation (value part)
  _policy1_nout = wght[index + 1U].first;
  _value1_nout  = wght[index + 7U].first;
  nin           = _resnet_nout;
  _head1_nout   = _policy1_nout + _value1_nout;
  _maxsize_out  = max(_maxsize_out, _head1_nout * NNAux::size_plane);
  if (wght[index].first != _policy1_nout * nin
      || wght[index + 2U].first != _policy1_nout
      || wght[index + 3U].first != _policy1_nout
      || wght[index + 6U].first != _value1_nout * nin
      || wght[index + 8U].first != _value1_nout
      || wght[index + 9U].first != _value1_nout)
    die(ERR_INT(msg_bad_wght_dim));
  _head1_weight = gen_head1_weight(_policy1_nout, _value1_nout, nin,
				   wght[index + 0U].second.get(),
				   wght[index + 6U].second.get());
  _head1_mean = gen_head1_mean(_policy1_nout, _value1_nout,
			       wght[index + 1U].second.get(),
			       wght[index + 2U].second.get(),
			       wght[index + 7U].second.get(),
			       wght[index + 8U].second.get());
  _head1_sd_inv = gen_head1_sd_inv(_policy1_nout, _value1_nout,
				   wght[index + 3U].second.get(),
				   wght[index + 9U].second.get());

  // load policy2 part (conv 1x1)
  // index + 4: weight
  // index + 5: bias
  uint nout    = NNAux::nch_out_policy;
  _policy2_nin = _policy1_nout;
  _maxsize_out = max(_maxsize_out, nout * NNAux::size_plane);
  if (wght[index + 4U].first != nout * _policy2_nin
      || wght[index + 5U].first != nout) die(ERR_INT(msg_bad_wght_dim));
  _policy2_weight.reset(new float [nout * _policy2_nin]);
  _policy2_bias.reset(new float [nout]);
  copy_n(wght[index + 4U].second.get(), nout * _policy2_nin,
	 _policy2_weight.get());
  copy_n(wght[index + 5U].second.get(), nout, _policy2_bias.get());

  // load value2 part (fc)
  // index + 10: weight
  // index + 11: bias
  _value2_nin  = _value1_nout * NNAux::size_plane;
  _value2_nout = wght[index + 11U].first;
  if (wght[index + 10U].first != _value2_nout * _value2_nin)
    die(ERR_INT(msg_bad_wght_dim));
  _value2_weight.reset(new float [_value2_nout * _value2_nin]);
  _value2_bias.reset(new float [_value2_nout]);
  copy_n(wght[index + 10U].second.get(), _value2_nout * _value2_nin,
	 _value2_weight.get());
  copy_n(wght[index + 11U].second.get(), _value2_nout, _value2_bias.get());

  // load value3 part (fc)
  // index + 12: weight
  // index + 13: bias
  _value3_nin  = _value2_nout;
  _value3_nout = wght[index + 13U].first;
  if (wght[index + 12U].first != _value3_nout * _value3_nin
      || _value3_nout != 1) die(ERR_INT(msg_bad_wght_dim));
  _value3_weight.reset(new float [_value3_nout * _value3_nin]);
  _value3_bias.reset(new float [_value3_nout]);
  copy_n(wght[index + 12U].second.get(), _value3_nout * _value3_nin,
	 _value3_weight.get());
  copy_n(wght[index + 13U].second.get(), _value3_nout, _value3_bias.get()); }

// weight[nout][nin]
// fin[nin][size_batch][size_plane]
// fout[nout][size_batch][size_plane]
// matV[size_tile_in][nin][size_batch][ntile]
// matU[size_tile_in][nout][nin]
// matM[size_tile_in][nout][size_batch][ntile]
// mean[nch]
// sd_inv[nch]
static void
compute_conv1x1(uint nout, uint nin, uint size_batch, const float *weight,
		const float *fin, float *fout) noexcept {
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
	      nout, size_batch * NNAux::size_plane, nin,
	      1.0f, weight, nin, fin, size_batch * NNAux::size_plane,
	      0.0f, fout, size_batch * NNAux::size_plane); }

static void compute_BNReLU(uint nch, uint size_batch, const float *mean,
			   const float *sd_inv, float *fio) noexcept {
  for (uint ch = 0; ch < nch; ++ch) {
    float x = mean[ch];
    float y = sd_inv[ch];
    float *f = fio + ch * size_batch * NNAux::size_plane;
    for (uint u = 0; u < size_batch * NNAux::size_plane; ++u)
      f[u] = max(0.0f, y * (f[u] - x)); } }

// in:  nnmoves[size_batch][NN::nmove]
// in:  weight[nch_out_policy][nch]
// in:  bias[nch_out_policy]
// in:  fin[nch][size_batch][size_plane]
// out: probs[size_batch][NN::nmove]
static void
compute_probs(uint nch, uint size_batch, const uint *sizes_nnmove,
	      const ushort *nnmoves, const float *weight, const float *bias,
	      const float *fin, float *probs) noexcept {
  for (uint ub = 0; ub < size_batch; ++ub) {
    const ushort *m = nnmoves + ub * NNAux::nmove;
    float *probs_b  = probs   + ub * NNAux::nmove;
    for (uint u = 0; u < sizes_nnmove[ub]; ++u) {
      assert(m[u] < NNAux::nch_out_policy * NNAux::size_plane);
      uint ch = m[u] / NNAux::size_plane;
      uint sq = m[u] % NNAux::size_plane;
      probs_b[u] = cblas_sdot(nch, weight + ch * nch, 1,
			      fin + ub * NNAux::size_plane + sq,
			      size_batch * NNAux::size_plane);
      probs_b[u] += bias[ch]; }
    softmax(sizes_nnmove[ub], probs_b); } }

static void
push_compute_matV(const OCL::Queue &queue, const OCL::Kernel &ker,
		  uint nch, uint nb, uint nn, uint nk,
		  const OCL::Memory &input,
		  const OCL::Memory &matV) noexcept {
  assert(queue.ok() && ker.ok() && 0 < nb && 0 < nn && 0 < nk && input.ok()
	 && matV.ok());
  ker.set_arg(0, sizeof(nb), &nb);
  ker.set_arg(1, sizeof(nk), &nk);
  ker.set_arg(2, sizeof(nn), &nn);
  ker.set_arg(3, input);
  ker.set_arg(4, matV);
  queue.push_kernel(ker, ntile * nch * nb); }

static void
push_compute_matA_BNReLU(const OCL::Queue &queue, const OCL::Kernel &ker,
			 uint nch, uint nb, uint nm, uint nn,
			 const OCL::Memory &matM,
			 const OCL::Memory &mean, const OCL::Memory &sd_inv,
			 const OCL::Memory &output) noexcept {
  assert(queue.ok() && ker.ok() && 0 < nch && 0 < nb
	 && matM.ok() && mean.ok() && sd_inv.ok() && output.ok());
  uint offc = nm * nn;
  ker.set_arg(0, sizeof(nb),   &nb);
  ker.set_arg(1, sizeof(offc), &offc);
  ker.set_arg(2, sizeof(nn),   &nn);
  ker.set_arg(3, matM);
  ker.set_arg(4, mean);
  ker.set_arg(5, sd_inv);
  ker.set_arg(6, output);
  queue.push_kernel(ker, nch * nb * ntile); }

void NNetOCL::ff(uint size_batch, const float *input, const uint *sizes_nnmove,
		 const ushort *nnmoves, float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);
  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("size_batch == 0"));

  // feed forward input layers
  // body part
  float *fbody = _fslot[0].get();
  float *f1    = _fslot[1].get();
  /*
  _queue.push_write(_cl_input,
		     size_batch * NNAux::nch_input * NNAux::size_plane
		     * sizeof(float), input);
		     */
  /*
  copy_n(input, size_batch * NNAux::nch_input * NNAux::size_plane,
	 static_cast<float *>(_cl_input_ptr));
  _queue.push_write(_cl_input,
		     size_batch * NNAux::nch_input * NNAux::size_plane
		     * sizeof(float), _cl_input_ptr);
		     */
  _mng_send.push(_queue, size_batch * NNAux::nch_input * NNAux::size_plane
		 * sizeof(float), input);
  push_compute_matV(_queue, _cl_compute_matV_input, NNAux::nch_input,
		    size_batch,
		    _mng_sgemm_batch_input.get_nn(),
		    _mng_sgemm_batch_input.get_nk(),
		    _mng_send.get(), _cl_matV);
  /*
  row_t row(new float[size_tile_in
		      * ceil_multi(NNAux::nch_input, sgemm_k_multi)
		      * ceil_multi(size_batch * ntile, sgemm_n_multi)]);
  _queue.push_read(_cl_matV,
		    size_tile_in
		    * ceil_multi(NNAux::nch_input, sgemm_k_multi)
		    * ceil_multi(size_batch * ntile, sgemm_n_multi)
		    * sizeof(float), row.get());
  _queue.finish();
  for (uint u0 = 0; u0 < size_tile_in; ++u0)
    for (uint u1 = 0; u1 < NNAux::nch_input; ++u1)
      for (uint u2 = 0; u2 < size_batch * ntile; ++u2)
	std::cout << u0 << " "
		  << row[u0
			 * ceil_multi(NNAux::nch_input, sgemm_k_multi)
			 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u1 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u2]
		  << std::endl;
		  std::terminate(); */

  _mng_sgemm_batch_input.push(_queue,
			      _cl_reswghts[0].matU, _cl_matV, _cl_matM);
  /*
  row_t row(new float[size_tile_in
		      * ceil_multi(_resnet_nout, sgemm_m_multi)
		      * ceil_multi(size_batch * ntile, sgemm_n_multi)]);
  _queue.push_read(_cl_matM,
		    size_tile_in
		    * ceil_multi(_resnet_nout, sgemm_m_multi)
		    * ceil_multi(size_batch * ntile, sgemm_n_multi)
		    * sizeof(float), row.get());
  _queue.finish();
  for (uint u0 = 0; u0 < size_tile_in; ++u0)
    for (uint u1 = 0; u1 < _resnet_nout; ++u1)
      for (uint u2 = 0; u2 < size_batch * ntile; ++u2)
	std::cout << u0 << " "
		  << row[u0
			 * ceil_multi(_resnet_nout, sgemm_m_multi)
			 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u1 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u2]
		  << std::endl;
		  std::terminate(); */
  
  push_compute_matA_BNReLU(_queue, _cl_compute_matA_BNReLU,
			   _resnet_nout, size_batch,
			   _mng_sgemm_batch_input.get_nm(),
			   _mng_sgemm_batch_input.get_nn(),
			   _cl_matM,
			   _cl_reswghts[0].mean, _cl_reswghts[0].sd_inv,
			   _cl_bypass);

  uint ulayer = 1U;
  for (; ulayer < _cl_reswghts.size(); ulayer += 2U) {
    push_compute_matV(_queue, _cl_compute_matV, _resnet_nout, size_batch,
		      _mng_sgemm_batch.get_nn(), _mng_sgemm_batch.get_nk(),
		      _cl_bypass, _cl_matV);

    /*
  row_t row(new float[size_tile_in
		      * ceil_multi(_resnet_nout, sgemm_k_multi)
		      * ceil_multi(size_batch * ntile, sgemm_n_multi)]);
  _queue.push_read(_cl_matV,
		    size_tile_in
		    * ceil_multi(_resnet_nout, sgemm_k_multi)
		    * ceil_multi(size_batch * ntile, sgemm_n_multi)
		    * sizeof(float), row.get());
  _queue.finish();
  for (uint u0 = 0; u0 < size_tile_in; ++u0)
    for (uint u1 = 0; u1 < _resnet_nout; ++u1)
      for (uint u2 = 0; u2 < size_batch * ntile; ++u2)
	std::cout << u0 << " "
		  << row[u0
			 * ceil_multi(_resnet_nout, sgemm_k_multi)
			 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u1 * ceil_multi(size_batch * ntile, sgemm_n_multi)
			 + u2]
		  << std::endl;
		  std::terminate(); */

    _mng_sgemm_batch.push(_queue,
			  _cl_reswghts[ulayer].matU, _cl_matV, _cl_matM);
    push_compute_matA_BNReLU(_queue, _cl_compute_matA_BNReLU,
			     _resnet_nout, size_batch,
			     _mng_sgemm_batch.get_nm(),
			     _mng_sgemm_batch.get_nn(),
			     _cl_matM, _cl_reswghts[ulayer].mean,
			     _cl_reswghts[ulayer].sd_inv, _cl_output);

    push_compute_matV(_queue, _cl_compute_matV, _resnet_nout, size_batch,
		      _mng_sgemm_batch.get_nn(), _mng_sgemm_batch.get_nk(),
 		      _cl_output, _cl_matV);
    _mng_sgemm_batch.push(_queue,
			  _cl_reswghts[ulayer + 1U].matU, _cl_matV, _cl_matM);
    push_compute_matA_BNReLU(_queue, _cl_compute_matA_BNReLU_join,
			     _resnet_nout, size_batch,
			     _mng_sgemm_batch.get_nm(),
			     _mng_sgemm_batch.get_nn(),
			     _cl_matM, _cl_reswghts[ulayer + 1U].mean,
			     _cl_reswghts[ulayer + 1U].sd_inv, _cl_bypass); }

  _queue.push_read(_cl_bypass,
		    _resnet_nout * size_batch * NNAux::size_plane
		    * sizeof(float), fbody);
  _queue.finish();
  /*
  for (uint u0 = 0; u0 < _resnet_nout; ++u0)
    for (uint u1 = 0; u1 < size_batch * NNAux::size_plane; ++u1)
	std::cout << u0 << " "
		  << fbody[u0 * size_batch * NNAux::size_plane + u1]
		  << std::endl;
  std::terminate();
  */
  // head part
  compute_conv1x1(_head1_nout, _resnet_nout, size_batch, _head1_weight.get(),
		  fbody, f1);
  compute_BNReLU(_head1_nout, size_batch, _head1_mean.get(),
		 _head1_sd_inv.get(), f1);
  
  compute_probs(_policy2_nin, size_batch, sizes_nnmove, nnmoves,
		_policy2_weight.get(), _policy2_bias.get(), f1, probs);
  

  // in:  f1[_policy1_nout + _value1_nout][size_batch][size_plane]
  // out: f2[size_batch][_value1_nout][size_plane]
  float *f2 = fbody;
  for (uint ub = 0; ub < size_batch; ++ub)
    for (uint ch = 0; ch < _value1_nout; ++ch) {
      uint ch2 = _policy1_nout + ch;
      const float *src = f1 + (ch2 * size_batch + ub) * NNAux::size_plane;
      float *dest      = f2 + (ub * _value1_nout + ch) * NNAux::size_plane;
      copy_n(src, NNAux::size_plane, dest); }

  // in:  _value2_weight[_value2_nout][_value2_nin]
  // in:  _value2_bias[_value2_nout]
  // in:  f1[size_batch][_value2_nin]
  // out: f2[size_batch][_value2_nout]
  swap(f1, f2);
  for (uint ub = 0; ub < size_batch; ++ub)
    copy_n(_value2_bias.get(), _value2_nout, f2 + ub * _value2_nout);
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
	      size_batch, _value2_nout, _value2_nin,
	      1.0f, f1, _value2_nin, _value2_weight.get(), _value2_nin,
	      1.0f, f2, _value2_nout);
  for (uint u = 0; u < size_batch * _value2_nout; ++u)
    f2[u] = max(0.0f, f2[u]);
  /*
  for (uint u = 0; u < size_batch * _value2_nout; ++u)
    std::cout << f2[u] << std::endl;
  std::terminate();
  */

  // in:  _value3_weight[_value3_nin]
  // in:  _value3_bias[1]
  // in:  f2[size_batch][_value3_nin]
  // out: values[size_batch]
  fill_n(values, size_batch, _value3_bias[0]);
  cblas_sgemv(CblasRowMajor, CblasNoTrans, size_batch, _value3_nin,
	      1.0f, f2, _value3_nin, _value3_weight.get(), 1, 1.0f, values, 1);
  for (uint ub = 0; ub < size_batch; ++ub) values[ub] = std::tanh(values[ub]);
}
#endif
