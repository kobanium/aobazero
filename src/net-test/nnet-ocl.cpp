// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(USE_OPENCL)
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ocl.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <cmath>
#if defined(USE_MKL)
#  include <mkl.h>
#elif defined(USE_OPENBLAS)
#  include <cblas.h>
#else
#  error "NO BLAS"
#endif
using std::copy_n;
using std::fill_n;
using std::forward;
using std::max;
using std::max_element;
using std::move;
using std::pair;
using std::string;
using std::to_string;
using std::swap;
using std::unique_ptr;
using std::vector;
using row_t  = unique_ptr<float []>;
using ushort = unsigned short;
using namespace ErrAux;

#include <chrono>
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::microseconds;
static double elapsed_sum = 0.0;
static uint nelapsed      = 0;

constexpr char msg_bad_wght_dim[] = "bad weight dimension";
constexpr uint len_kernel    = 3U;
constexpr uint size_kernel   = len_kernel * len_kernel;
constexpr uint len_tile_out  = 3U;
constexpr uint len_tile_in   = 5U;
constexpr uint size_tile_in  = len_tile_in * len_tile_in;
constexpr uint ntile_h       = 3U;
constexpr uint ntile_w       = 3U;
constexpr uint ntile         = ntile_h * ntile_w;
constexpr uint size_plane_in = size_tile_in * ntile;
constexpr uint pad           = 1U;
constexpr uint matM_pwM      = 8U;
constexpr uint matM_pwN      = 2U;

constexpr uint matM_lwMN     = 4U;
constexpr uint matM_lwK      = 16U;

constexpr float bn_factor    = 1.0f / 999.982f;
constexpr float bn_eps       = 1e-5f;

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
  "#define PAD "          + to_string(pad)               + "\n"
  "#define MATM_PWM "     + to_string(matM_pwM)          + "\n"
  "#define MATM_PWN "     + to_string(matM_pwN)          + "\n"
  "#define MATM_LWMN "    + to_string(matM_lwMN)         + "\n"
  "#define MATM_LWK "     + to_string(matM_lwK)          + R"(
#define uint unsigned int
float x1(float x) { return x; }
float x2(float x) { return x + x; }
float x3(float x) { return x + x + x; }
float x4(float x) { x += x; return x + x; }
float x6(float x) { x += x; return x + x + x; }
float x9(float x) { float y = x + x; y += y; return y + y + x; }
)";

const string code_compute_matV = R"(
void compute_matV_child(uint nchxnb, uint chb, uint uh, uint uw,
                        __global const float *fin, __global float *matV) {
  uint uca = NTILE * nchxnb * LEN_TILE_IN;
  uint ucb = NTILE * nchxnb;
  uint ucc = chb * NTILE + uh * NTILE_W + uw;
  int y0 = uh * LEN_TILE_OUT - PAD;
  int x0 = uw * LEN_TILE_OUT - PAD;
  float md[LEN_TILE_IN][LEN_TILE_IN];
  for (int y = 0; y < LEN_TILE_IN; ++y)
    for (int x = 0; x < LEN_TILE_IN; ++x)
      if (0 <= y0 + y && y0 + y < HEIGHT && 0 <= x0 + x && x0 + x < WIDTH)
        md[y][x] = fin[(y0 + y) * WIDTH + x0 + x];
      else md[y][x] = 0.0f;

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

__kernel void compute_matV_input(uint nb, __global const float *fin,
                                 __global float *matV) {
  uint nchxnb = NCH_INPUT * nb;
  uint gid    = get_global_id(0);
  uint loop   = gid / NTILE;
  uint utile  = gid % NTILE;
  uint ub     = loop / NCH_INPUT;
  uint ch     = loop % NCH_INPUT;
  uint chb    = ch * nb + ub;
  uint uh     = utile / NTILE_W;
  uint uw     = utile % NTILE_W;
  __global const float *fin_off = fin + loop * SIZE_PLANE;
  compute_matV_child(nchxnb, chb, uh, uw, fin_off, matV); }

__kernel void compute_matV(uint nb, __global const float *fin,
                           __global float *matV) {
  uint nchxnb = NCH_RESNET * nb;
  uint gid    = get_global_id(0);
  uint chb    = gid / NTILE;
  uint utile  = gid % NTILE;
  uint uh     = utile / NTILE_W;
  uint uw     = utile % NTILE_W;
  fin        += chb * SIZE_PLANE;
  compute_matV_child(nchxnb, chb, uh, uw, fin, matV); }
)";

const string code_compute_matA = R"(
float compute_BNReLU(float sd_inv, float mean, float x) {
  return max(0.0f, sd_inv * (x - mean)); }

__kernel void compute_matA_BNReLU(uint nb,
                                  __global const float *matM,
                                  __global const float *mean_array,
                                  __global const float *sd_inv_array,
                                  __global float *fout) {
  uint gid    = get_global_id(0);
  uint size_g = NTILE * NCH_RESNET * nb;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * size_g + gid];

  uint chb     = gid / NTILE;
  uint utile   = gid % NTILE;
  uint ch      = chb / nb;
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

__kernel void compute_matA_BNReLU_join(uint nb,
                                       __global const float *matM,
                                       __global const float *mean_array,
                                       __global const float *sd_inv_array,
                                       __global float *fout) {
  uint gid    = get_global_id(0);
  uint size_g = NTILE * NCH_RESNET * nb;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * size_g + gid];

  uint chb     = gid / NTILE;
  uint utile   = gid % NTILE;
  uint ch      = chb / nb;
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

const string code_compute_matM = R"(
__kernel void compute_matM_input(uint nn, __global const float *matA,
                                 __global const float *matB,
                                 __global float *matC) {
  uint nm    = NCH_RESNET;
  uint nk    = NCH_INPUT;
  uint areac = nm * nn;
  uint gid   = get_global_id(0);
  uint uloc  = gid / areac;
  uint uc    = gid % areac;
  uint um    = uc / nn;
  uint un    = uc % nn;
  matA      += uloc * nm * nk + um * nk;
  matB      += uloc * nk * nn + un;
  float sum  = 0.0f;
  for (uint uk = 0; uk < nk; ++uk) {
    sum  += *matA * *matB;
    matA += 1;
    matB += nn; }
  matC[gid] = sum; }

/*
__kernel void compute_matM(uint nn, __global const float *matA,
                           __global const float *matB, __global float *matC) {
  uint nm    = NCH_RESNET;
  uint nk    = NCH_RESNET;
  uint areac = nm * nn;
  uint gid   = get_global_id(0);
  uint uloc  = gid / areac;
  uint uc    = gid % areac;
  uint um    = uc / nn;
  uint un    = uc % nn;
  matA      += uloc * nm * nk + um * nk;
  matB      += uloc * nk * nn + un;
  float sum  = 0.0f;
  for (uint uk = 0; uk < nk; ++uk) {
    sum  += *matA * *matB;
    matA += 1;
    matB += nn; }
  matC[gid] = sum; } */

__kernel void compute_matM_l(uint nn, __global const float *gA,
                             __global const float *gB, __global float *gC) {
  uint nm   = NCH_RESNET;
  uint nk   = NCH_RESNET;
  uint uloc = get_global_id(0);
  uint um   = get_global_id(1);
  uint un   = get_global_id(2);
  uint ulm  = get_group_id(1);
  uint uln  = get_group_id(2);
  uint um1  = get_local_id(1);
  uint un1  = get_local_id(2);
  uint um0  = ulm * MATM_LWMN;
  uint un0  = uln * MATM_LWMN;
  uint nlk  = nk / MATM_LWK;
  uint lpt  = MATM_LWK / MATM_LWMN;
  gA       += uloc * nm * nk + um0 * nk;
  gB       += uloc * nk * nn + un0;
  gC       += uloc * nm * nn + um0 * nn + un0;

  __local float lA[MATM_LWMN][MATM_LWK];
  __local float lB[MATM_LWK][MATM_LWMN];
  float sum = 0.0f;
  for (uint ulk = 0; ulk < nlk; ++ulk) {

    for (uint u = 0; u < lpt; ++u) {
      lA[um1][un1*lpt + u] = gA[um1*nk + un1*lpt + u];
      lB[um1*lpt + u][un1] = gB[(um1*lpt + u) * nn + un1]; }

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint uk = 0; uk < MATM_LWK; ++uk)
      sum += lA[um1][uk] * lB[uk][un1];
    barrier(CLK_LOCAL_MEM_FENCE);

    gA += MATM_LWK;
    gB += MATM_LWK * nn; }
  gC[um1*nn + un1] = sum; }

/* (NCH_RESNET % MATM_PWM) == 0 */
/* (nn         % MATM_PWN) == 0 */
__kernel void compute_matM_p(uint nn, __global const float *gA,
                             __global const float *gB, __global float *gC) {
  uint nm   = NCH_RESNET;
  uint nk   = NCH_RESNET;
  uint npm  = NCH_RESNET / MATM_PWM;
  uint npn  = nn / MATM_PWN;
  uint npc  = npm * npn;
  uint gid  = get_global_id(0);
  uint uloc = gid / npc;
  uint upc  = gid % npc;
  uint upm  = upc / npn;
  uint upn  = upc % npn;
  uint um0  = upm * MATM_PWM;
  uint un0  = upn * MATM_PWN;
  gA       += uloc * nm * nk + um0 * nk;
  gB       += uloc * nk * nn + un0;
  gC       += uloc * nm * nn + um0 * nn + un0;
  float pC[MATM_PWM][MATM_PWN];
  for (uint um1 = 0; um1 < MATM_PWM; ++um1)
    for (uint un1 = 0; un1 < MATM_PWN; ++un1)
      pC[um1][un1] = 0.0f;

  for (uint uk = 0; uk < nk; ++uk) {

    float pB[MATM_PWN];
    for (uint un1 = 0; un1 < MATM_PWN; ++un1)
      pB[un1] = gB[un1];

    for (uint um1 = 0; um1 < MATM_PWM; ++um1) {
      float pA = gA[um1*nk];
      for (uint un1 = 0; un1 < MATM_PWN; ++un1)
        pC[um1][un1] += pA * pB[un1]; }

    gA += 1U;
    gB += nn; }

  for (uint um1 = 0; um1 < MATM_PWM; ++um1) {
    for (uint un1 = 0; un1 < MATM_PWN; ++un1) gC[un1] = pC[um1][un1];
    gC += nn; } }
/*
__kernel void sgemm_PWM_PWN(uint nn, __global const float *gA,
                            __global const float *gB, __global float *gC) {
  uint nm   = NCH_RESNET;
  uint nk   = NCH_RESNET;
  uint npm  = NCH_RESNET / MATM_PWM;
  uint npn  = nn / MATM_PWN;
  uint npc  = npm * npn;
  uint gid  = get_global_id(0);
  uint uloc = gid / npc;
  uint upc  = gid % npc;
  uint upm  = upc / npn;
  uint upn  = upc % npn;
  uint um0  = upm * MATM_PWM;
  uint un0  = upn * MATM_PWN;
  gA       += uloc * nm * nk + um0 * nk;
  gB       += uloc * nk * nn + un0;
  gC       += uloc * nm * nn + um0 * nn + un0;
  float pC[MATM_PWM][MATM_PWN];
  for (uint um1 = 0; um1 < MATM_PWM; ++um1)
    for (uint un1 = 0; un1 < MATM_PWN; ++un1)
      pC[um1][un1] = 0.0f;

  for (uint uk = 0; uk < nk; ++uk) {

    float pB[MATM_PWN];
    for (uint un1 = 0; un1 < MATM_PWN; ++un1)
      pB[un1] = gB[un1];

    for (uint um1 = 0; um1 < MATM_PWM; ++um1) {
      float pA = gA[um1*nk];
      for (uint un1 = 0; un1 < MATM_PWN; ++un1)
        pC[um1][un1] += pA * pB[un1]; }

    gA += 1U;
    gB += nn; }

  for (uint um1 = 0; um1 < MATM_PWM; ++um1) {
    for (uint un1 = 0; un1 < MATM_PWN; ++un1) gC[un1] = pC[um1][un1];
    gC += nn; } }*/
)";

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

//struct ResWght { row_t matU, mean, sd_inv; };
//std::vector<ResWght> _reswghts;
void NNetOCL::reset(uint maxsize_batch, const vector<pair<uint, row_t>> &wght,
		    int device_id) noexcept {
  assert(0 < maxsize_batch);

#if defined(USE_MKL)
  mkl_set_num_threads_local(1);
#else
  openblas_set_num_threads(1);
#endif
  _maxsize_batch = maxsize_batch;
  
  if (0 <= device_id) {
    get_device(device_id, _cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("bad device ID %u", device_id)); }
  else {
    get_best_device(_cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("no device found")); }
  std::cout << "- Device ID: " << device_id << "\n";
  std::cout << _cl_dev.gen_info();
  std::cout.flush();

  load(wght);
  for (auto &f : _fslot) f.reset(new float [_maxsize_batch * _maxsize_out]);
  
  _cl_input  = _cl_dev.gen_mem_r(sizeof(float) * _maxsize_batch
				 * NNAux::nch_input * NNAux::size_plane);
  _cl_matV   = _cl_dev.gen_mem_r(sizeof(float) * _maxsize_batch
				 * _conv3x3_nin_max * size_plane_in);
  _cl_matM   = _cl_dev.gen_mem_r(sizeof(float) * _maxsize_batch
				 * _conv3x3_nout_max * size_plane_in);
  _cl_bypass = _cl_dev.gen_mem_rw(sizeof(float) * _maxsize_batch
				  * _maxsize_out);
  _cl_output = _cl_dev.gen_mem_rw(sizeof(float) * _maxsize_batch
				  * _maxsize_out);

  string code0 = "#define NCH_RESNET " + to_string(_resnet_nout) + "\n";
  string code  = (code0 + code_common + code_compute_matM
		  + code_compute_matV + code_compute_matA);
  
  _cl_dev.build_program(code.c_str());
  _cl_compute_matV_input = _cl_dev.gen_kernel("compute_matV_input");
  assert(_cl_compute_matV_input.ok());
  std::cout << "  - Kernel: compute_matV_input\n";
  std::cout << _cl_compute_matV_input.gen_info();
  
  _cl_compute_matM_input = _cl_dev.gen_kernel("compute_matM_input");
  assert(_cl_compute_matM_input.ok());
  std::cout << "  - Kernel: compute_matM_input\n";
  std::cout << _cl_compute_matM_input.gen_info();
  
  _cl_compute_matV = _cl_dev.gen_kernel("compute_matV");
  assert(_cl_compute_matV.ok());
  std::cout << "  - Kernel: compute_matV\n";
  std::cout << _cl_compute_matV.gen_info();
  
  _cl_compute_matM = _cl_dev.gen_kernel("compute_matM_p");
  assert(_cl_compute_matM.ok());
  std::cout << "  - Kernel: compute_matM_p\n";
  std::cout << _cl_compute_matM.gen_info();
  
  _cl_compute_matA_BNReLU = _cl_dev.gen_kernel("compute_matA_BNReLU");
  assert(_cl_compute_matA_BNReLU.ok());
  std::cout << "  - Kernel: _cl_compute_matA_BNReLU\n";
  std::cout << _cl_compute_matA_BNReLU.gen_info();
  
  _cl_compute_matA_BNReLU_join
    = _cl_dev.gen_kernel("compute_matA_BNReLU_join");
  assert(_cl_compute_matA_BNReLU_join.ok());
  std::cout << "  - Kernel: _cl_compute_matA_BNReLU_join\n";
  std::cout << _cl_compute_matA_BNReLU_join.gen_info();
  std::cout.flush(); }

// in:  weight[nout][nin][size_kernel]
// ret: matrix_U[size_tile_in][nout][nin]
static row_t gen_matU(uint nout, uint nin, const float *weight) {
  assert(0 < nout && 0 < nin && weight);
  constexpr float matG[5][3] = { +1.0f / 2.0f, +0.0f,        +0.0f,
				 -1.0f / 2.0f, -1.0f / 2.0f, -1.0f / 2.0f,
				 -1.0f / 6.0f, +1.0f / 6.0f, -1.0f / 6.0f,
				 +1.0f / 6.0f, +1.0f / 3.0f, +2.0f / 3.0f,
				 +0.0f,        +0.0f,        +1.0f };

  row_t matU(new float [size_tile_in * nout * nin]);
  const uint nout_in = nout * nin;
  for (uint ch_io = 0; ch_io < nout_in; ++ch_io) {
    const float *mg = weight + ch_io * size_kernel;
    for (uint uh = 0; uh < len_tile_in; ++uh)
      for (uint uw = 0; uw < len_tile_in; ++uw) {
	float fsum = 0.0;
	for (uint u1 = 0; u1 < len_kernel; ++u1)
	  for (uint u2 = 0; u2 < len_kernel; ++u2)
	    fsum += matG[uh][u1] * matG[uw][u2] * mg[u1 * len_kernel + u2];
	matU[(uh * len_tile_in + uw) * nout_in + ch_io] = fsum; } }
  
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
  _resnet_nout      = wght[1].first;
  _conv3x3_nin_max  = max(NNAux::nch_input, _resnet_nout);
  _conv3x3_nout_max = _resnet_nout;
  _maxsize_out      = _resnet_nout * NNAux::size_plane;
  uint nin = NNAux::nch_input;
  uint index = 0;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (wght[index].first != _resnet_nout * nin * size_kernel
	|| wght[index + 1U].first != _resnet_nout
	|| wght[index + 2U].first != _resnet_nout
	|| wght[index + 3U].first != _resnet_nout)
      die(ERR_INT(msg_bad_wght_dim));
    size_t size_matU = size_tile_in * _resnet_nout * nin;
    OCL::Memory cl_matU   = _cl_dev.gen_mem_rw(sizeof(float) * size_matU);
    OCL::Memory cl_mean   = _cl_dev.gen_mem_rw(sizeof(float) * _resnet_nout);
    OCL::Memory cl_sd_inv = _cl_dev.gen_mem_rw(sizeof(float) * _resnet_nout);
    row_t matU   = gen_matU(_resnet_nout, nin, wght[index].second.get());
    row_t mean   = gen_mean(_resnet_nout, wght[index + 1U].second.get(),
			    wght[index + 2U].second.get());
    row_t sd_inv = gen_sd_inv(_resnet_nout, wght[index + 3U].second.get());
    _cl_dev.push_write(cl_matU,   sizeof(float) * size_matU, matU.get());
    _cl_dev.push_write(cl_mean,   sizeof(float) * _resnet_nout, mean.get());
    _cl_dev.push_write(cl_sd_inv, sizeof(float) * _resnet_nout, sd_inv.get());
    _cl_reswghts.push_back({move(cl_matU), move(cl_mean), move(cl_sd_inv)});
    _cl_dev.finish();
    nin = _resnet_nout;
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

static void push_compute_matV_input(uint nb,
				    const OCL::Device &dev,
				    const OCL::Kernel &ker,
				    const OCL::Memory &input,
				    const OCL::Memory &matV) noexcept {
  assert(0 < nb && dev.ok() && ker.ok() && input.ok() && matV.ok());
  ker.set_arg(0, sizeof(nb), &nb);
  ker.set_arg(1, input);
  ker.set_arg(2, matV);
  dev.push_kernel(ker, ntile * NNAux::nch_input * nb); }

static void push_compute_matM_input(uint nout, uint nb,
				    const OCL::Device &dev,
				    const OCL::Kernel &ker,
				    const OCL::Memory &matU,
				    const OCL::Memory &matV,
				    const OCL::Memory &matM) noexcept {
  assert(0 < nout && 0 < nb && dev.ok() && ker.ok() && matU.ok() && matV.ok()
	 && matM.ok());
  uint nn = nb * ntile;
  ker.set_arg(0, sizeof(nn), &nn);
  ker.set_arg(1, matU);
  ker.set_arg(2, matV);
  ker.set_arg(3, matM);
  dev.push_kernel(ker, size_tile_in * nout * nb * ntile); }

static void push_compute_matV(uint nin, uint nb,
			      const OCL::Device &dev,
			      const OCL::Kernel &ker,
			      const OCL::Memory &input,
			      const OCL::Memory &matV) noexcept {
  assert(0 < nin && 0 < nb && dev.ok() && ker.ok() && input.ok()
	 && matV.ok());
  ker.set_arg(0, sizeof(nb), &nb);
  ker.set_arg(1, input);
  ker.set_arg(2, matV);
  dev.push_kernel(ker, ntile * nin * nb); }

static void push_compute_matM(uint nout, uint nb,
			      const OCL::Device &dev,
			      const OCL::Kernel &ker,
			      const OCL::Memory &matU,
			      const OCL::Memory &matV,
			      const OCL::Memory &matM) noexcept {
  assert(0 < nout && 0 < nb && dev.ok() && ker.ok() && matU.ok() && matV.ok()
	 && matM.ok());
  /*
  uint nn = nb * ntile;
  ker.set_arg(0, sizeof(nn), &nn);
  ker.set_arg(1, matU);
  ker.set_arg(2, matV);
  ker.set_arg(3, matM);
  assert((nout % matM_lwMN) == 0
	 && (nout % matM_lwK) == 0
	 && (nn % matM_lwMN) == 0);
  size_t size_g[3] = { size_tile_in, nout, nn };
  size_t size_l[3] = { 1U, matM_lwMN, matM_lwMN };
  

  dev.enqueue_kernel(ker, 3U, size_g, size_l);
  dev.push_barrier();
  */
    
  uint nn = nb * ntile;
  assert((nout % matM_pwM) == 0 && (nn % matM_pwN) == 0);
  ker.set_arg(0, sizeof(nn), &nn);
  ker.set_arg(1, matU);
  ker.set_arg(2, matV);
  ker.set_arg(3, matM);
  /*
  dev.finish();
  steady_clock::time_point start = steady_clock::now();
  */	     
  dev.push_kernel(ker, (size_tile_in * (nout / matM_pwM) * (nn / matM_pwN)));
  /*
  dev.finish();
  steady_clock::time_point end = steady_clock::now();
  double elapsed
    = static_cast<double>(duration_cast<microseconds>(end - start).count());
  elapsed_sum += elapsed;
  nelapsed    += 1U;
  std::cout << std::endl;
  std::cout << elapsed << std::endl;
  std::cout << elapsed_sum / static_cast<double>(nelapsed) << std::endl;
  std::cout << std::endl; */
}

static void push_compute_matA_BNReLU(uint nout, uint nb,
				     const OCL::Device &dev,
				     const OCL::Kernel &ker,
				     const OCL::Memory &matM,
				     const OCL::Memory &mean,
				     const OCL::Memory &sd_inv,
				     const OCL::Memory &output) {
  assert(0 < nout && 0 < nb && dev.ok() && ker.ok() && matM.ok()
	 && output.ok());
  ker.set_arg(0, sizeof(nb), &nb);
  ker.set_arg(1, matM);
  ker.set_arg(2, mean);
  ker.set_arg(3, sd_inv);
  ker.set_arg(4, output);
  dev.push_kernel(ker, ntile * nout * nb); }

static void push_compute_matA_BNReLU_join(uint nout, uint nb,
					  const OCL::Device &dev,
					  const OCL::Kernel &ker,
					  const OCL::Memory &matM,
					  const OCL::Memory &mean,
					  const OCL::Memory &sd_inv,
					  const OCL::Memory &output) {
  assert(0 < nout && 0 < nb && dev.ok() && ker.ok() && matM.ok()
	 && mean.ok() && sd_inv.ok() && bypass.ok());
  ker.set_arg(0, sizeof(nb), &nb);
  ker.set_arg(1, matM);
  ker.set_arg(2, mean);
  ker.set_arg(3, sd_inv);
  ker.set_arg(4, output);
  dev.push_kernel(ker, ntile * nout * nb); }

void NNetOCL::ff(uint size_batch, const float *input, const uint *sizes_nnmove,
		 const ushort *nnmoves, float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);
  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("size_batch == 0"));
  // feed forward input layers
  // body part
  float *fbody = _fslot[0].get();
  float *f1    = _fslot[1].get();
  
  _cl_dev.push_write(_cl_input,
		     size_batch * NNAux::nch_input * NNAux::size_plane
		     * sizeof(float), input);
  push_compute_matV_input(size_batch, _cl_dev, _cl_compute_matV_input,
			  _cl_input, _cl_matV);
  push_compute_matM_input(_resnet_nout, size_batch, _cl_dev,
			  _cl_compute_matM_input, _cl_reswghts[0].matU,
			  _cl_matV, _cl_matM);
  push_compute_matA_BNReLU(_resnet_nout, size_batch, _cl_dev,
			   _cl_compute_matA_BNReLU, _cl_matM,
			   _cl_reswghts[0].mean, _cl_reswghts[0].sd_inv,
			   _cl_bypass);

  uint ulayer = 1U;
  for (; ulayer < _cl_reswghts.size(); ulayer += 2U) {
    push_compute_matV(_resnet_nout, size_batch, _cl_dev, _cl_compute_matV,
		      _cl_bypass, _cl_matV);
    push_compute_matM(_resnet_nout, size_batch, _cl_dev,
			  _cl_compute_matM,
			  _cl_reswghts[ulayer].matU, _cl_matV, _cl_matM);
    push_compute_matA_BNReLU(_resnet_nout, size_batch, _cl_dev,
			     _cl_compute_matA_BNReLU, _cl_matM,
			     _cl_reswghts[ulayer].mean,
			     _cl_reswghts[ulayer].sd_inv,
			     _cl_output);

    push_compute_matV(_resnet_nout, size_batch, _cl_dev, _cl_compute_matV,
		      _cl_output, _cl_matV);
    push_compute_matM(_resnet_nout, size_batch, _cl_dev,
			  _cl_compute_matM,
			  _cl_reswghts[ulayer + 1U].matU, _cl_matV, _cl_matM);
    push_compute_matA_BNReLU_join(_resnet_nout, size_batch, _cl_dev,
				  _cl_compute_matA_BNReLU_join, _cl_matM,
				  _cl_reswghts[ulayer + 1U].mean,
				  _cl_reswghts[ulayer + 1U].sd_inv,
				  _cl_bypass); }

  _cl_dev.push_read(_cl_bypass,
		    _resnet_nout * size_batch * NNAux::size_plane
		    * sizeof(float),
		    fbody);
  _cl_dev.finish();

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
