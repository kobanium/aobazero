// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "iobase.hpp"
#include "xzi.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
using ErrAux::die;

void do_from_stdin() noexcept;
void do_from_cmdline(char **argv) noexcept;

int main(int argc, char **argv) {
  std::cout << std::setfill('0') << std::hex;
  if (argc == 1) do_from_stdin();
  else           do_from_cmdline(argv);
  return 0; }

void do_from_stdin() noexcept {
  uint64_t crc64_value = 0;
  do {
    char buf[BUFSIZ];
    size_t size = std::cin.read(buf, sizeof(buf)).gcount();
    crc64_value = XZAux::crc64(buf, size, crc64_value);
  } while(!std::cin.eof());

  std::cout << std::setw(16) << crc64_value << std::setw(0);
  std::cout << " -" << std::endl; }

void do_from_cmdline(char **argv) noexcept {
  while (*++argv) {
    struct stat sb;
    if (stat(*argv, &sb) < 0) die(ERR_CLL("stat() failed"));
    if ((sb.st_mode & S_IFMT) != S_IFREG) {
      std::cout << *argv << " is not a regular file." << std::endl;
      argv += 1;
      continue; }
    
    std::cout << std::setw(16) << XZAux::crc64(FName(*argv)) << std::setw(0);
    std::cout << " " << *argv << std::endl; } }
