// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "iobase.hpp"
#include "osi.hpp"
#include <vector>

class Pipe {
  using uint = unsigned int;
  std::unique_ptr<class USIEngine []> _children;
  OSI::Selector _selector;
  uint _nchild, _max_csa, _print_csa, _print_speed;
  FName _cname, _dname_csa;
  Pipe() noexcept;
  ~Pipe() noexcept;
  Pipe(const Pipe &) = delete;
  Pipe & operator=(const Pipe &) = delete;
  
public:
  static Pipe & get() noexcept;

  void start(const char *cname, const char *dlog,
	     const std::vector<int> &devices, const char *cstr_csa,
	     uint max_csa, uint print_csa, uint print_speed) noexcept;
  void wait() noexcept;
  void end() noexcept;
};
