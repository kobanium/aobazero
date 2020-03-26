// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(USE_OPENCL_AOBA)
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ocl.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <cassert>
#include <chrono>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#define WMMA_ACCUMU16 0

using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using std::copy_n;
using std::cout;
using std::deque;
using std::endl;
using std::fill_n;
using std::forward;
using std::make_tuple;
using std::max;
using std::max_element;
using std::min;
using std::move;
using std::mutex;
using std::pair;
using std::set;
using std::sort;
using std::string;
using std::stringstream;
using std::swap;
using std::tie;
using std::to_string;
using std::tuple;
using std::unique_lock;
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
constexpr uint read_size_ave      = 400U;
constexpr uint send_nch_fill      = 17U * 8U + 2U;
constexpr uint size_align_local   = 16U;
constexpr float bn_factor         = 1.0f / 999.982f;
constexpr float bn_eps            = 1e-5f;

/*
  static double elapsed_sum = 0.0;
  static uint nelapsed      = 0;

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

const string code_decode =
  "#define NCH_FILL " + to_string(send_nch_fill) + R"(U
__kernel __attribute__((reqd_work_group_size(128, 1, 1)))
void zero_clear(__global float *p) {
  uint sq128 = get_global_id(0);
  uint chb   = get_global_id(1);
  p[chb*128U + sq128] = 0.0f; }

__kernel __attribute__((reqd_work_group_size(81, 1, 1)))
void plane_fill(__global const float *pvalue, __global const uint *pindex,
                __global float *p) {
  uint uplane = get_global_id(0);
  uint ich    = get_global_id(1);
  uint index  = pindex[INDEX_BLOCK + ich];
  p[index + uplane] = pvalue[ich]; }

__kernel void set_one(__global const uint *pindex, __global float *p) {
  uint index = pindex[2U*INDEX_BLOCK + get_global_id(0)];
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
    fsum += weight[chout*NCH_IN + chin] * fin[chin*NB*128U + ub*128U + sq];
  result[NB + gid] = fsum; }
)";

const string code_transform_value2 =
  "#define SIZE_PLANE " + to_string(NNAux::size_plane) + R"(U
__kernel __attribute__((reqd_work_group_size(SIZE_PLANE, 1, 1)))
void transform_value2(__global const float *fin, uint offin,
                      __global float *fout) {
  uint sq = get_global_id(0);
  uint ub = get_global_id(1);
  uint ch = get_global_id(2);
  fout[ch*SIZE_PLANE*NN_OUT + sq*NN_OUT + ub]
    = fin[offin + ch*NBATCH*128U + ub*128U + sq]; }
)";

const string code_BNReLU =
  "#define SIZE_PLANE " + to_string(NNAux::size_plane) + R"(U
__kernel __attribute__((reqd_work_group_size(SIZE_PLANE, 1, 1)))
void compute_BNReLU(__global const float *mean, __global const float *sd_inv,
                    __global const float *fin, __global float *fout) {
  uint sq = get_global_id(0);
  uint ub = get_global_id(1);
  uint ch = get_global_id(2);
  float a = sd_inv[ch];
  float b = mean[ch];
  fout[ch*NB*128U + ub*128U + sq]
    = max(0.0f, a*(fin[ch*NN_IN + ub*SIZE_PLANE + sq] - b));
}
)";

const string code_common =
  "#define SIZE_PLANE "   + to_string(NNAux::size_plane) + "U\n"
  "#define HEIGHT "       + to_string(NNAux::height)     + "U\n"
  "#define WIDTH "        + to_string(NNAux::width)      + "U\n"
  "#define SIZE_ALIGN "   + to_string(size_align_local)  + "U\n"
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

const string code_compute_matV_child = R"(
#ifdef STORE_HALF
void store(float f, uint off, __global half *p) { vstore_half(f, off, p); }
#else
void store(float f, uint off, __global float *p) { p[off] = f; }
#endif

void compute_matV_child(uint ch, uint ub, uint utile, uint uh, uint uw,
                        __local const float *flin, __local float *flV,
                        __global void *matV) {
  int y0 = uh*LEN_TILE_OUT - PAD;
  int x0 = uw*LEN_TILE_OUT - PAD;
  float md[LEN_TILE_IN][LEN_TILE_IN];
  for (int y = 0; y < LEN_TILE_IN; ++y)
    for (int x = 0; x < LEN_TILE_IN; ++x) {
      if (0 <= y0 + y && y0 + y < HEIGHT && 0 <= x0 + x && x0 + x < WIDTH)
        md[y][x] = flin[ub*SIZE_PLANE + (y0 + y)*WIDTH + x0 + x];
      else md[y][x] = 0.0f; }

  flV[(0U*LEN_TILE_IN + 0U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[0][0]) - x2(md[0][1]) - x4(md[0][2]) + x2(md[0][3])
      - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
      - x4(md[2][0]) + x2(md[2][1]) + x4(md[2][2]) - x2(md[2][3])
      + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]);
  flV[(1U*LEN_TILE_IN + 0U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[1][0]) + x2(md[1][1]) + x4(md[1][2]) - x2(md[1][3])
      - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
      + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]);
  flV[(2U*LEN_TILE_IN + 0U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
      - x6(md[2][0]) + x3(md[2][1]) + x6(md[2][2]) - x3(md[2][3])
      + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]);
  flV[(3U*LEN_TILE_IN + 0U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
      + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]);
  flV[(4U*LEN_TILE_IN + 0U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
      - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
      - x4(md[3][0]) + x2(md[3][1]) + x4(md[3][2]) - x2(md[3][3])
      + x2(md[4][0]) - x1(md[4][1]) - x2(md[4][2]) + x1(md[4][3]);
  flV[(0U*LEN_TILE_IN + 1U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[0][1]) - x2(md[0][2]) + x2(md[0][3])
      + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
      + x4(md[2][1]) + x2(md[2][2]) - x2(md[2][3])
      - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]);
  flV[(1U*LEN_TILE_IN + 1U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][1]) + x2(md[1][2]) - x2(md[1][3])
      + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
      - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]);
  flV[(2U*LEN_TILE_IN + 1U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
      + x6(md[2][1]) + x3(md[2][2]) - x3(md[2][3])
      - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]);
  flV[(3U*LEN_TILE_IN + 1U)*NB*NTILE + ub*NTILE + utile]
    = + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
      - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]);
  flV[(4U*LEN_TILE_IN + 1U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
      + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
      + x4(md[3][1]) + x2(md[3][2]) - x2(md[3][3])
      - x2(md[4][1]) - x1(md[4][2]) + x1(md[4][3]);
  flV[(0U*LEN_TILE_IN + 2U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[0][1]) - x6(md[0][2]) + x2(md[0][3])
      - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
      - x4(md[2][1]) + x6(md[2][2]) - x2(md[2][3])
      + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]);
  flV[(1U*LEN_TILE_IN + 2U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[1][1]) + x6(md[1][2]) - x2(md[1][3])
      - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
      + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]);
  flV[(2U*LEN_TILE_IN + 2U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
      - x6(md[2][1]) + x9(md[2][2]) - x3(md[2][3])
      + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]);
  flV[(3U*LEN_TILE_IN + 2U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
      + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]);
  flV[(4U*LEN_TILE_IN + 2U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
      - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
      - x4(md[3][1]) + x6(md[3][2]) - x2(md[3][3])
      + x2(md[4][1]) - x3(md[4][2]) + x1(md[4][3]);
  flV[(0U*LEN_TILE_IN + 3U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[0][1]) + x2(md[0][3]) + x1(md[1][1]) - x1(md[1][3])
      + x2(md[2][1]) - x2(md[2][3]) - x1(md[3][1]) + x1(md[3][3]);
  flV[(1U*LEN_TILE_IN + 3U)*NB*NTILE + ub*NTILE + utile]
    = + x2(md[1][1]) - x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
      - x1(md[3][1]) + x1(md[3][3]);
  flV[(2U*LEN_TILE_IN + 3U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[1][1]) + x2(md[1][3]) + x3(md[2][1]) - x3(md[2][3])
      - x1(md[3][1]) + x1(md[3][3]);
  flV[(3U*LEN_TILE_IN + 3U)*NB*NTILE + ub*NTILE + utile]
    = + x1(md[1][1]) - x1(md[1][3]) - x1(md[3][1]) + x1(md[3][3]);
  flV[(4U*LEN_TILE_IN + 3U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[1][1]) + x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
      + x2(md[3][1]) - x2(md[3][3]) - x1(md[4][1]) + x1(md[4][3]);
  flV[(0U*LEN_TILE_IN + 4U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[0][1]) - x2(md[0][2]) - x4(md[0][3]) + x2(md[0][4])
      - x2(md[1][1]) + x1(md[1][2]) + x2(md[1][3]) - x1(md[1][4])
      - x4(md[2][1]) + x2(md[2][2]) + x4(md[2][3]) - x2(md[2][4])
      + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]);
  flV[(1U*LEN_TILE_IN + 4U)*NB*NTILE + ub*NTILE + utile]
    = - x4(md[1][1]) + x2(md[1][2]) + x4(md[1][3]) - x2(md[1][4])
      - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
      + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]);
  flV[(2U*LEN_TILE_IN + 4U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][1]) - x6(md[2][1]) + x2(md[3][1])
      - x2(md[1][2]) + x3(md[2][2]) - x1(md[3][2])
      - x4(md[1][3]) + x6(md[2][3]) - x2(md[3][3])
      + x2(md[1][4]) - x3(md[2][4]) + x1(md[3][4]);
  flV[(3U*LEN_TILE_IN + 4U)*NB*NTILE + ub*NTILE + utile]
    = - x2(md[1][1]) + x2(md[3][1]) + x1(md[1][2]) - x1(md[3][2])
      + x2(md[1][3]) - x2(md[3][3]) - x1(md[1][4]) + x1(md[3][4]);
  flV[(4U*LEN_TILE_IN + 4U)*NB*NTILE + ub*NTILE + utile]
    = + x4(md[1][1]) - x2(md[1][2]) - x4(md[1][3]) + x2(md[1][4])
      - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
      - x4(md[3][1]) + x2(md[3][2]) + x4(md[3][3]) - x2(md[3][4])
      + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4]);

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint sq = 0; sq < LEN_TILE_IN*LEN_TILE_IN; ++sq)
    store(flV[sq*NB*NTILE + ub*NTILE + utile],
          sq*NK*NN + ch*NN + ub*NTILE + utile, matV); }
)";

const string code_compute_matA_child = R"(
#ifdef DO_JOIN
void func_BNReLU(__local float *f, uint off, float sd_inv, float mean,
                 float x) {
  f[off] = max(0.0f, sd_inv * (x - mean) + f[off]); }
#else
void func_BNReLU(__local float *f, uint off, float sd_inv, float mean,
                 float x) {
  f[off] = max(0.0f, sd_inv * (x - mean)); }
#endif

void compute_matA_child(uint origin, float mean, float sd_inv,
                        float mm[LEN_TILE_IN][LEN_TILE_IN],
                        __local float *flout) {
  func_BNReLU(flout, origin + 0U*WIDTH + 0U, sd_inv, mean,
              + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
              + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  func_BNReLU(flout, origin + 0U*WIDTH + 1U, sd_inv, mean,
              + mm[0][1] - mm[0][2] + x2(mm[0][3])
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              + mm[2][1] - mm[2][2] + x2(mm[2][3])
              + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  func_BNReLU(flout, origin + 0U*WIDTH + 2U, sd_inv, mean,
              + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
              + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  func_BNReLU(flout, origin + 1U*WIDTH + 0U, sd_inv, mean,
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
              + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));
  func_BNReLU(flout, origin + 1U*WIDTH + 1U, sd_inv, mean,
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              - mm[2][1] + mm[2][2]
              + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));
  func_BNReLU(flout, origin + 1U*WIDTH + 2U, sd_inv, mean,
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
              + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));
  func_BNReLU(flout, origin + 2U*WIDTH + 0U, sd_inv, mean,
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
              + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
              + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  func_BNReLU(flout, origin + 2U*WIDTH + 1U, sd_inv, mean,
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              + mm[2][1] - mm[2][2]
              + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
              + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  func_BNReLU(flout, origin + 2U*WIDTH + 2U, sd_inv, mean,
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
              + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
              + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]);
  barrier(CLK_LOCAL_MEM_FENCE); }
)";

const string code_compute_matA = R"(
#ifdef LOAD_HALF
float load(uint off, __global const half *p) { return vload_half(off, p); }
#else
float load(uint off, __global const float *p) { return p[off]; }
#endif

__kernel __attribute__((reqd_work_group_size(NTILE, NB, 1)))
void compute_matA_BNReLU(__global const void *matM,
                         __global const float *mean_array,
                         __global const float *sd_inv_array,
                         __global const float *fbypass,
                         __global float *fout) {
  uint utile = get_global_id(0);
  uint ub    = get_global_id(1);
  uint ch    = get_global_id(2);
  uint chb   = ch*NB + ub;
  float mm[LEN_TILE_IN][LEN_TILE_IN];

  __local float flM[LEN_TILE_IN*LEN_TILE_IN * NTILE*NB]
                __attribute__((aligned(SIZE_ALIGN)));
  for (uint sq = 0; sq < LEN_TILE_IN*LEN_TILE_IN; ++sq)
    flM[sq*NB*NTILE + ub*NTILE + utile]
      = load(sq*NM*NN + ch*NN + ub*NTILE + utile, matM);

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = flM[(uh_in * LEN_TILE_IN + uw_in)*NB*NTILE
                             + ub*NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint origin  = ub*SIZE_PLANE + (uh*WIDTH + uw)*LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  __local float flout[NB*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));

#ifdef DO_JOIN
  for (uint u = 0; u < NTILE; ++u)
    flout[u*NB*NTILE + ub*NTILE + utile]
      = fbypass[ch*NB*128U + u*NB*NTILE + ub*NTILE + utile];
  barrier(CLK_LOCAL_MEM_FENCE);
#endif

  compute_matA_child(origin, mean, sd_inv, mm, flout);

  for (uint u = 0; u < NTILE; ++u)
    fout[ch*NN_OUT + u*NB*NTILE + ub*NTILE + utile]
      = flout[u*NB*NTILE + ub*NTILE + utile]; }
)";

const string code_compute_matV = R"(
__kernel __attribute__((reqd_work_group_size(NTILE, NB, 1)))
void compute_matV(__global const float *fin, __global void *matV) {
  uint utile = get_global_id(0);
  uint ub    = get_global_id(1);
  uint ch    = get_global_id(2);
  uint uh    = utile / NTILE_W;
  uint uw    = utile % NTILE_W;
  __local float flin[NB*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));
  __local float flV[LEN_TILE_IN*LEN_TILE_IN*NB*NTILE]
                __attribute__((aligned(SIZE_ALIGN)));
  for (uint u = 0; u < 9U; ++u)
    flin[u*NB*NTILE + ub*NTILE + utile]
      = fin[ch*NB*SIZE_PLANE + u*NB*NTILE + ub*NTILE + utile];
  barrier(CLK_LOCAL_MEM_FENCE);
  compute_matV_child(ch, ub, utile, uh, uw, flin, flV, matV); }
)";

const string code_compute_matAV = R"(
#ifdef LOAD_HALF
float load(uint off, __global const half *p) { return vload_half(off, p); }
#else
float load(uint off, __global const float *p) { return p[off]; }
#endif

__kernel __attribute__((reqd_work_group_size(NTILE, NB, 1)))
void compute_matAV(__global const void *matM,
                   __global const float *mean_array,
                   __global const float *sd_inv_array,
#if defined(DO_JOIN) || defined(DO_FORK)
                   __global float *matV, __global float *fbypass) {
#else
                   __global float *matV) {
#endif
  uint utile = get_global_id(0);
  uint ub    = get_global_id(1);
  uint ch    = get_global_id(2);
  float mm[LEN_TILE_IN][LEN_TILE_IN];

  __local float flM[LEN_TILE_IN*LEN_TILE_IN * NTILE*NB]
                __attribute__((aligned(SIZE_ALIGN)));
  for (uint sq = 0; sq < LEN_TILE_IN*LEN_TILE_IN; ++sq)
    flM[sq*NB*NTILE + ub*NTILE + utile]
      = load(sq*NM*NN + ch*NN + ub*NTILE + utile, matM);

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint uh_in = 0; uh_in < LEN_TILE_IN; ++uh_in)
    for (uint uw_in = 0; uw_in < LEN_TILE_IN; ++uw_in)
      mm[uh_in][uw_in] = flM[(uh_in*LEN_TILE_IN + uw_in)*NB*NTILE
                             + ub*NTILE + utile];

  uint uh      = utile / NTILE_W;
  uint uw      = utile % NTILE_W;
  uint origin  = ub*SIZE_PLANE + (uh*WIDTH + uw)*LEN_TILE_OUT;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  __local float flout[NB*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));
  __local float flV[LEN_TILE_IN*LEN_TILE_IN*NB*NTILE]
                __attribute__((aligned(SIZE_ALIGN)));

#ifdef DO_JOIN
  for (uint u = 0; u < NTILE; ++u)
    flout[u*NB*NTILE + ub*NTILE + utile]
      = fbypass[ch*NB*128U + u*NB*NTILE + ub*NTILE + utile];
  barrier(CLK_LOCAL_MEM_FENCE);
#endif

  compute_matA_child(origin, mean, sd_inv, mm, flout);

#ifdef DO_FORK
  for (uint u = 0; u < NTILE; ++u)
    fbypass[ch*NB*128U + u*NB*NTILE + ub*NTILE + utile]
      = flout[u*NB*NTILE + ub*NTILE + utile];
#endif

  compute_matV_child(ch, ub, utile, uh, uw, flout, flV, matV); }
)";

const string code_compute_matM_wmma = R"(
__kernel __attribute__((reqd_work_group_size(SGEMM_NL*WRAP_SIZE,
                                             SGEMM_NL,1)))
void compute_matM(__global const uint *gA, __global const uint *gB,
#if WMMA_ACCUMU16
                  __global ushort *gC) {
#else
                  __global float *gC) {
#endif
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  uint ulm = get_local_id(1);
  uint ul  = get_local_id(1)*SGEMM_NL*WRAP_SIZE + get_local_id(0);
  uint uln = get_local_id(0) / WRAP_SIZE;
  uint ngk = NK / SGEMM_NLPTK;
  gA += ub*(OFFA/2U) + ugm*(SGEMM_NLPTM/2U);
  gB += ub*(OFFB/2U) + ugn*(SGEMM_NLPTN/2U);
  gC += ub*OFFC + ugm*SGEMM_NLPTM*NN + ugn*SGEMM_NLPTN;

  __local uint lA[SGEMM_NLPTK*(SGEMM_NLPTM/2U)] __attribute__((aligned(32)));
  __local uint lB[SGEMM_NLPTK*(SGEMM_NLPTN/2U)] __attribute__((aligned(32)));

#if WMMA_ACCUMU16
  uint pD[SGEMM_NPM][SGEMM_NPN][4];
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      for (uint u = 0; u < 4U; ++u) pD[upm][upn][u] = 0;
#else
  uint pD[SGEMM_NPM][SGEMM_NPN][8];
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      for (uint u = 0; u < 8U; ++u) pD[upm][upn][u] = 0;
#endif

  uint ulA1 = ul % (SGEMM_NLPTM/2U);
  uint ulA2 = ul / (SGEMM_NLPTM/2U);
  uint ulA3 = (SGEMM_NL*WRAP_SIZE) / (SGEMM_NPTM/2U);
  uint ulB1 = ul % (SGEMM_NLPTN/2U);
  uint ulB2 = ul / (SGEMM_NLPTN/2U);
  uint ulB3 = (SGEMM_NL*WRAP_SIZE) / (SGEMM_NPTN/2U);

  uint ldlA = SGEMM_NLPTM;
  uint ldlB = SGEMM_NLPTN;
  for (uint ugk = 0; ugk < ngk; ++ugk) {
    for (uint u = 0; u < SGEMM_NLPTK; u += ulA3)
      lA[(u + ulA2)*(SGEMM_NLPTM/2U) + ulA1] = gA[(u + ulA2)*(NM/2U) + ulA1];
    for (uint u = 0; u < SGEMM_NLPTK; u += ulB3)
      lB[(u + ulB2)*(SGEMM_NLPTN/2U) + ulB1] = gB[(u + ulB2)*(NN/2U) + ulB1];

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NPK; ++ulpk)
      for (uint upm = 0; upm < SGEMM_NPM; ++upm)
        for (uint upn = 0; upn < SGEMM_NPN; ++upn) {
#if WMMA_ACCUMU16
          asm("{ .reg .b32 a<8>;\n"
              "  .reg .b32 b<8>;\n"
              "  wmma.load.a.sync.aligned.col" SGEMM_TDM ".shared.f16\n"
              "    {a0, a1, a2, a3, a4, a5, a6, a7}, [%4], %5;\n"
              "  wmma.load.b.sync.aligned.row" SGEMM_TDM ".shared.f16\n"
              "    {b0, b1, b2, b3, b4, b5, b6, b7}, [%6], %7;\n"
              "  wmma.mma.sync.aligned.col.row" SGEMM_TDM ".f16.f16\n"
              "    {%0, %1, %2, %3},\n"
              "    {a0, a1, a2, a3, a4, a5, a6, a7},\n"
              "    {b0, b1, b2, b3, b4, b5, b6, b7},\n"
              "    {%0, %1, %2, %3}; }"
              : "+r"(pD[upm][upn][0]), "+r"(pD[upm][upn][1]),
                "+r"(pD[upm][upn][2]), "+r"(pD[upm][upn][3])
              : "l"(lA + ulpk*SGEMM_NTK*(SGEMM_NLPTM/2U)
                       + ulm*(SGEMM_NPTM/2U) + upm*(SGEMM_NTM/2U)), "r"(ldlA),
                "l"(lB + ulpk*SGEMM_NTK*(SGEMM_NLPTN/2U)
                       + uln*(SGEMM_NPTN/2U) + upn*(SGEMM_NTN/2U)), "r"(ldlB)
              : "memory");
#else
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
              : "l"(lA + ulpk*SGEMM_NTK*(SGEMM_NLPTM/2U)
                       + ulm*(SGEMM_NPTM/2U) + upm*(SGEMM_NTM/2U)), "r"(ldlA),
                "l"(lB + ulpk*SGEMM_NTK*(SGEMM_NLPTN/2U)
                       + uln*(SGEMM_NPTN/2U) + upn*(SGEMM_NTN/2U)), "r"(ldlB)
              : "memory");
#endif
        }
    barrier(CLK_LOCAL_MEM_FENCE);
    gA += SGEMM_NLPTK*(NM/2U);
    gB += SGEMM_NLPTK*(NN/2U); }

  uint ldC = NN;
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
#if WMMA_ACCUMU16
      asm("{  wmma.store.d.sync.aligned.row" SGEMM_TDM ".global.f16\n"
          "     [%4], {%0, %1, %2, %3}, %5; }"
          :: "r"(pD[upm][upn][0]), "r"(pD[upm][upn][1]),
             "r"(pD[upm][upn][2]), "r"(pD[upm][upn][3]),
             "l"(gC + (ulm*SGEMM_NPTM + upm*SGEMM_NTM)*NN
                       + uln*SGEMM_NPTN + upn*SGEMM_NTN), "r"(ldC)
          : "memory");
#else
      asm("{  wmma.store.d.sync.aligned.row" SGEMM_TDM ".global.f32\n"
          "     [%8], {%0, %1, %2, %3, %4, %5, %6, %7}, %9; }"
          :: "r"(pD[upm][upn][0]), "r"(pD[upm][upn][1]),
             "r"(pD[upm][upn][2]), "r"(pD[upm][upn][3]),
             "r"(pD[upm][upn][4]), "r"(pD[upm][upn][5]),
             "r"(pD[upm][upn][6]), "r"(pD[upm][upn][7]),
             "l"(gC + ulm*SGEMM_NPTM*NN + upm*SGEMM_NTM*NN
                    + uln*SGEMM_NPTN + upn*SGEMM_NTN), "r"(ldC)
          : "memory");
#endif
}
)";

const string code_compute_matM = R"(
__kernel __attribute__((reqd_work_group_size(SGEMM_NL, SGEMM_NL*SGEMM_NLFM,
                                             1)))
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
                  [SGEMM_NL*SGEMM_NLFM*SGEMM_NPM]
                  __attribute__((aligned(SIZE_ALIGN)));
  __local float lB[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NPN]
                  __attribute__((aligned(SIZE_ALIGN)));
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

  __local float lC[SGEMM_NL*SGEMM_NLFM*SGEMM_NPM][SGEMM_NL*SGEMM_NPN]
                __attribute__((aligned(SIZE_ALIGN)));
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      lC[ulm*SGEMM_NPM + upm][uln*SGEMM_NPN + upn] = pC[upm][upn];

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPM; u += ulB3)
    gC[(u + ulB2)*NN + ulB1] = lC[u + ulB2][ulB1]; }
)";

const string code_sgemm = R"(
__kernel __attribute__((reqd_work_group_size(SGEMM_NL, SGEMM_NL*SGEMM_NLFM,
                                             1)))
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
                  [SGEMM_NL*SGEMM_NLFM*SGEMM_NPM]
                  __attribute__((aligned(SIZE_ALIGN)));
  __local float lB[SGEMM_NL*SGEMM_NLFM*SGEMM_NPK]
                  [SGEMM_NL*SGEMM_NPN]
                  __attribute__((aligned(SIZE_ALIGN)));
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
__kernel __attribute__((reqd_work_group_size(1, SGEMM_NLFM, 1)))
void sgemm(__global const float *gA, __global const float *gB,
           __global float *gC) {
  uint ugm = get_group_id(1);
  uint ulm = get_local_id(1);
  uint ngk = NK / (SGEMM_NLFM * SGEMM_NPK);
  gA += ugm*SGEMM_NLFM;
  gC += ugm*SGEMM_NLFM;

  __local float lB[SGEMM_NLFM*SGEMM_NPK] __attribute__((aligned(SIZE_ALIGN)));
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
)";

const string code_resize_bias_ReLU = R"(
__kernel __attribute__((reqd_work_group_size(NN, NL, 1)))
void resize_bias_ReLU(__global const float *bias, __global const float *fin,
                      __global float *fout) {
  uint nn = get_global_id(0);
  uint nm = get_global_id(1);
  fout[nm*LD_OUT + nn] = fmax(0.0f, fin[nm*LD_IN + nn] + bias[nm]); }
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
    str += "#define WMMA_ACCUMU16 " + to_string(WMMA_ACCUMU16)  + "\n";
    str += "#define NM            " + to_string(nm)             + "U\n";
    str += "#define NN            " + to_string(nn)             + "U\n";
    str += "#define NK            " + to_string(nk)             + "U\n";
    str += "#define OFFA          " + to_string(nm*nk)          + "U\n";
    str += "#define OFFB          " + to_string(nk*nn)          + "U\n";
    str += "#define OFFC          " + to_string(nm*nn)          + "U\n";
    str += "#define WRAP_SIZE     " + to_string(size_wrap_wmma) + "U\n";
    str += "#define SGEMM_NL      " + to_string(param.nl)       + "U\n";
    str += "#define SGEMM_NPM     " + to_string(param.npm)      + "U\n";
    str += "#define SGEMM_NPN     " + to_string(param.npn)      + "U\n";
    str += "#define SGEMM_NPK     " + to_string(param.npk)      + "U\n";
    str += "#define SGEMM_NTM     " + to_string(param.ntm)      + "U\n";
    str += "#define SGEMM_NTN     " + to_string(param.ntn)      + "U\n";
    str += "#define SGEMM_NTK     " + to_string(ntk)            + "U\n";
    str += "#define SGEMM_NPTM    (SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NPTN    (SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_NPTK    (SGEMM_NPK * SGEMM_NTK)\n";
    str += "#define SGEMM_NLPTM   (SGEMM_NL * SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NLPTN   (SGEMM_NL * SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_NLPTK   (SGEMM_NL * SGEMM_NPK * SGEMM_NTK)\n";
    str += "#define SGEMM_TDM     " + str_tdm + "\n";
    str += code_compute_matM_wmma;
    return make_tuple(str, nm, nn, nk,
		      (nn / (param.npn*param.ntn)) * size_wrap_wmma,
		      nm / (param.npm*param.ntm),
		      param.nl * size_wrap_wmma, param.nl); }
  else {
    uint nm = ceil_multi(nm0, param.nl * param.nlfm * param.npm);
    uint nn = ceil_multi(nn0, param.nl * param.npn);
    uint nk = ceil_multi(nk0, param.nl * param.nlfm * param.npk);
    string str;
    str += "#define SIZE_ALIGN " + to_string(size_align_local) + "U\n";
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
    str += code_compute_matM;
    return make_tuple(str, nm, nn, nk,
		      nn / param.npn, nm / param.npm,
		      param.nl, param.nl * param.nlfm); } }

const string gen_code_sgemm(uint nm, uint nn, uint nk, const SgemmParam &param)
  noexcept {
  assert(0 < nm && 0 < nn && 0 < nk && param.ok());
  string str;
  if (param.npm == 1U && nn == 1U) {
    str += "#define SIZE_ALIGN " + to_string(size_align_local) + "U\n";
    str += "#define NM         " + to_string(nm)   + "U\n";
    str += "#define NK         " + to_string(nk)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += code_sgemm_n1; }
  else {
    str += "#define SIZE_ALIGN " + to_string(size_align_local) + "U\n";
    str += "#define NM         " + to_string(nm)         + "U\n";
    str += "#define NN         " + to_string(nn)         + "U\n";
    str += "#define NK         " + to_string(nk)         + "U\n";
    str += "#define SGEMM_NL   " + to_string(param.nl)   + "U\n";
    str += "#define SGEMM_NLFM " + to_string(param.nlfm) + "U\n";
    str += "#define SGEMM_NPM  " + to_string(param.npm)  + "U\n";
    str += "#define SGEMM_NPN  " + to_string(param.npn)  + "U\n";
    str += "#define SGEMM_NPK  " + to_string(param.npk)  + "U\n";
    str += code_sgemm; }
  return str; }

static bool test_wmma(const OCL::Device &dev) noexcept {
  assert(dev.ok());
  try {
    OCL::Queue queue = dev.gen_queue();
    OCL::Program pg  = queue.gen_program(R"(
__kernel void test_wmma() {
  uint laneid = get_global_id(0);
  __local ushort lA[256] __attribute__((aligned(32)));
  __local ushort lB[256] __attribute__((aligned(32)));
  __local float  lC[256] __attribute__((aligned(32)));
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
    const size_t size_g[3] = { size_wrap_wmma, 1U, 1U };
    const size_t size_l[3] = { size_wrap_wmma, 1U, 1U };
    queue.push_ndrange_kernel(ker, 3, size_g, size_l);
    queue.finish(); }
  catch (...) { return false; }
  return true; }

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
  tie(str, nm, nn, nk, size_g[0], size_g[1], size_l[0], size_l[1])
    = gen_code_compute_matM(nm0, nn0, nk0, param);
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

static SgemmParam tune_compute_matM(bool use_wmma, const OCL::Device &dev,
				    uint nbatch, uint nm0, uint nn0, uint nk0)
  noexcept {
  assert(dev.ok() && 0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax, nlstart, nlfmmax;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);
  if (!use_wmma) {
    wgmax   = min(static_cast<uint>(dev.gen_max_work_group_size()), 64U*64U);
    wgmin   = 16U;
    nlstart = min(nmax, 4U);
    mmax    = max(mmax, 64U / nmax);
    kmax    = max(kmax, 64U / nmax);
    if      (nmax <= 1U) nlfmmax = 128U;
    else if (nmax <= 2U) nlfmmax =  32U;
    else if (nmax <= 4U) nlfmmax =   8U;
    else                 nlfmmax =   2U;
    for (uint nl = nlstart; nl <= 16U; nl *= 2U)
      for (uint nlfm = 1U; nlfm <= nlfmmax; nlfm *= 2U) {
	if (nl*nl*nlfm < wgmin || wgmax < nl*nl*nlfm) continue;
	for (uint npm = 1U; npm <= 16U; npm *= 2U)
	  for (uint npn = 1U; npn <= 16U; npn *= 2U) {
	    if (256U < npm*npn) continue;
	    if (nl < npm) continue;
	    if (nl*nlfm < npn) continue;
	    for (uint npk = 1U; npk <= 4U; npk *= 2U) {
	      if (mmax < nl * nlfm * npm) continue;
	      if (nmax < nl * npn) continue;
	      if (kmax < nl * nlfm * npk) continue;
	      params.emplace_back(false, nl, nlfm, npm, npn, npk); } } } }
  else {
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
	    for (auto t : { /*make_tuple(32U, 8U),*/
		make_tuple(16U, 16U) /*make_tuple(8U, 32U)*/ }) {
	      tie(ntm, ntn) = t;
	      if (mmax < nl*npm*ntm) continue;
	      if (nmax < nl*npn*ntn) continue;
	      if (kmax < nl*npk*ntk) continue;
	      if (2U*nl*size_wrap_wmma < npm*ntm) continue;
	      if (2U*nl*size_wrap_wmma < npn*ntn) continue;
	      if (1U*nl*size_wrap_wmma < npn*ntn) continue;
	      params.emplace_back(true, true, nl, npm, npn, npk, ntm, ntn); } }

  OCL::Queue qtmp = dev.gen_queue();
  double time_best = DBL_MAX;
  SgemmParam param_best;
  while (! params.empty()) {
    SgemmParam param = params.front();
    params.pop_front();
    double elapsed = DBL_MAX;
    bool flag_error = false;
    try {
      elapsed = measure_compute_matM(qtmp, param, nbatch, nm0, nn0, nk0, 1U); }
    catch (...) { elapsed = DBL_MAX; flag_error = true; }

    if (flag_error) {
      for (auto it = params.begin(); it != params.end(); ) {
	if (param <= *it) it = params.erase(it);
	else ++it; } }
    else {
      param.time = elapsed;
      params_candi.push_back(param);
      if (elapsed < time_best) { time_best = elapsed; param_best = param; } } }
  if (time_best == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  double time_max = min(time_best * 2.0 + 20.0, time_best + 5000.0);
  for (auto it = params_candi.begin(); it != params_candi.end(); ) {
    if (time_max <= it->time) it = params_candi.erase(it);
    else ++it; }

  uint sample_size = static_cast<uint>(100000.0 / time_best);
  sample_size = min(sample_size, tune_sample_size);
  if (1U < sample_size) {
    time_best = DBL_MAX;
    while (! params_candi.empty()) {
      SgemmParam param = params_candi.front();
      params_candi.pop_front();
      double elapsed = measure_compute_matM(qtmp, param, nbatch, nm0, nn0, nk0,
					    sample_size);
      if (time_best <= elapsed) continue;
      time_best  = elapsed;
      param_best = param; } }
  if (time_best == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  return param_best; }

string SgemmParam::gen_info() const noexcept {
  string s;
  if (do_wmma) {
    s += string("NL:")   + to_string(nl)  + string(" ");
    s += string("NPM:")  + to_string(npm) + string(" ");
    s += string("NPN:")  + to_string(npn) + string(" ");
    s += string("NPK:")  + to_string(npk) + string(" ");
    s += string("NTM:")  + to_string(ntm) + string(" ");
    s += string("NTN:")  + to_string(ntn) + string(" (");
    s += to_string(static_cast<uint>(time)) + string("us)"); }
  else {
    s += string("NL:")   + to_string(nl)   + string(" ");
    s += string("NLFM:") + to_string(nlfm) + string(" ");
    s += string("NPM:")  + to_string(npm)  + string(" ");
    s += string("NPN:")  + to_string(npn)  + string(" ");
    s += string("NPK:")  + to_string(npk)  + string(" (");
    s += to_string(static_cast<uint>(time)) + string("us)"); }
  return s; }

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
  unique_ptr<uchar []> data(new uchar [size]);
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
  void *ptr = queue.push_map_w(mem_b, size);
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
/*
static double measure_send_zcopy(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar []> data(new uchar [size]);
  OCL::Event event;
  OCL::Memory mem = queue.gen_mem_map_hw_dr(size);
  sleep_for(microseconds(tune_sleep));
  void *ptr = queue.push_map_w(mem, size, event);
  event.wait();
  copy_n(data.get(), size, static_cast<uchar *>(ptr));
  queue.push_unmap(mem, ptr);
  queue.finish();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    ptr = queue.push_map_w(mem, size, event);
    event.wait();
    copy_n(data.get(), size, static_cast<uchar *>(ptr));
    queue.push_unmap(mem, ptr);
    queue.finish();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }
*/
constexpr char ManageSend::global_memory[];
constexpr char ManageSend::pinned_memory[];
constexpr char ManageSend::zero_copy[];
void ManageSend::start(const OCL::Device &dev, const OCL::Queue &queue,
		       uint maxsize_batch) noexcept {
  assert(dev.ok() && queue.ok() && 0 < maxsize_batch);
  _nbatch = maxsize_batch;
  size_t size_max       = (_nbatch * NNAux::nch_input * NNAux::size_plane 
			   * sizeof(float));
  double elapsed_global = DBL_MAX;
  double elapsed_pinned = DBL_MAX;
  //double elapsed_zcopy  = DBL_MAX;
  { size_t ave = send_size_ave * _nbatch;
    OCL::Queue qtmp = dev.gen_queue();
    try { elapsed_global = measure_send_global(qtmp, ave); } catch (...) {}
    try { elapsed_pinned = measure_send_pinned(qtmp, ave); } catch (...) {}
    /*try { elapsed_zcopy  = measure_send_zcopy (qtmp, ave); } catch (...) {}*/
  }

  /*if (elapsed_zcopy < elapsed_global && elapsed_zcopy < elapsed_pinned) {
    _time   = elapsed_zcopy;
    _method = zero_copy; }
    else*/
  if (elapsed_pinned < elapsed_global) {
    _time   = elapsed_pinned;
    _method = pinned_memory; }
  else {
    _time   = elapsed_global;
    _method = global_memory; }
  if (_time == DBL_MAX) die(ERR_INT("ManageSend() failed."));

  //_time   = elapsed_global;
  //_method = global_memory;

  //_time   = elapsed_pinned;
  //_method = pinned_memory;

  //_time   = elapsed_zcopy;
  //_method = zero_copy;

  if (_method == pinned_memory) {
    _mem_work = queue.gen_mem_hw_dr(size_max);
    for (uint u = 0; u < NNAux::nslot; ++u) {
      _mem_pin[u] = queue.gen_mem_map_hw_dr(size_max);
      _ptr_map[u] = queue.push_map_w(_mem_pin[u], size_max); } }
  else if (_method == zero_copy)
    _mem_work = queue.gen_mem_map_hw_dr(size_max);
  else _mem_work = queue.gen_mem_hw_dr(size_max); }

void ManageSend::end(const OCL::Queue &queue) noexcept {
  assert(_method && queue.ok());
  if (_method == pinned_memory) {
    for (uint u = 0; u < NNAux::nslot; ++u)
      queue.push_unmap(_mem_pin[u], _ptr_map[u]);
    queue.finish(); } }

void ManageSend::push(const OCL::Queue &queue, const void *p, size_t size,
		      uint uslot) noexcept {
  assert(_method && queue.ok() && p && 0 < size);

  if (_method == global_memory) queue.push_write(_mem_work, size, p);
  else if (_method == pinned_memory) {
    memcpy(_ptr_map[uslot], p, size);
    queue.push_write(_mem_work, size, _ptr_map[uslot]); }
  else {
    _ptr_map[uslot] = queue.push_map_w(_mem_work, size, _event);
    _event.wait();
    memcpy(_ptr_map[uslot], p, size);
    queue.push_unmap(_mem_work, _ptr_map[uslot]); } }

string ManageSend::gen_info() const noexcept {
  string s(_method);
  s += " (" + to_string(static_cast<uint>(_time)) + "us)";
  return s; }

void ManageDecode::start(const OCL::Queue &queue, const OCL::Memory &mem_in,
			 const OCL::Memory &mem_out, uint index_block,
			 uint maxsize_batch) noexcept {
  assert(queue.ok() && mem_in.ok() && mem_out.ok());
  _nbatch = maxsize_batch;
  _nm     = _nbatch * NNAux::nch_input * NNAux::size_plane;

  _zero_size_l[0] = _zero_size_g[0] = 128U;
  _zero_size_g[1] = NNAux::nch_input * _nbatch;
  _zero_size_l[1] = _zero_size_l[2] = _zero_size_g[2] = 1U;

  _one_size_l[0] = 32U;
  _one_size_l[1] = _one_size_g[1] = _one_size_l[2] = _one_size_g[2] = 1U;

  _fill_size_l[0] = _fill_size_g[0] = 81U;
  _fill_size_l[1] = _fill_size_l[2] = _fill_size_g[2] = 1U;
  _fill_size_g[1] = send_nch_fill * _nbatch;

  string code = ("#define INDEX_BLOCK " + to_string(index_block) + "U\n"
		 "#define NB          " + to_string(_nbatch) + "U\n"
		 + code_decode);
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker_zero_clear = pg.gen_kernel("zero_clear");
  _ker_zero_clear.set_arg(0, mem_out);

  _ker_plane_fill = pg.gen_kernel("plane_fill");
  _ker_plane_fill.set_arg(0, mem_in);
  _ker_plane_fill.set_arg(1, mem_in);
  _ker_plane_fill.set_arg(2, mem_out);

  _ker_set_one = pg.gen_kernel("set_one");
  _ker_set_one.set_arg(0, mem_in);
  _ker_set_one.set_arg(1, mem_out); }

void ManageDecode::push(const OCL::Queue &queue, uint n_one) noexcept {
  assert(queue.ok());
  queue.push_ndrange_kernel(_ker_zero_clear, 3, _zero_size_g, _zero_size_l);
  queue.push_ndrange_kernel(_ker_plane_fill, 3, _fill_size_g, _fill_size_l);
  _one_size_g[0] = n_one;
  queue.push_ndrange_kernel(_ker_set_one, 3, _one_size_g, _one_size_l); }

static double measure_recv_global(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar []> data(new uchar [size]);
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
  OCL::Event event;
  void *ptr = queue.push_map_r(mem_pin, size);
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

/*
static double measure_recv_zcopy(const OCL::Queue &queue, size_t size) {
  assert(queue.ok() && 0 < size);
  unique_ptr<uchar []> data(new uchar [size]);
  OCL::Memory mem = queue.gen_mem_map_hr_dw(size);
  OCL::Event event;
  void *ptr = queue.push_map_r(mem, size, event);
  event.wait();
  copy_n(static_cast<uchar *>(ptr), size, data.get());
  queue.push_unmap(mem, ptr, event);
  event.wait();
  uint count = 0;
  for (uint usample = 0; usample < tune_sample_size; ++usample) {
    sleep_for(microseconds(tune_sleep));
    steady_clock::time_point start = steady_clock::now();
    ptr = queue.push_map_r(mem, size, event);
    event.wait();
    copy_n(static_cast<uchar *>(ptr), size, data.get());
    queue.push_unmap(mem, ptr, event);
    event.wait();
    steady_clock::time_point end = steady_clock::now();
    count += static_cast<uint>(duration_cast<microseconds>(end
							   - start).count()); }
  return (static_cast<double>(count)
	  / static_cast<double>(tune_sample_size)); }
*/

constexpr char ManageRecv::global_memory[];
constexpr char ManageRecv::pinned_memory[];
constexpr char ManageRecv::zero_copy[];
void ManageRecv::start(const OCL::Device &dev, const OCL::Queue &queue,
		       size_t size_max, uint nbatch) noexcept {
  assert(dev.ok() && queue.ok() && 0 < size_max);

  double elapsed_global = DBL_MAX;
  double elapsed_pinned = DBL_MAX;
  //double elapsed_zcopy  = DBL_MAX;
  { size_t ave = read_size_ave * nbatch;
    OCL::Queue qtmp = dev.gen_queue();
    try { elapsed_global = measure_recv_global(qtmp, ave); } catch (...) {}
    try { elapsed_pinned = measure_recv_pinned(qtmp, ave); } catch (...) {}
    //try { elapsed_zcopy  = measure_recv_zcopy (qtmp, ave); } catch (...) {}
  }

  /*if (elapsed_zcopy < elapsed_global && elapsed_zcopy < elapsed_pinned) {
    _time   = elapsed_zcopy;
    _method = zero_copy; }
    else*/ if (elapsed_pinned < elapsed_global) {
    _time   = elapsed_pinned;
    _method = pinned_memory; }
  else {
    _time   = elapsed_global;
    _method = global_memory; }
  if (_time == DBL_MAX) die(ERR_INT("ManageRecv() failed."));

  //_time   = elapsed_global;
  //_method = global_memory;

  //_time   = elapsed_pinned;
  //_method = pinned_memory;

  //_time   = elapsed_zcopy;
  //_method = zero_copy;

  if (_method == pinned_memory) {
    _mem = queue.gen_mem_hr_dw(size_max * nbatch);
    for (uint u = 0; u < NNAux::nslot; ++u) {
      _mem_pin[u] = queue.gen_mem_map_hr_dw(size_max * nbatch);
      _ptr_map[u] = queue.push_map_r(_mem_pin[u], size_max * nbatch); } }
  else if (_method == zero_copy)
    _mem = queue.gen_mem_map_hr_dw(size_max * nbatch);
  else _mem = queue.gen_mem_hr_dw(size_max * nbatch); }

void ManageRecv::end(const OCL::Queue &queue) noexcept {
  assert(_method && queue.ok());
  if (_method != pinned_memory) return;
  for (uint u = 0; u < NNAux::nslot; ++u)
    queue.push_unmap(_mem_pin[u], _ptr_map[u]);
  queue.finish(); }

void ManageRecv::push(const OCL::Queue &queue, void *p, size_t size,
		      uint uslot) noexcept {
  assert(_method && queue.ok() && p && 0 < size && uslot < NNAux::nslot);
  if (_method == global_memory) queue.push_read(_mem, size, p, _event[uslot]);
  else if (_method == pinned_memory) {
    queue.push_read(_mem, size, _ptr_map[uslot], _event[uslot]);
    _ptr_out[uslot] = p;
    _size[uslot]    = size; }
  else {
    _ptr_map[uslot] = queue.push_map_r(_mem, size, _event[uslot]);
    _ptr_out[uslot] = p;
    _size[uslot]    = size; } }

void ManageRecv::wait(const OCL::Queue &queue, uint uslot)
  noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  if (_method == global_memory) _event[uslot].wait();
  else if (_method == pinned_memory) {
    _event[uslot].wait();
    memcpy(_ptr_out[uslot], _ptr_map[uslot], _size[uslot]); }
  else {
    _event[uslot].wait();
    memcpy(_ptr_out[uslot], _ptr_map[uslot], _size[uslot]);
    queue.push_unmap(_mem, _ptr_map[uslot]); } }

string ManageRecv::gen_info() const noexcept {
  string s(_method);
  s += " (" + to_string(static_cast<uint>(_time)) + "us)";
  return s; }

void ManageComputeMatM::start(const OCL::Queue &queue, uint nbatch, uint nm0,
			      uint nn0, uint nk0, const SgemmParam &param)
  noexcept {
  assert(queue.ok() && 0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0
	 && param.ok());
  string str;
  tie(str, _nm, _nn, _nk, _size_g[0], _size_g[1], _size_l[0], _size_l[1])
    = gen_code_compute_matM(nm0, nn0, nk0, param);
  OCL::Program pg = queue.gen_program(str.c_str());
  _ker = pg.gen_kernel("compute_matM");
  _nbatch    = nbatch;
  _size_g[2] = nbatch;
  _size_l[2] = 1U; }

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
  assert(queue.ok() && param.ok());
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
			bool, bool, bool do_transa, bool do_transb,
			bool do_transc, uint nm0, uint nn0, uint nk0,
			uint offa0, uint lda0, uint offb0, uint ldb0,
			uint offc0, uint ldc0) noexcept {
  assert(dev.ok() && queue.ok() && 0 < nm0 && 0 < nn0 && 0 < nk0);
  assert(0 < lda0 && 0 < ldb0 && 0 < ldc0);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax, nlstart, nlfmmax;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);

  _do_transa = do_transa;
  _do_transb = do_transb;
  _do_transc = do_transc;
  wgmax   = min(static_cast<uint>(dev.gen_max_work_group_size()), 4096U);
  wgmin   = max(mmax*nmax, 1U);
  wgmin   = min(wgmin, 16U);
  nlstart = min(mmax, nmax);
  nlstart = min(nlstart, 4U);
  mmax    = max(mmax, 64U / nmax);
  if      (nmax <= 1U) nlfmmax = 128U;
  else if (nmax <= 2U) nlfmmax =  32U;
  else if (nmax <= 4U) nlfmmax =   8U;
  else                 nlfmmax =   2U;
  for (uint nl = nlstart; nl <= 16U; nl *= 2U)
    for (uint nlfm = 1U; nlfm <= nlfmmax; nlfm *= 2U) {
      if (nl*nl*nlfm < wgmin || wgmax < nl*nl*nlfm) continue;
      for (uint npm = 1U; npm <= 16U; npm *= 2U)
	for (uint npn = 1U; npn <= 16U; npn *= 2U) {
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
  _nm = ceil_multi(nm0, _param.nl * _param.nlfm * _param.npm);
  _nn = ceil_multi(nn0, _param.nl * _param.npn);
  _nk = ceil_multi(nk0, _param.nl * _param.nlfm * _param.npk);
  size_g[0] = _nn / _param.npn;
  size_g[1] = _nm / _param.npm;
  size_g[2] = 1U;
  size_l[0] = _param.nl;
  size_l[1] = _param.nl * _param.nlfm;
  size_l[2] = 1U;

  OCL::Program pg = queue.gen_program(code_zero_clear);
  OCL::Kernel ker_zero_clear = pg.gen_kernel("zero_clear");

  string str = ("#define NM0   " + to_string(_nm0)  + "\n"
		"#define NN0   " + to_string(_nn0)  + "\n"
		"#define NK0   " + to_string(_nk0)  + "\n"
		"#define OFFA0 " + to_string(offa0) + "\n"
		"#define OFFB0 " + to_string(offb0) + "\n"
		"#define OFFC0 " + to_string(offc0) + "\n"
		"#define LDA0  " + to_string(lda0)  + "\n"
		"#define LDB0  " + to_string(ldb0)  + "\n"
		"#define LDC0  " + to_string(ldc0)  + "\n"
		"#define NM    " + to_string(_nm)   + "\n"
		"#define NN    " + to_string(_nn)   + "\n"
		"#define NK    " + to_string(_nk)   + "\n" + code_sgemm_aux);
  pg = queue.gen_program(str.c_str());
  _done_load_a = _done_load_b = false;

  if (do_transa) {
    mem_a = queue.gen_mem_drw(_nm * _nk * sizeof(float));
    ker_zero_clear.set_arg(0, mem_a);
    queue.push_kernel(ker_zero_clear, _nm * _nk);
    _ker_a = pg.gen_kernel("mat0_to_matA");
    _ker_a.set_arg(1, mem_a); }

  if (do_transb) {
    mem_b = queue.gen_mem_drw(_nk * _nn * sizeof(float));
    ker_zero_clear.set_arg(0, mem_b);
    queue.push_kernel(ker_zero_clear, _nk * _nn);
    _ker_b = pg.gen_kernel("mat0_to_matB");
    _ker_b.set_arg(1, mem_b); }

  if (do_transc) {
    mem_c = queue.gen_mem_drw(_nm * _nn * sizeof(float));
    ker_zero_clear.set_arg(0, mem_c);
    queue.push_kernel(ker_zero_clear, _nm * _nn);
    _ker_c = pg.gen_kernel("matC_to_mat0");
    _ker_c.set_arg(0, mem_c); }
  queue.finish();

  str = gen_code_sgemm(_nm, _nn, _nk, _param);
  pg = queue.gen_program(str.c_str());
  _ker_sgemm = pg.gen_kernel("sgemm");
  if (do_transa) _ker_sgemm.set_arg(0, mem_a);
  if (do_transb) _ker_sgemm.set_arg(1, mem_b);
  if (do_transc) _ker_sgemm.set_arg(2, mem_c);
}

void ManageSgemm::register_a(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok() && !_do_transa);
  _ker_sgemm.set_arg(0, mem); }

void ManageSgemm::register_b(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok() && !_do_transb);
  _ker_sgemm.set_arg(1, mem); }

void ManageSgemm::register_c(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok() && !_do_transc);
  _ker_sgemm.set_arg(2, mem); }

void ManageSgemm::register_a0(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok()); _ker_a.set_arg(0, mem); }

void ManageSgemm::register_b0(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok()); _ker_b.set_arg(0, mem); }

void ManageSgemm::register_c0(const OCL::Memory &mem) const noexcept {
  assert(_ker_sgemm.ok() && mem.ok()); _ker_c.set_arg(1, mem); }

void ManageSgemm::push_load_a(const OCL::Queue &queue) noexcept {
  assert(_ker_sgemm.ok() && queue.ok());
  queue.push_kernel(_ker_a, _nk0 * _nm0);
  _done_load_a = true; }

void ManageSgemm::push_load_b(const OCL::Queue &queue) noexcept {
  assert(_ker_sgemm.ok() && queue.ok());
  queue.push_kernel(_ker_b, _nk0 * _nn0);
  _done_load_b = true; }

void ManageSgemm::push(const OCL::Queue &queue) const noexcept {
  assert(queue.ok());
  if (!_done_load_a && _do_transa) queue.push_kernel(_ker_a, _nk0 * _nm0);
  if (!_done_load_b && _do_transb) queue.push_kernel(_ker_b, _nk0*_nn0);
  queue.push_ndrange_kernel(_ker_sgemm, 3U, size_g, size_l);
  if (_do_transc) queue.push_kernel(_ker_c, _nm0*_nn0); }

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

void ManageComputeMatV::start(bool store_half, const OCL::Queue &queue,
			      uint nch, uint nb, uint nn, uint nk,
			      const OCL::Memory &mem_matV) noexcept {
  string code;

  if (store_half) code += "#define STORE_HALF\n";
  code += ("#define NB " + to_string(nb) + "U\n"
	   "#define NK " + to_string(nk) + "U\n"
	   "#define NN " + to_string(nn) + "U\n"
	   + code_common + code_compute_matV_child + code_compute_matV);
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("compute_matV");
  _ker.set_arg(1, mem_matV);
  _size_l[0] = ntile;
  _size_l[1] = nb;
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch;
  _nm = ntile * nch * nb; }

void ManageComputeMatV::push(const OCL::Queue &queue,
			     const OCL::Memory &mem_in) const noexcept {
  assert(0 < _nm && queue.ok() && mem_in.ok());
  _ker.set_arg(0, mem_in);
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

void ManageComputeMatA::start(bool load_half, const OCL::Queue &queue,
			      bool flag_join, uint nch, uint nb, uint nm,
			      uint nn, uint nn_out,
			      const OCL::Memory &mem_matM,
			      const OCL::Memory &mem_bypass,
			      const OCL::Memory &mem_output) noexcept {
  assert(queue.ok() && mem_matM.ok() && mem_bypass.ok() && mem_output.ok());
  string code;
#if WMMA_ACCUMU16 == 1
  if (load_half) code += "#define LOAD_HALF\n";
#else
  (void)load_half;
#endif
  if (flag_join) code += "#define DO_JOIN\n";
  code += ("#define NB     " + to_string(nb) + "\n"
	   "#define NM     " + to_string(nm) + "\n"
	   "#define NN     " + to_string(nn) + "\n"
	   "#define NN_OUT " + to_string(nn_out) + "\n"
	   + code_common + code_compute_matA_child + code_compute_matA);

  OCL::Program pg;
  try { pg = queue.gen_program(code.c_str()); }
  catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    std::terminate(); }
  _ker = pg.gen_kernel("compute_matA_BNReLU");
  _ker.set_arg(0, mem_matM);
  _ker.set_arg(3, mem_bypass);
  _ker.set_arg(4, mem_output);
  _size_l[0] = ntile;
  _size_l[1] = nb;
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch;
  _nm = ntile * nch * nb; }

void ManageComputeMatA::push(const OCL::Queue &queue, const OCL::Memory &mean,
			     const OCL::Memory &sd_inv) const noexcept {
  assert(0 < _nm && queue.ok() && mean.ok() && sd_inv.ok());
  _ker.set_arg(1, mean);
  _ker.set_arg(2, sd_inv);
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

void ManageComputeMatAV::start(bool do_half, bool do_join, bool do_fork,
			       const OCL::Queue &queue, uint nch,
			       uint nb, uint nm, uint nn, uint nk,
			       const OCL::Memory &mem_matM,
			       const OCL::Memory &mem_matV,
			       const OCL::Memory &mem_bypass) noexcept {
  assert(queue.ok() && mem_matM.ok() && mem_matV.ok());
  string code;
#if WMMA_ACCUMU16 == 1
  if (do_half) code += "#define LOAD_HALF\n";
#endif
  if (do_half) code += "#define STORE_HALF\n";
  if (do_join) code += "#define DO_JOIN\n";
  if (do_fork) code += "#define DO_FORK\n";
  code += ("#define NB " + to_string(nb) + "U\n"
	   "#define NM " + to_string(nm) + "U\n"
	   "#define NN " + to_string(nn) + "U\n"
	   "#define NK " + to_string(nk) + "U\n"
	   + code_common + code_compute_matV_child
	   + code_compute_matA_child + code_compute_matAV);

  OCL::Program pg;
  try { pg = queue.gen_program(code.c_str()); }
  catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    std::terminate(); }
  _ker = pg.gen_kernel("compute_matAV");
  _ker.set_arg(0, mem_matM);
  _ker.set_arg(3, mem_matV);
  if (mem_bypass.ok()) _ker.set_arg(4, mem_bypass);
  _size_l[0] = ntile;
  _size_l[1] = nb;
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch; }

void ManageComputeMatAV::push(const OCL::Queue &queue, const OCL::Memory &mean,
			      const OCL::Memory &sd_inv) const noexcept {
  assert(queue.ok() && mean.ok() && sd_inv.ok());
  _ker.set_arg(1, mean);
  _ker.set_arg(2, sd_inv);
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

void ManageComputeBNReLU::start(const OCL::Queue &queue, uint nch, uint nb,
				uint nn_in,
				const OCL::Memory &mean,
				const OCL::Memory &sd_inv,
				const OCL::Memory &mem_in,
				const OCL::Memory &mem_out) noexcept {
  string code = ("#define NB    " + to_string(nb) + "U\n"
		 "#define NN_IN " + to_string(nn_in) + "U\n"
		 + code_BNReLU);
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("compute_BNReLU");
  _ker.set_arg(0, mean);
  _ker.set_arg(1, sd_inv);
  _ker.set_arg(2, mem_in);
  _ker.set_arg(3, mem_out);
  _size_l[0] = NNAux::size_plane;
  _size_l[1] = 1U;
  _size_l[2] = 1U;
  _size_g[0] = NNAux::size_plane;
  _size_g[1] = nb;
  _size_g[2] = nch; }

void ManageComputeBNReLU::push(const OCL::Queue &queue) const noexcept {
  assert(_ker.ok() && queue.ok());
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

//class ManageTransformBNReLU {
//  using uint = unsigned int;
//  size_t _size_g[3], _size_l[3];
//  OCL::Kernel _ker;
//public:
void ManageResizeBiasReLU::start(const OCL::Queue &queue, uint nm, uint nn,
				 uint ldin, uint ldout,
				 const OCL::Memory &mem_bias,
				 const OCL::Memory &mem_in,
				 const OCL::Memory &mem_out) noexcept {
  assert(queue.ok() && 0 < nm && 0 < nn && 0 < ldin && 0 < ldout);
  assert(mem_bias.ok() && mem_in.ok() && mem_out.ok());
  uint nl;
  for (nl = 16U; nm % nl; nl /= 2U);

  string code = ("#define NN     " + to_string(nn)    + "U\n"
		 "#define NL     " + to_string(nl)    + "U\n"
		 "#define LD_IN  " + to_string(ldin)  + "U\n"
		 "#define LD_OUT " + to_string(ldout) + "U\n"
		 + code_resize_bias_ReLU);
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("resize_bias_ReLU");
  _ker.set_arg(0, mem_bias);
  _ker.set_arg(1, mem_in);
  _ker.set_arg(2, mem_out);
  _size_l[0] = nn;
  _size_l[1] = nl;
  _size_l[2] = 1U;
  _size_g[0] = nn;
  _size_g[1] = nm;
  _size_g[2] = 1U; }

void ManageResizeBiasReLU::push(const OCL::Queue &queue) const noexcept {
  assert(_ker.ok() && queue.ok());
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

void ManageComputePolicy::start(const OCL::Queue &queue, uint nch_in,
				uint maxsize_batch,
				const OCL::Memory &mem_nnmove,
				const OCL::Memory &mem_weight,
				const OCL::Memory &mem_bias,
				const OCL::Memory &mem_in,
				const OCL::Memory &mem_out) noexcept {
  string code =
    "#define NCH_IN " + to_string(nch_in) + "U\n"
    "#define NB     " + to_string(maxsize_batch) + "U\n"
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
  assert(_ker.ok() && queue.ok());
  if (ntot_moves == 0) return;
  _ker.set_arg(1, sizeof(offin), &offin);
  queue.push_kernel(_ker, ntot_moves); }

void ManageTransformValue2::start(const OCL::Queue &queue, uint nch, uint nb,
				  const OCL::Memory &mem_in, uint offin,
				  uint nn_out, const OCL::Memory &mem_out)
  noexcept {
  string code =
    "#define NN_OUT " + to_string(nn_out) + "U\n"
    "#define NCH    " + to_string(nch) + "U\n"
    "#define NBATCH " + to_string(nb)  + "U\n" + code_transform_value2;
  OCL::Program pg = queue.gen_program(code.c_str());
  _ker = pg.gen_kernel("transform_value2");
  _ker.set_arg(0, mem_in);
  _ker.set_arg(1, sizeof(offin), &offin);
  _ker.set_arg(2, mem_out);
  _size_l[0] = NNAux::size_plane;
  _size_l[1] = 1U;
  _size_l[2] = 1U;
  _size_g[0] = NNAux::size_plane;
  _size_g[1] = nb;
  _size_g[2] = nch; }

void ManageTransformValue2::push(const OCL::Queue &queue) const noexcept {
  assert(_ker.ok() && queue.ok());
  queue.push_ndrange_kernel(_ker, 3, _size_g, _size_l); }

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
    NNAux::softmax(sizes_nnmove[ub], probs_b);
    fin += sizes_nnmove[ub]; } }

static void compress_data(uint index_block, uint nb0, uint nb, const float *in,
			  const uint *sizes_nnmove, const ushort *nnmoves,
			  void *out, size_t &size_write, uint &n_one,
			  uint &ntot_moves) noexcept {
  float *pvalue = reinterpret_cast<float *>(out);
  uint  *pindex = reinterpret_cast<uint *>(out) + index_block;
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

  pindex = reinterpret_cast<uint *>(out) + index_block*2U;
  n_one  = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint uposi = 0; uposi < 8U; ++uposi)
      for (uint ufplane = 0; ufplane < 28U; ++ufplane) {
	uint ch  = 45U * uposi + ufplane;
	uint bch = (ub * NNAux::nch_input + ch) * NNAux::size_plane;
	uint chb = (ch * nb + ub) * NNAux::size_plane;
	for (uint u = 0; u < NNAux::size_plane; ++u) {
	  if (in[bch + u] < 0.5f) continue;
	  pindex[n_one++] = chb + u; } }

  uint n_one_end = ceil_multi(n_one, 32U);
  for (; n_one < n_one_end; ++n_one)
    pindex[n_one] = NNAux::nch_input * 128U * nb + (n_one % 32U);

  pindex = reinterpret_cast<uint *>(out) + index_block*2U + n_one;
  ntot_moves = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint unn = 0; unn < sizes_nnmove[ub]; ++unn) {
      uint nnmove = nnmoves[ub * NNAux::nmove + unn];
      ntot_moves += 1U;
      assert(nnmove < NNAux::nch_out_policy * NNAux::size_plane);
      *pindex++ = ub * NNAux::nch_out_policy * NNAux::size_plane + nnmove; }
  
  size_write = (2U*index_block + n_one + ntot_moves) * sizeof(uint); }

NNetOCL::~NNetOCL() noexcept { _mng_send.end(_queue); }

string NNetOCL::reset(uint maxsize_batch,
		      const vector<pair<uint, row_t>> &wght, int device_id,
		      bool use_half, bool flag_out, bool do_sleep) noexcept {
  assert(0 < maxsize_batch);
  _do_sleep = do_sleep;
  stringstream lines;

  for (uint uslot = 0; uslot < NNAux::nslot; ++uslot)
    _pool_slots[NNAux::nslot - uslot - 1U] = uslot;
  _pool_size = NNAux::nslot;

  //
  // compute network dimension
  //
  _maxsize_batch = maxsize_batch;
  _index_block   = ceil_multi(maxsize_batch * send_nch_fill, 32U);
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
  _maxsize_out = max(_maxsize_out, NNAux::nch_input * 128U + 32U);
  _maxsize_out = max(_maxsize_out, 128U * _resnet_nout);
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

  lines << "- Device ID: " << device_id << "\n";
  lines << _cl_dev.gen_info();

  bool use_wmma = false;
  if (use_half) {
    use_wmma = test_wmma(_cl_dev);
    std::cout << "  Wmma support:         " << (use_wmma ? "Yes" : "No")
	      << "\n"; }

  _queue = _cl_dev.gen_queue();
    
  lines << "  Send:                 ";

  _mng_send.start(_cl_dev, _queue, maxsize_batch);
  lines << _mng_send.gen_info() << "\n";

  lines << "  Recv:                 ";
  _mng_recv.start(_cl_dev, _queue, (1U + NNAux::nmove) * sizeof(float),
		  maxsize_batch);
  lines << _mng_recv.gen_info() << "\n";

  SgemmParam param_matM
    = tune_compute_matM(use_wmma, _cl_dev, size_tile_in, _resnet_nout,
			maxsize_batch * ntile, _resnet_nout);
  lines << "  Matrix M:             ";
  lines << param_matM.gen_info() << "\n";
  _mng_compute_matM_input.start(_queue, size_tile_in, _resnet_nout,
				maxsize_batch * ntile, NNAux::nch_input,
				param_matM);
  _mng_compute_matM.start(_queue, size_tile_in, _resnet_nout,
			  maxsize_batch * ntile, _resnet_nout, param_matM);

  lines << "  Head 1:               ";
  _mng_head1.start(_cl_dev, _queue, true, false, true, false, false,
		   _head1_nout, maxsize_batch * NNAux::size_plane,
		   _resnet_nout, 0,
		   _head1_nout, 0, maxsize_batch * NNAux::size_plane, 0,
		   maxsize_batch * NNAux::size_plane);
  lines << _mng_head1.gen_info() << "\n";

  lines << "  Value 2:              ";
  _mng_value2.start(_cl_dev, _queue, true, false, true, false, false,
		    _value2_nout, maxsize_batch, _value2_nin, 0, _value2_nout,
		    0, maxsize_batch, 0, maxsize_batch);
  lines << _mng_value2.gen_info() << "\n";

  lines << "  Value 3:              ";
  _mng_value3.start(_cl_dev, _queue, true, false, false, true, true,
		    maxsize_batch, 1U, _value3_nin, 0, maxsize_batch, 0, 1U, 0,
		    1U);
  lines << _mng_value3.gen_info() << "\n";

  uint mmax, nmax, kmax;
  mmax = max(_mng_compute_matM_input.get_nm(), _mng_compute_matM.get_nm());
  nmax = max(_mng_compute_matM_input.get_nn(), _mng_compute_matM.get_nn());
  kmax = max(_mng_compute_matM_input.get_nk(), _mng_compute_matM.get_nk());
  uint sizeV = size_tile_in * kmax * nmax;
  uint sizeM = size_tile_in * mmax * nmax;
  uint size_output = maxsize_batch * _maxsize_out;
  size_output = max(size_output, _mng_head1.get_nk() * _mng_head1.get_nn());

  for (uint u = 0; u < NNAux::nslot; ++u) {
    _ptr_input[u].reset(new uchar [sizeof(float) * maxsize_batch * 
				   (NNAux::nmove
				    + NNAux::nch_input * NNAux::size_plane)]);
    _ptr_result[u].reset(new float [maxsize_batch * (NNAux::nmove + 1U)]);
    _slots_sizes_nnmove[u].reset(new uint [maxsize_batch]); }

  _cl_matV   = _queue.gen_mem_drw(sizeof(float) * sizeV);
  _cl_matM   = _queue.gen_mem_drw(sizeof(float) * sizeM);
  _cl_bypass = _queue.gen_mem_drw(sizeof(float) * size_output);
  _cl_output = _queue.gen_mem_drw(sizeof(float) * size_output);
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

  _mng_decode.start(_queue, _mng_send.get_work(), _cl_output, _index_block,
		    maxsize_batch);

  _mng_compute_matM_input.register_b(_cl_matV);
  _mng_compute_matM_input.register_c(_cl_matM);
  _mng_compute_matM.register_b(_cl_matV);
  _mng_compute_matM.register_c(_cl_matM);
  _mng_compute_matV_input.start(use_wmma, _queue, NNAux::nch_input,
				maxsize_batch,
				_mng_compute_matM_input.get_nn(),
				_mng_compute_matM_input.get_nk(),
				_cl_matV);
  _mng_compute_matAV_input.start(use_wmma, false, true, _queue, _resnet_nout,
				 maxsize_batch, _mng_compute_matM.get_nm(),
				 _mng_compute_matM.get_nn(),
				 _mng_compute_matM.get_nk(),
				 _cl_matM, _cl_matV, _cl_bypass);
  _mng_compute_matAV.start(use_wmma, false, false, _queue, _resnet_nout,
			   maxsize_batch, _mng_compute_matM.get_nm(),
			   _mng_compute_matM.get_nn(),
			   _mng_compute_matM.get_nk(),
			   _cl_matM, _cl_matV, OCL::Memory());
  _mng_compute_matAV_join.start(use_wmma, true, true, _queue, _resnet_nout,
				maxsize_batch, _mng_compute_matM.get_nm(),
				_mng_compute_matM.get_nn(),
				_mng_compute_matM.get_nk(),
				_cl_matM, _cl_matV, _cl_bypass);
  _mng_compute_matA_join.start(use_wmma, _queue, true, _resnet_nout,
			       maxsize_batch, _mng_compute_matM.get_nm(),
			       _mng_compute_matM.get_nn(),
			       _mng_head1.get_nn(),
			       _cl_matM, _cl_bypass, _cl_output);

  load(use_wmma, wght);

  _mng_head1.register_a0(_cl_head1_wght);
  _mng_head1.register_b(_cl_output);
  _mng_head1.register_c(_cl_bypass);
  _mng_head1.push_load_a(_queue);

  _mng_compute_BNReLU.start(_queue, _head1_nout, maxsize_batch,
			    _mng_head1.get_nn(),
			    _cl_head1_mean, _cl_head1_sd_inv, _cl_bypass,
			    _cl_output);
  _mng_compute_policy.start(_queue, _policy2_nin, maxsize_batch,
			    _mng_send.get_work(), _cl_policy2_wght,
			    _cl_policy2_bias, _cl_output, _mng_recv.get());
  _mng_transform_value2.start(_queue, _value1_nout, maxsize_batch, _cl_output,
			      _policy1_nout * maxsize_batch * 128U,
			      _mng_value2.get_nn(), _cl_bypass);

  _mng_resize_bias_ReLU_value3.start(_queue, _value2_nout, maxsize_batch,
				     _mng_value2.get_nn(),
				     _mng_value3.get_nm(),
				     _cl_value2_bias,
				     _cl_output, _cl_bypass);

  _mng_value2.register_a0(_cl_value2_wght);
  _mng_value2.register_b(_cl_bypass);
  _mng_value2.register_c(_cl_output);
  _mng_value2.push_load_a(_queue);

  _mng_value3.register_a(_cl_bypass);
  _mng_value3.register_b0(_cl_value3_wght);
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
  _cl_value3_wght.clear();
  if (flag_out) cout << lines.str() << std::flush;
  return lines.str(); }

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
  uint wait_id = push_ff(size_batch, input, sizes_nnmove, nnmoves,
			 probs, values);
  wait_ff(wait_id); }

uint NNetOCL::push_ff(uint size_batch, const float *input,
		      const uint *sizes_nnmove, const ushort *nnmoves,
		      float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);
  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("size_batch == 0"));

  unique_lock<mutex> lock(_m);
  _cv.wait(lock, [&]{ return 0 < _pool_size; });
  uint uslot = _pool_slots[ --_pool_size ];
  lock.unlock();

  size_t size_write;
  uint n_one, ntot_moves;
  compress_data(_index_block, size_batch, _maxsize_batch, input, sizes_nnmove,
		nnmoves, _ptr_input[uslot].get(), size_write, n_one,
		ntot_moves);
  _mng_send.push(_queue, _ptr_input[uslot].get(), size_write, uslot);
  _mng_decode.push(_queue, n_one);

  // body part
  _mng_compute_matV_input.push(_queue, _cl_output);
  _mng_compute_matM_input.push(_queue, _cl_reswghts[0].matU);
  _mng_compute_matAV_input.push(_queue, _cl_reswghts[0].mean,
				_cl_reswghts[0].sd_inv);
  uint ulayer = 1U;
  for (; ulayer + 2U < _cl_reswghts.size(); ulayer += 2U) {
    _mng_compute_matM.push(_queue, _cl_reswghts[ulayer].matU);
    _mng_compute_matAV.push(_queue, _cl_reswghts[ulayer].mean,
			   _cl_reswghts[ulayer].sd_inv);
    _mng_compute_matM.push(_queue, _cl_reswghts[ulayer + 1U].matU);
    _mng_compute_matAV_join.push(_queue, _cl_reswghts[ulayer + 1U].mean,
				 _cl_reswghts[ulayer + 1U].sd_inv); }
  
  _mng_compute_matM.push(_queue, _cl_reswghts[ulayer].matU);
  _mng_compute_matAV.push(_queue, _cl_reswghts[ulayer].mean,
			  _cl_reswghts[ulayer].sd_inv);
  _mng_compute_matM.push(_queue, _cl_reswghts[ulayer + 1U].matU);  
  _mng_compute_matA_join.push(_queue, _cl_reswghts[ulayer + 1U].mean,
			      _cl_reswghts[ulayer + 1U].sd_inv);

  // head part
  // in:  f1[_policy1_nout + _value1_nout][size_batch][size_plane]
  // out: f2[size_batch][_value1_nout][size_plane]
  _mng_head1.push(_queue);
  _mng_compute_BNReLU.push(_queue);
  _mng_compute_policy.push(_queue, ntot_moves, 2U*_index_block + n_one);

  _mng_transform_value2.push(_queue);
  _mng_value2.push(_queue);
  _mng_resize_bias_ReLU_value3.push(_queue);
  _mng_value3.push(_queue);
  _mng_recv.push(_queue, _ptr_result[uslot].get(),
		 (_maxsize_batch + ntot_moves) * sizeof(float),	uslot);
  memcpy(_slots_sizes_nnmove[uslot].get(), sizes_nnmove,
	 sizeof(uint) * size_batch);
  _slots_size_batch[uslot] = size_batch;
  _slots_probs[uslot]      = probs;
  _slots_values[uslot]     = values;
  return uslot; }

void NNetOCL::wait_ff(uint uslot) noexcept {
  assert(uslot < NNAux::nslot);
  if (_do_sleep) {
    static double elapsed_ave = 0.0;
    steady_clock::time_point start = steady_clock::now();
    sleep_for(duration<double, std::ratio<7, 10000000>>(elapsed_ave));
    _mng_recv.wait(_queue, uslot);
    steady_clock::time_point end = steady_clock::now();
    double elapsed
      = static_cast<double>(duration_cast<microseconds>(end - start).count());
    elapsed_ave = 0.95 * elapsed_ave + 0.05 * elapsed; }
  else _mng_recv.wait(_queue, uslot);

  compute_probs(_slots_size_batch[uslot], _slots_sizes_nnmove[uslot].get(),
		_ptr_result[uslot].get() + _maxsize_batch,
		_slots_probs[uslot]);
  for (uint ub = 0; ub < _slots_size_batch[uslot]; ++ub)
    _slots_values[uslot][ub]
      = std::tanh(_ptr_result[uslot][ub] + _value3_bias[0]);

  unique_lock<mutex> lock(_m);
  assert(_pool_size < NNAux::nslot);
  _pool_slots[ _pool_size++ ] = uslot;
  lock.unlock();
  _cv.notify_one(); }
#endif
