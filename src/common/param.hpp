// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
namespace Ver {
  constexpr unsigned char major       = 1;
  constexpr unsigned char minor       = 11;	// 10...2021/09/30, 11...2022/01/10 kldgain
  constexpr unsigned short usi_engine = 18;
}

#define AOBA_UNIQUE ".jBQoNA7kEd"

namespace Param {
  using uint = unsigned int;
  constexpr uint maxnum_child         = 1024U;
  constexpr uint maxlen_play_learn    = 513U;
  constexpr uint maxlen_play          = 4096U;
  // 81*81*2 + (81*7) = 13122 + 567 = 13689 * 512 = 7008768
  constexpr uint len_seq_prn          = 7008768U;
//constexpr char name_autousi[]       = "/tmp/autousi"   AOBA_UNIQUE;
  constexpr char name_server[]        = "/tmp/server"    AOBA_UNIQUE;
  constexpr char name_sem_nnet[]      = "/sem-nnet"      AOBA_UNIQUE;
  constexpr char name_sem_lock_nnet[] = "/sem-lock-nnet" AOBA_UNIQUE;
  constexpr char name_seq_prn[]       = "/mmap-seq-prn"  AOBA_UNIQUE;
  constexpr char name_mmap_nnet[]     = "/mmap-nnet"     AOBA_UNIQUE;
}
