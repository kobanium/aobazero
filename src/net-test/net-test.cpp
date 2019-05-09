// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
using namespace ErrAux;

static void do_test(std::istream &ifs) noexcept;

int main(int argc, char **argv) {
  if (argc == 1) {
    do_test(std::cin);
    return 0; }
  
  while (*++argv) {
    std::ifstream ifs(*argv);
    if (!ifs) die(ERR_INT("cannot open %s", *argv));
    do_test(ifs); }
  return 0; }
  
static void do_test(std::istream &ifs) noexcept {
  std::string string_line;
  while (std::getline(ifs, string_line)) {
    std::unique_ptr<char []> buf(new char [string_line.size() + 1]);
  } }
