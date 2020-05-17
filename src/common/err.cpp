// 2019 Team AobaZero
// This source code is in the public domain.
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include "err.hpp"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <iostream>
using std::cerr;
using std::endl;
using std::terminate;
using std::exception;

void ErrAux::die(const exception &e) noexcept {
  try { cerr << "\n" << e.what() << endl; }
  catch (...) {}
  terminate();
}

ErrCLL::ErrCLL(int line, const char *fname, const char *func, int no)
noexcept {
  assert(fname && func);
  snprintf(_buf, sizeof(_buf), "at line %d in %s: %s() failed (%d: %s)",
	   line, fname, func, no, strerror(no));
}

ErrInt::ErrInt(int line, const char *fname, const char *fmt, ...) noexcept {
  assert(fname && fmt);
  int len(snprintf(_buf, sizeof(_buf), "at line %d in %s: ", line, fname));
  va_list args;
  va_start(args, fmt);
  vsnprintf(_buf + len, sizeof(_buf) - len, fmt, args);
  va_end(args);
}
