// 2019 Team AobaZero
// This source code is in the public domain.
#include "shogi.h"

/* PRNG based on Mersenne Twister (M. Matsumoto and T. Nishimura, 1998). */
#define RAND_M 397
#define MASK_U 0x80000000U
#define MASK_L 0x7fffffffU
#define MASK32 0xffffffffU

void
ini_rand( unsigned int u )
{
  int i;

  rand_work.count   = RAND_N;
  rand_work.cnst[0] = 0;
  rand_work.cnst[1] = 0x9908b0dfU;

  u &= MASK32;
  rand_work.vec[0] = u;
  for ( i = 1; i < RAND_N; i++ ) 
    {
      u  = i + 1812433253U * ( u ^ ( u >> 30 ) );
      u &= MASK32;
      rand_work.vec[i] = u;
    }
}


unsigned int
rand32( void )
{
  unsigned int u, u0, u1, u2;
  int i;

  if ( rand_work.count == RAND_N )
    {
      rand_work.count = 0;

      for ( i = 0; i < RAND_N-RAND_M; i++ )
	{
	  u  = rand_work.vec[i]   & MASK_U;
	  u |= rand_work.vec[i+1] & MASK_L;
	  
	  u0 = rand_work.vec[ i + RAND_M ];
	  u1 = u >> 1;
	  u2 = rand_work.cnst[ u & 1 ];

	  rand_work.vec[i] = u0 ^ u1 ^ u2;
	}
	  
      for ( ; i < RAND_N-1 ;i++ )
	{
	  u  = rand_work.vec[i]   & MASK_U;
	  u |= rand_work.vec[i+1] & MASK_L;

	  u0 = rand_work.vec[ i + RAND_M - RAND_N ];
	  u1 = u >> 1;
	  u2 = rand_work.cnst[ u & 1 ];

	  rand_work.vec[i] = u0 ^ u1 ^ u2;
        }

      u  = rand_work.vec[RAND_N-1] & MASK_U;
      u |= rand_work.vec[0]        & MASK_L;

      u0 = rand_work.vec[ RAND_M - 1 ];
      u1 = u >> 1;
      u2 = rand_work.cnst[ u & 1 ];

      rand_work.vec[RAND_N-1] = u0 ^ u1 ^ u2;
    }
  
  u  = rand_work.vec[ rand_work.count++ ];
  u ^= ( u >> 11 );
  u ^= ( u <<  7 ) & 0x9d2c5680U;
  u ^= ( u << 15 ) & 0xefc60000U;
  u ^= ( u >> 18 );

  return u;
}


uint64_t
rand64( void )
{
  uint64_t h = rand32();
  uint64_t l = rand32();

  return l | ( h << 32 );
}
