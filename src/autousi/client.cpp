// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "client.hpp"
#include "err.hpp"
#include "jqueue.hpp"
#include "option.hpp"
#include "osi.hpp"
#include "param.hpp"
#include "xzi.hpp"
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstring>
using std::cerr;
using std::cout;
using std::current_exception;
using std::endl;
using std::exception;
using std::exception_ptr;
using std::ifstream;
using std::ios;
using std::lock_guard;
using std::make_shared;
using std::map;
using std::min;
using std::mutex;
using std::ofstream;
using std::rethrow_exception;
using std::set;
using std::set_terminate;
using std::string;
using std::shared_ptr;
using std::thread;
using std::unique_ptr;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using ErrAux::die;
using namespace IOAux;
using uchar = unsigned char;
using uint  = unsigned int;

constexpr uint snd_retry_interval  = 5U;    // in sec
constexpr uint snd_max_retry       = 3U;    // in sec
constexpr uint snd_sleep           = 200U;  // in msec
constexpr uint wght_polling        = 30U;  // in sec
constexpr uint wght_retry_interval = 7U;    // in sec

constexpr uint maxlen_rec_xz       = 1024U * 1024U;
constexpr uint len_head            = 5U;
constexpr char info_name[]         = "info.txt";
constexpr char tmp_name[]          = "tmp.bi_";
constexpr char corrupt_fmt[]       = ( "Corruption of temporary files. "
				       "Cleeanup files in %s/" );
constexpr char tmp_fmt[]           = "tmp%012" PRIi64 ".bi_";
constexpr char wght_xz_fmt[]       = "w%012" PRIi64 ".txt.xz";
constexpr char fmt_tmp_scn[]       = "tmp%16[^.].bi_";
constexpr char fmt_wght_xz_scn[]   = "w%16[^.].txt.xz";

set<FNameID> WghtFile::_all;
mutex WghtFile::_m;
void WghtFile::cleanup() {
  lock_guard<mutex> lock(_m);
  for (auto it = _all.begin(); it != _all.end(); it = _all.erase(it))
    remove(it->get_fname()); }

WghtFile::WghtFile(const FNameID &fxz, uint keep_wght) noexcept
  : _fname(fxz), _keep_wght(keep_wght) {
  uint len = _fname.get_len_fname();
  const char *p = _fname.get_fname();
  if (len < 4U || p[len-3U] != '.' || p[len-2U] != 'x' || p[len-1U] != 'z')
    die(ERR_INT("bad file name %s", p));
  _fname.cut_fname(3U);
  
  ifstream ifs(fxz.get_fname(), ios::binary);
  if (!ifs) die(ERR_INT("cannot open %s", fxz.get_fname()));
  
  ofstream ofs(_fname.get_fname(), ios::binary | ios::trunc);
  XZDecode<ifstream, ofstream> xzd;
  if (!xzd.decode(&ifs, &ofs, SIZE_MAX))
    die(ERR_INT("cannot decode %s", fxz.get_fname()));
  
  ifs.close();
  ofs.close();
  if (!ofs) die(ERR_INT("cannot write to %s", _fname.get_fname()));

  _crc64 = xzd.get_crc64();
  if (_keep_wght) return;
  lock_guard<mutex> lock(_m);
  _all.insert(_fname); }

WghtFile::~WghtFile() noexcept {
  if (_keep_wght) return;
  lock_guard<mutex> lock(_m);
  remove(_fname.get_fname());
  _all.erase(_fname); }

static uint16_t receive_header(const OSI::Conn &conn, uint TO, uint bufsiz) {
  char buf[8];
  conn.recv(buf, 8, TO, bufsiz);
  
  uint Major = static_cast<uchar>(buf[0]);
  uint Minor = static_cast<uchar>(buf[1]);
  if (Major != Ver::major || Ver::minor < Minor)
    die(ERR_INT("Please update autousi!"));
  
  return bytes_to_int<uint16_t>(buf + 2); }

Client::Client() noexcept
: _quit(false), _has_conn(false), _downloading(false), _nsend(0), _ndiscard(0),
  _wght_id(-1), _ver_engine(-1) {
  _buf_wght_time[0] = '\0'; }

Client::~Client() noexcept {}

void Client::get_new_wght() {
  // get new weight information
  OSI::Conn conn(_saddr.get(), _port);
  _ver_engine = receive_header(conn, _recvTO, _recv_bufsiz);

  char buf[BUFSIZ];
  static_assert(12 <= BUFSIZ, "BUSIZ too small");
  buf[0] = 1;
  conn.send(buf, 1, _sendTO, _send_bufsiz);
  conn.recv(buf, 12, _recvTO, _recv_bufsiz);
  make_time_stamp(_buf_wght_time, sizeof(_buf_wght_time), "%D %T");
  
  int64_t no_wght = bytes_to_int<int64_t>(buf);
  uint nblock     = bytes_to_int<uint>(buf + 8);
  if (nblock == 0) die(ERR_INT("invalid nblock value %d", nblock));
  if (no_wght < 0) die(ERR_INT("invalid weight no %" PRIi64 , no_wght));
  if (no_wght == _wght_id) return;
  if (no_wght < _wght_id) die(ERR_INT(corrupt_fmt, _dwght.get_fname()));

  // get old weight information file
  _downloading = true;
  int64_t no_wght_tmp;
  FName finfo(_dwght.get_fname(), info_name);
  try {
    map<string, string> m = {{"WeightNo", "0"}};
    Config::read(finfo.get_fname(), m);
    no_wght_tmp = Config::get<int64_t>(m, "WeightNo",
				       [](int64_t v){ return 0 <= v; }); }
  catch (const ErrInt &e) {
    cout << "\n" << e.what() << endl;
    no_wght_tmp = 0; }

  // update information file
  set<FNameID> set_tmp;
  grab_files(set_tmp, _dwght.get_fname(), fmt_tmp_scn, 0);
  if (no_wght_tmp != no_wght) {
    // cleanup all of old temporary files
    for (auto it = set_tmp.begin(); !set_tmp.empty(); it = set_tmp.erase(it))
      if (remove(it->get_fname()) < 0) die(ERR_CLL("remove"));
    
    ofstream ofs(finfo.get_fname());
    cout << "create new " << info_name << endl;
    ofs << "WeightNo " << no_wght << "\n";
    ofs.close();
    if (!ofs) {
      remove(finfo.get_fname());
      die(ERR_INT("cannot write to %s", finfo.get_fname())); } }
  else if (!set_tmp.empty()) {
    auto it = set_tmp.rbegin();
    if (nblock <= it->get_id())
      die(ERR_INT(corrupt_fmt, _dwght.get_fname())); }
  
  // flag existing blocks
  unique_ptr<bool []> flags(new bool [nblock]);
  for (uint u = 0; u < nblock; ++u) flags[u] = false;
  for (auto it = set_tmp.begin(); it != set_tmp.end(); ++it) {
    int64_t i64(it->get_id());
    assert(0 <= i64);
    assert(i64 < static_cast<int64_t>(nblock));
    flags[i64] = true; }
    
  // obtain blocks
  for (uint iblock = 0; iblock < nblock; ++iblock) {
    if (flags[iblock]) continue;
    buf[0] = 2;
    int_to_bytes<uint>(iblock, buf + 1);
    conn.send(buf, 5, _sendTO, _send_bufsiz);
    conn.recv(buf, 4, _recvTO, _recv_bufsiz);
    size_t len = bytes_to_int<uint>(buf);
    unique_ptr<char []> block(new char [len]);
    conn.recv(block.get(), len, _recvTO, _recv_bufsiz);

    FName ftmp(_dwght);
    ftmp.add_fmt_fname(tmp_fmt, iblock);
    
    ofstream ofs(ftmp.get_fname(), ios::binary);
    ofs.write(block.get(), len);
    ofs.close();
    if (!ofs) {
      remove(ftmp.get_fname());
      die(ERR_INT("cannot write to %s", ftmp.get_fname())); }
    _retry_count = 0; }
  
  grab_files(set_tmp, _dwght.get_fname(), fmt_tmp_scn, 0);
  if (set_tmp.size() != nblock) die(ERR_INT(corrupt_fmt, _dwght.get_fname()));

  // gather all temporaries
  FName ftmp(_dwght.get_fname(), tmp_name);
  ofstream ofs(ftmp.get_fname(), ios::binary);
  for (auto it = set_tmp.begin(); it != set_tmp.end(); ++it) {
    ifstream ifs(it->get_fname(), ios::binary);
    if (!ifs) die(ERR_INT("cannot read %s", it->get_fname()));
    
    while (true) {
      size_t len = ifs.read(buf, sizeof(buf)).gcount();
      if (len == 0) break;
      ofs.write(buf, len); } }
  
  ofs.close();
  if (!ofs) {
    remove(ftmp.get_fname());
    die(ERR_INT("cannot write to %s", ftmp.get_fname())); }

  FNameID fwght(no_wght, _dwght);
  fwght.add_fmt_fname(wght_xz_fmt, no_wght);
  if (rename(ftmp.get_fname(), fwght.get_fname()) < 0) die(ERR_CLL("rename"));
  
  for (auto it = set_tmp.begin(); !set_tmp.empty(); it = set_tmp.erase(it))
    if (remove(it->get_fname()) < 0) die(ERR_CLL("remove"));

  _wght_id = no_wght;

  lock_guard<mutex> lock(_m_wght);
  _wght = make_shared<const WghtFile>(fwght, _keep_wght); }

void Client::reader() noexcept {
  uint sec_wght  = 0;
  uint sec_retry = 0;
  bool do_retry  = false;
  _retry_count = 0;
  while (!_quit) {
    if (do_retry) sec_retry += 1;
    else          sec_wght  += 1;
    sleep_for(seconds(1));

    if ( do_retry && sec_retry < wght_retry_interval) continue;
    if (!do_retry && sec_wght  < wght_polling) continue;
    sec_wght = sec_retry = 0;
    
    try {
      get_new_wght();
      _retry_count = 0;
      _downloading = false;
      _has_conn    = true;
      do_retry     = false;
      continue; }
    catch (const exception &e) { cout << "\n" << e.what() << endl; }
    
    _downloading = false;
    _has_conn    = false;
    if (_max_retry < ++_retry_count)
      die(ERR_INT("retry count %u/%u, abort", _retry_count, _max_retry));
    cout << "\nconnect again ..." << endl;
    do_retry = true; } }

void Client::sender() noexcept {
  char buf[len_head];
  buf[0] = 0;

  while (true) {
    Job *pJob = _pJQueue->pop();
    if (!pJob) break;

    int_to_bytes<uint32_t>(static_cast<uint32_t>(pJob->get_len()), buf + 1);
    uint retry_count = 0;
    uint sec         = snd_retry_interval;
    while (!_quit) {
      if (sec < snd_retry_interval) {
	sec += 1U;
	sleep_for(seconds(1));
	continue; }
      
      try {
	OSI::Conn conn(_saddr.get(), _port);
	_ver_engine = receive_header(conn, _recvTO, _recv_bufsiz);
	conn.send(buf, len_head, _sendTO, _send_bufsiz);
	conn.send(pJob->get_p(), pJob->get_len(), _sendTO, _send_bufsiz);
	_nsend += 1U;
	_has_conn = true;
	pJob->reset();
	sleep_for(milliseconds(snd_sleep));
	break; }
      catch (const exception &e) { cout << "\n" << e.what() << endl; }

      _has_conn = false;
      if (snd_max_retry < ++retry_count) {
	cout << "\nfailed to send a game record" << endl;
	_ndiscard += 1U;
	pJob->reset();
	break; }
      else {
	cout << "\nconnect again ..." << endl;
	sec = 0; } } } }

Client & Client::get() noexcept {
  static Client instance;
  return instance; }

void Client::start(const char *dwght, const char *cstr_addr, uint port,
		   uint recvTO, uint recv_bufsiz, uint sendTO,
		   uint send_bufsiz, uint max_retry, uint size_queue,
		   uint keep_wght) noexcept {
  assert(dwght && cstr_addr);
  _dwght       = FName(dwght);
  _recvTO      = recvTO;
  _recv_bufsiz = recv_bufsiz;
  _sendTO      = sendTO;
  _send_bufsiz = send_bufsiz;
  _max_retry   = max_retry;
  _keep_wght   = keep_wght;
  _port        = port;
  FNameID fname = grab_max_file(_dwght.get_fname(), fmt_wght_xz_scn);
  _wght_id = fname.get_id();
  if (0 <= _wght_id ) _wght = make_shared<const WghtFile>(fname, _keep_wght);

  _saddr.reset(new char [strlen(cstr_addr) + 1U]);
  strcpy(_saddr.get(), cstr_addr);
  _prec_xz.reset(new char [maxlen_rec_xz]);
  _pJQueue.reset(new JQueue<Job>(size_queue));
  uint retry_count = 0;
  while (true) {
    try { get_new_wght(); break; }
    catch (const exception &e) { cout << "\n" << e.what() << endl; }
    if (_max_retry < ++retry_count)
      die(ERR_INT("retry count %u/%u, abort", retry_count, _max_retry));
    cout << "\nconnect again ..." << endl;
    sleep_for(seconds(wght_retry_interval)); }
  
  _downloading   = false;
  _has_conn      = true;  
  _thread_reader = thread(&Client::reader, this);
  _thread_sender = thread(&Client::sender, this); }

void Client::end() noexcept {
  _quit = true;
  _pJQueue->end();
  _thread_reader.join();
  _thread_sender.join(); }

void Client::add_rec(const char *p, size_t len) noexcept {
  XZEncode<PtrLen<const char>, PtrLen<char>> xze;
  PtrLen<const char> pl_in(p, len);
  PtrLen<char> pl_out(_prec_xz.get(), 0);
  xze.start(&pl_out, maxlen_rec_xz, 9);
  if (!xze.append(&pl_in) || !xze.end() || UINT32_MAX < pl_out.len) {
    cout << "\ntoo large compressed image of CSA record" << endl;
    return; }
  
  Job *pjob = _pJQueue->get_free();
  pjob->reset(pl_out.len);
  memcpy(pjob->get_p(), _prec_xz.get(), pl_out.len);
  _pJQueue->push_free(); }

shared_ptr<const WghtFile> Client::get_wght() noexcept {
  lock_guard<mutex> lock(_m_wght);
  return _wght; }
