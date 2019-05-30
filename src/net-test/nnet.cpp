// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "nnet.hpp"
#include "osi.hpp"
#include "shogibase.hpp"
#include "xzi.hpp"
#include <algorithm>
#include <fstream>
#include <queue>
#include <utility>
#include <vector>
#include <cassert>
using std::copy_n;
using std::fill_n;
using std::forward;
using std::ifstream;
using std::ios;
using std::max;
using std::min;
using std::move;
using std::pair;
using std::queue;
using std::swap;
using std::unique_ptr;
using std::vector;
using row_t  = unique_ptr<float []>;
using ushort = unsigned short;
using uchar  = unsigned char;
using namespace ErrAux;

constexpr char msg_bad_xz_fmt[] = "bad xz format";
constexpr char msg_bad_wght[]   = "bad weight format";

void preprocess(Conv &conv, BNorm &bn) noexcept {
  for (uint ch = 0; ch < conv.get_nout(); ++ch) bn._mean[ch] -= conv._bias[ch];
  conv._bias.reset(); }
  
ushort NN::encode_nnmove(const Action &a, const Color &turn) noexcept {
  assert(a.is_move() && turn.ok());
  Sq to = a.get_to().rel(turn);
  Pc pc = a.get_pc();
  if (a.is_drop()) {
    uint ch   = 2U * (64U + 2U) + pc.to_u();
    ushort us = static_cast<ushort>(ch * Sq::ok_size + to.to_u());
    assert(10692U <= us && us < 11259U);
    return us; }

  uint ch = 0;
  if (a.is_promotion()) ch += 64U + 2U;

  Sq from   = a.get_from().rel(turn);
  uchar dir = from.to_dir(to);
  assert(dir < 10U);
  if      (dir <  8U) ch += dir * 8U + from.to_distance(to) - 1U;
  else if (dir == 8U) ch += 64U;
  else if (dir == 9U) ch += 64U + 1U;

  ushort us = static_cast<ushort>(ch * Sq::ok_size + from.to_u());
  assert(us < 10692U);
  return us; }

static pair<uint, row_t> read_row(ifstream &ifs,
				  XZDecode<ifstream, PtrLen<char>> &xzd,
				  char *line, uint len_line) noexcept {
  assert(line && 0 < len_line);
  PtrLen<char> pl_line(line, 0);
  bool bRet = xzd.getline(&ifs, &pl_line, len_line, "\n\r");
  if (!bRet) die(ERR_INT(msg_bad_xz_fmt));
  if (len_line <= pl_line.len) die(ERR_INT("line too long"));
  pl_line.p[pl_line.len] = '\0';
  
  char *saveptr, *endptr;
  const char *token = OSI::strtok(line, " ", &saveptr);
  queue<float> queue_float;
  while (token) {
    float f = strtof(token, &endptr);
    if (endptr == token || *endptr != '\0' || f == -HUGE_VALF
	|| f == HUGE_VALF) die(ERR_INT(msg_bad_wght));
    queue_float.push(f);
    token = OSI::strtok(nullptr, " ", &saveptr); }
  
  pair<uint, row_t> ret(queue_float.size(), new float [queue_float.size()]);
  uint u = 0;
  while(!queue_float.empty()) {
    ret.second[u++] = queue_float.front();
    queue_float.pop(); }
  return ret; }

static void softmax(uint n, float *p) noexcept {
  assert(0 < n && p);
  float fmax = *std::max_element(p, p + n);
  float fsum = 0.0f;
  for (uint u = 0; u < n; ++u) {
    p[u] = expf(p[u] - fmax);
    fsum += p[u]; }
  
  float factor = 1.0 / fsum;
  for (uint u = 0; u < n; ++u) p[u] *= factor; }

void IP::reset(uint nin, uint nout, row_t &&weight, row_t &&bias) noexcept {
  _nin    = nin;
  _nout   = nout;
  _weight = forward<row_t>(weight);
  _bias   = forward<row_t>(bias); }

void IP::ff(const float *fin, float *fout) const noexcept {
  assert(fin && fout);
  cblas_sgemv(CblasRowMajor, CblasNoTrans, _nout, _nin, 1.0f,
	      _weight.get(), _nin, fin, 1, 0.0f, fout, 1);
  for (uint u = 0; u < _nout; ++u) fout[u] += _bias[u]; }

void IP::ff_relu(const float *fin, float *fout) const noexcept {
  assert(fin && fout);
  cblas_sgemv(CblasRowMajor, CblasNoTrans, _nout, _nin, 1.0f,
	      _weight.get(), _nin, fin, 1, 0.0f, fout, 1);
  for (uint u = 0; u < _nout; ++u)
    fout[u] = max(0.0f, fout[u] + _bias[u]); }

void Conv::reset(uint nin, uint nout, row_t &&weight, row_t &&bias)
  noexcept {
  _nin    = nin;
  _nout   = nout;
  _weight = forward<row_t>(weight);
  _bias   = forward<row_t>(bias); }

void BNorm::reset(uint nio, row_t &&mean, row_t &&sd) noexcept {
  _nio    = nio;
  _mean   = forward<row_t>(mean);
  _sd_inv = forward<row_t>(sd);
  constexpr float bn_factor = 1.0f / 999.982f;
  constexpr float bn_eps    = 1e-5f;
  for (uint ch = 0; ch < _nio; ++ch) {
    _sd_inv[ch] = 1.0f / std::sqrt(bn_factor * _sd_inv[ch] + bn_eps);
    _mean[ch] *= bn_factor; } }

void BNorm::ff_relu(float *fio) const noexcept {
  for (uint ch = 0; ch < _nio; ++ch) {
    for (uint u = 0; u < NN::size_plane; ++u)
      fio[u] = max(0.0f, _sd_inv[ch] * (fio[u] - _mean[ch]));
    fio += NN::size_plane; } }

void BNorm::ff_relu(float *fio, const float *fbypass) const noexcept {
  for (uint ch = 0; ch < _nio; ++ch) {
    for (uint u = 0; u < NN::size_plane; ++u)
      fio[u] = max(0.0f, fbypass[u] + _sd_inv[ch] * (fio[u] - _mean[ch]));
    fio     += NN::size_plane;
    fbypass += NN::size_plane; } }

void Conv_1x1::ff(const float *fin, float *fout) const noexcept {
  const float *pw = _weight.get();
  for (uint ch_out = 0; ch_out < _nout; ++ch_out) {
    if (_bias) fill_n(fout, NN::size_plane, _bias[ch_out]);
    else       fill_n(fout, NN::size_plane, 0.0f);
    for (uint ch_in = 0; ch_in < _nin; ++ch_in)
      for (uint u = 0; u < NN::size_plane; ++u)
	fout[u] += pw[ch_in] * fin[ch_in * NN::size_plane + u];
    
    fout += NN::size_plane;
    pw   += _nin; } }

#if defined(F_3x3_3x3)
void Conv_3x3::reset(uint nin, uint nout, row_t &&weight, row_t &&bias)
  noexcept {
  Conv::reset(nin, nout, forward<row_t>(weight), forward<row_t>(bias));
  _matrix_U.reset(new float [_size_tile_in * _nout * _nin]);
  _matrix_V.reset(new float [_size_tile_in * _nin * _ntile]);
  _matrix_M.reset(new float [_size_tile_in * _nout * _ntile]);
  // U_kc = G g_kc G^T
  // g_kc = _weight[k][c][.]
  constexpr float _matrix_G[5][3] = { 1.0f / 2.0f,  0.0f,         0.0f,
				      -1.0f / 2.0f, -1.0f / 2.0f, -1.0f / 2.0f,
				      -1.0f / 6.0f, 1.0f / 6.0f,  -1.0f / 6.0f,
				      1.0f / 6.0f,  1.0f / 3.0f,  2.0f / 3.0f,
				      0.0f,         0.0f,         1.0f };
  for (uint ch_io = 0; ch_io < _nout * _nin; ++ch_io) {
    const float *_matrix_g = &( _weight[ch_io * _size_kernel] );
    for (uint uh = 0; uh < _len_tile_in; ++uh)
      for (uint uw = 0; uw < _len_tile_in; ++uw) {
	float fsum = 0.0;
	for (uint u1 = 0; u1 < _len_kernel; ++u1)
	  for (uint u2 = 0; u2 < _len_kernel; ++u2)
	    fsum += (_matrix_G[uh][u1] * _matrix_G[uw][u2]
		     * _matrix_g[u1 * _len_kernel + u2]);
	_matrix_U[+ uh * _len_tile_in * _nout * _nin
		  + uw * _nout * _nin
		  + ch_io] = fsum; } } }

static float x1(float x) noexcept { return x; }
static float x2(float x) noexcept { return x + x; }
static float x3(float x) noexcept { return x + x + x; }
static float x4(float x) noexcept { x += x; return x + x; }
static float x6(float x) noexcept { x += x; return x + x + x; }
static float x9(float x) noexcept {
  float y = x + x; y += y; return y + y + x; }

void Conv_3x3::ff(const float *fin, float *fout) noexcept {
  constexpr int pad = static_cast<int>(_len_kernel / 2U);
  const uint ua = _len_tile_in * _ntile * _nin;
  const uint ub = _ntile * _nin;
  
  for (uint nh = 0; nh < _ntile_h; ++nh)
    for (uint nw = 0; nw < _ntile_w; ++nw)
      for (uint ch_in = 0; ch_in < _nin; ++ch_in) {
	const float *fin_c = fin + ch_in * NN::size_plane;
	float md[_len_tile_in][_len_tile_in];
	int y0 = static_cast<int>(nh * _len_tile_out) - pad;
	int x0 = static_cast<int>(nw * _len_tile_out) - pad;
	for (int y = 0; y < static_cast<int>(_len_tile_in); ++y)
	  for (int x = 0; x < static_cast<int>(_len_tile_in); ++x)
	    if (0 <= y0 + y && y0 + y < static_cast<int>(NN::height)
		&& 0 <= x0 + x && x0 + x < static_cast<int>(NN::width))
	      md[y][x] = fin_c[(y0 + y) * NN::width + x0 + x];
	    else md[y][x] = 0.0f;

	const uint uc = ch_in * _ntile + nh * _ntile_w + nw;
	_matrix_V[uc + ua*0U + ub*0U]
	  = (+ x4(md[0][0]) - x2(md[0][1]) - x4(md[0][2]) + x2(md[0][3])
	     - x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
	     - x4(md[2][0]) + x2(md[2][1]) + x4(md[2][2]) - x2(md[2][3])
	     + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*1U + ub*0U]
	  = (- x4(md[1][0]) + x2(md[1][1]) + x4(md[1][2]) - x2(md[1][3])
	     - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
	     + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*2U + ub*0U]
	  = (+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
	     - x6(md[2][0]) + x3(md[2][1]) + x6(md[2][2]) - x3(md[2][3])
	     + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*3U + ub*0U]
	  = (- x2(md[1][0]) + x1(md[1][1]) + x2(md[1][2]) - x1(md[1][3])
	     + x2(md[3][0]) - x1(md[3][1]) - x2(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*4U + ub*0U]
	  = (+ x4(md[1][0]) - x2(md[1][1]) - x4(md[1][2]) + x2(md[1][3])
	     - x2(md[2][0]) + x1(md[2][1]) + x2(md[2][2]) - x1(md[2][3])
	     - x4(md[3][0]) + x2(md[3][1]) + x4(md[3][2]) - x2(md[3][3])
	     + x2(md[4][0]) - x1(md[4][1]) - x2(md[4][2]) + x1(md[4][3]));
	_matrix_V[uc + ua*0U + ub*1U]
	  = (- x4(md[0][1]) - x2(md[0][2]) + x2(md[0][3])
	     + x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
	     + x4(md[2][1]) + x2(md[2][2]) - x2(md[2][3])
	     - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*1U + ub*1U]
	  = (+ x4(md[1][1]) + x2(md[1][2]) - x2(md[1][3])
	     + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
	     - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*2U + ub*1U]
	  = (- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
	     + x6(md[2][1]) + x3(md[2][2]) - x3(md[2][3])
	     - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*3U + ub*1U]
	  = (+ x2(md[1][1]) + x1(md[1][2]) - x1(md[1][3])
	     - x2(md[3][1]) - x1(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*4U + ub*1U]
	  = (- x4(md[1][1]) - x2(md[1][2]) + x2(md[1][3])
	     + x2(md[2][1]) + x1(md[2][2]) - x1(md[2][3])
	     + x4(md[3][1]) + x2(md[3][2]) - x2(md[3][3])
	     - x2(md[4][1]) - x1(md[4][2]) + x1(md[4][3]));
	_matrix_V[uc + ua*0U + ub*2U]
	  = (+ x4(md[0][1]) - x6(md[0][2]) + x2(md[0][3])
	     - x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
	     - x4(md[2][1]) + x6(md[2][2]) - x2(md[2][3])
	     + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*1U + ub*2U]
	  = (- x4(md[1][1]) + x6(md[1][2]) - x2(md[1][3])
	     - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
	     + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*2U + ub*2U]
	  = (+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
	     - x6(md[2][1]) + x9(md[2][2]) - x3(md[2][3])
	     + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*3U + ub*2U]
	  = (- x2(md[1][1]) + x3(md[1][2]) - x1(md[1][3])
	     + x2(md[3][1]) - x3(md[3][2]) + x1(md[3][3]));
	_matrix_V[uc + ua*4U + ub*2U]
	  = (+ x4(md[1][1]) - x6(md[1][2]) + x2(md[1][3])
	     - x2(md[2][1]) + x3(md[2][2]) - x1(md[2][3])
	     - x4(md[3][1]) + x6(md[3][2]) - x2(md[3][3])
	     + x2(md[4][1]) - x3(md[4][2]) + x1(md[4][3]));
	_matrix_V[uc + ua*0U + ub*3U]
	  = (- x2(md[0][1]) + x2(md[0][3]) + x1(md[1][1]) - x1(md[1][3])
	     + x2(md[2][1]) - x2(md[2][3]) - x1(md[3][1]) + x1(md[3][3]));
	_matrix_V[uc + ua*1U + ub*3U]
	  = (+ x2(md[1][1]) - x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
	     - x1(md[3][1]) + x1(md[3][3]));
	_matrix_V[uc + ua*2U + ub*3U]
	  = (- x2(md[1][1]) + x2(md[1][3]) + x3(md[2][1]) - x3(md[2][3])
	     - x1(md[3][1]) + x1(md[3][3]));
	_matrix_V[uc + ua*3U + ub*3U]
	  = (+ x1(md[1][1]) - x1(md[1][3]) - x1(md[3][1]) + x1(md[3][3]));
	_matrix_V[uc + ua*4U + ub*3U]
	  = (- x2(md[1][1]) + x2(md[1][3]) + x1(md[2][1]) - x1(md[2][3])
	     + x2(md[3][1]) - x2(md[3][3]) - x1(md[4][1]) + x1(md[4][3]));
	_matrix_V[uc + ua*0U + ub*4U]
	  = (+ x4(md[0][1]) - x2(md[0][2]) - x4(md[0][3]) + x2(md[0][4])
	     - x2(md[1][1]) + x1(md[1][2]) + x2(md[1][3]) - x1(md[1][4])
	     - x4(md[2][1]) + x2(md[2][2]) + x4(md[2][3]) - x2(md[2][4])
	     + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]));
	_matrix_V[uc + ua*1U + ub*4U]
	  = (- x4(md[1][1]) + x2(md[1][2]) + x4(md[1][3]) - x2(md[1][4])
	     - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
	     + x2(md[3][1]) - x1(md[3][2]) - x2(md[3][3]) + x1(md[3][4]));
	_matrix_V[uc + ua*2U + ub*4U]
	  = (+ x4(md[1][1]) - x6(md[2][1]) + x2(md[3][1])
	     - x2(md[1][2]) + x3(md[2][2]) - x1(md[3][2])
	     - x4(md[1][3]) + x6(md[2][3]) - x2(md[3][3])
	     + x2(md[1][4]) - x3(md[2][4]) + x1(md[3][4]));
	_matrix_V[uc + ua*3U + ub*4U]
	  = (- x2(md[1][1]) + x2(md[3][1]) + x1(md[1][2]) - x1(md[3][2])
	     + x2(md[1][3]) - x2(md[3][3]) - x1(md[1][4]) + x1(md[3][4]));
	_matrix_V[uc + ua*4U + ub*4U]
	  = (+ x4(md[1][1]) - x2(md[1][2]) - x4(md[1][3]) + x2(md[1][4])
	     - x2(md[2][1]) + x1(md[2][2]) + x2(md[2][3]) - x1(md[2][4])
	     - x4(md[3][1]) + x2(md[3][2]) + x4(md[3][3]) - x2(md[3][4])
	     + x2(md[4][1]) - x1(md[4][2]) - x2(md[4][3]) + x1(md[4][4])); }

  for (uint uin = 0; uin < _len_tile_in * _len_tile_in; ++uin)
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
		_nout, _ntile, _nin, 1.0f,
		&( _matrix_U[uin * _nout * _nin] ), _nin,
		&( _matrix_V[uin * _ntile * _nin] ), _ntile, 0.0f,
		&( _matrix_M[uin * _nout * _ntile] ), _ntile);

  for (uint ch_out = 0; ch_out < _nout; ++ch_out)
    for (uint nh = 0; nh < _ntile_h; ++nh)
      for (uint nw = 0; nw < _ntile_w; ++nw) {
	float mm[_len_tile_in][_len_tile_in];
	for (uint uh_in = 0; uh_in < _len_tile_in; ++uh_in)
	  for (uint uw_in = 0; uw_in < _len_tile_in; ++uw_in)
	    mm[uh_in][uw_in] = _matrix_M[uh_in * _len_tile_in * _nout * _ntile
					 + uw_in * _nout * _ntile
					 + ch_out * _ntile
					 + nh * _ntile_w
					 + nw];

	auto y = [&](uint uh_out, uint uw_out) -> float& {
	  return fout[ch_out * NN::size_plane
		      + (nh * _len_tile_out + uh_out) * NN::width
		      + (nw * _len_tile_out + uw_out)]; };

	y(0,0) = (+ mm[0][0] + mm[0][1] + mm[0][2] + mm[0][3]
		  + mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
		  + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
		  + mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]);

	y(0,1) = (+ mm[0][1] - mm[0][2] + x2(mm[0][3])
		  + mm[1][1] - mm[1][2] + x2(mm[1][3])
		  + mm[2][1] - mm[2][2] + x2(mm[2][3])
		  + mm[3][1] - mm[3][2] + x2(mm[3][3]));

      	y(0,2) = (+ mm[0][1] + mm[0][2] + x4(mm[0][3]) + mm[0][4]
		  + mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
		  + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
		  + mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]);
		     
	y(1,0) = (+ mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
		  - mm[2][0] - mm[2][1] - mm[2][2] - mm[2][3]
		  + x2(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3]));

	y(1,1) = (+ mm[1][1] - mm[1][2] + x2(mm[1][3])
		  - mm[2][1] + mm[2][2]
		  + x2(- mm[2][3] + mm[3][1] - mm[3][2] + x2(mm[3][3])));

	y(1,2) = (+ mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
		  - mm[2][1] - mm[2][2] - x4(mm[2][3]) - mm[2][4]
		  + x2(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4]));

      	y(2,0) = (+ mm[1][0] + mm[1][1] + mm[1][2] + mm[1][3]
		  + mm[2][0] + mm[2][1] + mm[2][2] + mm[2][3]
		  + x4(mm[3][0] + mm[3][1] + mm[3][2] + mm[3][3])
		  + mm[4][0] + mm[4][1] + mm[4][2] + mm[4][3]);
		  
	y(2,1) = (+ mm[1][1] - mm[1][2] + x2(mm[1][3])
		  + mm[2][1] - mm[2][2]
		  + x2(mm[2][3] + x2(mm[3][1] - mm[3][2] + x2(mm[3][3])))
		  + mm[4][1] - mm[4][2] + x2(mm[4][3]));

	y(2,2) = (+ mm[1][1] + mm[1][2] + x4(mm[1][3]) + mm[1][4]
		  + mm[2][1] + mm[2][2] + x4(mm[2][3]) + mm[2][4]
		  + x4(mm[3][1] + mm[3][2] + x4(mm[3][3]) + mm[3][4])
		  + mm[4][1] + mm[4][2] + x4(mm[4][3]) + mm[4][4]); }
  
  if (_bias)
    for (uint ch_out = 0; ch_out < _nout; ++ch_out)
      for (uint u = 0; u < NN::size_plane; ++u)
	fout[ch_out * NN::size_plane + u] += _bias[ch_out]; }

#else

void Conv_3x3::reset(uint nin, uint nout, row_t &&weight, row_t &&bias)
  noexcept {
  Conv::reset(nin, nout, forward<row_t>(weight), forward<row_t>(bias));
  _fcol.reset(new float [_nin * _size_kernel * NN::size_plane]); }

void Conv_3x3::ff(const float *fin, float *fout) noexcept {
  constexpr int pad = _len_kernel / 2U;
  float *fcol = _fcol.get();
  const float *fin_c = fin;
  for (uint ch_in = 0; ch_in < _nin; ++ch_in) {
    for (int kh = -pad; kh < static_cast<int>(_len_kernel) - pad; ++kh)
      for (int kw = -pad; kw < static_cast<int>(_len_kernel) - pad; ++kw)
	for (int y = kh; y < kh + static_cast<int>(NN::height); ++y)
	  if (0 <= y && y < static_cast<int>(NN::height)) {
	    for (int x = kw; x < static_cast<int>(NN::width) + kw; ++x)
	      if (0 <= x && x < static_cast<int>(NN::width))
		*fcol++ = fin_c[y * NN::width + x];
	      else *fcol++ = 0.0f; }
	  else for (uint ux = 0; ux < NN::width; ++ux) *fcol++ = 0.0f;
    fin_c += NN::size_plane; }

  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
	      _nout, NN::size_plane, _nin * _size_kernel, 1.0f,
	      _weight.get(), _nin * _size_kernel,
	      _fcol.get(), NN::size_plane, 0.0f,
	      fout, NN::size_plane);
  
  if (_bias)
    for (uint ch_out = 0; ch_out < _nout; ++ch_out)
      for (uint u = 0; u < NN::size_plane; ++u)
	fout[ch_out * NN::size_plane + u] += _bias[ch_out]; }
#endif

void NNet::reset(const FName &fwght) noexcept {
  load(fwght);
  for (auto &f : fslot) f.reset(new float [_maxsize_out]); }

void NNet::load(const FName &fwght) noexcept {
  ifstream ifs(fwght.get_fname(), ios::binary);
  if (!ifs) die(ERR_INT("cannot open %s", fwght.get_fname()));
  
  constexpr uint len_line = 1024U * 1024U * 64U;
  unique_ptr<char []> line_ptr(new char [len_line]);
  char *line = line_ptr.get();
  XZDecode<ifstream, PtrLen<char>> xzd;
  xzd.init();
  
  // read version
  PtrLen<char> pl_line(line, 0);
  bool bRet = xzd.getline(&ifs, &pl_line, len_line, "\n\r");
  if (!bRet) die(ERR_INT(msg_bad_xz_fmt));
  if (len_line <= pl_line.len) die(ERR_INT("line too long"));
  pl_line.p[pl_line.len] = '\0';
  char *endptr;
  long int v = strtol(line, &endptr, 10);
  if (endptr == line || *endptr != '\0' || v == LONG_MAX || v == LONG_MIN)
    die(ERR_INT(msg_bad_wght));
  _version = static_cast<uint>(v);
  
  // read weights and detect dimensions
  uint counter = 0;
  vector<pair<uint, row_t>> vec;
  for (uint counter = 0;; ++counter) {
    if (counter == 1024U) die(ERR_INT(msg_bad_wght));
    pair<uint, row_t> row = read_row(ifs, xzd, line, len_line);
    if (row.first == 0) break;
    vec.emplace_back(move(row)); }
  _digest = xzd.get_crc64();
  
  constexpr uint nrow_input = 4U;
  constexpr uint nrow_head  = 14U;
  uint nrow = static_cast<uint>(vec.size());
  if (nrow < nrow_input + nrow_head) die(ERR_INT(msg_bad_wght));
  
  uint nrow_res = nrow - nrow_input - nrow_head;
  if (nrow_res % 8U) die(ERR_INT(msg_bad_wght));
  
  uint index   = 0;
  
  // load body part
  Conv_3x3 conv_3x3;
  BNorm bn;
  uint nch_in  = NN::nch_input;
  uint nch_out = vec[1].first;
  _maxsize_out = nch_out * NN::size_plane;
  for (uint u = 0; u < 1U + nrow_res / 4U; ++u) {
    if (vec[index].first != nch_in * nch_out * 9U
	|| vec[index + 1U].first != nch_out
	|| vec[index + 2U].first != nch_out
	|| vec[index + 3U].first != nch_out) die(ERR_INT(msg_bad_wght));
    conv_3x3.reset(nch_in, nch_out,
		   move(vec[index].second),
		   move(vec[index + 1U].second));
    bn.reset(nch_out,
	     move(vec[index + 2U].second),
	     move(vec[index + 3U].second));
    index += 4U;
    preprocess(conv_3x3, bn);
    _body.emplace_back(move(conv_3x3), move(bn));
    nch_in = nch_out; }
  
  uint nch_body = nch_in;
  
  // load policy part
  nch_out = vec[index + 1U].first;
  if (vec[index].first != nch_body * nch_out
      || nch_out != vec[index+2U].first
      || nch_out != vec[index+3U].first) die(ERR_INT(msg_bad_wght));
  _conv_head_plcy1.reset(nch_body, nch_out,
			 move(vec[index].second),
			 move(vec[index + 1U].second));
  _bn_head_plcy1.reset(nch_out,
		       move(vec[index + 2U].second),
		       move(vec[index + 3U].second));
  _maxsize_out = max(_maxsize_out, nch_out * NN::size_plane);
  preprocess(_conv_head_plcy1, _bn_head_plcy1);
  index += 4U;

  nch_in  = nch_out;
  nch_out = vec[index + 1U].first;
  if (vec[index].first != nch_in * nch_out || nch_out != NN::nch_out_policy)
    die(ERR_INT(msg_bad_wght));
  _conv_head_plcy2.reset(nch_in, nch_out,
			 move(vec[index].second),
			 move(vec[index + 1U].second));
  _maxsize_out = max(_maxsize_out, nch_out * NN::size_plane);
  index += 2U;
  
  // load value part
  nch_out = vec[index + 1U].first;
  if (vec[index].first != nch_body * nch_out
      || nch_out != vec[index + 2U].first
      || nch_out != vec[index + 3U].first) die(ERR_INT(msg_bad_wght));
  _conv_head_vl1.reset(nch_body, nch_out,
		       move(vec[index].second), move(vec[index+ 1U].second));
  _bn_head_vl1.reset(nch_out,
		     move(vec[index + 2U].second),
		     move(vec[index + 3U].second));
  preprocess(_conv_head_vl1, _bn_head_vl1);
  _maxsize_out = max(_maxsize_out, nch_out * NN::size_plane);
  index += 4U;

  nch_in  = nch_out;
  nch_out = vec[index + 1U].first;
  if (vec[index].first != NN::size_plane * nch_in * nch_out)
    die(ERR_INT(msg_bad_wght));
  _head_vl2.reset(NN::size_plane * nch_in, nch_out,
		  move(vec[index].second), move(vec[index + 1U].second));
  index += 2;
  _maxsize_out = max(_maxsize_out, nch_out);
  
  nch_in  = nch_out;
  nch_out = vec[index + 1U].first;
  if (vec[index].first != nch_in * nch_out) die(ERR_INT(msg_bad_wght));
  _head_vl3.reset(nch_in, nch_out,
		  move(vec[index].second), move(vec[index + 1U].second));
  index += 2;
  _maxsize_out = max(_maxsize_out, 1U); }

float NNet::ff(const float *input, uint size_nnmove, ushort *nnmoves,
	       float *prob) noexcept {
  assert(input && 0 < size_nnmove && nnmoves && prob);
  
  // feed forward input layers
  float *fin     = fslot[0].get();
  float *fout    = fslot[1].get();
  float *fbypass = fslot[2].get();

  _body[0].first.ff(input, fout);
  _body[0].second.ff_relu(fout);
  uint nf = _body[0].second.get_nio() * NN::size_plane;
  for (uint ulayer = 1U; ulayer < _body.size(); ulayer += 2U) {
    swap(fin, fout);
    copy_n(fin, nf, fbypass);
    _body[ulayer].first.ff(fin, fout);
    _body[ulayer].second.ff_relu(fout);
    
    swap(fin, fout);
    _body[ulayer + 1U].first.ff(fin, fout);
    _body[ulayer + 1U].second.ff_relu(fout, fbypass); }
  
  float *fout_body = fbypass;
  swap(fout_body, fout);
  _conv_head_plcy1.ff(fout_body, fout);
  _bn_head_plcy1.ff_relu(fout);
  
  swap(fin, fout);
  _conv_head_plcy2.ff(fin, fout);

  assert(_conv_head_plcy2.get_nout() == NN::nch_out_policy);
  for (uint u = 0; u < size_nnmove; ++u) {
    assert(nnmoves[u] < NN::nch_out_policy * NN::size_plane);
    prob[u] = fout[nnmoves[u]]; }
  softmax(size_nnmove, prob);

  _conv_head_vl1.ff(fout_body, fout);
  _bn_head_vl1.ff_relu(fout);

  swap(fin, fout);
  _head_vl2.ff_relu(fin, fout);
  
  swap(fin, fout);
  _head_vl3.ff(fin, fout);
  assert(_head_vl3.get_nout() == 1U);
  return std::tanh(fout[0]); }
