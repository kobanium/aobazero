// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "osi.hpp"
#include <vector>
#include <queue>
#include <string>

class Pipe {
  using uint = unsigned int;
  std::queue<std::string> _moves_id0;
  std::unique_ptr<class USIEngine []> _children;
  OSI::Selector _selector;
  uint _nchild, _max_csa, _ngen_records;
  FName _cname, _dname_csa;
  Pipe() noexcept;
  ~Pipe() noexcept;
  Pipe(const Pipe &) = delete;
  Pipe & operator=(const Pipe &) = delete;
  bool play_update(char *line, class USIEngine &c) noexcept;
  
public:
  static Pipe & get() noexcept;
  void start(const char *cname, const char *dlog,
	     const std::vector<int> &devices, const char *cstr_csa,
	     uint max_csa) noexcept;
  void wait() noexcept;
  void end() noexcept;
  bool get_moves_id0(std::string &move) noexcept;
  
  bool is_closed(uint u) const noexcept;
  uint get_pid(uint u) const noexcept;
  uint get_nmove(uint u) const noexcept;
  double get_speed_average(uint u) const noexcept;
  uint get_ngen_records() const noexcept { return _ngen_records; };
};
