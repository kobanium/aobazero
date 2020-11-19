// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "jqueue.hpp"
#include "logging.hpp"
#include "datakeep.hpp"
#include "shogibase.hpp"
#include "hashtbl.hpp"
#include "param.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
using std::fill_n;
using std::ios;
using std::ifstream;
using std::lock_guard;
using std::make_shared;
using std::map;
using std::max;
using std::shared_ptr;
using std::set;
using std::string;
using std::thread;
using std::to_string;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using ErrAux::die;
using namespace IOAux;
using namespace Log;
using uint = unsigned int;

constexpr uint64_t size_cluster  = 10000U;
constexpr char fname_wght_list[] = "weight-list.cfg";
constexpr char fname_ema[]       = "ema.cfg";
constexpr char fname_ema_tmp[]   = "ema.cf_";
constexpr char fname_tmp[]       = "tmp.csa.x_";
constexpr char fmt_arch[]        = "arch%012" PRIi64 ".csa.xz";
constexpr char fmt_pool[]        = "no%012" PRIi64 ".csa.xz";
constexpr char fmt_wght_scn[]    = "w%16[^.].txt.xz";
constexpr char fmt_pool_scn[]    = "no%16[^.].csa.xz";
constexpr uint ema_keep_N        = 64U;
constexpr float ema_keep_max     = 0.32f;
const PtrLen<const char> pl_CSAsepa("/\n", 2);


static bool
examine_record(const char *rec, size_t len_rec, uint64_t &digest,
	       uint &len_play, float &ave_child, bool &flag_no_resign,
	       float &value_min, uint &node_type) noexcept {
  assert(rec);
  constexpr char newline[] = "\n";
  char buf[len_rec + 1U];
  char *saveptr_line;
  char *line;
  float value_min_black, value_min_white;

  value_min_black = 100.0f;
  value_min_white = 100.0f;
  value_min       = 100.0f;
  node_type       = 99U;

  len_play       = 0;
  flag_no_resign = false;
  ave_child = 0.0f;
  strcpy(buf, rec);
  line = strtok_r(buf, "\n", &saveptr_line);
  if (line[0] != '\'' || line[1] != 'w') return false;
  else {
    char *saveptr_token, *endptr, *token;
    uint64_t digest1, digest2;
    int64_t no;
    
    token = strtok_r(line+ 2, " ", &saveptr_token);
    if (!token) return false;

    no = strtoll(token, &endptr, 10);
    if (endptr == token || *endptr != '\0' || no < 0 || no == LLONG_MAX)
      return false;
  
    token = strtok_r(nullptr, "(:", &saveptr_token);
    token = strtok_r(nullptr, ")", &saveptr_token);
    if (!token) return false;

    errno = 0;
    digest1 = strtoull(token, &endptr, 16);
    if (endptr == token || *endptr != '\0' || errno == ERANGE)
      die(ERR_INT("bad crc64 %s", token));

    if (!WghtKeep::get().get_crc64(no, digest2)) return false;
    if (digest1 != digest2) return false;

    token = strtok_r(nullptr, ",", &saveptr_token);
    if (!token) return false;

    token = strtok_r(nullptr, ",", &saveptr_token);
    if (!token) return false;

    token = strtok_r(nullptr, ", ", &saveptr_token);
    if (token) {
      std::cerr << "DBG3: " << token << std::endl;
      if (strcmp(token, "no-resign") == 0) flag_no_resign = true;
      else return false; } }
  
  line = strtok_r(nullptr, "\n", &saveptr_line);
  if (!line || line[0] != '\'') return false;
  
  line = strtok_r(nullptr, "\n", &saveptr_line);
  if (!line || line[0] != 'P' || line[1] != 'I' || line[2] != '\0')
    return false;
  digest = XZAux::crc64(line, 2U, 0);
  digest = XZAux::crc64(newline, 1U, digest);
  
  line = strtok_r(nullptr, "\n", &saveptr_line);
  if (!line || line[0] != '+' || line[1] != '\0') return false;
  digest = XZAux::crc64(line, 1U, digest);
  digest = XZAux::crc64(newline, 1U, digest);
  
  Node<Param::maxlen_play_learn> node;
  uint tot_nchild = 0;
  bool has_result = false;
  enum Result { black_win = 0, draw, black_lose } result = draw;
  while (true) {
    line = strtok_r(nullptr, "\n", &saveptr_line);
    if (!line) break;
    digest = XZAux::crc64(line, digest);
    digest = XZAux::crc64(newline, 1U, digest);
    
    if (!has_result && line[0] == '%' && node.get_type().is_term()
	&& strcmp(line + 1, node.get_type().to_str()) == 0) {
      has_result = true;
      if      (node.get_type() == SAux::illegal_bwin) result = black_win;
      else if (node.get_type() == SAux::illegal_wwin) result = black_lose;
      else if (node.get_type() == SAux::repeated)     result = draw;
      else if (node.get_type() == SAux::maxlen_term)  result = draw;
      else die(ERR_INT("INTERNAL ERROR"));
	
      if      (result == black_win)  value_min = value_min_black;
      else if (result == black_lose) value_min = value_min_white;
      else value_min = std::min(value_min_black, value_min_white);
      node_type = node.get_type().to_u();
      continue; }
    
    const char *token;
    char *saveptr_token, *endptr;
    float value;
    long int num;
    Action action;
    token = strtok_r(line, ",\'", &saveptr_token);
    assert(token);
    action = node.action_interpret(token + 1, SAux::csa);
    if (!action.ok()) return false;
    if (token[0] == '%') { // TORYO or WINDCL
      node.take_action(action);
      token = strtok_r(nullptr, ",\'", &saveptr_token);
      if (!token) {
	if (node.get_type() == SAux::resigned) {
	  if (SAux::black == node.get_turn()) result = black_lose;
	  else                                result = black_win; }
	else if (node.get_type() == SAux::windclrd) {
	  if (SAux::black == node.get_turn()) result = black_win;
	  else                                result = black_lose; }
	else die(ERR_INT("INTERNAL ERROR"));
	
	if      (result == black_win)  value_min = value_min_black;
	else if (result == black_lose) value_min = value_min_white;
	else die(ERR_INT("INTERNAL ERROR"));
	node_type = node.get_type().to_u();
	continue; }

      if (strcmp(token, "autousi") == 0) {
	if (flag_no_resign) return false;
	if (node.get_type() != SAux::resigned) return false;
	node_type = node.get_type().to_u();
	continue; }

      return false; }

    token = strtok_r(nullptr, ",\'", &saveptr_token);
    if (!token || token[0] != 'v' || token[1] != '=') return false;

    token += 2;
    value = strtof(token, &endptr);
    if (endptr == token || *endptr != '\0' || value < 0.0f
	|| value == HUGE_VALF) return false;

    if (node.get_turn() == SAux::black) {
      if (value < value_min_black) value_min_black = value; }
    else {
      if (value < value_min_white) value_min_white = value; }

    token = strtok_r(nullptr, ",\'", &saveptr_token);
    if (!token) return false;

    num = strtol(token, &endptr, 10);
    if (endptr == token || *endptr != '\0' || num < 1 || num == LONG_MAX)
      return false;
    
    while (true) {
      token = strtok_r(nullptr, ",\'", &saveptr_token);
      if (!token) break;
      Action a = node.action_interpret(token, SAux::csa);
      assert(a.ok());
      if (!a.is_move()) return false;
      
      token = strtok_r(nullptr, ",\'", &saveptr_token);
      if (!token) return false;
      num = strtol(token, &endptr, 10);
      if (endptr == token || *endptr != '\0' || num < 1 || num == LONG_MAX)
	return false;
      tot_nchild += 1U; }

    len_play += 1U;
    node.take_action(action); }
  
  if (!node.get_type().is_term()) return false;
  ave_child = (float)tot_nchild / (float)len_play;
  return true; }

static void write_pooltemp(const FName &fxz, PtrLen<const char> pl) noexcept {
  assert(pl.ok());
  ofstream ofs(fxz.get_fname(), ios::binary | ios::trunc);
  XZEncode<PtrLen<const char>, ofstream> xze;
  xze.start(&ofs, SIZE_MAX, 9);
  xze.append(&pl);
  xze.end();
  ofs.close();
  if (!ofs) die(ERR_INT("cannot write to %s", fxz.get_fname())); }

class EMAKeep {
  string _signature;
  float _estimate[ema_keep_N];
  float _th;
  uint _deno;
  float _mrate;

  void compute_th() noexcept {
    float value;
    float step = ( ema_keep_max / static_cast<float>(ema_keep_N) );
    uint u;
    for (u = 0; u < ema_keep_N; u++) {
      if (_mrate < _estimate[u]) break; }

    _th = static_cast<float>(u) * step; }

public:
  explicit EMAKeep(uint deno, float mrate_init, float mrate) noexcept
    : _deno(deno), _mrate(mrate) {
    _signature  = to_string(ema_keep_max) + " ";
    _signature += to_string(ema_keep_N)   + " ";
    _signature += to_string(_deno);

    ifstream ifs(fname_ema);
    bool flag = false;
    if (ifs) {
      char buf[256];
      ifs.getline(buf, sizeof(buf));
      if (string(buf) == _signature) flag = true; }

    if (flag) {
      for (uint u = 0; u < ema_keep_N; u++) ifs >> _estimate[u]; }
    else fill_n(_estimate, ema_keep_N, mrate_init);

    compute_th(); }

  void update(float min) noexcept {
    uint u;
    float value, a, b;
    float step = ( ema_keep_max / static_cast<float>(ema_keep_N) );
    a = static_cast<float>(_deno - 1U) / static_cast<float>(_deno);
    b = 1.0 / static_cast<float>(_deno);

    for (u = 0; u < ema_keep_N; u++) {
      value = static_cast<float>(u + 1U) * step;
      if (min <= value) break;
      _estimate[u] *= a; }
    for (; u < ema_keep_N; u++) _estimate[u] = _estimate[u] * a + b;
    compute_th();
    {
      ofstream ofs(fname_ema_tmp);
      ofs << _signature << "\n";
      for (u = 0; u < ema_keep_N; u++) ofs << _estimate[u] << "\n";
      if (!ofs) die(ERR_INT("cannot write to %s", fname_ema_tmp));
    }
    if (rename(fname_ema_tmp, fname_ema) < 0) die(ERR_CLL("rename")); }

  uint get_th16() const noexcept { return static_cast<uint>(_th * 65536.0f); }
};

bool WghtKeep::get_crc64(int64_t no, uint64_t &digest) const noexcept {
  auto it = _map_wght.find(no);
  if (it == _map_wght.end()) return false;
  digest = it->second;
  return true; }

void WghtKeep::get_new_wght() noexcept {
  set<FNameID> no;
  grab_files(no, _dwght.get_fname(), fmt_wght_scn, _i64_now + 1);
  
  for (auto it = no.rbegin(); it != no.rend(); ++it) {
    int fd = open(it->get_fname(), O_RDONLY);
    if (fd < 0) die(ERR_CLL("open"));
    struct stat sb;
    if (fstat(fd, &sb) < 0) die(ERR_CLL("fstat"));

    shared_ptr<Wght> pw = make_shared<Wght>(it->get_id(), sb.st_size);
    PtrLen<char> pl = pw->ptrlen();
    if (sb.st_size != read(fd, pl.p, pl.len)) die(ERR_CLL("read"));
    close(fd);
    uint64_t digest;
    if (! is_weight_ok(pl, digest)) continue;
    
    _logger->out(nullptr, fmt_found_wght_s, it->get_fname());
    _i64_now = it->get_id();
    if (_map_wght.find(_i64_now) == _map_wght.end()) {
      ofstream ofs(fname_wght_list, ios::app);
      ofs << it->get_bname() << " " << std::hex << std::setw(16) << digest
	  << std::setw(0) << std::dec << std::endl;
      if (!ofs) die(ERR_INT("cannot write to %s", fname_wght_list));
      _map_wght[_i64_now] = digest; }
    
    lock_guard<mutex> lock(_m);
    _pwght = pw;
    break; }
  
  if (_i64_now < 0) die(ERR_INT("no weight found")); }

shared_ptr<const Wght> WghtKeep::get_pw() noexcept {
  lock_guard<mutex> lock(_m);
  return _pwght; }

void WghtKeep::worker() noexcept {
  uint sec(0);
  while (!_bEndWorker) {
    if (sec < _wght_poll) {
      sleep_for(seconds(1));
      sec += 1U;
      continue; }
    get_new_wght();
    sec = 0; } }

WghtKeep & WghtKeep::get() noexcept {
  static WghtKeep instance;
  return instance; }

void WghtKeep::start(Logger *logger, const char *dwght, uint wght_poll)
  noexcept {
  assert(logger && dwght && 0 < wght_poll);
  _logger    = logger;
  _wght_poll = wght_poll;
  _dwght     = FName(dwght);
  
  _logger->out(nullptr, load_list_s, fname_wght_list);
  ifstream ifs(fname_wght_list);
  if (ifs) {
    char buf[1024];
    while (ifs.getline(buf, sizeof(buf))) {
      char *token, *saveptr;
      token = strtok_r(buf, " \t\r", &saveptr);
      if (!token || token[0] == '#') continue;
      int64_t no = match_fname(token, fmt_wght_scn);
      if (no < 0) die(ERR_INT("bad weight name %s", token));
      
      token = strtok_r(nullptr, " \t\r", &saveptr);
      if (!token) die(ERR_INT("bad crc64"));
      errno = 0;
      char *endptr;
      unsigned long long int ull = strtoull(token, &endptr, 16);
      if (endptr == token || *endptr != '\0' || errno == ERANGE)
	die(ERR_INT("bad crc64 %s", token));
      
      uint64_t crc64v = ull;
      _logger->out(nullptr, "%012" PRIi64 " %016" PRIx64, no, crc64v);
      _map_wght[no] = crc64v; } }

  get_new_wght();
  _thread = thread(&WghtKeep::worker, this); }

void WghtKeep::end() noexcept {
  _bEndWorker = true;
  _thread.join(); }

void RecKeep::close_arch_tmp() noexcept {
  _xze.end();
  _ofs_arch_tmp.close();
  if (!_ofs_arch_tmp) die(ERR_INT("cannot write to %s",
				  _farch_tmp.get_fname())); }

RecKeep::RecKeep() noexcept {}
RecKeep::~RecKeep() noexcept {}

void RecKeep::open_arch_tmp() noexcept {
  _ofs_arch_tmp.open(_farch_tmp.get_fname(), ios::binary | ios::trunc);
  if (!_ofs_arch_tmp) die(ERR_INT("cannot write to %s",
				  _farch_tmp.get_fname()));
  _xze.start(&_ofs_arch_tmp, SIZE_MAX, 9);
  _bFirst = true; }

RecKeep & RecKeep::get() noexcept {
  static RecKeep instance;
  return instance; }

void RecKeep::start(Logger *logger, const char *darch, const char *dpool,
		    uint maxlen_job, size_t maxlen_rec, size_t maxlen_recv,
		    uint log2_nindex_redun, uint minlen_play,
		    uint minave_child, uint resign_ema_deno,
		    float resign_ema_init, float resign_mrate) noexcept {
  assert(logger && darch && dpool);
  int64_t i64, i64_start, i64_end;

  _ema_keep_ptr.reset(new EMAKeep(resign_ema_deno, resign_ema_init,
				  resign_mrate));
  _pJQueue.reset(new JQueue<JobIP>(maxlen_job));
  _prec.reset(new char [maxlen_rec]);
  _redundancy_table.reset(log2_nindex_redun, 2U << log2_nindex_redun);
  assert(_redundancy_table.ok());
  
  _logger       = logger;
  _maxlen_rec   = maxlen_rec;
  _minlen_play  = minlen_play;
  _minave_child = minave_child;
  _darch        = FName(darch);
  _dpool        = FName(dpool);
  _farch_tmp    = FName(darch, fname_tmp);
  _fpool_tmp    = FName(dpool, fname_tmp);
  _thread       = thread(&RecKeep::worker, this);
  open_arch_tmp();

  // grab files in the pool
  grab_files(_pool, dpool, fmt_pool_scn, 0);
  if (_pool.empty()) return;
  
  // check sequence of file no.
  i64 = _pool.begin()->get_id();
  for (auto it = _pool.begin(); it != _pool.end(); ++it, ++i64)
    if (i64 != it->get_id()) die(ERR_INT("invalid files in %s", dpool));
  
  // remove reminders
  i64_start = _pool.begin()->get_id();
  if ((i64_start % size_cluster) != 0) {
    i64_end  = i64_start / size_cluster;
    i64_end += INT64_C(1);
    i64_end *= size_cluster;
    if (i64_end <= _pool.rbegin()->get_id())
      for (auto it = _pool.begin();
	   it != _pool.end() && it->get_id() < i64_end; it = _pool.erase(it))
	if (remove(it->get_fname()) < 0) die(ERR_CLL("remove")); }
  if (_pool.empty()) return;
  
  // archive clusters
  XZDecode<ifstream, PtrLen<char>> xzd_pool;
  PtrLen<const char> pl_in;
  while (true) {
    assert(!_pool.empty());
    i64_start = _pool.begin()->get_id();
    i64_end   = i64_start + size_cluster;
    if (_pool.rbegin()->get_id() < i64_end) break;
    
    FName farch(_darch);
    farch.add_fmt_fname(fmt_arch, i64_start);
    ofstream ofs(farch.get_fname(), ios::binary | ios::trunc);
    bool flag_first = true;
    
    _xze.start(&ofs, SIZE_MAX, 9);
    for (i64 = i64_start; i64 < i64_end; ++i64) {
      if (flag_first) flag_first = false;
      else {
	pl_in = pl_CSAsepa;
	_xze.append(&pl_in); }
      
      FName fpool(_dpool);
      fpool.add_fmt_fname(fmt_pool, i64);
      ifstream ifs(fpool.get_fname(), ios::binary);
      if (!ifs) die(ERR_INT("cannot read from %s", fpool.get_fname()));

      PtrLen<char> pl_rec(_prec.get(), 0);
      if (!xzd_pool.decode(&ifs, &pl_rec, maxlen_rec - 1U))
	die(ERR_INT("XZDecode::decode()"));

      ifs.close();
      pl_in = pl_rec;
      _xze.append(&pl_in); }
    
    _xze.end();
    ofs.close();
    if (!ofs) die(ERR_INT("cannot write to %s", farch.get_fname()));

    for (auto it = _pool.begin(); it->get_id() < i64_end; it = _pool.erase(it))
      if (remove(it->get_fname()) < 0) ERR_CLL("remove"); }

  // write to arch's temp file
  i64_start = _pool.begin()->get_id();
  i64_end   = _pool.rbegin()->get_id() + INT64_C(1);
  assert((i64_start % size_cluster) == 0);
  assert(i64_end < i64_start + static_cast<int64_t>(size_cluster));
  
  for (i64 = i64_start; i64 < i64_end; ++i64) {
    if (_bFirst) _bFirst = false;
    else {
      pl_in = pl_CSAsepa;
      _xze.append(&pl_in); }
    
    FName fpool(_dpool);
    fpool.add_fmt_fname(fmt_pool, i64);
    ifstream ifs(fpool.get_fname(), ios::binary);
    if (!ifs) die(ERR_INT("cannot read from %s", fpool.get_fname()));
    
    PtrLen<char> pl_rec(_prec.get(), 0);
    if (!xzd_pool.decode(&ifs, &pl_rec, maxlen_rec - 1U))
      die(ERR_INT("XZDecode::decode()"));
    ifs.close();
    
    pl_in = pl_rec;
    _xze.append(&pl_in);
    if (!_ofs_arch_tmp)
      die(ERR_INT("cannot write to %s", _farch_tmp.get_fname())); } }

uint RecKeep::get_th16() const noexcept { return _ema_keep_ptr->get_th16(); }

void RecKeep::end() noexcept {
  _pJQueue->end();
  _thread.join(); }

void RecKeep::worker() noexcept {
  while (true) {
    JobIP *pjob = _pJQueue->pop();
    if (!pjob) break;
    transact(pjob);
    pjob->reset(); }
  close_arch_tmp(); }

void RecKeep::add(const char *prec, size_t len_rec, const OSI::IAddr & iaddr)
  noexcept {
  assert(prec);

  JobIP *pjob = _pJQueue->get_free();
  pjob->reset(len_rec);
  memcpy(pjob->get_p(), prec, len_rec);
  pjob->set_iaddr(iaddr);
  _pJQueue->push_free(); }

void RecKeep::transact(const JobIP *pjob) noexcept {
  assert(pjob);
  
  // add time stamp
  assert(63U < _maxlen_rec);
  int64_t id = (_pool.empty() ? 0 : _pool.rbegin()->get_id() + INT64_C(1));
  char *p    = _prec.get();
  size_t len_time = 0;
  len_time = snprintf(p, _maxlen_rec - 1U, "'no%012" PRIi64 " ", id);
  len_time += make_time_stamp(p + len_time, _maxlen_rec - len_time - 1U, "%D");
  p[len_time++] = '\n';
  
  // decode received message
  const PtrLen<const char> plxz(pjob->get_p(), pjob->get_len());
  PtrLen<const char> pl_in = plxz;
  PtrLen<char> pl_out(_prec.get() + len_time, 0);
  if (!_xzd.decode(&pl_in, &pl_out, _maxlen_rec - len_time - 1U)) {
    _logger->out(pjob, bad_XZ_format);
    return; }
  assert(len_time + pl_out.len < _maxlen_rec);
  pl_out.p[pl_out.len] = '\0';

  // examine received message
  uint64_t digest;
  uint len_play, node_type;
  float ave_child, value_min;
  bool flag_no_resign;
  if (!examine_record(pl_out.p, pl_out.len, digest, len_play, ave_child,
		      flag_no_resign, value_min, node_type)) {
    _logger->out(pjob, bad_CSA_format);
    return; }
  if (node_type == 99U) die(ERR_INT("INTERNAL ERROR"));
  
  if (len_play < _minlen_play) {
    _logger->out(pjob, "play too short (%u moves)", len_play);
    return; }
  
  if (ave_child < static_cast<float>(_minave_child)) {
    _logger->out(pjob, "too few children (%f per a move)", ave_child);
    return; }
  
  PtrLen<char> pl_rec(_prec.get(), len_time + pl_out.len);

  RedunValue & redun_value = _redundancy_table[Key64(digest)];
  if (1U < ++redun_value.count) {
    _logger->out(pjob, "duplication (crc64:%016" PRIx64 " no.:%" PRIu64
		 " count:%" PRIu64 ")",
		 digest, redun_value.no, redun_value.count);
    return; }
  redun_value.no = static_cast<uint64_t>(id);
  assert(_redundancy_table.ok());
  
  // save the record in pool
  write_pooltemp(_fpool_tmp, pl_rec);
  FNameID fpool(id, _dpool);
  fpool.add_fmt_fname(fmt_pool, id);
  if (rename(_fpool_tmp.get_fname(), fpool.get_fname()) < 0)
    die(ERR_CLL("rename"));
  _pool.insert(fpool);

  char buf[256];
  if (flag_no_resign) {
    _ema_keep_ptr->update(value_min);
    sprintf(buf, "resign:n, min:%.3f, th:%.3f", value_min,
	    _ema_keep_ptr->get_th16() * (1.0f / 65536.0f) );   }
  else sprintf(buf, "resign:y");

  _logger->out(pjob, "record no. %" PRIi64 " arrived (crc64:%016" PRIx64
	       ", ave:%4.1f, len:%3u, type:%u, %s)",
	       id, digest, ave_child, len_play, node_type, buf);

  // archive a cluster if possible
  int64_t i64_start = _pool.begin()->get_id();
  int64_t i64_end   = i64_start + size_cluster;
  if (i64_end <= _pool.rbegin()->get_id()) {
    close_arch_tmp();

    FName farch(_darch);
    farch.add_fmt_fname(fmt_arch, i64_start);
    if (rename(_farch_tmp.get_fname(), farch.get_fname()) < 0)
      die(ERR_CLL("rename"));
    
    for (auto it = _pool.begin();
	 it != _pool.end() && it->get_id() < i64_end; it = _pool.erase(it))
      if (remove(it->get_fname()) < 0) die(ERR_CLL("remove"));
    
    open_arch_tmp(); }

  // write to arch's temp file
  if (_bFirst) _bFirst = false;
  else {
    pl_in = pl_CSAsepa;
    _xze.append(&pl_in); }
  
  pl_in = pl_rec;
  _xze.append(&pl_in);
  if (!_ofs_arch_tmp)
    die(ERR_INT("cannot write to %s", _farch_tmp.get_fname())); }
