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
  std::queue<std::string> _moves_id0;
  std::vector<std::string> _devices_str;
  std::vector<class Device> _devices;
  std::vector<std::unique_ptr<class USIEngine>> _engines;
  uint _max_csa, _ngen_records, _verbose_eng;
  FName _cname, _logname;
  PlayManager() noexcept;
  ~PlayManager() noexcept;
  PlayManager(const PlayManager &) = delete;
  PlayManager & operator=(const PlayManager &) = delete;
  
public:
  static PlayManager & get() noexcept;
  void start(const char *cname, const char *dlog,
	     std::vector<std::string> &&devices, uint verbose_eng) noexcept;
  void engine_start(const FNameID &wfname, uint64_t crc64) noexcept;
  void engine_terminate() noexcept;
  std::deque<std::string> manage_play(bool has_conn) noexcept;
  void end() noexcept;
  bool get_moves_id0(std::string &move) noexcept;
  uint get_ngen_records() const noexcept { return _ngen_records; };
};
