// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace Config {
  void read(const char *fname, std::map<std::string, std::string> &m);
  const char *get_cstr(const std::map<std::string, std::string> &m,
		       const char *p, size_t maxlen);
  template<typename T>
  T get(const std::map<std::string, std::string> &m, const char *p,
	bool (*ok)(T) = [](T){ return true; });
  
  template<typename T>
  std::vector<T> getv(const std::map<std::string, std::string> &m,
		      const char *p, bool (*ok)(T) = [](T){ return true; });

  std::vector<std::string>
  get_vecstr(const std::map<std::string, std::string> &m, const char *p);
}

namespace Opt {
  int get(int argc, const char * const *argv, const char *opts) noexcept;
  extern const char *arg, *cmd;
  extern int ind, err, opt;
}
