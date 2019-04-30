// 2019 Team AobaZero
// This source code is in the public domain.
#include "err.hpp"
#include "osi.hpp"
#include "xzi.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
using std::ifstream;
using std::ofstream;
using std::ios;
using std::string;
using ErrAux::die;
using uint = unsigned int;
static void extract(const char *fname) noexcept;
constexpr char ftmp[] = "tmp.csa.x_";

int main(int argc, char **argv) {
  while (*++argv) {
    struct stat sb;
    if (stat(*argv, &sb) < 0) die(ERR_CLL("stat() failed"));
    if ((sb.st_mode & S_IFMT) != S_IFREG) {
      std::cout << *argv << " is not a regular file." << std::endl;
      argv += 1;
      continue; }

    extract(*argv); }
  return 0; }

static void extract(const char *fname) noexcept {
  assert(fname);
  ifstream ifs(fname, ios::binary);
  if (!ifs) die(ERR_INT("cannot read %s", fname));
  
  XZDecode<ifstream, PtrLen<char>> xzd;
  xzd.init();

  uint nline_tot = 0;
  uint nline     = 0;
  uint nplay     = 0;
  bool eof       = false;
  string str_rec, str_fname;

  do {
    char line[65536];
    PtrLen<char> pl_line(line, 0);
      
    if (!xzd.getline(&ifs, &pl_line, sizeof(line) - 1U, "\n"))
      die(ERR_INT("bad XZ format"));
    
    if (pl_line.len == 0) eof = true;
    else nline_tot += 1U;
    
    if (sizeof(line) <= pl_line.len) die(ERR_INT("line too long"));
    line[pl_line.len] = '\0';

    if (line[0] == '/' || eof) {
      std::cout << str_fname  << " " << nline << std::endl;
      ofstream ofs(ftmp, ios::binary | ios::trunc);
      XZEncode<PtrLen<const char>, ofstream> xze;
      PtrLen<const char> pl(str_rec.c_str(), str_rec.size());
      xze.start(&ofs, SIZE_MAX, 9);
      xze.append(&pl);
      xze.end();
      ofs.close();
      if (!ofs) die(ERR_INT("cannot write to %s", ftmp));
      if (rename(ftmp, str_fname.c_str()) < 0) die(ERR_CLL("rename"));

      str_rec = string("");
      nplay  += 1;
      nline   = 0; }
    else {
      str_rec += line;
      str_rec += "\n";
      nline   += 1U;
      if (nline == 1U) {
	char *saveptr;
	char *token = OSI::strtok(line, "' ", &saveptr);
	if (strlen(token) != 14U) die(ERR_INT("bad record number"));
	str_fname = string(token);
	str_fname += ".csa.xz"; } }
  } while (!eof);
  
  std::cout << "Line: " << nline_tot << std::endl;
  std::cout << "Play: " << nplay << std::endl; }
