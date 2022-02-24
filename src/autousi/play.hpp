// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include <memory>
#include <queue>
#include <deque>
#include <string>
#include <vector>
#include <cstdint>

class PlayManager {
  using uint = unsigned int;
  std::queue<std::string> _moves_eid0;
  std::vector<class Device> _devices;
  std::vector<std::unique_ptr<class USIEngine>> _engines;
  int64_t _wid;
  uint _max_csa, _ngen_records, _num_thinking, _verbose_eng, _silent_eng;
  FName _cname, _logname;
  PlayManager() noexcept;
  ~PlayManager() noexcept;
  PlayManager(const PlayManager &) = delete;
  PlayManager & operator=(const PlayManager &) = delete;
  
public:
  static PlayManager & get() noexcept;
  void start(const char *cname, const char *dlog, const char *dtune,
	     const std::vector<std::string> &devices, uint verbose_eng, uint silent_eng,
	     uint sleep_opencl, const FNameID &wfname, uint64_t crc64)
    noexcept;
  void end() noexcept;
  std::deque<std::string> manage_play(bool has_conn, const FNameID &wfname,
				      uint64_t crc64, float th_resign, const uint *phandicap_rate,
				      const uint average_winrate)
    noexcept;
  bool get_moves_eid0(std::string &move) noexcept;
  uint get_nengine() const noexcept {
    return static_cast<uint>(_engines.size()); }
  uint get_ngen_records() const noexcept { return _ngen_records; };
  uint get_nthinking() const noexcept { return _num_thinking; }
  bool get_do_resign(uint u) const noexcept;
  uint get_eid(uint u) const noexcept;
  uint get_nmove(uint u) const noexcept;
  int get_did(uint u) const noexcept;
  double get_time_average(uint u) const noexcept;
};
