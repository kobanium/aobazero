// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(USE_OPENCL_AOBA)
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-ocl.hpp"
#include <algorithm>
#include <deque>
#include <exception>
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
#include <cstdint>
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
using std::lock_guard;
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
using std::terminate;
using std::thread;
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
struct CLResWght { OCL::Memory matU, mean, sd_inv; };
static_assert(sizeof(float) == sizeof(int32_t),
	      "sizeof(float) == sizeof(int32_t) * 4U");
static_assert(sizeof(float) == sizeof(uint), "sizeof(float) == sizeof(uint)");
static_assert(sizeof(float) == sizeof(ushort) * 2U,
	      "sizeof(float) == sizeof(ushort) * 2U");

constexpr char msg_bad_wght_dim[]    = "bad weight dimension";
constexpr char msg_opencl_error[]    = "opencl";
constexpr uint tune_sample_size_base = 256U;
constexpr uint size_wrap_wmma        = 32U;
constexpr uint len_kernel            = 3U;
constexpr uint size_kernel           = len_kernel * len_kernel;
constexpr uint len_tile_out          = 3U;
constexpr uint len_tile_in           = 5U;
constexpr uint size_tile_in          = len_tile_in * len_tile_in;
constexpr uint ntile_h               = 3U;
constexpr uint ntile_w               = 3U;
constexpr uint ntile                 = ntile_h * ntile_w;
constexpr uint size_plane_in         = size_tile_in * ntile;
constexpr uint pad                   = 1U;
constexpr uint send_size_ave         = 3000U;
constexpr uint read_size_ave         = 400U;
constexpr uint size_align_local      = 32U;
constexpr float bn_factor            = 1.0f / 999.982f;
constexpr float bn_eps               = 1e-5f;

/*
  static int64_t elapsed_sum = 0;
  static int64_t nelapsed    = 0;
  _queue.finish();
  steady_clock::time_point start = steady_clock::now();
  _queue.finish();
  steady_clock::time_point end = steady_clock::now();
  int64_t elapsed = duration_cast<microseconds>(end - start).count();
  elapsed_sum += elapsed;
  nelapsed    += 1U;
  std::cout << std::endl
	    << elapsed << " " << elapsed_sum / nelapsed << std::endl;
*/

const string code_decode = R"(
__kernel void zero_clear(__global float *p) { p[get_global_id(0)] = 0.0f; }

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
    = max(0.0f, a*(fin[ch*NN_IN + ub*SIZE_PLANE + sq] - b)); }
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
float x3(float x) { return 3.0f * x; }
float x4(float x) { return 4.0f * x; }
float x6(float x) { return 6.0f * x; }
float x8(float x) { return 8.0f * x; }
float x9(float x) { return 9.0f * x; }
float x16(float x) { return 16.0f * x; }
//float x3(float x) { return x + x + x; }
//float x4(float x) { x += x; return x + x; }
//float x6(float x) { x += x; return x + x + x; }
//float x8(float x) { x += x; x += x; return x + x; }
//nfloat x9(float x) { float y = x + x; y += y; return y + y + x; }
//float x16(float x) { x += x; x += x; x += x; return x + x; }
)";

const string code_compute_matV_child = R"(
#ifdef STORE_HALF
void store(float f, uint off, __global half *p) { vstore_half(f, off, p); }
#else
void store(float f, uint off, __global float *p) { p[off] = f; }
#endif

void compute_matV_child(uint utile, uint dim_n_offset,
                        float md[LEN_TILE_IN][LEN_TILE_IN],
                        __local const float *flin, __global void *matV) {
  uint uh = utile / NTILE_W;
  uint uw = utile % NTILE_W;
  int y0  = uh*LEN_TILE_OUT - PAD;
  int x0  = uw*LEN_TILE_OUT - PAD;
  for (int y = 0; y < LEN_TILE_IN; ++y)
    for (int x = 0; x < LEN_TILE_IN; ++x) {
      if (0 <= y0 + y && y0 + y < HEIGHT && 0 <= x0 + x && x0 + x < WIDTH)
        md[y][x] = flin[(y0 + y)*WIDTH + x0 + x];
      else md[y][x] = 0.0f; }

  store(+ x4(md[0][0]) - x2(md[0][1]) - x4(md[0][2]) + x2(md[0][3])
        - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
        - x4(md[2][0]) + x2(md[2][1]) + x4(md[2][2]) - x2(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        (0U*LEN_TILE_IN + 0U)*NK*NN + dim_n_offset, matV);
  store(- x4(md[1][0]) + x2(md[1][1]) + x4(md[1][2]) - x2(md[1][3])
        - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        (1U*LEN_TILE_IN + 0U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
        - x6(md[2][0]) + x3(md[2][1]) + x6(md[2][2]) - x3(md[2][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        (2U*LEN_TILE_IN + 0U)*NK*NN + dim_n_offset, matV);
  store(- x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
        + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]),
        (3U*LEN_TILE_IN + 0U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
        - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
        - x4(md[3][0]) + x2(md[3][1]) + x4(md[3][2]) - x2(md[3][3])
        + x2(md[4][0]) - x1(md[4][1]) - x2(md[4][2]) + x1(md[4][3]),
        (4U*LEN_TILE_IN + 0U)*NK*NN + dim_n_offset, matV);

  store(- x4(md[0][1]) - x2(md[0][2]) + x2(md[0][3])
        + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
        + x4(md[2][1]) + x2(md[2][2]) - x2(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        (0U*LEN_TILE_IN + 1U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][1]) + x2(md[1][2]) - x2(md[1][3])
        + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        (1U*LEN_TILE_IN + 1U)*NK*NN + dim_n_offset, matV);
  store(- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
        + x6(md[2][1]) + x3(md[2][2]) - x3(md[2][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        (2U*LEN_TILE_IN + 1U)*NK*NN + dim_n_offset, matV);
  store(+ x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
        - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]),
        (3U*LEN_TILE_IN + 1U)*NK*NN + dim_n_offset, matV);
  store(- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
        + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
        + x4(md[3][1]) + x2(md[3][2]) - x2(md[3][3])
        - x2(md[4][1]) - x1(md[4][2]) + x1(md[4][3]),
        (4U*LEN_TILE_IN + 1U)*NK*NN + dim_n_offset, matV);

  store(+ x4(md[0][1]) - x6(md[0][2]) + x2(md[0][3])
        - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
        - x4(md[2][1]) + x6(md[2][2]) - x2(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        (0U*LEN_TILE_IN + 2U)*NK*NN + dim_n_offset, matV);
  store(- x4(md[1][1]) + x6(md[1][2]) - x2(md[1][3])
        - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        (1U*LEN_TILE_IN + 2U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
        - x6(md[2][1]) + x9(md[2][2]) - x3(md[2][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        (2U*LEN_TILE_IN + 2U)*NK*NN + dim_n_offset, matV);
  store(- x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
        + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]),
        (3U*LEN_TILE_IN + 2U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
        - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
        - x4(md[3][1]) + x6(md[3][2]) - x2(md[3][3])
        + x2(md[4][1]) - x3(md[4][2]) + x1(md[4][3]),
        (4U*LEN_TILE_IN + 2U)*NK*NN + dim_n_offset, matV);

  store(- x2(md[0][1]) + x2(md[0][3]) + x1(md[1][1]) - x1(md[1][3])
        + x2(md[2][1]) - x2(md[2][3]) - x1(md[3][1]) + x1(md[3][3]),
        (0U*LEN_TILE_IN + 3U)*NK*NN + dim_n_offset, matV);
  store(+ x2(md[1][1]) - x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
        - x1(md[3][1]) + x1(md[3][3]),
        (1U*LEN_TILE_IN + 3U)*NK*NN + dim_n_offset, matV);
  store(- x2(md[1][1]) + x2(md[1][3]) + x3(md[2][1]) - x3(md[2][3])
        - x1(md[3][1]) + x1(md[3][3]),
        (2U*LEN_TILE_IN + 3U)*NK*NN + dim_n_offset, matV);
  store(+ x1(md[1][1]) - x1(md[1][3]) - x1(md[3][1]) + x1(md[3][3]),
        (3U*LEN_TILE_IN + 3U)*NK*NN + dim_n_offset, matV);
  store(- x2(md[1][1]) + x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
        + x2(md[3][1]) - x2(md[3][3]) - x1(md[4][1]) + x1(md[4][3]),
        (4U*LEN_TILE_IN + 3U)*NK*NN + dim_n_offset, matV);

  store(+ x4(md[0][1]) - x2(md[0][2]) - x4(md[0][3]) + x2(md[0][4])
        - x2(md[1][1]) + x1(md[1][2]) + x2(md[1][3]) - x1(md[1][4])
        - x4(md[2][1]) + x2(md[2][2]) + x4(md[2][3]) - x2(md[2][4])
        + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]),
        (0U*LEN_TILE_IN + 4U)*NK*NN + dim_n_offset, matV);
  store(- x4(md[1][1]) + x2(md[1][2]) + x4(md[1][3]) - x2(md[1][4])
        - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
        + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]),
        (1U*LEN_TILE_IN + 4U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][1]) - x6(md[2][1]) + x2(md[3][1])
        - x2(md[1][2]) + x3(md[2][2]) - x1(md[3][2])
        - x4(md[1][3]) + x6(md[2][3]) - x2(md[3][3])
        + x2(md[1][4]) - x3(md[2][4]) + x1(md[3][4]),
        (2U*LEN_TILE_IN + 4U)*NK*NN + dim_n_offset, matV);
  store(- x2(md[1][1]) + x2(md[3][1]) + x1(md[1][2]) - x1(md[3][2])
        + x2(md[1][3]) - x2(md[3][3]) - x1(md[1][4]) + x1(md[3][4]),
        (3U*LEN_TILE_IN + 4U)*NK*NN + dim_n_offset, matV);
  store(+ x4(md[1][1]) - x2(md[1][2]) - x4(md[1][3]) + x2(md[1][4])
        - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
        - x4(md[3][1]) + x2(md[3][2]) + x4(md[3][3]) - x2(md[3][4])
        + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4]),
        (4U*LEN_TILE_IN + 4U)*NK*NN + dim_n_offset, matV); }
)";

const string code_compute_matA_child = R"(
#ifdef LOAD_HALF
float load(uint off, __global const half *p) { return vload_half(off, p); }
#else
float load(uint off, __global const float *p) { return p[off]; }
#endif

#ifdef DO_JOIN
void func_BNReLU(__local float *f, uint off, float sd_inv, float mean,
                 float x) {
  f[off] = max(0.0f, sd_inv * (x - mean) + f[off]); }
#else
void func_BNReLU(__local float *f, uint off, float sd_inv, float mean,
                 float x) {
  f[off] = max(0.0f, sd_inv * (x - mean)); }
#endif

void compute_matA_child(uint utile, uint dim_n_offset, float mean,
                        float sd_inv, float mm[LEN_TILE_IN][LEN_TILE_IN],
                        __global const void *matM,
                        __local float *flout) {
  for (uint uh = 0; uh < LEN_TILE_IN; ++uh)
    for (uint uw = 0; uw < LEN_TILE_IN; ++uw)
      mm[uh][uw] = load((uh*LEN_TILE_IN + uw)*NM*NN + dim_n_offset, matM);

  uint uh  = utile / NTILE_W;
  uint uw  = utile % NTILE_W;
  flout   += (uh*WIDTH + uw)*LEN_TILE_OUT;
#ifdef DO_JOIN
  barrier(CLK_LOCAL_MEM_FENCE);
#endif
  func_BNReLU(flout, 0U*WIDTH + 0U, sd_inv, mean,
              + mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
              + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
  func_BNReLU(flout, 0U*WIDTH + 1U, sd_inv, mean,
              + mm[0][1] - mm[0][2] + x2(mm[0][3])
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              + mm[2][1] - mm[2][2] + x2(mm[2][3])
              + mm[3][1] - mm[3][2] + x2(mm[3][3]));
  func_BNReLU(flout, 0U*WIDTH + 2U, sd_inv, mean,
              + mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
              + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
  func_BNReLU(flout, 1U*WIDTH + 0U, sd_inv, mean,
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
              + x2(mm[3][0]) + x2(mm[3][1]) + x2(mm[3][2]) + x2(mm[3][3]));
  func_BNReLU(flout, 1U*WIDTH + 1U, sd_inv, mean,
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              - mm[2][1] + mm[2][2]
              - x2(mm[2][3]) + x2(mm[3][1]) - x2(mm[3][2]) + x4(mm[3][3]));
  func_BNReLU(flout, 1U*WIDTH + 2U, sd_inv, mean,
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
              + x2(mm[3][1]) + x2(mm[3][2]) + x8(mm[3][3]) + x2(mm[3][4]));
  func_BNReLU(flout, 2U*WIDTH + 0U, sd_inv, mean,
              + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
              + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
              + x4(mm[3][0]) + x4(mm[3][1]) + x4(mm[3][2]) + x4(mm[3][3])
              + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
  func_BNReLU(flout, 2U*WIDTH + 1U, sd_inv, mean,
              + mm[1][1] - mm[1][2] + x2(mm[1][3])
              + mm[2][1] - mm[2][2]
              + x2(mm[2][3]) + x4(mm[3][1]) - x4(mm[3][2]) + x8(mm[3][3])
              + mm[4][1] - mm[4][2] + x2(mm[4][3]));
  func_BNReLU(flout, 2U*WIDTH + 2U, sd_inv, mean,
              + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
              + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
              + x4(mm[3][1]) + x4(mm[3][2]) + x16(mm[3][3]) + x4(mm[3][4])
              + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }
)";

const string code_compute_matA = R"(
__kernel __attribute__((reqd_work_group_size(NTILE, NBWS, 1)))
void compute_matA_BNReLU(__global const void *matM,
                         __global const float *mean_array,
                         __global const float *sd_inv_array,
                         __global const float *fbypass,
                         __global float *fout) {
  uint utile = get_global_id(0);
  uint ch    = get_global_id(2);
  uint ng    = get_num_groups(1);
  uint ug    = get_group_id(1);
  uint ub1   = get_local_id(1);
  __local float flout[NBWS*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));
#ifdef DO_JOIN
  for (uint u = 0; u < NTILE; ++u)
    flout[u*NBWS*NTILE + ub1*NTILE + utile]
      = fbypass[ch*ng*NTILE*BLOCK_SIZE + ug*NTILE*BLOCK_SIZE
                + u*BLOCK_SIZE + ub1*NTILE + utile];
#endif

  uint dim_n_offset = ch*NN + ug*NB_PARTITION + ub1*NTILE + utile;
  float mean   = mean_array[ch];
  float sd_inv = sd_inv_array[ch];
  float M[LEN_TILE_IN][LEN_TILE_IN];
  compute_matA_child(utile, dim_n_offset, mean, sd_inv, M, matM,
                     flout + ub1*SIZE_PLANE);

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint u = 0; u < NTILE; ++u)
    fout[ch*NN_OUT + (ug*NBWS + ub1)*SIZE_PLANE + u*NTILE + utile]
      = flout[ub1*SIZE_PLANE + u*NTILE + utile];
}
)";

const string code_compute_matV = R"(
__kernel __attribute__((reqd_work_group_size(NTILE, NBWS, 1)))
void compute_matV(__global const float *fin, __global void *matV) {
  uint utile = get_global_id(0);
  uint ch    = get_global_id(2);
  uint ug    = get_group_id(1);
  uint ub1   = get_local_id(1);
  __local float flin[NBWS*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));
  for (uint u = 0; u < 9U; ++u)
    flin[ub1*SIZE_PLANE + u*NTILE + utile]
      = fin[ch*NB*SIZE_PLANE + ug*NBWS*SIZE_PLANE + ub1*SIZE_PLANE
            + u*NTILE + utile];

  uint dim_n_offset = ch*NN + ug*NB_PARTITION + ub1*NTILE + utile;
  float M[LEN_TILE_IN][LEN_TILE_IN];
  barrier(CLK_LOCAL_MEM_FENCE);
  compute_matV_child(utile, dim_n_offset, M, flin + ub1*SIZE_PLANE, matV); }
)";

const string code_compute_matAV = R"(
__kernel __attribute__((reqd_work_group_size(NTILE, NBWS, 1)))
void compute_matAV(__global const void *matM,
                   __global const float *mean_array,
                   __global const float *sd_inv_array,
#if defined(DO_JOIN) || defined(DO_FORK)
                   __global float *matV, __global float *fbypass) {
#else
                   __global float *matV) {
#endif
  uint utile = get_global_id(0);
  uint ch    = get_global_id(2);
  uint ng    = get_num_groups(1);
  uint ug    = get_group_id(1);
  uint ub1   = get_local_id(1);
  __local float flout[NBWS*SIZE_PLANE] __attribute__((aligned(SIZE_ALIGN)));
#ifdef DO_JOIN
  for (uint u = 0; u < NTILE; ++u)
    flout[u*NBWS*NTILE + ub1*NTILE + utile]
      = fbypass[ch*ng*NTILE*BLOCK_SIZE + ug*NTILE*BLOCK_SIZE
                + u*BLOCK_SIZE + ub1*NTILE + utile];
#endif

  uint dim_n_offset = ch*NN + ug*NB_PARTITION + ub1*NTILE + utile;
  float mean        = mean_array[ch];
  float sd_inv      = sd_inv_array[ch];
  float M[LEN_TILE_IN][LEN_TILE_IN];
  compute_matA_child(utile, dim_n_offset, mean, sd_inv, M, matM,
                     flout + ub1*SIZE_PLANE);

  barrier(CLK_LOCAL_MEM_FENCE);
#ifdef DO_FORK
  for (uint u = 0; u < NTILE; ++u)
    fbypass[ch*ng*NTILE*BLOCK_SIZE + ug*NTILE*BLOCK_SIZE
            + u*BLOCK_SIZE + ub1*NTILE + utile]
      = flout[u*NBWS*NTILE + ub1*NTILE + utile];
#endif
  compute_matV_child(utile, dim_n_offset, M, flout + ub1*SIZE_PLANE, matV); }
)";


const string code_compute_matM_wmma = R"(
#if WMMA_ACCUMU16
void wmma_mma(uint *mc, __local const uint *ma, __local const uint *mb) {
  asm("{ wmma.mma.sync.aligned.col.row" SGEMM_TDM ".f16.f16\n"
      "    {%0, %1, %2, %3},\n"
      "    {%4, %5, %6, %7, %8, %9, %10, %11},\n"
      "    {%12, %13, %14, %15, %16, %17, %18, %19},\n"
      "    {%0, %1, %2, %3}; }"
      : "+r"(mc[0]), "+r"(mc[1]), "+r"(mc[2]), "+r"(mc[3])
      : "r"(ma[SGEMM_NLM*WRAP_SIZE*0]), "r"(ma[SGEMM_NLM*WRAP_SIZE*1]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*2]), "r"(ma[SGEMM_NLM*WRAP_SIZE*3]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*4]), "r"(ma[SGEMM_NLM*WRAP_SIZE*5]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*6]), "r"(ma[SGEMM_NLM*WRAP_SIZE*7]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*0]), "r"(mb[SGEMM_NLN*WRAP_SIZE*1]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*2]), "r"(mb[SGEMM_NLN*WRAP_SIZE*3]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*4]), "r"(mb[SGEMM_NLN*WRAP_SIZE*5]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*6]), "r"(mb[SGEMM_NLN*WRAP_SIZE*7])); }

void wmma_store(__global float *dest, const float *src) {
  asm("{  wmma.store.d.sync.aligned.row" SGEMM_TDM ".global.f16\n"
      "     [%4], {%0, %1, %2, %3}, %5; }"
      :: "r"(src[0]), "r"(src[1]), "r"(src[2]), "r"(src[3]),
         "l"(dest), "r"(NN) : "memory"); }

#  define TYPE_OUT  ushort
#  define ACCU_SIZE 4U
#else
void wmma_mma(uint *mc, __local const uint *ma, __local const uint *mb) {
  asm("{ wmma.mma.sync.aligned.col.row" SGEMM_TDM ".f32.f32\n"
      "    {%0, %1, %2, %3, %4, %5, %6, %7},\n"
      "    {%8, %9, %10, %11, %12, %13, %14, %15},\n"
      "    {%16, %17, %18, %19, %20, %21, %22, %23},\n"
      "    {%0, %1, %2, %3, %4, %5, %6, %7}; }"
      : "+r"(mc[0]), "+r"(mc[1]), "+r"(mc[2]), "+r"(mc[3]),
        "+r"(mc[4]), "+r"(mc[5]), "+r"(mc[6]), "+r"(mc[7])
      : "r"(ma[SGEMM_NLM*WRAP_SIZE*0]), "r"(ma[SGEMM_NLM*WRAP_SIZE*1]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*2]), "r"(ma[SGEMM_NLM*WRAP_SIZE*3]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*4]), "r"(ma[SGEMM_NLM*WRAP_SIZE*5]),
        "r"(ma[SGEMM_NLM*WRAP_SIZE*6]), "r"(ma[SGEMM_NLM*WRAP_SIZE*7]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*0]), "r"(mb[SGEMM_NLN*WRAP_SIZE*1]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*2]), "r"(mb[SGEMM_NLN*WRAP_SIZE*3]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*4]), "r"(mb[SGEMM_NLN*WRAP_SIZE*5]),
        "r"(mb[SGEMM_NLN*WRAP_SIZE*6]), "r"(mb[SGEMM_NLN*WRAP_SIZE*7])); }

void wmma_store(__global float *dest, const float *src) {
  asm("{  wmma.store.d.sync.aligned.row" SGEMM_TDM ".global.f32\n"
      "     [%8], {%0, %1, %2, %3, %4, %5, %6, %7}, %9; }"
      :: "r"(src[0]), "r"(src[1]), "r"(src[2]), "r"(src[3]),
         "r"(src[4]), "r"(src[5]), "r"(src[6]), "r"(src[7]),
         "l"(dest), "r"(NN) : "memory"); }

#  define TYPE_OUT  uint
#  define ACCU_SIZE 8U
#endif

void wmma_load_a(__local uint *dest, __global const uint *src) {
  asm("{ wmma.load.a.sync.aligned.col" SGEMM_TDM ".global.f16\n"
      "    {%0, %1, %2, %3, %4, %5, %6, %7}, [%8], %9; }"
      : "=r"(dest[SGEMM_NLM*WRAP_SIZE*0]), "=r"(dest[SGEMM_NLM*WRAP_SIZE*1]),
        "=r"(dest[SGEMM_NLM*WRAP_SIZE*2]), "=r"(dest[SGEMM_NLM*WRAP_SIZE*3]),
        "=r"(dest[SGEMM_NLM*WRAP_SIZE*4]), "=r"(dest[SGEMM_NLM*WRAP_SIZE*5]),
        "=r"(dest[SGEMM_NLM*WRAP_SIZE*6]), "=r"(dest[SGEMM_NLM*WRAP_SIZE*7])
      : "l"(src), "r"(NM) : "memory"); }

void wmma_load_b(__local uint *dest, __global const uint *src) {
  asm("{ wmma.load.b.sync.aligned.row" SGEMM_TDM ".global.f16\n"
      "    {%0, %1, %2, %3, %4, %5, %6, %7}, [%8], %9; }"
      : "=r"(dest[SGEMM_NLN*WRAP_SIZE*0]), "=r"(dest[SGEMM_NLN*WRAP_SIZE*1]),
        "=r"(dest[SGEMM_NLN*WRAP_SIZE*2]), "=r"(dest[SGEMM_NLN*WRAP_SIZE*3]),
        "=r"(dest[SGEMM_NLN*WRAP_SIZE*4]), "=r"(dest[SGEMM_NLN*WRAP_SIZE*5]),
        "=r"(dest[SGEMM_NLN*WRAP_SIZE*6]), "=r"(dest[SGEMM_NLN*WRAP_SIZE*7])
      : "l"(src), "r"(NN) : "memory"); }

#if (SGEMM_NLM == 1U && SGEMM_NLN == 1U)
__kernel __attribute__((reqd_work_group_size(WRAP_SIZE, 1, 1)))
void compute_matM(__global const uint *gA, __global const uint *gB,
                  __global TYPE_OUT *gC) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  gA += ub*(OFFA/2U) + ugm*(SGEMM_NPTM/2U);
  gB += ub*(OFFB/2U) + ugn*(SGEMM_NPTN/2U);
  gC += ub*OFFC + ugm*SGEMM_NPTM*NN + ugn*SGEMM_NPTN;

  uint pD[SGEMM_NPM][SGEMM_NPN][ACCU_SIZE];
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      for (uint u = 0; u < ACCU_SIZE; ++u) pD[upm][upn][u] = 0;

  __local uint la[SGEMM_NPK][SGEMM_NPM][8][WRAP_SIZE]
               __attribute__((aligned(32)));
  __local uint lb[SGEMM_NPK][SGEMM_NPN][8][WRAP_SIZE]
               __attribute__((aligned(32)));

  uint ulane = get_local_id(0);
  uint ngk   = NK / SGEMM_NPTK;
  for (uint ugk = 0; ugk < ngk; ++ugk) {

    for (uint upk = 0; upk < SGEMM_NPK; ++upk)
      for (uint upm = 0; upm < SGEMM_NPM; ++upm)
        wmma_load_a(&la[upk][upm][0][ulane],
                    gA + upk*SGEMM_NTK*(NM/2U) + upm*(SGEMM_NTM/2U));

    for (uint upk = 0; upk < SGEMM_NPK; ++upk)
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        wmma_load_b(&lb[upk][upn][0][ulane],
                    gB + upk*SGEMM_NTK*(NN/2U) + upn*(SGEMM_NTN/2U));

    for (uint upk = 0; upk < SGEMM_NPK; ++upk)
      for (uint upm = 0; upm < SGEMM_NPM; ++upm)
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
           wmma_mma(&pD[upm][upn][0],
                   &la[upk][upm][0][ulane], &lb[upk][upn][0][ulane]);

    gA += SGEMM_NPTK*(NM/2U);
    gB += SGEMM_NPTK*(NN/2U); }

  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      wmma_store(gC + upm*SGEMM_NTM*NN + upn*SGEMM_NTN, &pD[upm][upn][0]); }

#else

__kernel __attribute__((reqd_work_group_size(SGEMM_NLN*WRAP_SIZE,
                                             SGEMM_NLM,1)))
void compute_matM(__global const uint *gA, __global const uint *gB,
                  __global TYPE_OUT *gC) {
  uint ub  = get_global_id(2);
  uint ugm = get_group_id(1);
  uint ugn = get_group_id(0);
  gA += ub*(OFFA/2U) + ugm*(SGEMM_NLPTM/2U);
  gB += ub*(OFFB/2U) + ugn*(SGEMM_NLPTN/2U);
  gC += ub*OFFC + ugm*SGEMM_NLPTM*NN + ugn*SGEMM_NLPTN;

  uint pD[SGEMM_NPM][SGEMM_NPN][ACCU_SIZE];
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      for (uint u = 0; u < ACCU_SIZE; ++u) pD[upm][upn][u] = 0;

  __local uint la[SGEMM_NPK][SGEMM_NPM][8][SGEMM_NLM][WRAP_SIZE]
               __attribute__((aligned(32)));
  __local uint lb[SGEMM_NPK][SGEMM_NPN][8][SGEMM_NLN][WRAP_SIZE]
               __attribute__((aligned(32)));

  uint ulm   = get_local_id(1);
  uint uln   = get_local_id(0) / WRAP_SIZE;
  uint ulane = get_local_id(0) % WRAP_SIZE;
  uint ngk   = NK / SGEMM_NPTK;
  uint ulm_r = (ulm*SGEMM_NLN + uln) % SGEMM_NLM;
  uint uln_r = (ulm*SGEMM_NLN + uln) / SGEMM_NLM;
  for (uint ugk = 0; ugk < ngk; ++ugk) {

    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint u = uln_r; u < SGEMM_NPM*SGEMM_NPK; u += SGEMM_NLN) {
      uint upm = u % SGEMM_NPM;
      uint upk = u / SGEMM_NPM;
      wmma_load_a(&la[upk][upm][0][ulm_r][ulane],
                  gA + upk*SGEMM_NTK*(NM/2U)
                     + upm*(SGEMM_NLTM/2U) + ulm_r*(SGEMM_NTM/2U)); }
    for (uint u = ulm; u < SGEMM_NPN*SGEMM_NPK; u += SGEMM_NLM) {
      uint upn = u % SGEMM_NPN;
      uint upk = u / SGEMM_NPN;
      wmma_load_b(&lb[upk][upn][0][uln][ulane],
                  gB + upk*SGEMM_NTK*(NN/2U)
                     + upn*(SGEMM_NLTN/2U) + uln*(SGEMM_NTN/2U)); }
    barrier(CLK_LOCAL_MEM_FENCE);

    for (uint upk = 0; upk < SGEMM_NPK; ++upk)
      for (uint upm = 0; upm < SGEMM_NPM; ++upm)
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          wmma_mma(&pD[upm][upn][0],
                   &la[upk][upm][0][ulm][ulane], &lb[upk][upn][0][uln][ulane]);
    gA += SGEMM_NPTK*(NM/2U);
    gB += SGEMM_NPTK*(NN/2U); }

  gC += ulm*SGEMM_NTM*NN + uln*SGEMM_NTN;
  for (uint upm = 0; upm < SGEMM_NPM; ++upm)
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      wmma_store(gC + upm*SGEMM_NLTM*NN + upn*SGEMM_NLTN, &pD[upm][upn][0]); }
#endif
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
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulA3)
      lA[u + ulA2][ulA1] = gA[(u + ulA2)*NM + ulA1];
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulB3)
      lB[u + ulB2][ulB1] = gB[(u + ulB2)*NN + ulB1];
    barrier(CLK_LOCAL_MEM_FENCE);

    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; ++ulpk) {
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = lB[ulpk][upn*SGEMM_NL + uln];
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = lA[ulpk][ulm*SGEMM_NPM + upm];
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn]; } }
    gA += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NM;
    gB += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NN; }

#define lC lB
  for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      lC[ulm][upn*SGEMM_NL + uln] = pC[upm][upn];
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM; u += ulB3)
      gC[(u + ulB2)*SGEMM_NPM*NN + upm*NN + ulB1] = lC[u + ulB2][ulB1]; } }
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
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulA3)
      lA[u + ulA2][ulA1] = gA[(u + ulA2)*NM + ulA1];
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; u += ulB3)
      lB[u + ulB2][ulB1] = gB[(u + ulB2)*NN + ulB1];
    barrier(CLK_LOCAL_MEM_FENCE);

    for (uint ulpk = 0; ulpk < SGEMM_NL*SGEMM_NLFM*SGEMM_NPK; ++ulpk) {
      for (uint upn = 0; upn < SGEMM_NPN; ++upn)
        pB[upn] = lB[ulpk][upn*SGEMM_NL + uln];
      for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
        pA = lA[ulpk][ulm*SGEMM_NPM + upm];
        for (uint upn = 0; upn < SGEMM_NPN; ++upn)
          pC[upm][upn] += pA * pB[upn]; } }
    gA += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NM;
    gB += SGEMM_NL*SGEMM_NLFM*SGEMM_NPK*NN; }

#define lC lB
  for (uint upm = 0; upm < SGEMM_NPM; ++upm) {
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint upn = 0; upn < SGEMM_NPN; ++upn)
      lC[ulm][upn*SGEMM_NL + uln] = pC[upm][upn];
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint u = 0; u < SGEMM_NL*SGEMM_NLFM; u += ulB3)
      gC[(u + ulB2)*SGEMM_NPM*NN + upm*NN + ulB1] = lC[u + ulB2][ulB1]; } }
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

const string code_change_dim = R"(
__kernel void to1(__global const float *p0, __global float *p1) {
  uint gid  = get_global_id(0);
  uint urow = gid / NM;
  uint ucol = gid % NM;
  p1[urow*NM1 + ucol] = p0[OFF0 + urow*NM0 + ucol]; }

__kernel void to0(__global float *p0, __global const float *p1) {
  uint gid  = get_global_id(0);
  uint urow = gid / NM;
  uint ucol = gid % NM;
  p0[OFF0 + urow*NM0 + ucol] = p1[urow*NM1 + ucol]; }
)";

const string code_resize_bias_ReLU = R"(
__kernel __attribute__((reqd_work_group_size(NN, NL, 1)))
void resize_bias_ReLU(__global const float *bias, __global const float *fin,
                      __global float *fout) {
  uint nn = get_global_id(0);
  uint nm = get_global_id(1);
  fout[nm*LD_OUT + nn] = fmax(0.0f, fin[nm*LD_IN + nn] + bias[nm]); }
)";

static uint ceil_power2(uint u) noexcept {
  uint u0;
  for (u0 = 1U; u0 < u; u0 *= 2U) assert(u0 <= UINT_MAX / 2U);
  return u0; }

//constexpr uint batch_bundle_size =  7U;
//constexpr uint AV_align          = 64U;
constexpr uint batch_bundle_size =  14U;
constexpr uint AV_align          = 128U;
constexpr uint partition_len_n() noexcept {
  return NNAux::ceil_multi(batch_bundle_size*ntile, AV_align); }
constexpr uint AV_local_size(uint bs) noexcept {
  return (bs < batch_bundle_size) ? bs : batch_bundle_size; }
constexpr uint AV_block_size(uint bs) noexcept {
  return NNAux::ceil_multi(AV_local_size(bs) * ntile, AV_align); }

static bool is_bs_allowed(uint bs) noexcept {
  uint major = bs / batch_bundle_size;
  uint minor = bs % batch_bundle_size;
  if (major == 0 && 0 < minor) return true;
  if (0 < major && minor == 0) return true;
  return false; }

static uint len_n(uint bs) noexcept {
  assert(is_bs_allowed(bs));
  uint major = bs / batch_bundle_size;
  uint minor = bs % batch_bundle_size;
  if (major == 0) return minor * ntile;
  return (major - 1U) * partition_len_n() + batch_bundle_size * ntile; }

static uint AV_num_groups(uint bs) noexcept {
  assert(is_bs_allowed(bs));
  uint major = bs / batch_bundle_size;
  if (major == 0) return 1U;
  return major; }

static tuple<string, uint, uint, uint, size_t, size_t, size_t, size_t>
gen_code_compute_matM(uint nm0, uint nn0, uint nk0, const SgemmParam &param)
  noexcept {
  assert(0 < nm0 && 0 < nn0 && 0 < nk0 && param.ok());
  if (param.do_wmma) {
    uint ntk = 16U;
    uint nm = NNAux::ceil_multi(nm0, param.nlm * param.npm * param.ntm);
    uint nn = NNAux::ceil_multi(nn0, param.nln * param.npn * param.ntn);
    uint nk = NNAux::ceil_multi(nk0, param.npk * ntk);
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
    str += "#define SGEMM_NLM     " + to_string(param.nlm)      + "U\n";
    str += "#define SGEMM_NLN     " + to_string(param.nln)      + "U\n";
    str += "#define SGEMM_NPM     " + to_string(param.npm)      + "U\n";
    str += "#define SGEMM_NPN     " + to_string(param.npn)      + "U\n";
    str += "#define SGEMM_NPK     " + to_string(param.npk)      + "U\n";
    str += "#define SGEMM_NTM     " + to_string(param.ntm)      + "U\n";
    str += "#define SGEMM_NTN     " + to_string(param.ntn)      + "U\n";
    str += "#define SGEMM_NTK     " + to_string(ntk)            + "U\n";
    str += "#define SGEMM_NPTM    (SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NPTN    (SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_NPTK    (SGEMM_NPK * SGEMM_NTK)\n";
    str += "#define SGEMM_NLTM    (SGEMM_NLM * SGEMM_NTM)\n";
    str += "#define SGEMM_NLTN    (SGEMM_NLN * SGEMM_NTN)\n";
    str += "#define SGEMM_NLPTM   (SGEMM_NLM * SGEMM_NPM * SGEMM_NTM)\n";
    str += "#define SGEMM_NLPTN   (SGEMM_NLN * SGEMM_NPN * SGEMM_NTN)\n";
    str += "#define SGEMM_TDM     " + str_tdm + "\n";
    str += code_compute_matM_wmma;
    return make_tuple(str, nm, nn, nk,
		      (nn / (param.npn*param.ntn)) * size_wrap_wmma,
		      nm / (param.npm*param.ntm),
		      param.nln * size_wrap_wmma, param.nlm); }
  else {
    uint nm = NNAux::ceil_multi(nm0, param.nl * param.nlfm * param.npm);
    uint nn = NNAux::ceil_multi(nn0, param.nl * param.npn);
    uint nk = NNAux::ceil_multi(nk0, param.nl * param.nlfm * param.npk);
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
{
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

static bool test_wmma(const OCL::Context &context) {
  assert(context.ok());
  try {
    OCL::Queue queue     = context.gen_queue();
    OCL::Program pg      = context.gen_program(R"(
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
    queue.push_ndrange_kernel(ker, 3, size_g, size_l); }
  catch (const ErrInt &e) {
    if (! strstr(e.what(), msg_opencl_error)) throw;
    return false; }
  catch (...) { throw; }
  return true; }

static OCL::Memory gen_mem_init(const OCL::Context &context, uint size,
				const float *ptr, bool to_half = false)
  noexcept {
  assert(context.ok() && ptr);
  OCL::Queue queue = context.gen_queue();
  if (to_half) {
    OCL::Memory mem_tmp = gen_mem_init(context, size, ptr);
    constexpr char str[] = R"(
__kernel void foo(__global const float *f, __global half *h) {
  vstore_half(f[get_global_id(0)], get_global_id(0), h); }
)";
    OCL::Memory mem = context.gen_mem_drw(sizeof(ushort) * size);
    OCL::Kernel ker = context.gen_program(str).gen_kernel("foo");
    ker.set_arg(0, mem_tmp);
    ker.set_arg(1, mem);
    queue.push_kernel(ker, size);
    return mem; }
  OCL::Memory mem = context.gen_mem_hw_dr(sizeof(float) * size);
  queue.push_write(mem, sizeof(float) * size, ptr);
  return mem; }

static OCL::Memory gen_mem_init(const OCL::Context &context, uint size,
				float value = 0.0f, bool to_half = false)
  noexcept {
  assert(context.ok());
  OCL::Queue queue = context.gen_queue();
  OCL::Memory mem;
  const char *pg;
  if (to_half) {
    mem = context.gen_mem_drw(sizeof(ushort) * size);
    pg = R"(
__kernel void foo(float value, __global half *h) {
  vstore_half(value, get_global_id(0), h); }
)"; }
  else {
    mem = context.gen_mem_drw(sizeof(float) * size);
    pg = R"(
__kernel void foo(float value, __global float *f) {
  f[get_global_id(0)] = value; }
)"; }

  OCL::Kernel ker = context.gen_program(pg).gen_kernel("foo");
  ker.set_arg(0, sizeof(float), &value);
  ker.set_arg(1, mem);
  queue.push_kernel(ker, size);
  return mem; }

static double measure_compute_matM(const OCL::Context &context,
				   const SgemmParam &param,
				   uint nbatch, uint nm0, uint nn0, uint nk0,
				   uint sample_size) {
  assert(context.ok() && param.ok());
  assert(0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0 && 0 < sample_size);
  size_t size_g[3], size_l[3];
  uint nm, nn, nk;
  string str;
  tie(str, nm, nn, nk, size_g[0], size_g[1], size_l[0], size_l[1])
    = gen_code_compute_matM(nm0, nn0, nk0, param);
  size_g[2] = nbatch;
  size_l[2] = 1U;
  OCL::Kernel ker   = context.gen_program(str).gen_kernel("compute_matM");
  OCL::Memory mem_a = gen_mem_init(context, nbatch * nm * nk, 0.01f,
				   param.do_half);
  OCL::Memory mem_b = gen_mem_init(context, nbatch * nk * nn, 0.02f,
				   param.do_half);
  OCL::Memory mem_c = context.gen_mem_drw(nbatch * nm * nn * sizeof(float));
  OCL::Queue queue  = context.gen_queue();
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
  int64_t elapsed = duration_cast<microseconds>(end - start).count();
  return (elapsed / static_cast<int64_t>(sample_size)); }

static SgemmParam tune_compute_matM(bool use_wmma, const OCL::Device &device,
				    const OCL::Context &context, uint nbatch,
				    uint nm0, uint nn0, uint nk0) {
  assert(device.ok() && context.ok());
  assert(0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);
  if (!use_wmma) {
    uint nlfmmax, nlstart;
    wgmax   = min(static_cast<uint>(device.gen_max_work_group_size()),
		  64U*64U);
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
    for (uint nlm = 1U; nlm <= 2U; nlm *= 2U)
      for (uint nln = 1U; nln <= 2U; nln *= 2U)
	for (uint npm = 1U; npm <= 4U; npm *= 2U)
	  for (uint npn = 1U; npn <= 4U; npn *= 2U)
	    for (uint npk = 1U; npk <= 2U; npk *= 2U)
	      for (auto t : { /*make_tuple(32U, 8U),*/
		  make_tuple(16U, 16U) /*make_tuple(8U, 32U)*/ }) {
		tie(ntm, ntn) = t;
		if (mmax < nlm*npm*ntm) continue;
		if (nmax < nln*npn*ntn) continue;
		if (kmax < npk*ntk) continue;
		if (npn*npk < nlm) continue;
		if (npm*npk < nln) continue;
		params.emplace_back(true, true,
				    nlm, nln, npm, npn, npk, ntm, ntn); } }

  double time_best = DBL_MAX;
  SgemmParam param_best;
  while (! params.empty()) {
    SgemmParam param = params.front();
    params.pop_front();
    double elapsed = DBL_MAX;
    bool flag_error = false;

    try {
      elapsed = measure_compute_matM(context, param, nbatch, nm0, nn0, nk0,
				     1U); }
    catch (const ErrInt &e) {
      if (! strstr(e.what(), msg_opencl_error)) throw;
      elapsed = DBL_MAX; flag_error = true; }
    catch (...) { throw; }

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

  uint sample_size = max(2U, static_cast<uint>(100000.0 / time_best));
  sample_size = min(sample_size, tune_sample_size_base * 4U);
  if (1U < sample_size) {
    time_best = DBL_MAX;
    while (! params_candi.empty()) {
      SgemmParam param = params_candi.front();
      params_candi.pop_front();
      double elapsed = measure_compute_matM(context, param, nbatch, nm0, nn0,
					    nk0, sample_size);
      if (time_best <= elapsed) continue;
      time_best  = elapsed;
      param_best = param; } }
  if (time_best == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  return param_best; }

string SgemmParam::gen_info() const noexcept {
  string s;
  if (do_wmma) {
    s += string("NLM:") + to_string(nlm) + string(" ");
    s += string("NLN:") + to_string(nln) + string(" ");
    s += string("NPM:") + to_string(npm) + string(" ");
    s += string("NPN:") + to_string(npn) + string(" ");
    s += string("NPK:") + to_string(npk) + string(" ");
    s += string("NTM:") + to_string(ntm) + string(" ");
    s += string("NTN:") + to_string(ntn) + string(" (");
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

void ManageDecode::start(const OCL::Context &context,
			 const OCL::MemPinned mem_in[NNAux::nslot],
			 const OCL::Memory mem_out[NNAux::nslot],
			 uint maxsize_batch) noexcept {
  assert(context.ok());

  uint index_block = NNAux::ceil_multi(maxsize_batch * NNAux::nch_input_fill,
				       32U);
  _zero_size = maxsize_batch * NNAux::nch_input * NNAux::size_plane;
  _fill_size_l[0] = _fill_size_g[0] = 81U;
  _fill_size_l[1] = _fill_size_l[2] = _fill_size_g[2] = 1U;
  _fill_size_g[1] = NNAux::nch_input_fill * maxsize_batch;

  string code = ("#define INDEX_BLOCK " + to_string(index_block) + "U\n"
		 "#define NB          " + to_string(maxsize_batch) + "U\n"
		 + code_decode);
  OCL::Program pg = context.gen_program(code);

  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_in[u].ok() && mem_out[u].ok());
    _ker_zero_clear[u] = pg.gen_kernel("zero_clear");
    _ker_zero_clear[u].set_arg(0, mem_out[u]);

    _ker_plane_fill[u] = pg.gen_kernel("plane_fill");
    _ker_plane_fill[u].set_arg(0, mem_in[u]);
    _ker_plane_fill[u].set_arg(1, mem_in[u]);
    _ker_plane_fill[u].set_arg(2, mem_out[u]);

    _ker_set_one[u] = pg.gen_kernel("set_one");
    _ker_set_one[u].set_arg(0, mem_in[u]);
    _ker_set_one[u].set_arg(1, mem_out[u]); } }

void ManageDecode::push(const OCL::Queue &queue, uint n_one, uint uslot)
  noexcept {
  assert(queue.ok());
  //nqueue.push_ndrange_kernel(_ker_zero_clear[uslot], 3, _zero_size_g,
  //_zero_size_l);
  queue.push_kernel(_ker_zero_clear[uslot], _zero_size);
  queue.push_ndrange_kernel(_ker_plane_fill[uslot], 3, _fill_size_g,
			    _fill_size_l);
  //_one_size_g[0] = n_one;
  //queue.push_ndrange_kernel(_ker_set_one[uslot], 3, _one_size_g,
  //_one_size_l);
  queue.push_kernel(_ker_set_one[uslot], n_one); }

void ManageComputeMatM::start(const OCL::Context &context, uint nbatch,
			      uint nm0, uint nn0, uint nk0,
			      const SgemmParam &param) noexcept {
  assert(context.ok() && 0 < nbatch && 0 < nm0 && 0 < nn0 && 0 < nk0
	 && param.ok());
  string str;
  tie(str, _nm, _nn, _nk, _size_g[0], _size_g[1], _size_l[0], _size_l[1])
    = gen_code_compute_matM(nm0, nn0, nk0, param);
  _size_g[2] = nbatch;
  _size_l[2] = 1U;
  OCL::Program pg = context.gen_program(str);
  for (uint u = 0; u < NNAux::nslot; ++u)
    _ker[u] = pg.gen_kernel("compute_matM"); }

void ManageComputeMatM::set(const OCL::Memory &mem_a,
			    const OCL::Memory mem_b[NNAux::nslot],
			    const OCL::Memory mem_c[NNAux::nslot]) noexcept {
  assert(mem_a.ok());
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_b[u].ok() && mem_c[u].ok());
    _ker[u].set_arg(0, mem_a);
    _ker[u].set_arg(1, mem_b[u]);
    _ker[u].set_arg(2, mem_c[u]); } }

void ManageComputeMatM::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  queue.push_ndrange_kernel(_ker[uslot], 3U, _size_g, _size_l); }

static double measure_sgemm(const OCL::Context &context,
			    const SgemmParam param, uint nm0, uint nn0,
			    uint nk0, uint sample_size) {
  assert(context.ok() && param.ok());
  assert(0 < nm0 && 0 < nn0 && 0 < nk0 && 0 < sample_size);
  uint nm = NNAux::ceil_multi(nm0, param.nl * param.nlfm * param.npm);
  uint nn = NNAux::ceil_multi(nn0, param.nl * param.npn);
  uint nk = NNAux::ceil_multi(nk0, param.nl * param.nlfm * param.npk);
  string str        = gen_code_sgemm(nm, nn, nk, param);
  OCL::Kernel ker   = context.gen_program(str).gen_kernel("sgemm");
  OCL::Memory mem_a = gen_mem_init(context, nm * nk, 0.01f);
  OCL::Memory mem_b = gen_mem_init(context, nk * nn, 0.02f);
  OCL::Memory mem_c = context.gen_mem_drw(nm * nn * sizeof(float));
  OCL::Queue queue  = context.gen_queue();
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
  int64_t elapsed = duration_cast<microseconds>(end - start).count();
  return (elapsed / static_cast<int64_t>(sample_size)); }

class MngChgDim {
  OCL::Memory _mem;
  OCL::Kernel _ker;
  uint _size;
  bool _done;
public:
  explicit MngChgDim() noexcept : _done(false) {}
  // move upper left nn x nm region from nn0 x nm0 matrix to nn1 x nm1 matrix
  //   if rev is false
  void reset(const OCL::Context &context, uint nn, uint nm, uint nm0,
	     uint off0, uint nn1, uint nm1, bool rev) noexcept {
    assert(context.ok());
    _mem = gen_mem_init(context, nm1 * nn1, 0.0f);
    _size = nn * nm;
    string str = ("#define NM   " + to_string(nm)   + "\n"
		  "#define NM0  " + to_string(nm0)  + "\n"
		  "#define NM1  " + to_string(nm1)  + "\n"
		  "#define OFF0 " + to_string(off0) + "\n" + code_change_dim);
    if (rev) _ker = context.gen_program(str).gen_kernel("to0");
    else     _ker = context.gen_program(str).gen_kernel("to1");
    _ker.set_arg(1, _mem); }
  void push(const OCL::Queue &queue) noexcept {
    assert(queue.ok()); queue.push_kernel(_ker, _size); _done = true; }
  template <typename M> void set(const M &mem) const noexcept {
    assert(mem.ok()); _ker.set_arg(0, mem); }
  const OCL::Memory &get() const noexcept { return _mem; }
  bool is_done() const noexcept { return _done; } };

void ManageSgemm::start(const OCL::Device &device, const OCL::Context &context,
			uint nker, bool, bool, uint nm0, uint nn0, uint nk0,
			uint offa, uint lda, uint offb, uint ldb, uint offc,
			uint ldc) {
  assert(device.ok() && context.ok() && 0 < nm0 && 0 < nn0 && 0 < nk0);
  assert(0 < lda && 0 < ldb && 0 < ldc);
  deque<SgemmParam> params, params_candi;
  uint wgmin, wgmax, nlstart, nlfmmax;
  uint mmax = ceil_power2(nm0);
  uint nmax = ceil_power2(nn0);
  uint kmax = ceil_power2(nk0);

  wgmax   = min(static_cast<uint>(device.gen_max_work_group_size()), 4096U);
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

  double time = DBL_MAX;
  while (! params.empty()) {
    SgemmParam param = params.front();
    params.pop_front();
    double elapsed = DBL_MAX;
    bool flag_error = false;
    try { elapsed = measure_sgemm(context, param, nm0, nn0, nk0, 1U); }
    catch (const ErrInt &e) {
      if (! strstr(e.what(), msg_opencl_error)) throw;
      elapsed = DBL_MAX; flag_error = true; }
    catch (...) { throw; }

    if (flag_error) {
      for (auto it = params.begin(); it != params.end(); ) {
	if (param <= *it) it = params.erase(it);
	else ++it; } }
    else {
      param.time = elapsed;
      params_candi.push_back(param);
      if (elapsed < time) { time = elapsed; _param = param; } } }
  if (time == DBL_MAX) die(ERR_INT("ManageComputeMatM() failed."));

  double time_max = min(time * 2.0 + 20.0, time + 5000.0);
  for (auto it = params_candi.begin(); it != params_candi.end(); ) {
    if (time_max <= it->time) it = params_candi.erase(it);
    else ++it; }
  
  uint sample_size = max(2U, static_cast<uint>(100000.0 / time));
  sample_size = min(sample_size, tune_sample_size_base);
  if (1U < sample_size) {
    time = DBL_MAX;
    while (! params_candi.empty()) {
      SgemmParam param = params_candi.front();
      params_candi.pop_front();
      double elapsed = measure_sgemm(context, param, nm0, nn0, nk0,
				     sample_size);
      if (time <= elapsed) continue;
      time  = elapsed;
      _param = param; } }
  if (time == DBL_MAX) die(ERR_INT("ManageSgemm() failed."));
  _param.time = time;

  _nker = nker;
  _nm0  = nm0;  _nn0  = nn0;  _nk0  = nk0;
  _lda  = lda;  _ldb  = ldb;  _ldc  = ldc;
  _offa = offa; _offb = offb; _offc = offc;
  _nm = NNAux::ceil_multi(nm0, _param.nl * _param.nlfm * _param.npm);
  _nn = NNAux::ceil_multi(nn0, _param.nl * _param.npn);
  _nk = NNAux::ceil_multi(nk0, _param.nl * _param.nlfm * _param.npk);
  size_g[0] = _nn / _param.npn;
  size_g[1] = _nm / _param.npm;
  size_g[2] = 1U;
  size_l[0] = _param.nl;
  size_l[1] = _param.nl * _param.nlfm;
  size_l[2] = 1U;

  string str = gen_code_sgemm(_nm, _nn, _nk, _param);
  OCL::Program pg = context.gen_program(str);
  _pker_sgemm.reset(new OCL::Kernel [nker]);
  for (uint u = 0; u < nker; ++u) _pker_sgemm[u] = pg.gen_kernel("sgemm"); }

void ManageSgemm::set_a(const OCL::Memory &mem) const noexcept {
  assert(mem.ok());
  for (uint u = 0; u < _nker; ++u) _pker_sgemm[u].set_arg(0, mem); }
void ManageSgemm::set_array_a(const OCL::Memory mem[]) const noexcept {
  for (uint u = 0; u < _nker; ++u) {
    assert(mem[u].ok());
    _pker_sgemm[u].set_arg(0, mem[u]); } }
void ManageSgemm::set_a_chg_dim(const OCL::Context &context,
				const OCL::Memory &mem) noexcept {
  assert(context.ok() && mem.ok());
  _pmng_chg_dim_a.reset(new MngChgDim);
  _pmng_chg_dim_a->reset(context, _nk0, _nm0, _lda, _offa, _nk, _nm, false);
  for (uint u = 0; u < _nker; ++u)
    _pker_sgemm[u].set_arg(0, _pmng_chg_dim_a->get());
  _pmng_chg_dim_a->set(mem); }
void ManageSgemm::chg_dim_a_beforhand(const OCL::Context &context) noexcept {
  assert(context.ok()); _pmng_chg_dim_a->push(context.gen_queue()); }

void ManageSgemm::set_b(const OCL::Memory &mem) const noexcept {
  assert(mem.ok());
  for (uint u = 0; u < _nker; ++u) _pker_sgemm[u].set_arg(1, mem); }
void ManageSgemm::set_array_b(const OCL::Memory mem[]) const noexcept {
  for (uint u = 0; u < _nker; ++u) {
    assert(mem[u].ok());
    _pker_sgemm[u].set_arg(1, mem[u]); } }
void ManageSgemm::set_b_chg_dim(const OCL::Context &context,
				const OCL::Memory &mem) noexcept {
  assert(context.ok() && mem.ok());
  _pmng_chg_dim_b.reset(new MngChgDim);
  _pmng_chg_dim_b->reset(context, _nk0, _nn0, _ldb, _offb, _nk, _nn, false);
  for (uint u = 0; u < _nker; ++u)
    _pker_sgemm[u].set_arg(1, _pmng_chg_dim_b->get());
  _pmng_chg_dim_b->set(mem); }
void ManageSgemm::chg_dim_b_beforhand(const OCL::Context &context) noexcept {
  assert(context.ok()); _pmng_chg_dim_b->push(context.gen_queue()); }

template <typename M> void ManageSgemm::set_array_c(const M mem[])
  const noexcept {
  for (uint u = 0; u < _nker; ++u) {
    assert(mem[u].ok());
    _pker_sgemm[u].set_arg(2, mem[u]); } }
template <typename M>
void ManageSgemm::set_array_c_chg_dim(const OCL::Context &context,
				      const M mem[]) noexcept {
  assert(context.ok());
  _array_mng_chg_dim_c.reset(new MngChgDim [_nker]);
  for (uint u = 0; u < _nker; ++u) {
    assert(mem[u].ok());
    _array_mng_chg_dim_c[u].reset(context, _nm0, _nn0, _ldc, _offc, _nm, _nn,
				  true);
    _pker_sgemm[u].set_arg(2, _array_mng_chg_dim_c[u].get());
    _array_mng_chg_dim_c[u].set(mem[u]); } }

void ManageSgemm::push(const OCL::Queue &queue, uint uker) const noexcept {
  assert(queue.ok() && uker < _nker);
  if (_pmng_chg_dim_a && !_pmng_chg_dim_a->is_done())
    _pmng_chg_dim_a->push(queue);
  if (_pmng_chg_dim_b && !_pmng_chg_dim_b->is_done())
    _pmng_chg_dim_b->push(queue);
  queue.push_ndrange_kernel(_pker_sgemm[uker], 3U, size_g, size_l);
  if (_pmng_chg_dim_c) _pmng_chg_dim_c->push(queue);
  if (_array_mng_chg_dim_c) _array_mng_chg_dim_c[uker].push(queue); }

string ManageSgemm::gen_info() const noexcept {
  string s;
  s += string("NL:")   + to_string(_param.nl)   + string(" ");
  s += string("NLFM:") + to_string(_param.nlfm) + string(" ");
  s += string("NPM:")  + to_string(_param.npm)  + string(" ");
  s += string("NPN:")  + to_string(_param.npn)  + string(" ");
  s += string("NPK:")  + to_string(_param.npk)  + string(" (");
  s += to_string(static_cast<uint>(_param.time)) + string("us)");
  return s; }

void ManageComputeMatV::start(bool store_half, const OCL::Context &context,
			      uint nch, uint nb, uint nn, uint nk,
			      const OCL::Memory mem_in[NNAux::nslot],
			      const OCL::Memory mem_matV[NNAux::nslot])
  noexcept {
  assert(context.ok() && is_bs_allowed(nb));
  _size_l[0] = ntile;
  _size_l[1] = AV_local_size(nb);
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch;

  string code;
  if (store_half) code += "#define STORE_HALF\n";
  code += ("#define NBWS         " + to_string(AV_local_size(nb)) + "U\n"
	   "#define BLOCK_SIZE   " + to_string(AV_block_size(nb)) + "U\n"
	   "#define NB_PARTITION " + to_string(partition_len_n()) + "U\n"
	   "#define NB           " + to_string(nb) + "U\n"
	   "#define NK           " + to_string(nk) + "U\n"
	   "#define NN           " + to_string(nn) + "U\n"
	   + code_common + code_compute_matV_child + code_compute_matV);
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_in[u].ok() && mem_matV[u].ok());
    _ker[u] = pg.gen_kernel("compute_matV");
    _ker[u].set_arg(0, mem_in[u]);
    _ker[u].set_arg(1, mem_matV[u]); } }

void ManageComputeMatV::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok());
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

void ManageComputeMatA::start(bool load_half, const OCL::Context &context,
			      bool flag_join, uint nch, uint nb, uint nm,
			      uint nn, uint nn_out,
			      const OCL::Memory mem_matM[NNAux::nslot],
			      const OCL::Memory mem_bypass[NNAux::nslot],
			      const OCL::Memory &mean,
			      const OCL::Memory &sd_inv,
			      const OCL::Memory mem_out[NNAux::nslot])
  noexcept {
  assert(context.ok() && mean.ok() && sd_inv.ok() && is_bs_allowed(nb));
  _size_l[0] = ntile;
  _size_l[1] = AV_local_size(nb);
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch;

  string code;
#if WMMA_ACCUMU16 == 1
  if (load_half) code += "#define LOAD_HALF\n";
#else
  (void)load_half;
#endif
  if (flag_join) code += "#define DO_JOIN\n";
  code += ("#define NBWS         " + to_string(AV_local_size(nb)) + "U\n"
	   "#define BLOCK_SIZE   " + to_string(AV_block_size(nb)) + "U\n"
	   "#define NB_PARTITION " + to_string(partition_len_n()) + "U\n"
	   "#define NB           " + to_string(nb) + "\n"
	   "#define NM           " + to_string(nm) + "\n"
	   "#define NN           " + to_string(nn) + "\n"
	   "#define NN_OUT       " + to_string(nn_out) + "\n"
	   + code_common + code_compute_matA_child + code_compute_matA);

  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_matM[u].ok() && mem_bypass[u].ok() && mem_out[u].ok());
    _ker[u] = pg.gen_kernel("compute_matA_BNReLU");
    _ker[u].set_arg(0, mem_matM[u]);
    _ker[u].set_arg(1, mean);
    _ker[u].set_arg(2, sd_inv);
    _ker[u].set_arg(3, mem_bypass[u]);
    _ker[u].set_arg(4, mem_out[u]); } }

void ManageComputeMatA::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok());
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

void ManageComputeMatAV::start(bool do_half, bool do_join, bool do_fork,
			       const OCL::Context &context, uint nch, uint nb,
			       uint nm, uint nn, uint nk,
			       const OCL::Memory mem_matM[NNAux::nslot],
			       const OCL::Memory mem_matV[NNAux::nslot],
			       const OCL::Memory &mean,
			       const OCL::Memory &sd_inv,
			       const OCL::Memory *mem_bypass) noexcept {
  assert(context.ok() && mean.ok() && sd_inv.ok() && is_bs_allowed(nb));
  _size_l[0] = ntile;
  _size_l[1] = AV_local_size(nb);
  _size_l[2] = 1U;
  _size_g[0] = ntile;
  _size_g[1] = nb;
  _size_g[2] = nch;

  string code;
#if WMMA_ACCUMU16 == 1
  if (do_half) code += "#define LOAD_HALF\n";
#endif
  if (do_half) code += "#define STORE_HALF\n";
  if (do_join) code += "#define DO_JOIN\n";
  if (do_fork) code += "#define DO_FORK\n";
  code += ("#define NBWS         " + to_string(AV_local_size(nb)) + "U\n"
	   "#define BLOCK_SIZE   " + to_string(AV_block_size(nb)) + "U\n"
	   "#define NB_PARTITION " + to_string(partition_len_n()) + "U\n"
	   "#define NB           " + to_string(nb) + "U\n"
	   "#define NM           " + to_string(nm) + "U\n"
	   "#define NN           " + to_string(nn) + "U\n"
	   "#define NK           " + to_string(nk) + "U\n"
	   + code_common + code_compute_matV_child + code_compute_matA_child
	   + code_compute_matAV);
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_matM[u].ok() && mem_matV[u].ok());
    _ker[u] = pg.gen_kernel("compute_matAV");
    _ker[u].set_arg(0, mem_matM[u]);
    _ker[u].set_arg(1, mean);
    _ker[u].set_arg(2, sd_inv);
    _ker[u].set_arg(3, mem_matV[u]);
    if (mem_bypass) {
      assert(mem_bypass[u].ok());
      _ker[u].set_arg(4, mem_bypass[u]); } } }

void ManageComputeMatAV::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

void ManageComputeBNReLU::start(const OCL::Context &context, uint nch, uint nb,
				uint nn_in, const OCL::Memory &mean,
				const OCL::Memory &sd_inv,
				const OCL::Memory mem_in[NNAux::nslot],
				const OCL::Memory mem_out[NNAux::nslot])
  noexcept {
  assert(context.ok() && mean.ok() && sd_inv.ok());
  _size_l[0] = NNAux::size_plane;
  _size_l[1] = 1U;
  _size_l[2] = 1U;
  _size_g[0] = NNAux::size_plane;
  _size_g[1] = nb;
  _size_g[2] = nch;
  string code = ("#define NB    " + to_string(nb)    + "U\n"
		 "#define NN_IN " + to_string(nn_in) + "U\n" + code_BNReLU);
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_in[u].ok() && mem_out[u].ok());
    _ker[u] = pg.gen_kernel("compute_BNReLU");
    _ker[u].set_arg(0, mean);
    _ker[u].set_arg(1, sd_inv);
    _ker[u].set_arg(2, mem_in[u]);
    _ker[u].set_arg(3, mem_out[u]); } }

void ManageComputeBNReLU::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

void ManageResizeBiasReLU::start(const OCL::Context &context, uint nm, uint nn,
				 uint ldin, uint ldout,
				 const OCL::Memory &mem_bias,
				 const OCL::Memory mem_in[NNAux::nslot],
				 const OCL::Memory mem_out[NNAux::nslot])
  noexcept {
  assert(context.ok() && 0 < nm && 0 < nn && 0 < ldin && 0 < ldout);
  assert(mem_bias.ok());
  uint nl;
  for (nl = 16U; nm % nl; nl /= 2U);
  _size_l[0] = nn;
  _size_l[1] = nl;
  _size_l[2] = 1U;
  _size_g[0] = nn;
  _size_g[1] = nm;
  _size_g[2] = 1U;

  string code = ("#define NN     " + to_string(nn)    + "U\n"
		 "#define NL     " + to_string(nl)    + "U\n"
		 "#define LD_IN  " + to_string(ldin)  + "U\n"
		 "#define LD_OUT " + to_string(ldout) + "U\n"
		 + code_resize_bias_ReLU);
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_in[u].ok() && mem_out[u].ok());
    _ker[u] = pg.gen_kernel("resize_bias_ReLU");
    _ker[u].set_arg(0, mem_bias);
    _ker[u].set_arg(1, mem_in[u]);
    _ker[u].set_arg(2, mem_out[u]); } }

void ManageResizeBiasReLU::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

void ManageComputePolicy::start(const OCL::Context &context, uint nch_in,
				uint maxsize_batch,
				const OCL::MemPinned mem_nnmoves[NNAux::nslot],
				const OCL::Memory &mem_weight,
				const OCL::Memory &mem_bias,
				const OCL::Memory mem_in[NNAux::nslot],
				const OCL::MemPinned mem_out[NNAux::nslot])
  noexcept {
  assert(context.ok() && mem_weight.ok() && mem_bias.ok());
  string code =
    "#define NCH_IN " + to_string(nch_in)        + "U\n"
    "#define NB     " + to_string(maxsize_batch) + "U\n"
    + code_compute_policy;
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_nnmoves[u].ok() && mem_in[u].ok() && mem_out[u].ok());
    _ker[u] = pg.gen_kernel("compute_policy");
    _ker[u].set_arg(0, mem_nnmoves[u]);
    _ker[u].set_arg(2, mem_weight);
    _ker[u].set_arg(3, mem_bias);
    _ker[u].set_arg(4, mem_in[u]);
    _ker[u].set_arg(5, mem_out[u]); } }

void ManageComputePolicy::push(const OCL::Queue &queue, uint ntot_moves,
			       uint offin, uint uslot) const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  if (ntot_moves == 0) return;
  _ker[uslot].set_arg(1, sizeof(offin), &offin);
  queue.push_kernel(_ker[uslot], ntot_moves); }

void ManageTransformValue2::start(const OCL::Context &context, uint nch,
				  uint nb,
				  const OCL::Memory mem_in[NNAux::nslot],
				  uint offin, uint nn_out,
				  const OCL::Memory mem_out[NNAux::nslot])
  noexcept {
  assert(context.ok());
  _size_l[0] = NNAux::size_plane;
  _size_l[1] = 1U;
  _size_l[2] = 1U;
  _size_g[0] = NNAux::size_plane;
  _size_g[1] = nb;
  _size_g[2] = nch;
  string code =
    "#define NN_OUT " + to_string(nn_out) + "U\n"
    "#define NCH    " + to_string(nch)    + "U\n"
    "#define NBATCH " + to_string(nb)     + "U\n" + code_transform_value2;
  OCL::Program pg = context.gen_program(code);
  for (uint u = 0; u < NNAux::nslot; ++u) {
    assert(mem_in[u].ok() && mem_out[u].ok());
    _ker[u] = pg.gen_kernel("transform_value2");
    _ker[u].set_arg(0, mem_in[u]);
    _ker[u].set_arg(1, sizeof(offin), &offin);
    _ker[u].set_arg(2, mem_out[u]); } }

void ManageTransformValue2::push(const OCL::Queue &queue, uint uslot)
  const noexcept {
  assert(queue.ok() && uslot < NNAux::nslot);
  queue.push_ndrange_kernel(_ker[uslot], 3, _size_g, _size_l); }

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
  assert(bias && mean);
  row_t row(new float [nch]);
  for (uint ch = 0; ch < nch; ++ch) row[ch] = mean[ch] * bn_factor - bias[ch];
  return row; }

static row_t gen_sd_inv(uint nch, const float *sd) noexcept {
  assert(sd);
  row_t row(new float [nch]);
  for (uint ch = 0; ch < nch; ++ch)
    row[ch] = 1.0f / std::sqrt(sd[ch] * bn_factor + bn_eps);
  return row; }

static row_t gen_head1_trans_weight(uint nout1, uint nout2, uint nin,
				    const float *w1, const float *w2)
  noexcept {
  assert(w1 && w2);
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
  assert(bias1&& mean1 && bias2 && mean2);
  row_t row(new float [nch1 + nch2]);
  for (uint ch = 0; ch < nch1; ++ch)
    row[ch] = mean1[ch] * bn_factor - bias1[ch];
  for (uint ch = 0; ch < nch2; ++ch)
    row[nch1 + ch] = mean2[ch] * bn_factor - bias2[ch];
  return row; }

static row_t gen_head1_sd_inv(uint nch1, uint nch2, const float *sd1,
			      const float *sd2) noexcept {
  assert(sd1 && sd2);
  row_t row(new float [nch1 + nch2]);
  for (uint ch = 0; ch < nch1; ++ch)
    row[ch] = 1.0f / std::sqrt(sd1[ch] * bn_factor + bn_eps);
  for (uint ch = 0; ch < nch2; ++ch)
    row[nch1 + ch] = 1.0f / std::sqrt(sd2[ch] * bn_factor + bn_eps);
  return row; }

static row_t gen_matrix_trans(uint nm, uint nn, const float *p) noexcept {
  assert(p);
  row_t row(new float [nm * nn]);
  for (uint um = 0; um < nm; ++um)
    for (uint un = 0; un < nn; ++un) row[un*nm + um] = p[um*nn + un];
  return row; }

static void compute_probs(uint nb, const uint *sizes_nnmove, const float *fin,
			  float *probs) noexcept {
  assert(sizes_nnmove && fin && probs);
  for (uint ub = 0; ub < nb; ++ub) {
    float *probs_b = probs + ub * NNAux::nmove;
    copy_n(fin, sizes_nnmove[ub], probs_b);
    NNAux::softmax(sizes_nnmove[ub], probs_b);
    fin += sizes_nnmove[ub]; } }

static void compress_data(uint nb0, uint nb, const float *in,
			  const uint *sizes_nnmove, const ushort *nnmoves,
			  void *out, size_t &size_write, uint &n_one,
			  uint &ntot_moves, uint &index_moves) noexcept {
  assert(in && sizes_nnmove && nnmoves && out);
  float *pvalue = static_cast<float *>(out);
  uint  *pindex = static_cast<uint *>(out) + NNAux::fill_block_size(nb);
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

  pindex = static_cast<uint *>(out) + NNAux::fill_block_size(nb)*2U;
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

  index_moves = NNAux::fill_block_size(nb)*2U + NNAux::ceil_multi(n_one, 32U);
  pindex = static_cast<uint *>(out) + index_moves;
  ntot_moves = 0;
  for (uint ub = 0; ub < nb0; ++ub)
    for (uint unn = 0; unn < sizes_nnmove[ub]; ++unn) {
      uint nnmove = nnmoves[ub * NNAux::nmove + unn];
      ntot_moves += 1U;
      assert(nnmove < NNAux::nch_out_policy * NNAux::size_plane);
      *pindex++ = ub * NNAux::nch_out_policy * NNAux::size_plane + nnmove; }
  
  size_write = (index_moves + ntot_moves) * sizeof(uint); }

NNetOCL::NNetOCL() noexcept : _pool1_slot_size(NNAux::nslot) {}

string NNetOCL::reset(uint maxsize_batch,
		      const vector<pair<uint, row_t>> &wght, int device_id,
		      bool use_half, bool flag_out, bool do_sleep) noexcept {

  //
  // setup thread objects
  //
  lock_guard<mutex> lock_pool1(_m_pool1_slot);
  assert(0 < maxsize_batch);
  if (_pool1_slot_size != NNAux::nslot) die(ERR_INT("Internal Error"));

  _do_sleep = do_sleep;
  _elapsed_wait_ff = 0;
  for (uint uslot = 0; uslot < NNAux::nslot; ++uslot) {
    _pool1_slots[NNAux::nslot - uslot - 1U] = uslot;
    _slots_sizes_nnmove[uslot].reset(new uint [maxsize_batch]); }

  //
  // compute weight dimension
  //
  if (! is_bs_allowed(maxsize_batch))
    die(ERR_INT("bad batch size %u", maxsize_batch));
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
  uint resnet_nout = wght[1].first;
  uint nin         = NNAux::nch_input;
  uint index       = 0;
  _nres_block      = nrow_res / 4U;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (wght[index].first != resnet_nout * nin * size_kernel
	|| wght[index + 1U].first != resnet_nout
	|| wght[index + 2U].first != resnet_nout
	|| wght[index + 3U].first != resnet_nout)
      die(ERR_INT(msg_bad_wght_dim));
    nin = resnet_nout;
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
  uint policy1_nout = wght[index + 1U].first;
  uint value1_nout  = wght[index + 7U].first;
  uint head1_nout   = policy1_nout + value1_nout;
  nin = resnet_nout;
  if (wght[index].first != policy1_nout * nin
      || wght[index + 2U].first != policy1_nout
      || wght[index + 3U].first != policy1_nout
      || wght[index + 6U].first != value1_nout * nin
      || wght[index + 8U].first != value1_nout
      || wght[index + 9U].first != value1_nout)
    die(ERR_INT(msg_bad_wght_dim));

  // load policy2 part (conv 1x1)
  // index + 4: weight
  // index + 5: bias
  uint policy2_nin = policy1_nout;
  if (wght[index + 4U].first != NNAux::nch_out_policy * policy2_nin
      || wght[index + 5U].first != NNAux::nch_out_policy)
    die(ERR_INT(msg_bad_wght_dim));

  // load value2 part (fc)
  // index + 10: weight
  // index + 11: bias
  uint value2_nin  = value1_nout * NNAux::size_plane;
  uint value2_nout = wght[index + 11U].first;
  if (wght[index + 10U].first != value2_nout * value2_nin)
    die(ERR_INT(msg_bad_wght_dim));

  // load value3 part (fc)
  // index + 12: weight
  // index + 13: bias
  uint value3_nin  = value2_nout;
  uint value3_nout = wght[index + 13U].first;
  if (wght[index + 12U].first != value3_nout * value3_nin
      || value3_nout != 1) die(ERR_INT(msg_bad_wght_dim));

  //
  // setup opencl device, context and queues
  //
  OCL::Device device;
  if (0 <= device_id) {
    get_device(device_id, device);
    if (!device.ok()) die(ERR_INT("bad device ID %u", device_id)); }
  else {
    get_best_device(device);
    if (!device.ok()) die(ERR_INT("no device found")); }

  stringstream lines;
  lines << "- Device ID: " << device_id << "\n";
  lines << device.gen_info();

  bool use_wmma = false;
  OCL::Context context = device.gen_context();
  if (use_half) {
    use_wmma = test_wmma(context);
    lines << "  Wmma Support:         " << (use_wmma ? "Yes\n" : "No\n"); }

  for (uint u = 0; u < NNAux::nslot; ++u) _queue_a[u] = context.gen_queue();

  //
  // compute sgemm dimensions
  //
  SgemmParam param_matM
    = tune_compute_matM(use_wmma, device, context, size_tile_in,
			resnet_nout, len_n(maxsize_batch), resnet_nout);
  lines << "  Matrix M:             ";
  lines << param_matM.gen_info() << "\n";
  _pmng_compute_matM.reset(new ManageComputeMatM [_nres_block + 1U]);
  _pmng_compute_matM[0].start(context, size_tile_in, resnet_nout,
			      len_n(maxsize_batch), NNAux::nch_input,
			      param_matM);
  for (uint u = 1U; u < _nres_block + 1U; ++u)
    _pmng_compute_matM[u].start(context, size_tile_in, resnet_nout,
				len_n(maxsize_batch), resnet_nout,
				param_matM);

  lines << "  Head 1:               ";
  _mng_head1.start(device, context, NNAux::nslot, true, false, head1_nout,
		   maxsize_batch * NNAux::size_plane, resnet_nout, 0,
		   head1_nout, 0, maxsize_batch * NNAux::size_plane, 0,
		   maxsize_batch * NNAux::size_plane);
  lines << _mng_head1.gen_info() << "\n";

  lines << "  Value 2:              ";
  _mng_value2.start(device, context, NNAux::nslot, true, false, value2_nout,
		    maxsize_batch, value2_nin, 0, value2_nout, 0,
		    maxsize_batch, 0, maxsize_batch);
  lines << _mng_value2.gen_info() << "\n";

  lines << "  Value 3:              ";
  _mng_value3.start(device, context, NNAux::nslot, true, false,
		    maxsize_batch, 1U, value3_nin, 0, maxsize_batch, 0, 1U, 0,
		    1U);
  lines << _mng_value3.gen_info() << "\n";

  uint size_decode     = maxsize_batch * NNAux::nch_input * NNAux::size_plane;
  uint size_bypass     = (AV_block_size(maxsize_batch) * ntile
			  * AV_num_groups(maxsize_batch) * resnet_nout);
  uint sizeV_input     = (_pmng_compute_matM[0].get_nk()
			  * _pmng_compute_matM[0].get_nn() * size_tile_in);
  uint sizeV           = (_pmng_compute_matM[1].get_nk()
			  * _pmng_compute_matM[0].get_nn() * size_tile_in);
  uint sizeM           = (_pmng_compute_matM[0].get_nm()
			  * _pmng_compute_matM[0].get_nn() * size_tile_in);
  uint size_resout     = _mng_head1.get_nk() * _mng_head1.get_nn();
  uint size_head1_out  = _mng_head1.get_nm() * _mng_head1.get_nn();
  uint size_BNReLU_out = maxsize_batch * head1_nout * 128U;
  uint size_transform  = _mng_value2.get_nk() * _mng_value2.get_nn();
  uint size_value2_out = _mng_value2.get_nm() * _mng_value2.get_nn();
  uint size_value3_in  = _mng_value3.get_nm() * _mng_value3.get_nk();

  //
  // setup opencl memory buffers and kernels
  //
  nin   = NNAux::nch_input;
  index = 0;
  for (uint u = 0; u < 1U + _nres_block; ++u) {
    uint nm     = _pmng_compute_matM[u].get_nm();
    uint nk     = _pmng_compute_matM[u].get_nk();
    uint sizeU  = size_tile_in * nm * nk;
    row_t matU  = gen_matU(resnet_nout, nin, wght[index].second.get(), nm, nk);
    row_t mean  = gen_mean(resnet_nout, wght[index + 1U].second.get(),
			   wght[index + 2U].second.get());
    row_t sdinv = gen_sd_inv(resnet_nout, wght[index + 3U].second.get());
    OCL::Memory cl_matU  = gen_mem_init(context, sizeU, matU.get(), use_wmma);
    OCL::Memory cl_mean  = gen_mem_init(context, resnet_nout, mean.get());
    OCL::Memory cl_sdinv = gen_mem_init(context, resnet_nout, sdinv.get());
    _vec_reswghts.push_back({move(cl_matU), move(cl_mean), move(cl_sdinv)});
    nin = resnet_nout;
    index += 4U; }

  for (uint u = 0; u < NNAux::nslot; ++u) {
    _mem_decode[u]     = gen_mem_init(context, size_decode, 0.0f);
    _mem_matV_input[u] = gen_mem_init(context, sizeV_input, 0.0f, use_wmma);
    _mem_matV[u]       = gen_mem_init(context, sizeV, 0.0f, use_wmma);
    _mem_matM[u]       = gen_mem_init(context, sizeM, 0.0f);
    _mem_bypass[u]     = gen_mem_init(context, size_bypass, 0.0f);
    _mem_resout[u]     = gen_mem_init(context, size_resout, 0.0f);
    _mem_head1_out[u]  = gen_mem_init(context, size_head1_out, 0.0f);
    _mem_BNReLU_out[u] = gen_mem_init(context, size_BNReLU_out, 0.0f);
    _mem_transform[u]  = gen_mem_init(context, size_transform, 0.0f);
    _mem_value2_out[u] = gen_mem_init(context, size_value2_out, 0.0f);
    _mem_value3_in[u]  = gen_mem_init(context, size_value3_in, 0.0f);
    _mem_in[u]         = context.gen_mem_pin_hw_dr(sizeof(uint)
						   * 2048U * maxsize_batch);
    _mem_out[u]        = context.gen_mem_pin_hr_dw(sizeof(float)
						   * (1U + NNAux::nmove)
						   * maxsize_batch); }
  
  row_t row = gen_head1_trans_weight(policy1_nout, value1_nout, resnet_nout,
				     wght[index + 0U].second.get(),
				     wght[index + 6U].second.get());
  OCL::Memory mem_head1_wght
    = gen_mem_init(context, resnet_nout * head1_nout, row.get());
  
  row = gen_head1_mean(policy1_nout, value1_nout,
		       wght[index + 1U].second.get(),
		       wght[index + 2U].second.get(),
		       wght[index + 7U].second.get(),
		       wght[index + 8U].second.get());
  _mem_head1_mean = gen_mem_init(context, head1_nout, row.get());

  row = gen_head1_sd_inv(policy1_nout, value1_nout,
			 wght[index + 3U].second.get(),
			 wght[index + 9U].second.get());
  _mem_head1_sd_inv = gen_mem_init(context, head1_nout, row.get());
  
  _mem_policy2_wght = gen_mem_init(context,
				   NNAux::nch_out_policy * policy2_nin,
				   wght[index + 4U].second.get());
  
  _mem_policy2_bias = gen_mem_init(context, NNAux::nch_out_policy,
				   wght[index + 5U].second.get());

  row = gen_matrix_trans(value2_nout, value2_nin,
			 wght[index + 10U].second.get());
  OCL::Memory mem_value2_wght
    = gen_mem_init(context, value2_nout * value2_nin, row.get());

  _mem_value2_bias = gen_mem_init(context, value2_nout,
				  wght[index + 11U].second.get());
  OCL::Memory mem_value3_wght
    = gen_mem_init(context, value3_nin, wght[index + 12U].second.get());
  _value3_bias.reset(new float [value3_nout]);
  copy_n(wght[index + 13U].second.get(), value3_nout, _value3_bias.get());


  _pmng_compute_matAV.reset(new ManageComputeMatAV [_nres_block]);
  _mng_decode.start(context, _mem_in, _mem_decode, maxsize_batch);
  _mng_compute_matV_first.start(use_wmma, context, NNAux::nch_input,
				maxsize_batch,
				_pmng_compute_matM[0].get_nn(),
				_pmng_compute_matM[0].get_nk(),
				_mem_decode, _mem_matV_input);
  _pmng_compute_matM[0].set(_vec_reswghts[0].matU, _mem_matV_input, _mem_matM);
  _pmng_compute_matAV[0].start(use_wmma, false, true, context, resnet_nout,
			       maxsize_batch, _pmng_compute_matM[1].get_nm(),
			       _pmng_compute_matM[1].get_nn(),
			       _pmng_compute_matM[1].get_nk(),
			       _mem_matM, _mem_matV, _vec_reswghts[0].mean,
			       _vec_reswghts[0].sd_inv, _mem_bypass);
  uint u = 1U;
  for (; u + 1U < _nres_block; u += 2U) {
    _pmng_compute_matM[u].set(_vec_reswghts[u].matU, _mem_matV, _mem_matM);
    _pmng_compute_matAV[u].start(use_wmma, false, false, context, resnet_nout,
				 maxsize_batch, _pmng_compute_matM[1].get_nm(),
				 _pmng_compute_matM[1].get_nn(),
				 _pmng_compute_matM[1].get_nk(),
				 _mem_matM, _mem_matV, _vec_reswghts[u].mean,
				 _vec_reswghts[u].sd_inv, nullptr);

    _pmng_compute_matM[u+1].set(_vec_reswghts[u+1].matU, _mem_matV, _mem_matM);
    _pmng_compute_matAV[u+1].start(use_wmma, true, true, context,
				   resnet_nout, maxsize_batch,
				   _pmng_compute_matM[1].get_nm(),
				   _pmng_compute_matM[1].get_nn(),
				   _pmng_compute_matM[1].get_nk(),
				   _mem_matM, _mem_matV,
				   _vec_reswghts[u+1].mean,
				   _vec_reswghts[u+1].sd_inv, _mem_bypass); }

  _pmng_compute_matM[u].set(_vec_reswghts[u].matU, _mem_matV, _mem_matM);
  _pmng_compute_matAV[u].start(use_wmma, false, false, context,
			       resnet_nout, maxsize_batch,
			       _pmng_compute_matM[1].get_nm(),
			       _pmng_compute_matM[1].get_nn(),
			       _pmng_compute_matM[1].get_nk(),
			       _mem_matM, _mem_matV, _vec_reswghts[u].mean,
			       _vec_reswghts[u].sd_inv, nullptr);

  _pmng_compute_matM[u+1].set(_vec_reswghts[u+1].matU, _mem_matV, _mem_matM);
  _mng_compute_matA_last.start(use_wmma, context, true, resnet_nout,
			       maxsize_batch, _pmng_compute_matM[1].get_nm(),
			       _pmng_compute_matM[1].get_nn(),
			       _mng_head1.get_nn(), _mem_matM, _mem_bypass,
			       _vec_reswghts[u+1].mean,
			       _vec_reswghts[u+1].sd_inv, _mem_resout);
  
  _mng_head1.set_a_chg_dim(context, mem_head1_wght);
  _mng_head1.chg_dim_a_beforhand(context);
  _mng_head1.set_array_b(_mem_resout);
  _mng_head1.set_array_c(_mem_head1_out);

  _mng_compute_BNReLU.start(context, head1_nout, maxsize_batch,
			    _mng_head1.get_nn(), _mem_head1_mean,
			    _mem_head1_sd_inv, _mem_head1_out,
			    _mem_BNReLU_out);

  _mng_compute_policy.start(context, policy2_nin, maxsize_batch,
			    _mem_in, _mem_policy2_wght, _mem_policy2_bias,
			    _mem_BNReLU_out, _mem_out);

  _mng_transform_value2.start(context, value1_nout, maxsize_batch,
			      _mem_BNReLU_out,
			      policy1_nout * maxsize_batch * 128U,
			      _mng_value2.get_nn(), _mem_transform);

  _mng_value2.set_a_chg_dim(context, mem_value2_wght);
  _mng_value2.chg_dim_a_beforhand(context);
  _mng_value2.set_array_b(_mem_transform);
  _mng_value2.set_array_c(_mem_value2_out);

  _mng_resize_bias_ReLU_value3.start(context, value2_nout, maxsize_batch,
				     _mng_value2.get_nn(),
				     _mng_value3.get_nm(),
				     _mem_value2_bias, _mem_value2_out,
				     _mem_value3_in);

  _mng_value3.set_array_a(_mem_value3_in);
  _mng_value3.set_b_chg_dim(context, mem_value3_wght);
  _mng_value3.chg_dim_b_beforhand(context);
  _mng_value3.set_array_c_chg_dim(context, _mem_out);

  if (flag_out) cout << lines.str() << std::flush;
  return lines.str(); }

// feed forward neural network
void NNetOCL::ff(uint size_batch, const float *input, const uint *sizes_nnmove,
		 const ushort *nnmoves, float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);
  uint wait_id = push_ff(size_batch, input, sizes_nnmove, nnmoves,
			 probs, values);
  wait_ff(wait_id); }

uint NNetOCL::push_ff(uint size_batch, const float *input,
		      const uint *sizes_nnmove, const ushort *nnmoves,
		      float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);

  unique_lock<mutex> lock_pool1(_m_pool1_slot);
  _cv_pool1_slot.wait(lock_pool1, [&]{ return 0 < _pool1_slot_size; });
  assert(0 < _pool1_slot_size);
  uint uslot = _pool1_slots[ --_pool1_slot_size ];
  lock_pool1.unlock();

  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("invalid size_batch"));

  size_t size_write;
  uint n_one, ntot_moves, index_moves;
  tie(size_write, n_one, ntot_moves, index_moves)
    = NNAux::pack_batch(size_batch, _maxsize_batch, input, sizes_nnmove,
			nnmoves, _mem_in[uslot].get_pointer());
  memcpy(_slots_sizes_nnmove[uslot].get(), sizes_nnmove,
	 sizeof(uint) * size_batch);
  _slots_size_batch[uslot] = size_batch;
  _slots_probs[uslot]      = probs;
  _slots_values[uslot]     = values;

  _queue_a[uslot].push_write(_mem_in[uslot], size_write);
  _mng_decode.push(_queue_a[uslot], n_one, uslot);
    
  // body part
  uint ulayer = 0U;
  _mng_compute_matV_first.push(_queue_a[uslot], uslot);
  for (; ulayer < _nres_block; ++ulayer) {
    _pmng_compute_matM[ulayer].push(_queue_a[uslot], uslot);
    _pmng_compute_matAV[ulayer].push(_queue_a[uslot], uslot); }
  _pmng_compute_matM[ulayer].push(_queue_a[uslot], uslot);
  _mng_compute_matA_last.push(_queue_a[uslot], uslot);
  
  // head part
  // in:  f1[policy1_nout + value1_nout][size_batch][size_plane]
  // out: f2[size_batch][value1_nout][size_plane]
  _mng_head1.push(_queue_a[uslot], uslot);
  _mng_compute_BNReLU.push(_queue_a[uslot], uslot);
  _mng_compute_policy.push(_queue_a[uslot], ntot_moves, index_moves, uslot);
  
  _mng_transform_value2.push(_queue_a[uslot], uslot);
  _mng_value2.push(_queue_a[uslot], uslot);
  
  _mng_resize_bias_ReLU_value3.push(_queue_a[uslot], uslot);
  _mng_value3.push(_queue_a[uslot], uslot);
  
  _queue_a[uslot].push_read(_mem_out[uslot],
			    (_maxsize_batch + ntot_moves) * sizeof(float));
  return uslot; }

void NNetOCL::wait_ff(uint uslot) noexcept {
  assert(uslot < NNAux::nslot);

  if (_do_sleep) {
    steady_clock::time_point start = steady_clock::now();
    sleep_for(microseconds(INT64_C(3) * _elapsed_wait_ff / INT64_C(4)));
    _queue_a[uslot].finish();
    steady_clock::time_point end = steady_clock::now();
    int64_t elapsed = duration_cast<microseconds>(end - start).count();
    _elapsed_wait_ff = ((INT64_C(99) * _elapsed_wait_ff + elapsed)
			/ INT64_C(100)); }
  else _queue_a[uslot].finish();

  compute_probs(_slots_size_batch[uslot], _slots_sizes_nnmove[uslot].get(),
		static_cast<float *>(_mem_out[uslot].get_pointer())
		+ _maxsize_batch, _slots_probs[uslot]);
  for (uint ub = 0; ub < _slots_size_batch[uslot]; ++ub)
    _slots_values[uslot][ub]
      = std::tanh(static_cast<float *>(_mem_out[uslot].get_pointer())[ub]
		  + _value3_bias[0]);

  unique_lock<mutex> lock(_m_pool1_slot);
  assert(_pool1_slot_size < NNAux::nslot);
  _pool1_slots[ _pool1_slot_size++ ] = uslot;
  lock.unlock();
  _cv_pool1_slot.notify_one(); }
#endif
