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
#include <tuple>
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
using std::make_tuple;
using std::max;
using std::min;
using std::max_element;
using std::move;
using std::pair;
using std::sort;
using std::string;
using std::tie;
using std::to_string;
using std::tuple;
using std::deque;
using std::set;
using std::swap;
using std::unique_ptr;
using std::vector;
using row_t  = unique_ptr<float []>;
using uint   = unsigned int;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;
static_assert(sizeof(float) == sizeof(uint), "sizeof(float) == sizeof(uint)");
static_assert(sizeof(float) == sizeof(ushort) * 2U,
	      "sizeof(float) == sizeof(ushort) * 2U");

static double elapsed_sum = 0.0;
static uint nelapsed      = 0;

constexpr char msg_bad_wght_dim[] = "bad weight dimension";
constexpr uint tune_sample_size   = 128U;
constexpr uint tune_sleep         = 1000U; // usec
constexpr uint size_wrap_wmma     = 32U;
constexpr uint len_kernel         = 3U;
constexpr uint size_kernel        = len_kernel * len_kernel;
constexpr uint len_tile_out       = 3U;
constexpr uint len_tile_in        = 5U;
constexpr uint size_tile_in       = len_tile_in * len_tile_in;
constexpr uint ntile_h            = 3U;
constexpr uint ntile_w            = 3U;
constexpr uint ntile              = ntile_h * ntile_w;
constexpr uint size_plane_in      = size_tile_in * ntile;
constexpr uint pad                = 1U;
constexpr uint send_size_ave      = 3000U;
constexpr uint read_size_ave      = 600U;
constexpr uint send_nch_fill      = 17U * 8U + 2U;
constexpr float bn_factor         = 1.0f / 999.982f;
constexpr float bn_eps            = 1e-5f;
/*
  _queue.finish();
  steady_clock::time_point start = steady_clock::now();
  _queue.finish();
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

constexpr char code_zero_clear[] = R"(
__kernel void zero_clear(__global float *p) { p[get_global_id(0)] = 0.0f; }
)";

const string code_send =
  "#define NCH_FILL " + to_string(send_nch_fill) + R"(U
__kernel void zero_clear(__global float *p) { p[get_global_id(0)] = 0.0f; }

__kernel void plane_fill(__global const float *pvalue,
                         __global const uint *pindex, __global float *p) {
  uint gid    = get_global_id(0);
  uint uplane = gid / (NCH_FILL * NBATCH);
  uint ich    = gid % (NCH_FILL * NBATCH);
  uint index  = pindex[NCH_FILL * NBATCH + ich];
  p[index + uplane] = pvalue[ich]; }

__kernel void set_one(__global const uint *pindex, __global float *p) {
  uint index = pindex[2U * NCH_FILL * NBATCH + get_global_id(0)];
  p[index] = 1.0f; }
)";

const string code_compute_policy =
  "#define NCH_OUT    " + to_string(NNAux::nch_out_policy) + "U\n"
  "#define SIZE_PLANE " + to_string(NNAux::size_plane)     + R"(U
__kernel void compute_policy(__global const uint *nnmoves, uint offin,
                             __global const float *weight,
                             __global const float *bias,
                             __global const float *fin,
                             __global float *result) {
  uint gid    = get_global_id(0);
  uint uv     = nnmoves[offin + gid];
  uint ub     = uv / (NCH_OUT * SIZE_PLANE);
  uint nnmove = uv % (NCH_OUT * SIZE_PLANE);
  uint chout  = nnmove / SIZE_PLANE;
  uint sq     = nnmove % SIZE_PLANE;
  float fsum  = bias[chout];
  for (uint chin = 0; chin < NCH_IN; ++chin)
    fsum += weight[chout*NCH_IN + chin]
            * fin[chin*NBATCH*SIZE_PLANE + ub*SIZE_PLANE + sq];
  result[NBATCH + gid] = fsum; }
)";

const string code_transform_value2 =
  "#define SIZE_PLANE " + to_string(NNAux::size_plane) + R"(U
__kernel void transform_value2(__global const float *fin, uint offin,
                               __global float *fout) {
  uint gid = get_global_id(0);
  uint bch = gid / SIZE_PLANE;
  uint sq  = gid % SIZE_PLANE;
  uint ub  = bch / NCH;
  uint ch  = bch % NCH;
  //fout[gid] = fin[offin + (ch*NBATCH + ub)*SIZE_PLANE + sq]; }
  fout[(ch*SIZE_PLANE + sq)*NBATCH + ub]
    = fin[offin + (ch*NBATCH + ub)*SIZE_PLANE + sq]; }
)";

const string code_BNReLU =
  "#define SIZE_PLANE " + to_string(NNAux::size_plane) + R"(U
__kernel void compute_BNReLU(__global const float *mean,
                             __global const float *sd_inv,
                             __global float *fio) {
  uint gid = get_global_id(0);
  uint ch  = gid / (NBATCH * SIZE_PLANE);
  fio[gid] = max(0.0f, sd_inv[ch] * (fio[gid] - mean[ch])); }
)";

const string code_common =
  "#define SIZE_PLANE "   + to_string(NNAux::size_plane) + "U\n"
  "#define HEIGHT "       + to_string(NNAux::height)     + "U\n"
  "#define WIDTH "        + to_string(NNAux::width)      + "U\n"
  "#define LEN_TILE_IN "  + to_string(len_tile_in)       + "U\n"
  "#define LEN_TILE_OUT " + to_string(len_tile_out)      + "U\n"
  "#define NTILE_H "      + to_string(ntile_h)           + "U\n"
  "#define NTILE_W "      + to_string(ntile_w)           + "U\n"
  "#define NTILE "        + to_string(ntile)             + "U\n"
  "#define PAD "          + to_string(pad)               + R"(U
float x1(float x) { return x; }
float x2(float x) { return x + x; }
float x3(float x) { return x + x + x; }
float x4(float x) { x += x; return x + x; }
float x6(float x) { x += x; return x + x + x; }
float x9(float x) { float y = x + x; y += y; return y + y + x; }
)";

const string code_compute_matV = R"(
#ifdef DO_HALF
void store(float f, uint off, __global half *p) { vstore_half(f, off, p); }
#else
void store(float f, uint off, __global float *p) { p[off] = f; }
#endif

__kernel void compute_matV(__global const float *fin, __global void *matV) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / NB;
  uint ub    = chb % NB;
  uint uh    = utile / NTILE_W;
  uint uw    = utile % NTILE_W;

  int y0 = uh * LEN_TILE_OUT - PAD;
  int x0 = uw * LEN_TILE_OUT - PAD;
  float md[LEN_TILE_IN][LEN_TILE_IN];
  for (int y = 0; y < LEN_TILE_IN; ++y)
    for (int x = 0; x < LEN_TILE_IN; ++x) {
      if (0 <= y0 + y && y0 + y < HEIGHT && 0 <= x0 + x && x0 + x < WIDTH)
        md[y][x] = fin[chb * SIZE_PLANE + (y0 + y) * WIDTH + x0 + x];
      else md[y][x] = 0.0f; }

  uint uca = NK * NN * LEN_TILE_IN;
  uint ucb = NK * NN;
  uint ucc = ch * NN + ub * NTILE + uh * NTILE_W + uw;
  store(+ x4(md[0][0]) - x2(md[0][1]) - x4(md[0][2]) + x2(md[0][3])
        - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
        - x4(md[2][0]) + x2(md[2][1]) + x4(md[2][2]) - x2(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        ucc + uca*0U + ucb*0U, matV);
  store(- x4(md[1][0]) + x2(md[1][1]) + x4(md[1][2]) - x2(md[1][3])
        - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        ucc + uca*1U + ucb*0U, matV);
  store(+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
        - x6(md[2][0]) + x3(md[2][1]) + x6(md[2][2]) - x3(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        ucc + uca*2U + ucb*0U, matV);
  store(- x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        ucc + uca*3U + ucb*0U, matV);
  store(+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
        - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
        - x4(md[3][0]) + x2(md[3][1]) + x4(md[3][2]) - x2(md[3][3])
        + x2(md[4][0]) - x1(md[4][1]) - x2(md[4][2]) + x1(md[4][3]),
        ucc + uca*4U + ucb*0U, matV);
  store(- x4(md[0][1]) - x2(md[0][2]) + x2(md[0][3])
        + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
        + x4(md[2][1]) + x2(md[2][2]) - x2(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        ucc + uca*0U + ucb*1U, matV);
  store(+ x4(md[1][1]) + x2(md[1][2]) - x2(md[1][3])
        + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        ucc + uca*1U + ucb*1U, matV);
  store(- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
        + x6(md[2][1]) + x3(md[2][2]) - x3(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        ucc + uca*2U + ucb*1U, matV);
  store(+ x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        ucc + uca*3U + ucb*1U, matV);
  store(- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
        + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
        + x4(md[3][1]) + x2(md[3][2]) - x2(md[3][3])
        - x2(md[4][1]) - x1(md[4][2]) + x1(md[4][3]),
        ucc + uca*4U + ucb*1U, matV);
  store(+ x4(md[0][1]) - x6(md[0][2]) + x2(md[0][3])
        - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
        - x4(md[2][1]) + x6(md[2][2]) - x2(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        ucc + uca*0U + ucb*2U, matV);
  store(- x4(md[1][1]) + x6(md[1][2]) - x2(md[1][3])
        - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        ucc + uca*1U + ucb*2U, matV);
  store(+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
        - x6(md[2][1]) + x9(md[2][2]) - x3(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        ucc + uca*2U + ucb*2U, matV);
  store(- x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        ucc + uca*3U + ucb*2U, matV);
  store(+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
        - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
        - x4(md[3][1]) + x6(md[3][2]) - x2(md[3][3])
        + x2(md[4][1]) - x3(md[4][2]) + x1(md[4][3]),
        ucc + uca*4U + ucb*2U, matV);
  store(- x2(md[0][1]) + x2(md[0][3]) + x1(md[1][1]) - x1(md[1][3])
        + x2(md[2][1]) - x2(md[2][3]) - x1(md[3][1]) + x1(md[3][3]),
        ucc + uca*0U + ucb*3U, matV);
  store(+ x2(md[1][1]) - x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
        - x1(md[3][1]) + x1(md[3][3]),
        ucc + uca*1U + ucb*3U, matV);
  store(- x2(md[1][1]) + x2(md[1][3]) + x3(md[2][1]) - x3(md[2][3])
        - x1(md[3][1]) + x1(md[3][3]),
        ucc + uca*2U + ucb*3U, matV);
  store(+ x1(md[1][1]) - x1(md[1][3]) - x1(md[3][1]) + x1(md[3][3]),
        ucc + uca*3U + ucb*3U, matV);
  store(- x2(md[1][1]) + x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
        + x2(md[3][1]) - x2(md[3][3]) - x1(md[4][1]) + x1(md[4][3]),
        ucc + uca*4U + ucb*3U, matV);
  store(+ x4(md[0][1]) - x2(md[0][2]) - x4(md[0][3]) + x2(md[0][4])
        - x2(md[1][1]) + x1(md[1][2]) + x2(md[1][3]) - x1(md[1][4])
        - x4(md[2][1]) + x2(md[2][2]) + x4(md[2][3]) - x2(md[2][4])
        + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]),
        ucc + uca*0U + ucb*4U, matV);
  store(- x4(md[1][1]) + x2(md[1][2]) + x4(md[1][3]) - x2(md[1][4])
        - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
        + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]),
        ucc + uca*1U + ucb*4U, matV);
  store(+ x4(md[1][1]) - x6(md[2][1]) + x2(md[3][1])
        - x2(md[1][2]) + x3(md[2][2]) - x1(md[3][2])
        - x4(md[1][3]) + x6(md[2][3]) - x2(md[3][3])
        + x2(md[1][4]) - x3(md[2][4]) + x1(md[3][4]),
        ucc + uca*2U + ucb*4U, matV);
  store(- x2(md[1][1]) + x2(md[3][1]) + x1(md[1][2]) - x1(md[3][2])
        + x2(md[1][3]) - x2(md[3][3]) - x1(md[1][4]) + x1(md[3][4]),
        ucc + uca*3U + ucb*4U, matV);
  store(+ x4(md[1][1]) - x2(md[1][2]) - x4(md[1][3]) + x2(md[1][4])
        - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
        - x4(md[3][1]) + x2(md[3][2]) + x4(md[3][3]) - x2(md[3][4])
        + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4]),
        ucc + uca*4U + ucb*4U, matV); }
)";

const string code_compute_matA = R"(
float func_BNReLU(float sd_inv, float mean, float x) {
  return max(0.0f, sd_inv * (x - mean)); }

__kernel void compute_matA_BNReLU(__global const float *matM,
                                  __global const float *mean_array,
                                  __global const float *sd_inv_array,
                                  __global float *fout) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / NB;
  uint ub    = chb % NB;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * OFFC
                              + ch * NN + ub * NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint ucd     = chb * SIZE_PLANE + (uh * WIDTH + uw) * LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  fout[ucd + 0U * WIDTH + 0U]
    = func_BNReLU(sd_inv, mean,
                     + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  fout[ucd + 0U * WIDTH + 1U]
    = func_BNReLU(sd_inv, mean,
                     + mm[0][1] - mm[0][2] + x2(mm[0][3])
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2] + x2(mm[2][3])
                     + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  fout[ucd + 0U * WIDTH + 2U]
    = func_BNReLU(sd_inv, mean,
                     + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  fout[ucd + 1U * WIDTH + 0U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
                     + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));
  fout[ucd + 1U * WIDTH + 1U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     - mm[2][1] + mm[2][2]
                     + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));
  fout[ucd + 1U * WIDTH + 2U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
                     + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));
  fout[ucd + 2U * WIDTH + 0U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
                     + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  fout[ucd + 2U * WIDTH + 1U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2]
                     + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
                     + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  fout[ucd + 2U * WIDTH + 2U]
    = func_BNReLU(sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
                     + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }

float func_BNReLU_join(float join, float sd_inv, float mean, float x) {
  return max(0.0f, join + sd_inv * (x - mean)); }

__kernel void compute_matA_BNReLU_join(__global const float *matM,
                                       __global const float *mean_array,
                                       __global const float *sd_inv_array,
                                       __global float *fout) {
  uint gid   = get_global_id(0);
  uint chb   = gid / NTILE;
  uint utile = gid % NTILE;
  uint ch    = chb / NB;
  uint ub    = chb % NB;
  float mm[LEN_TILE_IN][LEN_TILE_IN];
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = matM[(uh_in * LEN_TILE_IN + uw_in) * OFFC
                              + ch * NN + ub * NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint ucd     = chb * SIZE_PLANE + (uh * WIDTH + uw) * LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  fout[ucd + 0U * WIDTH + 0U]
    = func_BNReLU_join(fout[ucd + 0U * WIDTH + 0U], sd_inv, mean,
                     + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  fout[ucd + 0U * WIDTH + 1U]
    = func_BNReLU_join(fout[ucd + 0U * WIDTH + 1U], sd_inv, mean,
                     + mm[0][1] - mm[0][2] + x2(mm[0][3])
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2] + x2(mm[2][3])
                     + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  fout[ucd + 0U * WIDTH + 2U]
    = func_BNReLU_join(fout[ucd + 0U * WIDTH + 2U], sd_inv, mean,
                     + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  fout[ucd + 1U * WIDTH + 0U]
    = func_BNReLU_join(fout[ucd + 1U * WIDTH + 0U], sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
                     + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));
  fout[ucd + 1U * WIDTH + 1U]
    = func_BNReLU_join(fout[ucd + 1U * WIDTH + 1U], sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     - mm[2][1] + mm[2][2]
                     + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));
  fout[ucd + 1U * WIDTH + 2U]
    = func_BNReLU_join(fout[ucd + 1U * WIDTH + 2U], sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
                     + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));
  fout[ucd + 2U * WIDTH + 0U]
    = func_BNReLU_join(fout[ucd + 2U * WIDTH + 0U], sd_inv, mean,
                     + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
                     + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
                     + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
                     + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  fout[ucd + 2U * WIDTH + 1U]
    = func_BNReLU_join(fout[ucd + 2U * WIDTH + 1U], sd_inv, mean,
                     + mm[1][1] - mm[1][2] + x2(mm[1][3])
                     + mm[2][1] - mm[2][2]
                     + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
                     + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  fout[ucd + 2U * WIDTH + 2U]
    = func_BNReLU_join(fout[ucd + 2U * WIDTH + 2U], sd_inv, mean,
                     + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
                     + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
                     + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
                     + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }
)";

const string code_compute_matM_wmma = R"(
void compute_matM(__global const uint *gA, __global const uint *gB,
                  __global float *gC) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint ul  = get_local_id(1) * SGEMM_NL * WRAP_SIZE + get_local_id(0);
  uint uln = get_local_id(0) / WRAP_SIZE;
  uint ngk = NK / SGEMM_NLPTK;
  gA += ub*OFFA_2 + ugm*SGEMM_NLPTM_2;
  gB += ub*OFFB_2 + ugn*SGEMM_NLPTN_2;
  gC += ub*OFFC + ugm*SGEMM_NLPTM*NN + ugn*SGEMM_NLPTN;

  __local uint lA[SGEMM_NLPTK * SGEMM_NLPTM_2] __attribute__((aligned(32)));
  __local uint lB[SGEMM_NLPTK * SGEMM_NLPTN_2] __attribute__((aligned(32)));

  uint pD[SGEMM_NPM][SGEMM_NPN][8];
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      for (uint u = 0; u < 8U; ++u) pD[upm][upn][u] = 0;

  uint ulA1 = (ul % SGEMM_NLPTM_2);
  uint ulA2 = (ul / SGEMM_NLPTM_2);
  uint ulA3 = (SGEMM_NL*WRAP_SIZE / SGEMM_NPTM_2);
  uint ulB1 = (ul % SGEMM_NLPTN_2);
  uint ulB2 = (ul / SGEMM_NLPTN_2);
  uint ulB3 = (SGEMM_NL*WRAP_SIZE / SGEMM_NPTN_2);

  uint ldlA = SGEMM_NLPTM;
  uint ldlB = SGEMM_NLPTN;
  for (uint ugk = 0; ugk < ngk; ++ugk) {
    
    for (uint u = 0; u < SGEMM_NLPTK; u += ulA3)
      lA[(u + ulA2)*SGEMM_NLPTM_2 + ulA1] = gA[(u + ulA2)*NM_2 + ulA1];

    for (uint u = 0; u < SGEMM_NLPTK; u += ulB3)
      lB[(u + ulB2)*SGEMM_NLPTN_2 + ulB1] = gB[(u + ulB2)*NN_2 + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NPK; ++ulpk)
      for (uint upm = 0; upm < SGEMM_NPM; ++upm)
        for (uint upn = 0; upn < SGEMM_NPN; ++upn) {
          asm("{ .reg .b32 a<8>;\n"
              "  .reg .b32 b<8>;\n"
              "  wmma.load.a.sync.aligned.col" SGEMM_TDM ".shared.f16\n"
              "    {a0, a1, a2, a3, a4, a5, a6, a7}, [%8], %9;\n"
              "  wmma.load.b.sync.aligned.row" SGEMM_TDM ".shared.f16\n"
              "    {b0, b1, b2, b3, b4, b5, b6, b7}, [%10], %11;\n"
              "  wmma.mma.sync.aligned.col.row" SGEMM_TDM ".f32.f32\n"
              "    {%0, %1, %2, %3, %4, %5, %6, %7},\n"
              "    {a0, a1, a2, a3, a4, a5, a6, a7},\n"
              "    {b0, b1, b2, b3, b4, b5, b6, b7},\n"
              "    {%0, %1, %2, %3, %4, %5, %6, %7}; }"
              : "+r"(pD[upm][upn][0]), "+r"(pD[upm][upn][1]),
                "+r"(pD[upm][upn][2]), "+r"(pD[upm][upn][3]),
                "+r"(pD[upm][upn][4]), "+r"(pD[upm][upn][5]),
                "+r"(pD[upm][upn][6]), "+r"(pD[upm][upn][7])
              : "l"(lA + ulpk*SGEMM_NTK*SGEMM_NLPTM_2
                       + ulm*SGEMM_NPTM_2 + upm*SGEMM_NTM_2), "r"(ldlA),
                "l"(lB + ulpk*SGEMM_NTK*SGEMM_NLPTN_2
                       + uln*SGEMM_NPTN_2 + upn*SGEMM_NTN_2), "r"(ldlB)
              : "memory");
        }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NLPTK*NM_2;
    gB += SGEMM_NLPTK*NN_2; }

  uint ldgC = NN;
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      asm("{  wmma.store.d.sync.aligned.row" SGEMM_TDM ".global.f32\n"
          "     [%8], {%0, %1, %2, %3, %4, %5, %6, %7}, %9; }"
          :: "r"(pD[upm][upn][0]), "r"(pD[upm][upn][1]),
             "r"(pD[upm][upn][2]), "r"(pD[upm][upn][3]),
             "r"(pD[upm][upn][4]), "r"(pD[upm][upn][5]),
             "r"(pD[upm][upn][6]), "r"(pD[upm][upn][7]),
             "l"(gC + (ulm*SGEMM_NPTM + upm*SGEMM_NTM)*NN
                    + uln*SGEMM_NPTN + upn*SGEMM_NTN), "r"(ldgC)
          : "memory");
}
)";

const string code_compute_matM_half = R"(
void compute_matM(__global const uint *gA, __global const uint *gB,
                  __global float *gC) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint uln = get_local_id(0);
  uint ul  = ulm*SGEMM_NL + uln;
  uint ngk = NK / (SGEMM_NL * SGEMM_NLFM * SGEMM_NPK);
  gA += (ub*OFFA + ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM) / 2U;
  gB += (ub*OFFB + ugn*SGEMM_NL*SGEMM_NPN) / 2U;
  gC += ub*OFFC + ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM*NN
                + ugn*SGEMM_NL*SGEMM_NPN;

  __local uint lA[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                 [(SGEMM_NL*SGEMM_NLFM*SGEMM_NPM) / 2U];
  __local uint lB[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                 [(SGEMM_NL*SGEMM_NPN) / 2U];
  float pC[SGEMM_NPM][SGEMM_NPN];

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn) pC[upm][upn] = 0.0f;

  uint ulA1 = ul % ((SGEMM_NL*SGEMM_NLFM*SGEMM_NPM) / 2U);
  uint ulA2 = ul / ((SGEMM_NL*SGEMM_NLFM*SGEMM_NPM) / 2U);
  uint ulA3 = (2U*SGEMM_NL) / SGEMM_NPM;
  uint ulB1 = ul % ((SGEMM_NL*SGEMM_NPN) / 2U);
  uint ulB2 = ul / ((SGEMM_NL*SGEMM_NPN) / 2U);
  uint ulB3 = (2U*SGEMM_NL*SGEMM_NLFM) / SGEMM_NPN;

  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulA3)
      lA[u + ulA2][ulA1] = gA[(u + ulA2)*(NM / 2U) + ulA1];
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulB3)
      lB[u + ulB2][ulB1] = gB[(u + ulB2)*(NN / 2U) + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; ++ulpk) {
      /*
      float pB[SGEMM_NPN];
      float pA;
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = vload_half(ulpk*SGEMM_NL*SGEMM_NPN + uln*SGEMM_NPN + upn,
                             (__local const half *)lB);
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = vload_half(ulpk*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM
                        + ulm*SGEMM_NPM + upm, (__local const half *)lA);
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn];
      */
      uint pB[SGEMM_NPN];
      uint pA_pair;
      ushort pA;
      for (uint upn = 0; upn < (SGEMM_NPN/2U); ++upn)
        pB[upn] = lB[ulpk][uln*(SGEMM_NPN/2U) + upn];

      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = ((__local const ushort *)lA)[ulpk*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM
                                          + ulm*SGEMM_NPM + upm];
        asm("mov.b32 %0, {%1, %1};\n" : "=r"(pA_pair) : "h"(pA));
        for (uint upn = 0; upn < (SGEMM_NPN/2U); ++upn) {
          float f1, f2;
          asm("{ .reg .b32 r;\n"
              "  .reg .b16 h1, h2;\n"
              "  mul.f16x2 r, %2, %3;\n"
              "  mov.b32 {h1, h2}, r;\n"
              "  cvt.f32.f16 %0, h1;\n"
              "  cvt.f32.f16 %1, h2; }\n"
              : "=r"(f1), "=r"(f2) : "r"(pA_pair), "r"(pB[upn]));
          pC[upm][2U*upn]      += f1;
          pC[upm][2U*upn + 1U] += f2;
    } } }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += (SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NM) / 2U;
    gB += (SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NN) / 2U; }

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      gC[(ulm*SGEMM_NPM + upm)*NN + uln*SGEMM_NPN + upn] = pC[upm][upn]; }
)";

const string code_compute_matM = R"(
void compute_matM(__global const float *gA, __global const float *gB,
                  __global float *gC) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint uln = get_local_id(0);
  uint ul  = ulm*SGEMM_NL + uln;
  uint ngk = NK / (SGEMM_NL * SGEMM_NLFM * SGEMM_NPK);
  gA += ub*OFFA + ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM;
  gB += ub*OFFB + ugn*SGEMM_NL*SGEMM_NPN;
  gC += ub*OFFC + ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM*NN
                + ugn*SGEMM_NL*SGEMM_NPN;

  __local float lA[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NLFM*SGEMM_NPM];
  __local float lB[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NPN];
  float pC[SGEMM_NPM][SGEMM_NPN];
  float pB[SGEMM_NPN];
  float pA;

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn) pC[upm][upn] = 0.0f;

  uint ulA1 = ul % (SGEMM_NL*SGEMM_NLFM*SGEMM_NPM);
  uint ulA2 = ul / (SGEMM_NL*SGEMM_NLFM*SGEMM_NPM);
  uint ulA3 = SGEMM_NL / SGEMM_NPM;
  uint ulB1 = ul % (SGEMM_NL*SGEMM_NPN);
  uint ulB2 = ul / (SGEMM_NL*SGEMM_NPN);
  uint ulB3 = (SGEMM_NL*SGEMM_NLFM) / SGEMM_NPN;

  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulA3)
      lA[u + ulA2][ulA1] = gA[(u + ulA2)*NM + ulA1];
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulB3)
      lB[u + ulB2][ulB1] = gB[(u + ulB2)*NN + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; ++ulpk) {
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = lB[ulpk][uln*SGEMM_NPN + upn];
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = lA[ulpk][ulm*SGEMM_NPM + upm];
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn]; } }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NM;
    gB += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NN; }

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      gC[(ulm*SGEMM_NPM + upm)*NN + uln*SGEMM_NPN + upn] = pC[upm][upn]; }
)";

const string code_sgemm = R"(
void sgemm(__global const float *gA, __global const float *gB,
           __global float *gC) {
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint uln = get_local_id(0);
  uint ul  = ulm*SGEMM_NL + uln;
  uint ngk = NK / (SGEMM_NL * SGEMM_NLFM * SGEMM_NPK);
  gA += ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM;
  gB += ugn*SGEMM_NL*SGEMM_NPN;
  gC += ugm*SGEMM_NL*SGEMM_NLFM*SGEMM_NPM*NN + ugn*SGEMM_NL*SGEMM_NPN;

  __local float lA[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NLFM*SGEMM_NPM];
  __local float lB[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NPN];
  float pC[SGEMM_NPM][SGEMM_NPN];
  float pB[SGEMM_NPN];
  float pA;

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn) pC[upm][upn] = 0.0f;

  uint ulA1 = ul % (SGEMM_NL*SGEMM_NLFM*SGEMM_NPM);
  uint ulA2 = ul / (SGEMM_NL*SGEMM_NLFM*SGEMM_NPM);
  uint ulA3 = SGEMM_NL / SGEMM_NPM;
  uint ulB1 = ul % (SGEMM_NL*SGEMM_NPN);
  uint ulB2 = ul / (SGEMM_NL*SGEMM_NPN);
  uint ulB3 = (SGEMM_NL*SGEMM_NLFM) / SGEMM_NPN;

  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulA3)
      lA[u + ulA2][ulA1] = gA[(u + ulA2)*NM + ulA1];
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulB3)
      lB[u + ulB2][ulB1] = gB[(u + ulB2)*NN + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; ++ulpk) {
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = lB[ulpk][uln*SGEMM_NPN + upn];
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = lA[ulpk][ulm*SGEMM_NPM + upm];
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn]; } }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NM;
    gB += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NN; }

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      gC[(ulm*SGEMM_NPM + upm)*NN + uln*SGEMM_NPN + upn] = pC[upm][upn]; }
)";

const string code_sgemm_n1 = R"(
void sgemm(__global const float *gA, __global const float *gB,
           __global float *gC) {
  uint ugm = get_group_id(1);
  uint ulm = get_local_id(1);
  uint ngk = NK / (SGEMM_NLFM * SGEMM_NPK);
  gA += ugm*SGEMM_NLFM;
  gC += ugm*SGEMM_NLFM;

  __local float lB[SGEMM_NLFM*SGEMM_NPK];
  float fsum = 0.0f;
  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NPK; ++u)
      lB[u*SGEMM_NLFM + ulm] = gB[u*SGEMM_NLFM + ulm];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NLFM*SGEMM_NPK; ++ulpk)
      fsum += lB[ulpk] * gA[ulpk*NM + ulm];
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NLFM*SGEMM_NPK*NM;
    gB += SGEMM_NLFM*SGEMM_NPK; }
  gC[ulm] = fsum; }
)";

const string code_sgemm_aux = R"(
__kernel void mat0_to_matA(__global const float *p0, __global float *p1) {
  uint gid   = get_global_id(0);
  uint urow0 = gid / NM0;
  uint ucol0 = gid % NM0;
  p1[urow0*NM + ucol0] = p0[OFFA0 + urow0*LDA0 + ucol0]; }

__kernel void mat0_to_matB(__global const float *p0, __global float *p1) {
  uint gid   = get_global_id(0);
  uint urow0 = gid / NN0;
  uint ucol0 = gid % NN0;
  p1[urow0*NN + ucol0] = p0[OFFB0 + urow0*LDB0 + ucol0]; }

__kernel void matC_to_mat0(__global const float *p0, __global float *p1) {
  uint gid   = get_global_id(0);
  uint urow0 = gid / NN0;
  uint ucol0 = gid % NN0;
  p1[OFFC0 + urow0*LDC0 + ucol0] = p0[urow0*NN + ucol0]; }

__kernel void matC_to_mat0_bias_ReLU(__global const float *p0,
                                     __global const float *bias,
                                     __global float *p1) {
  uint gid   = get_global_id(0);
  uint urow0 = gid / NN0;
  uint ucol0 = gid % NN0;
  p1[OFFC0 + urow0*LDC0 + ucol0]
    = fmax(0.0f, p0[urow0*NN + ucol0] + bias[urow0]); }
)";

static uint ceil_multi(uint u, uint mul) noexcept {
  assert(1U <= mul && u <= UINT_MAX - mul + 1U);
  return ((u + mul - 1U) / mul) * mul; }

static uint ceil_power2(uint u) noexcept {
  uint u0;
  for (u0 = 1U; u0 < u; u0 *= 2U) assert(u0 <= UINT_MAX / 2U);
  return u0; }

static tuple<string, uint, uint, uint, size_t, size_t, size_t, size_t>
gen_code_compute_matM(uint nm0, uint nn0, uint nk0, const SgemmParam &param)
  noexcept {
  assert(0 < nm0 && 0 < nn0 && 0 < nk0 && param.ok());
  if (param.do_wmma) {
    uint ntk = 16U;
    uint nm = ceil_multi(nm0, param.nl * param.npm * param.ntm);
    uint nn = ceil_multi(nn0, param.nl * param.npn * param.ntn);
    uint nk = ceil_multi(nk0, param.nl * param.npk * ntk);
    string str, str_tdm;
    if (param.ntm ==  8) str_tdm = R"(".m8n32k16")";
    if (param.ntm == 16) str_tdm = R"(".m16n16k16")";
    if (param.ntm == 32) str_tdm = R"(".m32n8k16")";
    str += "#define NM        " + to_string(nm)             + "U\n";
    str += "#define NN        " + to_string(nn)             + "U\n";
    str += "#define NK        " + to_string(nk)             + "U\n";
    str += "#define OFFA      " + to_string(nm*nk)          + "U\n";
    str += "#define OFFB      " + to_string(nk*nn)          + "U\n";
    str += "#define OFFC      " + to_string(nm*nn)          + "U\n";
    str += "#define WRAP_SIZE " + to_string(size_wrap_wmma) + "U\n";
    str += "#define SGEMM_NL  " + to_string(param.nl)       + "U\n";
    str += "#define SGEMM_NPM " + to_string(param.npm)      + "U\n";
    str += "#define SGEMM_NPN " + to_string(param.npn)      + "U\n";
    str += "#define SGEMM_NPK " + to_string(param.npk)      + "U\n";
    str += "#define SGEMM_NTM " + to_string(param.ntm)      + "U\n";
    str += "#define SGEMM_NTN " + to_string(param.ntn)      + "U\n";
    str += "#define SGEMM_NTK " + to_string(ntk)            + "U\n";
    str += "#define SGEMM_NPTM    (SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NPTN    (SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_NPTK    (SGEMM_NPK * SGEMM_NTK)\n";
    str += "#define SGEMM_NLPTM   (SGEMM_NL * SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NLPTN   (SGEMM_NL * SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_NLPTK   (SGEMM_NL * SGEMM_NPK * SGEMM_NTK)\n";
    str += "#define NM_2          (NM   / 2U)\n";
    str += "#define NN_2          (NN   / 2U)\n";
    str += "#define OFFA_2        (OFFA / 2U)\n";
    str += "#define OFFB_2        (OFFB / 2U)\n";
    str += "#define SGEMM_NTM_2   (SGEMM_NTM   / 2U)\n";
    str += "#define SGEMM_NTN_2   (SGEMM_NTN   / 2U)\n";
    str += "#define SGEMM_NPTM_2  (SGEMM_NPTM  / 2U)\n";
    str += "#define SGEMM_NPTN_2  (SGEMM_NPTN  / 2U)\n";
    str += "#define SGEMM_NLPTM_2 (SGEMM_NLPTM / 2U)\n";
    str += "#define SGEMM_NLPTN_2 (SGEMM_NLPTN / 2U)\n";
    str += "#define SGEMM_TDM     " + str_tdm + "\n";
    str += "__kernel __attribute__((reqd_work_group_size("
      + to_string(param.nl * size_wrap_wmma) + ", "
      + to_string(param.nl) + ", 1)))";
    str += code_compute_matM_wmma;
    return make_tuple(str, nm, nn, nk,
		      (nn / (param.npn*param.ntn)) * size_wrap_wmma,
		      nm / (param.npm*param.ntm),
		      param.nl * size_wrap_wmma, param.nl); }
  else if (param.do_half) {
    uint nm = ceil_multi(nm0, param.nl * param.nlfm * param.npm);
    uint nn = ceil_multi(nn0, param.nl * param.npn);
    uint nk = ceil_multi(nk0, param.nl * param.nlfm * param.npk);
    string str;
    str += "#define NM         " + to_string(nm)         + "U\n";
    str += "#define NN         " + to_string(nn)         + "U\n";
    str += "#define NK         " + to_string(nk)         + "U\n";
    str += "#define OFFA       " + to_string(nm*nk)      + "U\n";
    str += "#define OFFB       " + to_string(nk*nn)      + "U\n";
    str += "#define OFFC       " + to_string(nm*nn)      + "U\n";
    str += "#define SGEMM_NL   " + to_string(param.nl)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPM  " + to_string(param.npm)  + "U\n";
    str += "#define SGEMM_NPN  " + to_string(param.npn)  + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += "__kernel __attribute__((reqd_work_group_size("
      + to_string(param.nl) + ", " + to_string(param.nl*param.nlfm) + ", 1)))";
    str += code_compute_matM_half;
    return make_tuple(str, nm, nn, nk,
		      nn / param.npn, nm / param.npm,
		      param.nl, param.nl * param.nlfm); }
  else {
    uint nm = ceil_multi(nm0, param.nl * param.nlfm * param.npm);
    uint nn = ceil_multi(nn0, param.nl * param.npn);
    uint nk = ceil_multi(nk0, param.nl * param.nlfm * param.npk);
    string str;
    str += "#define NM         " + to_string(nm)         + "U\n";
    str += "#define NN         " + to_string(nn)         + "U\n";
    str += "#define NK         " + to_string(nk)         + "U\n";
    str += "#define OFFA       " + to_string(nm*nk)      + "U\n";
    str += "#define OFFB       " + to_string(nk*nn)      + "U\n";
    str += "#define OFFC       " + to_string(nm*nn)      + "U\n";
    str += "#define SGEMM_NL   " + to_string(param.nl)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPM  " + to_string(param.npm)  + "U\n";
    str += "#define SGEMM_NPN  " + to_string(param.npn)  + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += "__kernel __attribute__((reqd_work_group_size("
      + to_string(param.nl) + ", " + to_string(param.nl*param.nlfm) + ", 1)))";
    str += code_compute_matM;
    return make_tuple(str, nm, nn, nk,
		      nn / param.npn, nm / param.npm,
		      param.nl, param.nl * param.nlfm); } }

const string gen_code_sgemm(uint nm, uint nn, uint nk,
			    const SgemmParam &param) noexcept {
  string str;
  if (param.npm == 1U && nn == 1U) {
    str += "#define NM         " + to_string(nm)   + "U\n";
    str += "#define NK         " + to_string(nk)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += "__kernel __attribute__((reqd_work_group_size(1,"
      + to_string(param.nlfm) + ",1)))";
    str += code_sgemm_n1; }
  else {
    str += "#define NM         " + to_string(nm)         + "U\n";
    str += "#define NN         " + to_string(nn)         + "U\n";
    str += "#define NK         " + to_string(nk)         + "U\n";
    str += "#define SGEMM_NL   " + to_string(param.nl)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPM  " + to_string(param.npm)  + "U\n";
    str += "#define SGEMM_NPN  " + to_string(param.npn)  + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += "__kernel __attribute__((reqd_work_group_size("
      + to_string(param.nl) + ", " + to_string(param.nl*param.nlfm) + ", 1)))";
    str += code_sgemm; }
  return str; }

static bool test_wmma(const OCL::Device &dev) noexcept {
  try {
    OCL::Queue queue = dev.gen_queue();
    OCL::Program pg  = queue.gen_program(R"(
__kernel void test_wmma() {
  uint laneid = get_global_id(0);
  __local ushort lA[256] __attribute__((aligned(32)));
  __local ushort lB[256] __attribute__((aligned(32)));
  __local float lC[256]  __attribute__((aligned(32)));
  uint pD[8];
  for (uint u = 0; u < 8U; ++u) lA[laneid*8U + u] = 0;
  for (uint u = 0; u < 8U; ++u) lB[laneid*8U + u] = 0;
  for (uint u = 0; u < 8U; ++u) lC[laneid*8U + u] = 0.0f;
  for (uint u = 0; u < 8U; ++u) pD[u] = 0;
  uint stride = 16U;
  asm(".reg .b32 a<8>;\n"
      ".reg .b32 b<8>;\n"
      "wmma.load.a.sync.aligned.col.m16n16k16.shared.f16\n"
      "  {a0, a1, a2, a3, a4, a5, a6, a7}, [%8], %9;\n"
      "wmma.load.b.sync.aligned.row.m16n16k16.shared.f16\n"
      "  {b0, b1, b2, b3, b4, b5, b6, b7}, [%10], %11;\n"
      "wmma.mma.sync.aligned.col.row.m16n16k16.f32.f32\n"
      "  {%0, %1, %2, %3, %4, %5, %6, %7}, {a0, a1, a2, a3, a4, a5, a6, a7},\n"
      "  {b0, b1, b2, b3, b4, b5, b6, b7}, {%0, %1, %2, %3, %4, %5, %6, %7};\n"
      "wmma.store.d.sync.aligned.row.m16n16k16.shared.f32\n"
      "  [%12], {%0, %1, %2, %3, %4, %5, %6, %7}, %13;"
      : "+r"(pD[0]), "+r"(pD[1]), "+r"(pD[2]), "+r"(pD[3]),
        "+r"(pD[4]), "+r"(pD[5]), "+r"(pD[6]), "+r"(pD[7])
      : "l"(lA), "r"(stride), "l"(lB), "r"(stride), "l"(lC), "r"(stride)
      : "memory"); }
)");
    OCL::Kernel ker = pg.gen_kernel("test_wmma");
    const size_t size_g[3] = { 32U, 1U, 1U };
    const size_t size_l[3] = { 32U, 1U, 1U };
    queue.push_ndrange_kernel(ker, 3, size_g, size_l);
    queue.finish(); }
  catch (...) { return false; }
  return true; }

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
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar> data(new uchar [size]);
  OCL::Memory mem = queue.gen_mem_hw_dr(size);
  queue.push_write(mem, size, data.get());
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    queue.push_write(mem, size, data.get());
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }

static double measure_send_pinned(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  OCL::Memory mem_a = queue.gen_mem_hw_dr(size);
  OCL::Memory mem_b = queue.gen_mem_map_hw_dr(size);
  void *ptr = queue.map_w(mem_b, size);
  queue.push_write(mem_a, size, ptr);
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    queue.push_write(mem_a, size, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  queue.push_unmap(mem_b, ptr);
  queue.finish();
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }

static double measure_send_zcopy(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar> data(new uchar [size]);
  OCL::Memory mem = queue.gen_mem_map_hw_dr(size);
  sleep_for(microseconds(tune_sleep));
  void *ptr = queue.map_w(mem, size);
  copy_n(data.get(), size, static_cast<uchar *>(ptr));
  queue.push_unmap(mem, ptr);
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    ptr = queue.map_w(mem, size);
    copy_n(data.get(), size, static_cast<uchar *>(ptr));
    queue.push_unmap(mem, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }

constexpr char ManageSend::global_memory[];
constexpr char ManageSend::pinned_memory[];
constexpr char ManageSend::zero_copy[];
void ManageSend::start(const OCL::Device &dev, const OCL::Queue &queue,
		       uint maxsize_batch) noexcept {
  assert(dev.ok() && queue.ok() && 0 < maxsize_batch);
  _nbatch = maxsize_batch;
  _nm     = _nbatch * NNAux::nch_input * NNAux::size_plane;
  size_t size_max = _nm * sizeof(float);
  double elapsed_global = DBL_MAX;
  double elapsed_pinned = DBL_MAX;
  double elapsed_zcopy  = DBL_MAX;
  { size_t ave = send_size_ave * _nbatch;
    OCL::Queue qtmp = dev.gen_queue();
    try { elapsed_global = measure_send_global(qtmp, ave); } catch (...) {}
    try { elapsed_pinned = measure_send_pinned(qtmp, ave); } catch (...) {}
    try { elapsed_zcopy  = measure_send_zcopy (qtmp, ave); } catch (...) {} }

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
    _mem_work = queue.gen_mem_hw_dr(size_max);
    _mem_b    = queue.gen_mem_map_hw_dr(size_max);
    _ptr      = queue.map_w(_mem_b, size_max); }
  else if (_method == zero_copy)
    _mem_work = queue.gen_mem_map_hw_dr(size_max);
  else _mem_work = queue.gen_mem_hw_dr(size_max);

  _mem_out = queue.gen_mem_drw(size_max);
  string code = "#define NBATCH " + to_string(_nbatch) + "U\n" + code_send;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker_zero_clear = pg.gen_kernel("zero_clear");
  _ker_zero_clear.set_arg(0, _mem_out);

  _ker_plane_fill = pg.gen_kernel("plane_fill");
  _ker_plane_fill.set_arg(0, _mem_work);
  _ker_plane_fill.set_arg(1, _mem_work);
  _ker_plane_fill.set_arg(2, _mem_out);

  _ker_set_one = pg.gen_kernel("set_one");
  _ker_set_one.set_arg(0, _mem_work);
  _ker_set_one.set_arg(1, _mem_out); }

void ManageSend::end(const OCL::Queue &queue) noexcept {
  assert(_method && queue.ok());
  if (_method == pinned_memory) {
    queue.push_unmap(_mem_b, _ptr);
    queue.finish(); } }

void ManageSend::push(const OCL::Queue &queue, const void *p, size_t size,
		      uint n_one) noexcept {
  assert(_method && queue.ok() && p && 0 < size && 0 < n_one);

  queue.push_kernel(_ker_zero_clear, _nm);

  if (_method == global_memory) queue.push_write(_mem_work, size, p);
  else if (_method == pinned_memory) {
    memcpy(_ptr, p, size);
    queue.push_write(_mem_work, size, _ptr); }
  else {
    _ptr = queue.map_w(_mem_work, size);
    memcpy(_ptr, p, size);
    queue.push_unmap(_mem_work, _ptr); }

  queue.push_kernel(_ker_set_one, n_one);
  queue.push_kernel(_ker_plane_fill,
		    send_nch_fill * _nbatch * NNAux::size_plane); }

string ManageSend::gen_info() const noexcept {
  string s(_method);
  s += " (" + to_string(static_cast<uint>(_time)) + "us)";
  return s; }

static double measure_recv_global(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar> data(new uchar [size]);
  OCL::Memory mem = queue.gen_mem_hr_dw(size);
  queue.push_read(mem, size, data.get());
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    queue.push_read(mem, size, data.get());
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }

static double measure_recv_pinned(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  OCL::Memory mem     = queue.gen_mem_hr_dw(size);
  OCL::Memory mem_pin = queue.gen_mem_map_hr_dw(size);
  void *ptr = queue.map_r(mem_pin, size);
  queue.push_read(mem, size, ptr);
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    queue.push_read(mem, size, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  queue.push_unmap(mem_pin, ptr);
  queue.finish();
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }

static double measure_recv_zcopy(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar> data(new uchar [size]);
  OCL::Memory mem = queue.gen_mem_map_hr_dw(size);
  void *ptr = queue.map_r(mem, size);
  copy_n(static_cast<uchar *>(ptr), size, data.get());
  queue.push_unmap(mem, ptr);
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    ptr = queue.map_r(mem, size);
    copy_n(static_cast<uchar *>(ptr), size, data.get());
    queue.push_unmap(mem, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }
/*
  double _time;
  OCL::Memory _mem, _mem_pin;
  const char *_method;
  void *_ptr;
  uint _nbatch; */
constexpr char ManageRecv::global_memory[];
constexpr char ManageRecv::pinned_memory[];
constexpr char ManageRecv::zero_copy[];
void ManageRecv::start(const OCL::Device &dev, const OCL::Queue &queue,
		       size_t size_max, uint nbatch) noexcept {
  assert(dev.ok() && queue.ok() && 0 < size_max);

  double elapsed_global = DBL_MAX;
  double elapsed_pinned = DBL_MAX;
  double elapsed_zcopy  = DBL_MAX;
  { size_t ave = read_size_ave * nbatch;
    OCL::Queue qtmp = dev.gen_queue();
    try { elapsed_global = measure_recv_global(qtmp, ave); } catch (...) {}
    try { elapsed_pinned = measure_recv_pinned(qtmp, ave); } catch (...) {}
    try { elapsed_zcopy  = measure_recv_zcopy (qtmp, ave); } catch (...) {} }

  if (elapsed_zcopy < elapsed_global && elapsed_zcopy < elapsed_pinned) {
    _time   = elapsed_zcopy;
    _method = zero_copy; }
  else if (elapsed_pinned < elapsed_global) {
    _time   = elapsed_pinned;
    _method = pinned_memory; }
  else {
    _time   = elapsed_global;
    _method = global_memory; }
  if (_time == DBL_MAX) die(ERR_INT("ManageRecv() failed."));

  if (_method == pinned_memory) {
    _mem     = queue.gen_mem_hr_dw(size_max * nbatch);
    _mem_pin = queue.gen_mem_map_hr_dw(size_max * nbatch);
    _ptr     = queue.map_r(_mem_pin, size_max * nbatch); }
  else if (_method == zero_copy)
    _mem = queue.gen_mem_map_hr_dw(size_max * nbatch);
  else _mem = queue.gen_mem_hr_dw(size_max * nbatch); }

void ManageRecv::end(const OCL::Queue &queue) noexcept {
  assert(_method && queue.ok());
  if (_method == pinned_memory) {
    queue.push_unmap(_mem_pin, _ptr);
    queue.finish(); } }

void ManageRecv::push_finish(const OCL::Queue &queue, void *p, size_t size)
  noexcept {
  assert(_method && queue.ok() && p && 0 < size);
  if (_method == global_memory) {
    queue.push_read(_mem, size, p);
    queue.finish(); }
  else if (_method == pinned_memory) {
    queue.push_read(_mem, size, _ptr);
    queue.finish();
    memcpy(p, _ptr, size); }
  else {
    _ptr = queue.map_r(_mem, size);
    memcpy(p, _ptr, size);
    queue.push_unmap(_mem, _ptr);
    queue.finish(); } }

string ManageRecv::gen_info() const noexcept {
  string s(_method);
  s += " (" + to_string(static_cast<uint>(_time)) + "us)";
  return s; }

static OCL::Memory push_write(bool use_half, const OCL::Queue &queue,
			      const float *ptr, size_t size) noexcept {
  if (use_half) {
    constexpr char str[] = R"(
__kernel void foo(__global const float *f, __global half *h) {
  uint gid = get_global_id(0);
  vstore_half(f[gid], gid, h); }
)";
    OCL::Memory mem_tmp = queue.gen_mem_hw_dr(sizeof(float) * size);
    OCL::Memory mem     = queue.gen_mem_drw(sizeof(ushort) * size);
    OCL::Program pg     = queue.gen_program(str);
    OCL::Kernel ker     = pg.gen_kernel("foo");
    ker.set_arg(0, mem_tmp);
    ker.set_arg(1, mem);
    queue.push_write(mem_tmp, sizeof(float) * size, ptr);
    queue.push_kernel(ker, size);
    queue.finish();
    return mem; }
  OCL::Memory mem = queue.gen_mem_hw_dr(sizeof(float) * size);
  queue.push_write(mem, sizeof(float) * size, ptr);
  return mem; }

static double measure_compute_matM(const OCL::Queue &queue,
				   const SgemmParam &param,
				   uint nbatch, uint nm0, uint nn0, uint nk0,
				   uint sample_size) {
  assert(queue.ok() && param.ok());
  assert(0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0 && 0 < sample_size);
  size_t size_g[3], size_l[3];
  uint nm, nn, nk;
  string str;
  tie(str, nm, nn, nk, size_g[0], size_g[1], size_l[0],
      size_l[1]) = gen_code_compute_matM(nm0, nn0, nk0, param);
  size_g[2] = nbatch;
  size_l[2] = 1U;
  unique_ptr<float []> host_a(new float [nbatch * nm * nk]);
  unique_ptr<float []> host_b(new float [nbatch * nk * nn]);
  unique_ptr<float []> host_c(new float [nbatch * nm * nn]);
  fill_n(host_a.get(), nbatch * nm * nk, 0.1f);
  fill_n(host_b.get(), nbatch * nk * nn, 0.01f);
  OCL::Program pg   = queue.gen_program(str.c_str());
  OCL::Kernel ker   = pg.gen_kernel("compute_matM");
  OCL::Memory mem_a = push_write(param.do_half, queue, host_a.get(),
				 nbatch * nm * nk);
  OCL::Memory mem_b = push_write(param.do_half, queue, host_b.get(),
				 nbatch * nk * nn);
  OCL::Memory mem_c = queue.gen_mem_drw(nbatch * nm * nn * sizeof(float));
  ker.set_arg(0, mem_a);
  ker.set_arg(1, mem_b);
  ker.set_arg(2, mem_c);
  queue.push_ndrange_kernel(ker, 3U, size_g, size_l);
  queue.finish();
  steady_clock::time_point start = steady_clock::now();
  for (uint usample = 0; usample < sample_size; ++usample)
    queue.push_ndrange_kernel(ker, 3U, size_g, size_l);
  queue.finish();
  steady_clock::time_point end = steady_clock::now();
  double elapsed = static_cast<double>
    (duration_cast<microseconds>(end - start).count());
  return (elapsed / static_cast<double>(sample_size)); }

void ManageComputeMatM::start(const OCL::Device &dev, const OCL::Queue &queue,
			      uint nbatch, uint nm0, uint nn0, uint nk0,
			      bool use_half, bool use_wmma) noexcept {
  assert(dev.ok() && queue.ok() && 0 < nbatch && 0 < nm0 && 0 < nn0
	 && 0 < nk0);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax, nlstart, nlfmmax, factor;
  uint mmax  = ceil_power2(nm0);
  uint nmax  = ceil_power2(nn0);
  uint kmax  = ceil_power2(nk0);

  factor  = use_half ? 2U : 1U;
  mmax    = max(mmax, factor);
  nmax    = max(nmax, factor);
  wgmax   = min(static_cast<uint>(dev.gen_max_work_group_size()), 4096U);
  wgmin   = max(mmax*nmax/2U, 1U);
  wgmin   = min(wgmin, 16U);
  nlstart = min(mmax, nmax);
  nlstart = min(nlstart, 4U);
  mmax    = max(mmax, 64U / nmax);
  if      (nmax <= 1U) nlfmmax = 128U;
  else if (nmax <= 2U) nlfmmax =  32U;
  else if (nmax <= 4U) nlfmmax =   8U;
  else                 nlfmmax =   2U;
  for (uint nl = nlstart; nl <= 64U; nl *= 2U) {
    for (uint nlfm = 1U; nlfm <= nlfmmax; nlfm *= 2U) {
      if (nl*nl*nlfm < wgmin || wgmax < nl*nl*nlfm) continue;
      for (uint npm = 1U; npm <= 32U; npm *= 2U) {
	for (uint npn = factor; npn <= 32U; npn *= 2U) {
	  if (256U < npm*npn) continue;
	  if (factor*nl < npm) continue;
	  if (factor*nl*nlfm < npn) continue;
	  for (uint npk = 1U; npk <= min(4U, nl); npk *= 2U) {
	    if (mmax < nl * nlfm * npm) continue;
	    if (nmax < nl * npn) continue;
	    if (kmax < nl * nlfm * npk) continue;
	    if (nl * nlfm * npm < factor) continue;
	    if (nl * npn < factor) continue;
	    if (nlfm * npm * npk < factor) continue;
            if (npn * npk < factor) continue;
	    params.emplace_back(use_half, nl, nlfm, npm, npn, npk); } } } } }
#if 0
  if (use_wmma) {
    mmax = ceil_power2(nm0);
    nmax = ceil_power2(nn0);
    kmax = ceil_power2(nk0);
    mmax = max(mmax, 8U);
    nmax = max(nmax, 8U);
    kmax = max(kmax, 16U);
    if      (nmax == 8U)  mmax = max(mmax, 32U);
    else if (nmax == 16U) mmax = max(mmax, 16U);
    uint ntm, ntn, ntk = 16U;
    for (uint nl = 1U; nl <= 16U; nl *= 2U)
      for (uint npm = 1U; npm <= 4U; npm *= 2U)
	for (uint npn = 1U; npn <= 4U; npn *= 2U)
	  for (uint npk = 1U; npk <= 4U; npk *= 2U)
	    for (auto t : { make_tuple(16U, 16U) }) {
	      tie(ntm, ntn) = t;
	      if (mmax < nl*npm*ntm) continue;
	      if (nmax < nl*npn*ntn) continue;
	      if (kmax < nl*npk*ntk) continue;
	      if (2U*nl*size_wrap_wmma < npm*ntm) continue;
	      if (2U*nl*size_wrap_wmma < npn*ntn) continue;
	      params.emplace_back(use_half, true, nl, npm, npn, npk, ntm, ntn);
	    } }
#endif

  OCL::Queue qtmp = dev.gen_queue();
  _time = DBL_MAX;
  while (! params.empty()) {
    SgemmParam param = params.front();
    params.pop_front();
    double elapsed = DBL_MAX;
    bool flag_error = false;
    try {
      elapsed = measure_compute_matM(qtmp, param, nbatch, nm0, nn0, nk0, 1U); }
    catch (...) { elapsed = DBL_MAX; flag_error = true; }
    //catch (std::exception &e) {
    //  std::cout << e.what() << std::endl;
    //  std::terminate();
    //  elapsed = DBL_MAX; flag_error = true; }

    if (flag_error) {
      for (auto it = params.begin(); it != params.end(); ) {
	if (param <= *it) it = params.erase(it);
	else ++it; } }
    else {
      param.time = elapsed;
      params_candi.push_back(param);
      if (elapsed < _time) { _time = elapsed; _param = param; } } }
  if (_time == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  double time_max = min(_time * 2.0 + 20.0, _time + 5000.0);
  for (auto it = params_candi.begin(); it != params_candi.end(); ) {
    if (time_max <= it->time) it = params_candi.erase(it);
    else ++it; }

  uint sample_size = static_cast<uint>(100000.0 / _time);
  sample_size = min(sample_size, tune_sample_size);
  if (1U < sample_size) {
    qtmp = dev.gen_queue();
    _time = DBL_MAX;
    while (! params_candi.empty()) {
      SgemmParam param = params_candi.front();
      params_candi.pop_front();
      double elapsed = measure_compute_matM(qtmp, param, nbatch, nm0, nn0, nk0,
					    sample_size);
      if (_time <= elapsed) continue;
      _time  = elapsed;
      _param = param; } }
  if (_time == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  string str;
  tie(str, _nm, _nn, _nk, _size_g[0], _size_g[1], _size_l[0], _size_l[1])
    = gen_code_compute_matM(nm0, nn0, nk0, _param);
  OCL::Program pg = queue.gen_program(str.c_str());
  _ker = pg.gen_kernel("compute_matM");
  _nbatch    = nbatch;
  _size_g[2] = nbatch;
  _size_l[2] = 1U; }

string ManageComputeMatM::gen_info() const noexcept {
  assert(0 < _nbatch);
  string s;
  if (_param.do_wmma) {
    s += string("NL:")   + to_string(_param.nl)   + string(" ");
    s += string("NPM:")  + to_string(_param.npm)  + string(" ");
    s += string("NPN:")  + to_string(_param.npn)  + string(" ");
    s += string("NPK:")  + to_string(_param.npk)  + string(" ");
    s += string("NTM:")  + to_string(_param.ntm)  + string(" ");
    s += string("NTN:")  + to_string(_param.ntn)  + string(" (");
    s += to_string(static_cast<uint>(_time)) + string("us)"); }
  else {
    s += string("NL:")   + to_string(_param.nl)   + string(" ");
    s += string("NLFM:") + to_string(_param.nlfm) + string(" ");
    s += string("NPM:")  + to_string(_param.npm)  + string(" ");
    s += string("NPN:")  + to_string(_param.npn)  + string(" ");
    s += string("NPK:")  + to_string(_param.npk)  + string(" (");
    s += to_string(static_cast<uint>(_time)) + string("us)"); }
  return s; }

void ManageComputeMatM::register_b(const OCL::Memory &mem) const noexcept {
  assert(mem.ok()); _ker.set_arg(1, mem); }

void ManageComputeMatM::register_c(const OCL::Memory &mem) const noexcept {
  assert(mem.ok()); _ker.set_arg(2, mem); }

void ManageComputeMatM::push(const OCL::Queue &queue, const OCL::Memory &mem_a)
  const noexcept {
  assert(queue.ok() && mem_a.ok());
  _ker.set_arg(0, mem_a);
  queue.push_ndrange_kernel(_ker, 3U, _size_g, _size_l); }

static double measure_sgemm(const OCL::Queue &queue, const SgemmParam param,
			    uint nm0, uint nn0, uint nk0, uint sample_size) {
  assert(qtmp.ok() && param.ok());
  assert(0 < nm0 && 0 < nn0 && 0 < nk0 && 0 < sample_size);
  uint nm = ceil_multi(nm0, param.nl * param.nlfm * param.npm);
  uint nn = ceil_multi(nn0, param.nl * param.npn);
  uint nk = ceil_multi(nk0, param.nl * param.nlfm * param.npk);
  unique_ptr<float []> host_a(new float [nm * nk]);
  unique_ptr<float []> host_b(new float [nk * nn]);
  unique_ptr<float []> host_c(new float [nm * nn]);
  string str        = gen_code_sgemm(nm, nn, nk, param);
  OCL::Program pg   = queue.gen_program(str.c_str());
  OCL::Kernel ker   = pg.gen_kernel("sgemm");
  OCL::Memory mem_a = queue.gen_mem_hw_dr(nm * nk * sizeof(float));
  OCL::Memory mem_b = queue.gen_mem_hw_dr(nk * nn * sizeof(float));
  OCL::Memory mem_c = queue.gen_mem_drw(nm * nn * sizeof(float));
  fill_n(host_a.get(), nm * nk, 0.1f);
  fill_n(host_b.get(), nk * nn, 0.01f);
  queue.push_write(mem_a, nm * nk * sizeof(float), host_a.get());
  queue.push_write(mem_b, nk * nn * sizeof(float), host_b.get());
  ker.set_arg(0, mem_a);
  ker.set_arg(1, mem_b);
  ker.set_arg(2, mem_c);
  const size_t size_g[3] = { nn / param.npn, nm / param.npm, 1U };
  const size_t size_l[3] = { param.nl, param.nl * param.nlfm, 1U };
  queue.push_ndrange_kernel(ker, 3U, size_g, size_l);
  queue.finish();
  steady_clock::time_point start = steady_clock::now();
  for (uint usample = 0; usample < sample_size; ++usample)
    queue.push_ndrange_kernel(ker, 3U, size_g, size_l);
  queue.finish();
  steady_clock::time_point end = steady_clock::now();
  double elapsed = static_cast<double>
    (duration_cast<microseconds>(end - start).count());
  return (elapsed / static_cast<double>(sample_size)); }

void ManageSgemm::start(const OCL::Device &dev, const OCL::Queue &queue,
			bool, bool, uint nm0, uint nn0, uint nk0,
			uint offa0, uint lda0, uint offb0, uint ldb0,
			uint offc0, uint ldc0, bool do_bias_ReLU) noexcept {
  assert(0 < nm0 && 0 < nn0 && 0 < nk0);
  assert(0 < lda && 0 < ldb && 0 < ldc);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax, nlstart, nlfmmax;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);

  _do_bias_ReLU = do_bias_ReLU;
  wgmax   = min(static_cast<uint>(dev.gen_max_work_group_size()), 4096U);
  wgmin   = max(mmax*nmax/2U, 1U);
  wgmin   = min(wgmin, 16U);
  nlstart = min(mmax, nmax);
  nlstart = min(nlstart, 4U);
  mmax    = max(mmax, 64U / nmax);
  if      (nmax <= 1U) nlfmmax = 128U;
  else if (nmax <= 2U) nlfmmax =  32U;
  else if (nmax <= 4U) nlfmmax =   8U;
  else                 nlfmmax =   2U;
  for (uint nl = nlstart; nl <= 64U; nl *= 2U)
    for (uint nlfm = 1U; nlfm <= nlfmmax; nlfm *= 2U) {
      if (nl*nl*nlfm < wgmin || wgmax < nl*nl*nlfm) continue;
      for (uint npm = 1U; npm <= 32U; npm *= 2U)
	for (uint npn = 1U; npn <= 32U; npn *= 2U) {
	  if (256U < npm*npn) continue;
	  if (nl < npm) continue;
	  if (nl*nlfm < npn) continue;
	  for (uint npk = 1U; npk <= min(4U, nl); npk *= 2U) {
	    if (mmax < nl * nlfm * npm) continue;
	    if (nmax < nl * npn) continue;
	    if (kmax < nl * nlfm * npk) continue;
	    params.emplace_back(false, nl, nlfm, npm, npn, npk); } } }

  OCL::Queue qtmp = dev.gen_queue();
  _time = DBL_MAX;
  while (! params.empty()) {
    SgemmParam param = params.front();
    params.pop_front();
    double elapsed = DBL_MAX;
    bool flag_error = false;
    try {
      elapsed = measure_sgemm(qtmp, param, nm0, nn0, nk0, 1U); }
    catch (...) { elapsed = DBL_MAX; flag_error = true; }
    if (flag_error) {
      for (auto it = params.begin(); it != params.end(); ) {
	if (param <= *it) it = params.erase(it);
	else ++it; } }
    else {
      param.time = elapsed;
      params_candi.push_back(param);
      if (elapsed < _time) { _time = elapsed; _param = param; } } }
  if (_time == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  double time_max = min(_time * 2.0 + 20.0, _time + 5000.0);
  for (auto it = params_candi.begin(); it != params_candi.end(); ) {
    if (time_max <= it->time) it = params_candi.erase(it);
    else ++it; }
  
  uint sample_size = static_cast<uint>(100000.0 / _time);
  sample_size = min(sample_size, tune_sample_size);
  if (1U < sample_size) {
    qtmp = dev.gen_queue();
    _time = DBL_MAX;
    while (! params_candi.empty()) {
      SgemmParam param = params_candi.front();
      params_candi.pop_front();
      double elapsed = measure_sgemm(qtmp, param, nm0, nn0, nk0, sample_size);
      if (_time <= elapsed) continue;
      _time  = elapsed;
      _param = param; } }
  if (_time == DBL_MAX) die(ERR_INT("ManageSgemm() failed."));

  _nm0 = nm0;
  _nn0 = nn0;
  _nk0 = nk0;
  uint nm = ceil_multi(nm0, _param.nl * _param.nlfm * _param.npm);
  uint nn = ceil_multi(nn0, _param.nl * _param.npn);
  uint nk = ceil_multi(nk0, _param.nl * _param.nlfm * _param.npk);
  size_g[0] = nn / _param.npn;
  size_g[1] = nm / _param.npm;
  size_g[2] = 1U;
  size_l[0] = _param.nl;
  size_l[1] = _param.nl * _param.nlfm;
  size_l[2] = 1U;

  OCL::Program pg = queue.gen_program(code_zero_clear);
  OCL::Kernel ker_zero_clear = pg.gen_kernel("zero_clear");

  mem_a = queue.gen_mem_drw(nm * nk * sizeof(float));
  ker_zero_clear.set_arg(0, mem_a);
  queue.push_kernel(ker_zero_clear, nm * nk);

  mem_b = queue.gen_mem_drw(nk * nn * sizeof(float));
  ker_zero_clear.set_arg(0, mem_b);
  queue.push_kernel(ker_zero_clear, nk * nn);

  mem_c = queue.gen_mem_drw(nm * nn * sizeof(float));
  ker_zero_clear.set_arg(0, mem_c);
  queue.push_kernel(ker_zero_clear, nm * nn);
  queue.finish();

  string str =
    "#define NM0   " + to_string(_nm0)  + "\n"
    "#define NN0   " + to_string(_nn0)  + "\n"
    "#define NK0   " + to_string(_nk0)  + "\n"
    "#define OFFA0 " + to_string(offa0) + "\n"
    "#define OFFB0 " + to_string(offb0) + "\n"
    "#define OFFC0 " + to_string(offc0) + "\n"
    "#define LDA0  " + to_string(lda0)  + "\n"
    "#define LDB0  " + to_string(ldb0)  + "\n"
    "#define LDC0  " + to_string(ldc0)  + "\n"
    "#define NM    " + to_string(nm)    + "\n"
    "#define NN    " + to_string(nn)    + "\n"
    "#define NK    " + to_string(nk)    + "\n" + code_sgemm_aux;
  pg = queue.gen_program(str.c_str());
  _done_load_a = _done_load_b = false;

  _ker_a = pg.gen_kernel("mat0_to_matA");
  _ker_a.set_arg(1, mem_a);

  _ker_b = pg.gen_kernel("mat0_to_matB");
  _ker_b.set_arg(1, mem_b);

  if (do_bias_ReLU) {
    _ker_c = pg.gen_kernel("matC_to_mat0_bias_ReLU");
    _ker_c.set_arg(0, mem_c); }
  else {
    _ker_c = pg.gen_kernel("matC_to_mat0");
    _ker_c.set_arg(0, mem_c); }

  str = gen_code_sgemm(nm, nn, nk, _param);
  pg = queue.gen_program(str.c_str());
  _ker_sgemm = pg.gen_kernel("sgemm");
  _ker_sgemm.set_arg(0, mem_a);
  _ker_sgemm.set_arg(1, mem_b);
  _ker_sgemm.set_arg(2, mem_c); }

void ManageSgemm::register_a0(const OCL::Memory &mem) const noexcept {
  assert(0 < _nm0 && mem.ok()); _ker_a.set_arg(0, mem); }

void ManageSgemm::register_b0(const OCL::Memory &mem) const noexcept {
  assert(0 < _nm0 && mem.ok()); _ker_b.set_arg(0, mem); }

void ManageSgemm::register_c0(const OCL::Memory &mem) const noexcept {
  assert(0 < _nm0 && mem.ok());
  if (_do_bias_ReLU) _ker_c.set_arg(2, mem);
  else               _ker_c.set_arg(1, mem); }

void ManageSgemm::register_bias(const OCL::Memory &mem) const noexcept {
  assert(0 < _nm0 && mem.ok() && _do_bias_ReLU); _ker_c.set_arg(1, mem); }

void ManageSgemm::push_load_a(const OCL::Queue &queue) noexcept {
  assert(0 < _nm0);
  queue.push_kernel(_ker_a, _nk0 * _nm0);
  _done_load_a = true; }

void ManageSgemm::push_load_b(const OCL::Queue &queue) noexcept {
  assert(0 < _nm0);
  queue.push_kernel(_ker_b, _nk0 * _nn0);
  _done_load_b = true; }

void ManageSgemm::push(const OCL::Queue &queue) const noexcept {
  assert(0 < _nm0 && queue.ok() && mem_a0.ok() && mem_b0.ok() && mem_c0.ok());
  if (!_done_load_a) queue.push_kernel(_ker_a, _nk0 * _nm0);
  if (!_done_load_b) queue.push_kernel(_ker_b, _nk0 * _nn0);
  queue.push_ndrange_kernel(_ker_sgemm, 3U, size_g, size_l);
  queue.push_kernel(_ker_c, _nm0 * _nn0); }

string ManageSgemm::gen_info() const noexcept {
  assert(0 < _nm0);
  string s;
  s += string("NL:")   + to_string(_param.nl)   + string(" ");
  s += string("NLFM:") + to_string(_param.nlfm) + string(" ");
  s += string("NPM:")  + to_string(_param.npm)  + string(" ");
  s += string("NPN:")  + to_string(_param.npn)  + string(" ");
  s += string("NPK:")  + to_string(_param.npk)  + string(" (");
  s += to_string(static_cast<uint>(_time)) + string("us)");
  return s; }

void ManageComputeMatV::start(bool use_half, const OCL::Queue &queue,
			      uint nch, uint nb, uint nn, uint nk,
			      const OCL::Memory &mem_matV) noexcept {
  string code;
  if (use_half) code += "#define DO_HALF\n";
  code +=
    "#define NB " + to_string(nb) + "U\n"
    "#define NK " + to_string(nk) + "U\n"
    "#define NN " + to_string(nn) + "U\n" + code_common + code_compute_matV;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("compute_matV");
  _ker.set_arg(1, mem_matV);
  _nm = ntile * nch * nb; }

void ManageComputeMatV::push(const OCL::Queue &queue,
			     const OCL::Memory &mem_in) const noexcept {
  assert(0 < _nm && queue.ok());
  _ker.set_arg(0, mem_in);
  queue.push_kernel(_ker, _nm); }

void ManageComputeMatA::start(const OCL::Queue &queue, bool flag_join,
			      uint nch, uint nb, uint nm, uint nn,
			      const OCL::Memory &mem_matM,
			      const OCL::Memory &mem_output) noexcept {
  uint offc = nm * nn;
  string code =
    "#define NB   " + to_string(nb)   + "\n"
    "#define OFFC " + to_string(offc) + "\n"
    "#define NN   " + to_string(nn)   + "\n" + code_common + code_compute_matA;
  OCL::Program pg = queue.gen_program(code.c_str());
  if (flag_join) _ker = pg.gen_kernel("compute_matA_BNReLU_join");
  else           _ker = pg.gen_kernel("compute_matA_BNReLU");
  _ker.set_arg(0, mem_matM);
  _ker.set_arg(3, mem_output);
  _nm = ntile * nch * nb; }

void ManageComputeMatA::push(const OCL::Queue &queue, const OCL::Memory &mean,
			     const OCL::Memory &sd_inv) const noexcept {
  assert(0 < _nm && mean.ok() && sd_inv.ok());
  _ker.set_arg(1, mean);
  _ker.set_arg(2, sd_inv);
  queue.push_kernel(_ker, _nm); }

void ManageComputeBNReLU::start(const OCL::Queue &queue, uint nch, uint nb,
				const OCL::Memory &mean,
				const OCL::Memory &sd_inv,
				const OCL::Memory &mem_io) noexcept {
  string code = "#define NBATCH " + to_string(nb) + "U\n" + code_BNReLU;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("compute_BNReLU");
  _ker.set_arg(0, mean);
  _ker.set_arg(1, sd_inv);
  _ker.set_arg(2, mem_io);
  _nm = nch * nb * NNAux::size_plane; }

void ManageComputeBNReLU::push(const OCL::Queue &queue) const noexcept {
  assert(0 < _nm); queue.push_kernel(_ker, _nm); }

void ManageComputePolicy::start(const OCL::Queue &queue, uint nch_in,
				uint maxsize_batch,
				const OCL::Memory &mem_nnmove,
				const OCL::Memory &mem_weight,
				const OCL::Memory &mem_bias,
				const OCL::Memory &mem_in,
				const OCL::Memory &mem_out) noexcept {
  string code =
    "#define NCH_IN " + to_string(nch_in) + "U\n"
    "#define NBATCH " + to_string(maxsize_batch) + "U\n"
    + code_compute_policy;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("compute_policy");
  _ker.set_arg(0, mem_nnmove);
  _ker.set_arg(2, mem_weight);
  _ker.set_arg(3, mem_bias);
  _ker.set_arg(4, mem_in);
  _ker.set_arg(5, mem_out); }

void ManageComputePolicy::push(const OCL::Queue &queue, uint ntot_moves,
			       uint offin) const noexcept {
  assert(_ker.ok());
  _ker.set_arg(1, sizeof(offin), &offin);
  queue.push_kernel(_ker, ntot_moves); }

void ManageTransformValue2::start(const OCL::Queue &queue, uint nch, uint nb,
				  const OCL::Memory &mem_in, uint offin,
				  const OCL::Memory &mem_out) noexcept {
  string code =
    "#define NCH "    + to_string(nch) + "U\n"
    "#define NBATCH " + to_string(nb)  + "U\n" + code_transform_value2;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("transform_value2");
  _ker.set_arg(0, mem_in);
  _ker.set_arg(1, sizeof(offin), &offin);
  _ker.set_arg(2, mem_out);
  _nm = nch * nb * NNAux::size_plane; }

void ManageTransformValue2::push(const OCL::Queue &queue) const noexcept {
  assert(0 < _nm); queue.push_kernel(_ker, _nm); }

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

static row_t gen_head1_trans_weight(uint nout1, uint nout2, uint nin,
				    const float *w1, const float *w2)
  noexcept {
  row_t row(new float [(nout1 + nout2) * nin]);
  copy_n(w1, nout1 * nin, row.get());
  copy_n(w2, nout2 * nin, row.get() + nout1 * nin);

  uint nout = nout1 + nout2;
  row_t row_trans(new float [nout * nin]);
  for (uint uin = 0; uin < nin; ++uin)
    for (uint uout = 0; uout < nout; ++uout)
      row_trans[uin*nout + uout] = row[uout*nin + uin];

  return row_trans; }

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

static row_t gen_matrix_trans(uint nm, uint nn, const float *p) noexcept {
  row_t row(new float [nm * nn]);
  for (uint um = 0; um < nm; ++um)
    for (uint un = 0; un < nn; ++un) row[un*nm + um] = p[um*nn + un];
  return row; }

static void compute_probs(uint nb, const uint *sizes_nnmove, const float *fin,
			  float *probs) noexcept {
  for (uint ub = 0; ub < nb; ++ub) {
    float *probs_b = probs + ub * NNAux::nmove;
    copy_n(fin, sizes_nnmove[ub], probs_b);
    softmax(sizes_nnmove[ub], probs_b);
    fin += sizes_nnmove[ub]; } }

static void compress_data(uint nb0, uint nb, const float *in,
			  const uint *sizes_nnmove, const ushort *nnmoves,
			  void *out, size_t &size_write, uint &n_one,
			  uint &ntot_moves) noexcept {
  float *pvalue = reinterpret_cast<float *>(out);
  uint  *pindex = reinterpret_cast<uint *>(out) + send_nch_fill * nb;
  uint index;
  for (uint ub = 0; ub < nb; ++ub) {
    for (uint uposi = 0; uposi < 8U; ++uposi)
      for (uint ufplane = 28; ufplane < 45U; ++ufplane) {
	uint ch = 45U * uposi + ufplane;
	index   = (ub * NNAux::nch_input + ch) * NNAux::size_plane;
	*pindex++ = (ch * nb + ub) * NNAux::size_plane;
	*pvalue++ = (ub < nb0) ? in[index] : 0.0f; }
    index     = (ub * NNAux::nch_input + 360U) * NNAux::size_plane;
    *pindex++ = (360U * nb + ub) * NNAux::size_plane;
    *pvalue++ = (ub < nb0) ? in[index] : 0.0f;
    index     = (ub * NNAux::nch_input + 361U) * NNAux::size_plane;
    *pindex++ = (361U * nb + ub) * NNAux::size_plane;
    *pvalue++ = (ub < nb0) ? in[index] : 0.0f; }

  n_one = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint uposi = 0; uposi < 8U; ++uposi)
      for (uint ufplane = 0; ufplane < 28U; ++ufplane) {
	uint ch  = 45U * uposi + ufplane;
	uint bch = (ub * NNAux::nch_input + ch) * NNAux::size_plane;
	uint chb = (ch * nb + ub) * NNAux::size_plane;
	for (uint u = 0; u < NNAux::size_plane; ++u) {
	  if (in[bch + u] < 0.5f) continue;
	  n_one    += 1U;
	  *pindex++ = chb + u; } }

  ntot_moves = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint unn = 0; unn < sizes_nnmove[ub]; ++unn) {
      uint nnmove = nnmoves[ub * NNAux::nmove + unn];
      ntot_moves += 1U;
      assert(nnmove < NNAux::nch_out_policy * NNAux::size_plane);
      *pindex++ = ub * NNAux::nch_out_policy * NNAux::size_plane + nnmove; }

  size_write = (2U*send_nch_fill*nb + n_one + ntot_moves) * sizeof(uint); }

NNetOCL::~NNetOCL() noexcept { _mng_send.end(_queue); }

void NNetOCL::reset(uint maxsize_batch, const vector<pair<uint, row_t>> &wght,
		    int device_id, bool use_half) noexcept {
  assert(0 < maxsize_batch);
#if defined(USE_MKL)
  mkl_set_num_threads_local(1);
#else
  openblas_set_num_threads(1);
#endif

  //
  // compute network dimension
  //
  _maxsize_batch = maxsize_batch;
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
  _resnet_nout = wght[1].first;
  _maxsize_out = _resnet_nout * NNAux::size_plane;
  _nres_block  = nrow_res / 4U;
  uint nin = NNAux::nch_input;
  uint index = 0;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (wght[index].first != _resnet_nout * nin * size_kernel
	|| wght[index + 1U].first != _resnet_nout
	|| wght[index + 2U].first != _resnet_nout
	|| wght[index + 3U].first != _resnet_nout)
      die(ERR_INT(msg_bad_wght_dim));
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

  // load policy2 part (conv 1x1)
  // index + 4: weight
  // index + 5: bias
  _policy2_nin = _policy1_nout;
  _maxsize_out = max(_maxsize_out, NNAux::nch_out_policy * NNAux::size_plane);
  if (wght[index + 4U].first != NNAux::nch_out_policy * _policy2_nin
      || wght[index + 5U].first != NNAux::nch_out_policy)
    die(ERR_INT(msg_bad_wght_dim));

  // load value2 part (fc)
  // index + 10: weight
  // index + 11: bias
  _value2_nin  = _value1_nout * NNAux::size_plane;
  _value2_nout = wght[index + 11U].first;
  _maxsize_out = max(_maxsize_out, _value2_nin);
  _maxsize_out = max(_maxsize_out, _value2_nout);
  if (wght[index + 10U].first != _value2_nout * _value2_nin)
    die(ERR_INT(msg_bad_wght_dim));

  // load value3 part (fc)
  // index + 12: weight
  // index + 13: bias
  _value3_nin  = _value2_nout;
  _value3_nout = wght[index + 13U].first;
  if (wght[index + 12U].first != _value3_nout * _value3_nin
      || _value3_nout != 1) die(ERR_INT(msg_bad_wght_dim));

  //
  // setup opencl kurnels
  //
  if (0 <= device_id) {
    get_device(device_id, _cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("bad device ID %u", device_id)); }
  else {
    get_best_device(_cl_dev);
    if (!_cl_dev.ok()) die(ERR_INT("no device found")); }
  
  std::cout << "- Device ID: " << device_id << "\n";
  std::cout << _cl_dev.gen_info();

  bool use_wmma = false;
  if (use_half) {
    use_wmma = test_wmma(_cl_dev);
    std::cout << "  Wmma support:         "
	      << (use_wmma ? "Yes" : "No") << std::endl; }

  std::cout << "  Send:                 ";
  std::cout.flush();
  _queue = _cl_dev.gen_queue();
  _mng_send.start(_cl_dev, _queue, maxsize_batch);
  std::cout << _mng_send.gen_info() << std::endl;

  std::cout << "  Recv:                 ";
  std::cout.flush();
  _mng_recv.start(_cl_dev, _queue, (1U + NNAux::nmove) * sizeof(float),
		  maxsize_batch);
  std::cout << _mng_recv.gen_info() << std::endl;

  std::cout << "  Matrix M Input:       ";
  std::cout.flush();
  _mng_compute_matM_input.start(_cl_dev, _queue, size_tile_in, _resnet_nout,
				maxsize_batch * ntile, NNAux::nch_input,
				use_half, use_wmma);
  std::cout << _mng_compute_matM_input.gen_info() << std::endl;

  std::cout << "  Matrix M:             ";
  std::cout.flush();
  _mng_compute_matM.start(_cl_dev, _queue, size_tile_in, _resnet_nout,
			  maxsize_batch * ntile, _resnet_nout,
			  use_half, use_wmma);
  std::cout << _mng_compute_matM.gen_info() << std::endl;

  std::cout << "  Head 1:               ";
  std::cout.flush();
  _mng_head1.start(_cl_dev, _queue, true, false, _head1_nout,
		   maxsize_batch * NNAux::size_plane, _resnet_nout, 0,
		   _head1_nout, 0, maxsize_batch * NNAux::size_plane, 0,
		   maxsize_batch * NNAux::size_plane);
  std::cout << _mng_head1.gen_info() << std::endl;

  std::cout << "  Value 2:              ";
  std::cout.flush();
  _mng_value2.start(_cl_dev, _queue, true, false, _value2_nout, maxsize_batch,
		    _value2_nin, 0, _value2_nout, 0, maxsize_batch, 0,
		    maxsize_batch, true);
  std::cout << _mng_value2.gen_info() << std::endl;

  std::cout << "  Value 3:              ";
  std::cout.flush();
  _mng_value3.start(_cl_dev, _queue, true, false, maxsize_batch, 1U,
		    _value3_nin, 0, maxsize_batch, 0, 1U, 0, 1U, false);
  std::cout << _mng_value3.gen_info() << std::endl;

  uint mmax, nmax, kmax;
  mmax = max(_mng_compute_matM_input.get_nm(), _mng_compute_matM.get_nm());
  nmax = max(_mng_compute_matM_input.get_nn(), _mng_compute_matM.get_nn());
  kmax = max(_mng_compute_matM_input.get_nk(), _mng_compute_matM.get_nk());
  uint sizeV = size_tile_in * kmax * nmax;
  uint sizeM = size_tile_in * mmax * nmax;
  _ptr_input_c.reset(new uchar [sizeof(float) * maxsize_batch * 
				(NNAux::nmove
				 + NNAux::nch_input * NNAux::size_plane)]);
  _ptr_result.reset(new float [maxsize_batch * (NNAux::nmove + 1U)]);

  _cl_matV   = _queue.gen_mem_drw(sizeof(float) * sizeV);
  _cl_matM   = _queue.gen_mem_drw(sizeof(float) * sizeM);
  _cl_bypass = _queue.gen_mem_drw(sizeof(float)
				  * maxsize_batch * _maxsize_out);
  _cl_output = _queue.gen_mem_drw(sizeof(float)
				  * maxsize_batch * _maxsize_out);
  _cl_result = _queue.gen_mem_hr_dw(sizeof(float) * maxsize_batch
				    * (NNAux::nmove + 1U));
  _cl_head1_mean   = _queue.gen_mem_hw_dr(sizeof(float) * _head1_nout);
  _cl_head1_sd_inv = _queue.gen_mem_hw_dr(sizeof(float) * _head1_nout);
  _cl_policy2_wght = _queue.gen_mem_hw_dr(sizeof(float)
					  * NNAux::nch_out_policy
					  * _policy2_nin);
  _cl_policy2_bias = _queue.gen_mem_hw_dr(sizeof(float)
					  * NNAux::nch_out_policy);
  _cl_value2_bias  = _queue.gen_mem_hw_dr(sizeof(float) * _value2_nout);
  _cl_head1_wght   = _queue.gen_mem_hw_dr(sizeof(float)
					  * _resnet_nout * _head1_nout);
  _cl_value2_wght  = _queue.gen_mem_hw_dr(sizeof(float)
					  * _value2_nout * _value2_nin);
  _cl_value3_wght  = _queue.gen_mem_hw_dr(sizeof(float) * _value3_nin);

  _mng_compute_matM_input.register_b(_cl_matV);
  _mng_compute_matM_input.register_c(_cl_matM);
  _mng_compute_matM.register_b(_cl_matV);
  _mng_compute_matM.register_c(_cl_matM);
  _mng_compute_matV_input.start(use_half, _queue, NNAux::nch_input,
				maxsize_batch,
				_mng_compute_matM_input.get_nn(),
				_mng_compute_matM_input.get_nk(),
				_cl_matV);
  _mng_compute_matV.start(use_half, _queue, _resnet_nout, maxsize_batch,
			  _mng_compute_matM.get_nn(),
			  _mng_compute_matM.get_nk(), _cl_matV);
  _mng_compute_matA_input.start(_queue, false, _resnet_nout, maxsize_batch,
				_mng_compute_matM_input.get_nm(),
				_mng_compute_matM_input.get_nn(),
				_cl_matM, _cl_bypass);
  _mng_compute_matA.start(_queue, false, _resnet_nout, maxsize_batch,
			  _mng_compute_matM.get_nm(),
			  _mng_compute_matM.get_nn(),
			  _cl_matM, _cl_output);
  _mng_compute_matA_join.start(_queue, true, _resnet_nout, maxsize_batch,
			       _mng_compute_matM.get_nm(),
			       _mng_compute_matM.get_nn(),
			       _cl_matM, _cl_bypass);

  std::cout << "  Loading Weights ... ";
  std::cout.flush();
  load(use_half, wght);
  std::cout << "done" << std::endl;;

  _mng_head1.register_a0(_cl_head1_wght);
  _mng_head1.register_b0(_cl_bypass);
  _mng_head1.register_c0(_cl_bypass);
  _mng_head1.push_load_a(_queue);

  _mng_compute_BNReLU.start(_queue, _head1_nout, maxsize_batch, 
			    _cl_head1_mean, _cl_head1_sd_inv, _cl_bypass);

  _mng_compute_policy.start(_queue, _policy2_nin, maxsize_batch,
			    _mng_send.get_work(), _cl_policy2_wght,
			    _cl_policy2_bias, _cl_bypass, _mng_recv.get());
			    //_cl_result);

  _mng_transform_value2.start(_queue, _value1_nout, maxsize_batch, _cl_bypass,
			      _policy1_nout * maxsize_batch
			      * NNAux::size_plane,
			      _cl_output);

  _mng_value2.register_a0(_cl_value2_wght);
  _mng_value2.register_b0(_cl_output);
  _mng_value2.register_c0(_cl_output);
  _mng_value2.register_bias(_cl_value2_bias);
  _mng_value2.push_load_a(_queue);

  _mng_value3.register_a0(_cl_output);
  _mng_value3.register_b0(_cl_value3_wght);
  //_mng_value3.register_c0(_cl_result);
  _mng_value3.register_c0(_mng_recv.get());
  _mng_value3.push_load_b(_queue);

  OCL::Program pg = _queue.gen_program(code_zero_clear);
  OCL::Kernel ker_zero_clear = pg.gen_kernel("zero_clear");
  ker_zero_clear.set_arg(0, _cl_matV);
  _queue.push_kernel(ker_zero_clear, sizeV);
  ker_zero_clear.set_arg(0, _cl_matM);
  _queue.push_kernel(ker_zero_clear, sizeM);
  _queue.finish();
  _cl_head1_wght.clear();
  _cl_value2_wght.clear();
  _cl_value3_wght.clear(); }

void NNetOCL::load(bool use_half, const vector<pair<uint, row_t>> &wght)
  noexcept {
  uint nin   = NNAux::nch_input;
  uint nm    = _mng_compute_matM_input.get_nm();
  uint nk    = _mng_compute_matM_input.get_nk();
  uint index = 0;
  for (uint u = 0; u < 1U + _nres_block; ++u) {
    uint sizeU = size_tile_in * nm * nk;
    row_t matU = gen_matU(_resnet_nout, nin, wght[index].second.get(), nm, nk);
    row_t mean = gen_mean(_resnet_nout, wght[index + 1U].second.get(),
			  wght[index + 2U].second.get());
    row_t sdinv= gen_sd_inv(_resnet_nout, wght[index + 3U].second.get());
    OCL::Memory cl_matU = push_write(use_half, _queue, matU.get(), sizeU);
    OCL::Memory cl_mean = push_write(false, _queue, mean.get(), _resnet_nout);
    OCL::Memory cl_sdinv= push_write(false, _queue, sdinv.get(), _resnet_nout);
    _cl_reswghts.push_back({move(cl_matU), move(cl_mean), move(cl_sdinv)});
    _queue.finish();
    nin = _resnet_nout;
    nm  = _mng_compute_matM.get_nm();
    nk  = _mng_compute_matM.get_nk();
    index += 4U; }

  row_t row = gen_head1_trans_weight(_policy1_nout, _value1_nout,
				     _resnet_nout,
				     wght[index + 0U].second.get(),
				     wght[index + 6U].second.get());
  _queue.push_write(_cl_head1_wght, sizeof(float) * _resnet_nout * _head1_nout,
		    row.get());
  _queue.finish();

  row = gen_head1_mean(_policy1_nout, _value1_nout,
		       wght[index + 1U].second.get(),
		       wght[index + 2U].second.get(),
		       wght[index + 7U].second.get(),
		       wght[index + 8U].second.get());
  _queue.push_write(_cl_head1_mean, sizeof(float) * _head1_nout, row.get());
  _queue.finish();
  row = gen_head1_sd_inv(_policy1_nout, _value1_nout,
			 wght[index + 3U].second.get(),
			 wght[index + 9U].second.get());
  _queue.push_write(_cl_head1_sd_inv, sizeof(float) * _head1_nout, row.get());
  _queue.finish();
  
  row.reset(new float [NNAux::nch_out_policy * _policy2_nin]);
  copy_n(wght[index + 4U].second.get(), NNAux::nch_out_policy * _policy2_nin,
	 row.get());
  _queue.push_write(_cl_policy2_wght,
		    sizeof(float) * NNAux::nch_out_policy * _policy2_nin,
		    row.get());
  _queue.finish();

  row.reset(new float [NNAux::nch_out_policy]);
  copy_n(wght[index + 5U].second.get(), NNAux::nch_out_policy,
	 row.get());
  _queue.push_write(_cl_policy2_bias, sizeof(float) * NNAux::nch_out_policy,
		    row.get());
  _queue.finish();

  row = gen_matrix_trans(_value2_nout, _value2_nin,
			 wght[index + 10U].second.get());
  _queue.push_write(_cl_value2_wght,
		    sizeof(float) * _value2_nout * _value2_nin,
		    row.get());
  _queue.push_write(_cl_value2_bias, sizeof(float) * _value2_nout,
		    wght[index + 11U].second.get());
  _queue.push_write(_cl_value3_wght, sizeof(float) * _value3_nin,
		    wght[index + 12U].second.get());
  _queue.finish();

  _value3_bias.reset(new float [_value3_nout]);
  copy_n(wght[index + 13U].second.get(), _value3_nout, _value3_bias.get()); }

// feed forward neural network
void NNetOCL::ff(uint size_batch, const float *input, const uint *sizes_nnmove,
		 const ushort *nnmoves, float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);
  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("size_batch == 0"));

  size_t size_write;
  uint n_one, ntot_moves;
  compress_data(size_batch, _maxsize_batch, input, sizes_nnmove, nnmoves,
		_ptr_input_c.get(), size_write, n_one, ntot_moves);
  _mng_send.push(_queue, _ptr_input_c.get(), size_write, n_one);

  // body part
  _mng_compute_matV_input.push(_queue, _mng_send.get());
  _mng_compute_matM_input.push(_queue, _cl_reswghts[0].matU);
  _mng_compute_matA_input.push(_queue, _cl_reswghts[0].mean,
			       _cl_reswghts[0].sd_inv);
  uint ulayer = 1U;
  for (; ulayer < _cl_reswghts.size(); ulayer += 2U) {
    _mng_compute_matV.push(_queue, _cl_bypass);
    _mng_compute_matM.push(_queue, _cl_reswghts[ulayer].matU);
    _mng_compute_matA.push(_queue, _cl_reswghts[ulayer].mean,
			   _cl_reswghts[ulayer].sd_inv);

    _mng_compute_matV.push(_queue, _cl_output);
    _mng_compute_matM.push(_queue, _cl_reswghts[ulayer + 1U].matU);
    _mng_compute_matA_join.push(_queue, _cl_reswghts[ulayer + 1U].mean,
				_cl_reswghts[ulayer + 1U].sd_inv); }

  // head part
  // in:  f1[_policy1_nout + _value1_nout][size_batch][size_plane]
  // out: f2[size_batch][_value1_nout][size_plane]
  _mng_head1.push(_queue);
  _mng_compute_BNReLU.push(_queue);

  _mng_compute_policy.push(_queue, ntot_moves,
			   2U*send_nch_fill*_maxsize_batch + n_one);
  _mng_transform_value2.push(_queue);
  _mng_value2.push(_queue);
  _mng_value3.push(_queue);
  _mng_recv.push_finish(_queue, _ptr_result.get(),
			(_maxsize_batch + ntot_moves) * sizeof(float));
  //_queue.push_read(_cl_result, (_maxsize_batch + ntot_moves) * sizeof(float),
  //_ptr_result.get());
  compute_probs(size_batch, sizes_nnmove, _ptr_result.get() + _maxsize_batch,
		probs);
  for (uint ub = 0; ub < size_batch; ++ub)
    values[ub] = std::tanh(_ptr_result[ub] + _value3_bias[0]); }
#endif
