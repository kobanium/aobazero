// Team AobaZero
// This source code is in the public domain.
#include "shogi.h"

int CONV
popu_count012( unsigned int u0, unsigned int u1, unsigned int u2 )
{
  int counter = 0;
  while ( u0 ) { counter++;  u0 &= u0 - 1U; }
  while ( u1 ) { counter++;  u1 &= u1 - 1U; }
  while ( u2 ) { counter++;  u2 &= u2 - 1U; }
  return counter;
}


#if defined(_MSC_VER)

int CONV
first_one012( unsigned int u0, unsigned int u1, unsigned int u2 )
{
  unsigned long index;

  if ( _BitScanReverse( &index, u0 ) ) { return 26 - index; }
  if ( _BitScanReverse( &index, u1 ) ) { return 53 - index; }
  _BitScanReverse( &index, u2 );
  return 80 - index;
}

int CONV
last_one210( unsigned int u2, unsigned int u1, unsigned int u0 )
{
  unsigned long index;

  if ( _BitScanForward( &index, u2 ) ) { return 80 - index; }
  if ( _BitScanForward( &index, u1 ) ) { return 53 - index; }
  _BitScanForward( &index, u0 );
  return 26 - index;
}

int CONV
first_one01( unsigned int u0, unsigned int u1 )
{
  unsigned long index;

  if ( _BitScanReverse( &index, u0 ) ) { return 26 - index; }
  _BitScanReverse( &index, u1 );
  return 53 - index;
}

int CONV
first_one12( unsigned int u1, unsigned int u2 )
{
  unsigned long index;
  
  if ( _BitScanReverse( &index, u1 ) ) { return 53 - index; }
  _BitScanReverse( &index, u2 );
  return 80 - index;
}

int CONV
last_one01( unsigned int u0, unsigned int u1 )
{
  unsigned long index;

  if ( _BitScanForward( &index, u1 ) ) { return 53 - index; }
  _BitScanForward( &index, u0 );
  return 26 - index;
}

int CONV
last_one12( unsigned int u1, unsigned u2 )
{
  unsigned long index;

  if ( _BitScanForward( &index, u2 ) ) { return 80 - index; }
  _BitScanForward( &index, u1 );
  return 53 - index;
}

int CONV
first_one1( unsigned int u1 )
{
  unsigned long index;
  
  _BitScanReverse( &index, u1 );
  return 53 - index;
}

int CONV
first_one2( unsigned int u2 )
{
  unsigned long index;
  
  _BitScanReverse( &index, u2 );
  return 80 - index;
}

int CONV
last_one0( unsigned int u0 )
{
  unsigned long index;
  
  _BitScanForward( &index, u0 );
  return 26 - index;
}

int CONV
last_one1( unsigned int u1 )
{
  unsigned long index;
  
  _BitScanForward( &index, u1 );
  return 53 - index;
}

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )

int
first_one012( unsigned int u0, unsigned int u1, unsigned int u2 )
{
  if ( u0 ) { return __builtin_clz( u0 ) - 5; }
  if ( u1 ) { return __builtin_clz( u1 ) + 22; }
  return __builtin_clz( u2 ) + 49;
}


int
last_one210( unsigned int u2, unsigned int u1, unsigned int u0 )
{
  if ( u2 ) { return 80 - __builtin_ctz( u2 ); }
  if ( u1 ) { return 53 - __builtin_ctz( u1 ); }
  return 26 - __builtin_ctz( u0 );
}


int
first_one01( unsigned int u0, unsigned int u1 )
{
  if ( u0 ) { return __builtin_clz( u0 ) - 5; }
  return __builtin_clz( u1 ) + 22;
}


int
first_one12( unsigned int u1, unsigned int u2 )
{
  if ( u1 ) { return __builtin_clz( u1 ) + 22; }
  return __builtin_clz( u2 ) + 49;
}


int
last_one01( unsigned int u0, unsigned int u1 )
{
  if ( u1 ) { return 53 - __builtin_ctz( u1 ); }
  return 26 - __builtin_ctz( u0 );
}


int
last_one12( unsigned int u1, unsigned int u2 )
{
  if ( u2 ) { return 80 - __builtin_ctz( u2 ); }
  return 53 - __builtin_ctz( u1 );
}


int first_one1( unsigned int u1 ) { return __builtin_clz( u1 ) + 22; }
int first_one2( unsigned int u2 ) { return __builtin_clz( u2 ) + 49; }
int last_one0( unsigned int u0 ) { return 26 - __builtin_ctz( u0 ); }
int last_one1( unsigned int u1 ) { return 53 - __builtin_ctz( u1 ); }

#else

int
first_one012( unsigned int u0, unsigned int u1, unsigned int u2 )
{
  if ( u0 & 0x7fc0000 ) { return aifirst_one[u0>>18] +  0; }
  if ( u0 & 0x7fffe00 ) { return aifirst_one[u0>> 9] +  9; }
  if ( u0 & 0x7ffffff ) { return aifirst_one[u0    ] + 18; }

  if ( u1 & 0x7fc0000 ) { return aifirst_one[u1>>18] + 27; }
  if ( u1 & 0x7fffe00 ) { return aifirst_one[u1>> 9] + 36; }
  if ( u1 & 0x7ffffff ) { return aifirst_one[u1    ] + 45; }

  if ( u2 & 0x7fc0000 ) { return aifirst_one[u2>>18] + 54; }
  if ( u2 & 0x7fffe00 ) { return aifirst_one[u2>> 9] + 63; }
  return aifirst_one[u2] + 72;
}


int
last_one210( unsigned int u2, unsigned int u1, unsigned int u0 )
{
  unsigned int j;

  j = u2 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 72; }
  j = u2 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] + 63; }
  if ( u2 & 0x7ffffff ) { return ailast_one[u2>>18] + 54; }

  j = u1 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 45; }
  j = u1 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] + 36; }
  if ( u1 & 0x7ffffff ) { return ailast_one[u1>>18] + 27; }

  j = u0 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 18; }
  j = u0 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] +  9; }
  return ailast_one[u0>>18];
}


int
first_one01( unsigned int u0, unsigned int u1 )
{
  if ( u0 & 0x7fc0000 ) { return aifirst_one[u0>>18] +  0; }
  if ( u0 & 0x7fffe00 ) { return aifirst_one[u0>> 9] +  9; }
  if ( u0 & 0x7ffffff ) { return aifirst_one[u0    ] + 18; }

  if ( u1 & 0x7fc0000 ) { return aifirst_one[u1>>18] + 27; }
  if ( u1 & 0x7fffe00 ) { return aifirst_one[u1>> 9] + 36; }
  return aifirst_one[ u1 ] + 45;
}


int
first_one12( unsigned int u1, unsigned int u2 )
{
  if ( u1 & 0x7fc0000 ) { return aifirst_one[u1>>18] + 27; }
  if ( u1 & 0x7fffe00 ) { return aifirst_one[u1>> 9] + 36; }
  if ( u1 & 0x7ffffff ) { return aifirst_one[u1    ] + 45; }

  if ( u2 & 0x7fc0000 ) { return aifirst_one[u2>>18] + 54; }
  if ( u2 & 0x7fffe00 ) { return aifirst_one[u2>> 9] + 63; }
  return aifirst_one[ u2 ] + 72;
}


int
last_one01( unsigned int u0, unsigned int u1 )
{
  unsigned int j;

  j = u1 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 45; }
  j = u1 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] + 36; }
  if ( u1 & 0x7ffffff ) { return ailast_one[u1>>18] + 27; }

  j = u0 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 18; }
  j = u0 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] +  9; }
  return ailast_one[u0>>18];
}


int
last_one12( unsigned int u1, unsigned int u2 )
{
  unsigned int j;

  j = u2 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 72; }
  j = u2 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] + 63; }
  if ( u2 & 0x7ffffff ) { return ailast_one[u2>>18] + 54; }

  j = u1 & 0x00001ff;  if ( j ) { return ailast_one[j    ] + 45; }
  j = u1 & 0x003ffff;  if ( j ) { return ailast_one[j>> 9] + 36; }
  return ailast_one[u1>>18] + 27;
}


int
first_one1( unsigned int u1 )
{
  if ( u1 & 0x7fc0000U ) { return aifirst_one[u1>>18] + 27; }
  if ( u1 & 0x7fffe00U ) { return aifirst_one[u1>> 9] + 36; }
  return aifirst_one[u1] + 45;
}


int
first_one2( unsigned int u2 )
{
  if ( u2 & 0x7fc0000U ) { return aifirst_one[u2>>18] + 54; }
  if ( u2 & 0x7fffe00U ) { return aifirst_one[u2>> 9] + 63; }
  return aifirst_one[u2] + 72;
}


int
last_one0( unsigned int i )
{
  unsigned int j;

  j = i & 0x00001ffU;  if ( j ) { return ailast_one[j    ] + 18; }
  j = i & 0x003ffffU;  if ( j ) { return ailast_one[j>> 9] +  9; }
  return ailast_one[i>>18];
}


int
last_one1( unsigned int u1 )
{
  unsigned int j;

  j = u1 & 0x00001ffU;  if ( j ) { return ailast_one[j    ] + 45; }
  j = u1 & 0x003ffffU;  if ( j ) { return ailast_one[j>> 9] + 36; }
  return ailast_one[u1>>18] + 27;
}

#endif
