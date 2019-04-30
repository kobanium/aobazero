// Copyright (C) 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <iostream>
using std::cerr;
using std::endl;
using std::terminate;
using std::exception;

void Err::die(const exception &e) noexcept {
  try { cerr << e.what() << endl; }
  catch (...) {}
  terminate();
}

ErrCLL::ErrCLL(int line, const char *fname, const char *func, int no)
  noexcept {
  assert(fname);
  assert(func);
  snprintf(_buf, sizeof(_buf), "at line %d in %s: %s() failed (%d: %s)",
	   line, fname, func, no, strerror(no));
}

ErrInt::ErrInt(int line, const char *fname, const char *fmt, ...)
  noexcept {
  assert(fname);
  assert(fmt);
  int len(snprintf(_buf, sizeof(_buf), "at line %d in %s: ", line, fname));
  va_list args;
  va_start(args, fmt);
  vsnprintf(_buf + len, sizeof(_buf) - len, fmt, args);
  va_end(args);
}
