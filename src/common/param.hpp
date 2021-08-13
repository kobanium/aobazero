// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
namespace Ver {
  constexpr unsigned char major       = 1;
  constexpr unsigned char minor       = 8;
  constexpr unsigned short usi_engine = 14;
}

namespace Param {
  using uint = unsigned int;
  constexpr uint maxnum_child         = 1024U;
  constexpr uint maxlen_play_learn    = 513U;
  constexpr uint maxlen_play          = 4096U;
  // 81*81*2 + (81*7) = 13122 + 567 = 13689 * 512 = 7008768
  constexpr uint len_seq_prn          = 7008768U;
  constexpr char name_autousi[]       = "/tmp/autousi.jBQoNA7kEd";
  constexpr char name_server[]        = "/tmp/server.jBQoNA7kEd";
  constexpr char name_sem_nnet[]      = "/sn.jBQoNA7kEd";
  constexpr char name_sem_lock_nnet[] = "/sln.jBQoNA7kEd";
  constexpr char name_seq_prn[]       = "/msp.jBQoNA7kEd";
  constexpr char name_mmap_nnet[]     = "/mn.jBQoNA7kEd";
}
