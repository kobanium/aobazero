// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet-cpu.hpp"
#if ! defined(USE_OPENBLAS) && ! defined(USE_MKL)
using std::pair;
using std::vector;
using namespace ErrAux;
void NNetCPU::reset(uint, const vector<pair<uint, row_t>> &, int) noexcept {
  die(ERR_INT("No CPU BLAS support")); }
#else
#  include <algorithm>
#  include <utility>
#  include <vector>
#  include <cassert>
#  include <cmath>
#  include <omp.h>
#  if defined(USE_MKL)
#    include <mkl.h>
#  elif defined(USE_OPENBLAS)
#    include <cblas.h>
#  endif
using std::copy_n;
using std::fill_n;
using std::forward;
using std::max;
using std::max_element;
using std::move;
using std::pair;
using std::swap;
using std::vector;
using row_t  = std::unique_ptr<float []>;
using ushort = unsigned short;
using namespace ErrAux;

constexpr char msg_bad_wght_dim[] = "bad weight dimension";

static float x1(float x) noexcept { return x; }
static float x2(float x) noexcept { return 2.0f * x; }
static float x3(float x) noexcept { return 3.0f * x; }
static float x4(float x) noexcept { return 4.0f * x; }
static float x6(float x) noexcept { return 6.0f * x; }
static float x8(float x) noexcept { return 8.0f * x; }
static float x9(float x) noexcept { return 9.0f * x; }
static float x16(float x) noexcept { return 16.0f * x; }

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
constexpr float bn_factor    = 1.0f / 999.982f;
constexpr float bn_eps       = 1e-5f;

void NNetCPU::reset(uint maxsize_batch, const vector<pair<uint, row_t>> &wght,
		    int thread_num) noexcept {
  assert(0 < maxsize_batch);
  if (thread_num == -1) thread_num = omp_get_max_threads();
  _thread_num = thread_num;

#if defined(USE_MKL)
#  if ! defined(__INTEL_COMPILER) && defined(__linux__) && ! defined(__MIC__)
  if (mkl_set_threading_layer(MKL_THREADING_GNU) < 0)
    die(ERR_INT("mkl_set_interface_layer() failed."));
#  endif
  mkl_set_num_threads(thread_num);
#endif

  _maxsize_batch = maxsize_batch;
  load(wght);
  
  _matM.reset(new float [_maxsize_batch * _conv3x3_nout_max * size_plane_in]);
  _matV.reset(new float [_maxsize_batch * _conv3x3_nin_max  * size_plane_in]);
  for (auto &f : _fslot) f.reset(new float [_maxsize_batch * _maxsize_out]); }

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

static row_t gen_head1_sd_inv(uint nch1, uint nch2,
			      const float *sd1, const float *sd2) noexcept {
  row_t row(new float [nch1 + nch2]);
  for (uint ch = 0; ch < nch1; ++ch)
    row[ch] = 1.0f / std::sqrt(sd1[ch] * bn_factor + bn_eps);
  for (uint ch = 0; ch < nch2; ++ch)
    row[nch1 + ch] = 1.0f / std::sqrt(sd2[ch] * bn_factor + bn_eps);
  return row; }

void NNetCPU::load(const vector<pair<uint, row_t>> &wght) noexcept {
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
  row_t matU, mean, sd_inv;
  uint nin = NNAux::nch_input;
  uint index = 0;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (wght[index].first != _resnet_nout * nin * size_kernel
	|| wght[index + 1U].first != _resnet_nout
	|| wght[index + 2U].first != _resnet_nout
	|| wght[index + 3U].first != _resnet_nout)
      die(ERR_INT(msg_bad_wght_dim));
    matU = gen_matU(_resnet_nout, nin, wght[index].second.get());
    mean = gen_mean(_resnet_nout, wght[index + 1U].second.get(),
		    wght[index + 2U].second.get());
    sd_inv = gen_sd_inv(_resnet_nout, wght[index + 3U].second.get());
    _reswghts.push_back({move(matU), move(mean), move(sd_inv)});
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

// fin[size_plane]
// matrix_V[size_tile_in][nchxnb][ntile]
static void compute_matV_child(uint nchxnb, uint chb, const float *fin,
			       float *matV) noexcept {
  assert(0 < nchxnb && chb < nchxnb && fin && matV);
  const uint uca = len_tile_in * nchxnb * ntile;
  const uint ucb = nchxnb * ntile;
  for (uint uh = 0; uh < ntile_h; ++uh)
    for (uint uw = 0; uw < ntile_w; ++uw) {
      const uint ucc = chb * ntile + uh * ntile_w + uw;
  
      float md[len_tile_in][len_tile_in];
      int y0 = static_cast<int>(uh * len_tile_out) - pad;
      int x0 = static_cast<int>(uw * len_tile_out) - pad;
      for (int y = 0; y < static_cast<int>(len_tile_in); ++y)
	for (int x = 0; x < static_cast<int>(len_tile_in); ++x)
	  if (0 <= y0 + y && y0 + y < static_cast<int>(NNAux::height)
	      && 0 <= x0 + x && x0 + x < static_cast<int>(NNAux::width))
	    md[y][x] = fin[(y0 + y) * NNAux::width + x0 + x];
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
	   + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4])); } }

// in: matM[size_tile_in][nchxnb][ntile]
// out: fout[size_plane]
static void
compute_matA_child(uint nchxnb, uint chb, const float *matM, float *fout)
  noexcept {
  for (uint uh = 0; uh < ntile_h; ++uh)
    for (uint uw = 0; uw < ntile_w; ++uw) {
      float mm[len_tile_in][len_tile_in];
      for (uint uh_in = 0; uh_in < len_tile_in; ++uh_in)
	for (uint uw_in = 0; uw_in < len_tile_in; ++uw_in)
	  mm[uh_in][uw_in] = matM[uh_in * len_tile_in * nchxnb * ntile
				  + uw_in * nchxnb * ntile
				  + chb * ntile
				  + uh * ntile_w + uw];
      
      const uint ucd = uh * len_tile_out * NNAux::width + uw * len_tile_out;
      fout[ucd + 0U * NNAux::width + 0U]
	= (+ mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
	   + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
	   + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
	   + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);
      fout[ucd + 0U * NNAux::width + 1U]
	= (+ mm[0][1] - mm[0][2] + x2(mm[0][3])
	   + mm[1][1] - mm[1][2] + x2(mm[1][3])
	   + mm[2][1] - mm[2][2] + x2(mm[2][3])
	   + mm[3][1] - mm[3][2] + x2(mm[3][3]));
      fout[ucd + 0U * NNAux::width + 2U]
	= (+ mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
	   + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
	   + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
	   + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
      fout[ucd + 1U * NNAux::width + 0U]
	= (+ mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
	   - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
	   + x2(mm[3][0]) + x2(mm[3][1]) + x2(mm[3][2]) + x2(mm[3][3]));
      fout[ucd + 1U * NNAux::width + 1U]
	= (+ mm[1][1] - mm[1][2] + x2(mm[1][3])
	   - mm[2][1] + mm[2][2] - x2(mm[2][3])
           + x2(mm[3][1]) - x2(mm[3][2]) + x4(mm[3][3]));
      fout[ucd + 1U * NNAux::width + 2U]
	= (+ mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
	   - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
	   + x2(mm[3][1]) + x2(mm[3][2]) + x8(mm[3][3]) + x2(mm[3][4]));
      fout[ucd + 2U * NNAux::width + 0U]
	= (+ mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
	   + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
	   + x4(mm[3][0]) + x4(mm[3][1]) + x4(mm[3][2]) + x4(mm[3][3])
	   + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
      fout[ucd + 2U * NNAux::width + 1U]
	= (+ mm[1][1] - mm[1][2] + x2(mm[1][3])
	   + mm[2][1] - mm[2][2] + x2(mm[2][3])
           + x4(mm[3][1]) - x4(mm[3][2]) + x8(mm[3][3])
	   + mm[4][1] - mm[4][2] + x2(mm[4][3]));
      fout[ucd + 2U * NNAux::width + 2U]
	= (+ mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
	   + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
	   + x4(mm[3][1]) + x4(mm[3][2]) + x16(mm[3][3]) + x4(mm[3][4])
	   + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); } }
  
static void compute_matV_input(uint size_batch, const float *fin, float *matV)
  noexcept {
  assert(0 < size_batch && fin && matV);
  const uint nchxnb = NNAux::nch_input * size_batch;

#pragma omp parallel for
  for (int loop = 0; loop < static_cast<int>(nchxnb); ++loop) {
    uint chb_in = loop;
    uint ub  = chb_in / NNAux::nch_input;
    uint ch  = chb_in % NNAux::nch_input;
    uint chb = ch * size_batch + ub;
    const float *p = fin + chb_in * NNAux::size_plane;
    compute_matV_child(nchxnb, chb, p, matV); } }

// in:  weight[nout][nin]
// in:  fin[nin][size_batch][size_plane]
// out: fout[nout][size_batch][size_plane]
static void
compute_conv1x1(uint nout, uint nin, uint size_batch, const float *weight,
		const float *fin, float *fout) noexcept {
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
	      nout, size_batch * NNAux::size_plane, nin,
	      1.0f, weight, nin, fin, size_batch * NNAux::size_plane,
	      0.0f, fout, size_batch * NNAux::size_plane); }

// in:  matU[size_tile_in][nout][nin]
// in:  matV[size_tile_in][nin][size_batch][ntile]
// out: matM[size_tile_in][nout][size_batch][ntile]
static void compute_matM(uint nout, uint nin, uint size_batch,
			 const float *matU, const float *matV, float *matM)
  noexcept {
#if defined(USE_MKL)
  constexpr CBLAS_TRANSPOSE trans[1] = { CblasNoTrans };
  constexpr int group_size[1]    = { static_cast<int>(size_tile_in) };
  constexpr float alpha_array[1] = { 1.0f };
  constexpr float beta_array[1]  = { 0.0f };
  const int m_array[1]           = { static_cast<int>(nout) };
  const int n_array[1]           = { static_cast<int>(ntile * size_batch) };
  const int k_array[1]           = { static_cast<int>(nin) };
  const float *a_array[size_tile_in], *b_array[size_tile_in];
  float *c_array[size_tile_in];
  for (uint u = 0; u < size_tile_in; ++u) {
    a_array[u] = matU + u * nout * nin;
    b_array[u] = matV + u * nin * size_batch * ntile;
    c_array[u] = matM + u * nout * ntile * size_batch; }

  cblas_sgemm_batch(CblasRowMajor, trans, trans, m_array, n_array, k_array,
		    alpha_array, a_array, k_array, b_array, n_array,
		    beta_array, c_array, n_array, 1, group_size);
#else
#  pragma omp parallel for
  for (int loop = 0; loop < static_cast<int>(size_tile_in); ++loop) {
    uint uin = loop;
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
		nout, ntile * size_batch, nin,
		1.0f, matU + uin * nout * nin, nin,
		matV + uin * nin * size_batch * ntile,ntile * size_batch,
		0.0f, matM + uin * nout * ntile * size_batch,
		ntile * size_batch); }
#endif
}

// in: matM[size_tile_in][nch][size_batch][ntile]
// in: mean[nch]
// in: sd_inv[nch]
// out: fork[nch][size_batch][size_plane]
// out: matV[size_tile_in][nch][size_batch][ntile]
static void
compute_matA_BNReLU_fork_matV(uint nch, uint size_batch, const float *matM,
			      const float *mean, const float *sd_inv,
			      float *fork, float *matV) noexcept {
  uint nchxnb = nch * size_batch;
#pragma omp parallel for
  for (int loop = 0; loop < static_cast<int>(nchxnb); ++loop) {
    uint chb = loop;
    uint ch = chb / size_batch;
    float fout[NNAux::size_plane];
    compute_matA_child(nchxnb, chb, matM, fout);
    for (uint u = 0; u < NNAux::size_plane; ++u)
      fork[chb * NNAux::size_plane + u] = fout[u]
	= max(0.0f, sd_inv[ch] * (fout[u] - mean[ch]));
    compute_matV_child(nchxnb, chb, fout, matV); } }

static void
compute_matA_BNReLU_join_fork_matV(uint nch, uint size_batch,
				   const float *matM, const float *mean,
				   const float *sd_inv, float *fork,
				   float *matV) noexcept {
  int nchxnb = static_cast<int>(nch * size_batch);
#pragma omp parallel for
  for (int loop = 0; loop < nchxnb; ++loop) {
    uint chb = static_cast<uint>(loop);
    uint ch = chb / size_batch;
    float fout[NNAux::size_plane];
    compute_matA_child(nchxnb, chb, matM, fout);
    for (uint u = 0; u < NNAux::size_plane; ++u) {
      float &f = fork[chb * NNAux::size_plane + u];
      f = fout[u] = max(0.0f, f + sd_inv[ch] * (fout[u] - mean[ch])); }
    compute_matV_child(nchxnb, chb, fout, matV); } }

static void
compute_matA_BNReLU_join_fork(uint nch, uint size_batch, const float *matM,
			      const float *mean, const float *sd_inv,
			      float *fork) noexcept {
  uint nchxnb = nch * size_batch;
  for (uint chb = 0; chb < nchxnb; ++chb) {
    uint ch = chb / size_batch;
    float fout[NNAux::size_plane];
    compute_matA_child(nchxnb, chb, matM, fout);
    for (uint u = 0; u < NNAux::size_plane; ++u) {
      float &f = fork[chb * NNAux::size_plane + u];
      f = fout[u] = max(0.0f, f + sd_inv[ch] * (fout[u] - mean[ch])); } } }

static void
compute_matA_BNReLU_matV(uint nch, uint size_batch, const float *matM,
			 const float *mean, const float *sd_inv, float *matV)
  noexcept {
  uint nchxnb = nch * size_batch;
#pragma omp parallel for
  for (int loop = 0; loop < static_cast<int>(nchxnb); ++loop) {
    uint chb = loop;
    uint ch = chb / size_batch;
    float fout[NNAux::size_plane];
    compute_matA_child(nchxnb, chb, matM, fout);
    for (uint u = 0; u < NNAux::size_plane; ++u)
      fout[u] = max(0.0f, sd_inv[ch] * (fout[u] - mean[ch]));
    compute_matV_child(nchxnb, chb, fout, matV); } }

// in:     mean[nch]
// in:     sd_inv[nch]
// in/out: fio[nch][size_batch][size_plane]
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
    const ushort *m = nnmoves + ub * SAux::maxsize_moves;
    float *probs_b  = probs   + ub * SAux::maxsize_moves;
    for (uint u = 0; u < sizes_nnmove[ub]; ++u) {
      assert(m[u] < NNAux::nch_out_policy * NNAux::size_plane);
      uint ch = m[u] / NNAux::size_plane;
      uint sq = m[u] % NNAux::size_plane;
      probs_b[u] = cblas_sdot(nch, weight + ch * nch, 1,
			      fin + ub * NNAux::size_plane + sq,
			      size_batch * NNAux::size_plane);
      probs_b[u] += bias[ch]; }
    NNAux::softmax(sizes_nnmove[ub], probs_b); } }

uint NNetCPU::push_ff(uint size_batch, const float *input,
		      const uint *sizes_nnmove, const ushort *nnmoves,
		      float *probs, float *values) noexcept {
  assert(input && sizes_nnmove && nnmoves && probs && values);

  omp_set_num_threads(_thread_num);
#if defined(USE_OPENBLAS)
  openblas_set_num_threads(1);
#endif

  if (size_batch == 0 || _maxsize_batch < size_batch)
    die(ERR_INT("size_batch == 0"));
  
  // feed forward input layers
  // body part
  float *fbody = _fslot[0].get();
  float *f1    = _fslot[1].get();
  compute_matV_input(size_batch, input, _matV.get());
  compute_matM(_resnet_nout, NNAux::nch_input, size_batch,
	       _reswghts[0].matU.get(), _matV.get(), _matM.get());
  compute_matA_BNReLU_fork_matV(_resnet_nout, size_batch, _matM.get(),
				_reswghts[0].mean.get(),
				_reswghts[0].sd_inv.get(),
				fbody, _matV.get());

  uint ulayer = 1U;
  for (; ulayer + 2U < _reswghts.size(); ulayer += 2U) {
    compute_matM(_resnet_nout, _resnet_nout, size_batch,
		 _reswghts[ulayer].matU.get(), _matV.get(), _matM.get());
    compute_matA_BNReLU_matV(_resnet_nout, size_batch, _matM.get(),
			     _reswghts[ulayer].mean.get(),
			     _reswghts[ulayer].sd_inv.get(),
			     _matV.get());

    compute_matM(_resnet_nout, _resnet_nout, size_batch,
		 _reswghts[ulayer + 1U].matU.get(), _matV.get(), _matM.get());
    compute_matA_BNReLU_join_fork_matV(_resnet_nout, size_batch, _matM.get(),
				       _reswghts[ulayer + 1U].mean.get(),
				       _reswghts[ulayer + 1U].sd_inv.get(),
				       fbody, _matV.get()); }
  
  compute_matM(_resnet_nout, _resnet_nout, size_batch,
	       _reswghts[ulayer].matU.get(), _matV.get(), _matM.get());
  compute_matA_BNReLU_matV(_resnet_nout, size_batch, _matM.get(),
			   _reswghts[ulayer].mean.get(),
			   _reswghts[ulayer].sd_inv.get(),
			   _matV.get());

  compute_matM(_resnet_nout, _resnet_nout, size_batch,
	       _reswghts[ulayer + 1U].matU.get(), _matV.get(), _matM.get());
  compute_matA_BNReLU_join_fork(_resnet_nout, size_batch, _matM.get(),
				_reswghts[ulayer + 1U].mean.get(),
				_reswghts[ulayer + 1U].sd_inv.get(),
				fbody);

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
  return 0; }
#endif
