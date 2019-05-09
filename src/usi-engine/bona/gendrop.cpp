// 2019 Team AobaZero
// This source code is in the public domain.
#include "shogi.h"

unsigned int * CONV
b_gen_drop( tree_t * restrict __ptree__, unsigned int * restrict pmove )
{
  const tree_t * restrict ptree = __ptree__;
  bitboard_t bb_target;
  unsigned int ihand, ibb_target0a, ibb_target0b, ibb_pawn_cmp, utemp;
  unsigned int ais_pawn[nfile];
  int nhand, ito, i, nolance, noknight;
  int ahand[6];

  if ( ! HAND_B ) { return pmove; }     /* return! */
  ihand = HAND_B;
  nhand = 0;
  if ( IsHandKnight( ihand ) ) { ahand[ nhand++ ] = Drop2Move(knight); }
  noknight = nhand;
  if ( IsHandLance( ihand ) )  { ahand[ nhand++ ] = Drop2Move(lance); }
  nolance  = nhand;
  if ( IsHandSilver( ihand ) ) { ahand[ nhand++ ] = Drop2Move(silver); }
  if ( IsHandGold( ihand ) )   { ahand[ nhand++ ] = Drop2Move(gold); }
  if ( IsHandBishop( ihand ) ) { ahand[ nhand++ ] = Drop2Move(bishop); }
  if ( IsHandRook( ihand ) )   { ahand[ nhand++ ] = Drop2Move(rook); }

  BBOr( bb_target, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_target, bb_target );
  ibb_target0a = bb_target.p[0] & 0x7fc0000U;
  ibb_target0b = bb_target.p[0] & 0x003fe00U;
  bb_target.p[0] &= 0x00001ffU;
  bb_target.p[1] &= 0x7ffffffU;
  bb_target.p[2] &= 0x7ffffffU;

  if ( IsHandPawn( ihand ) )
    {
      ibb_pawn_cmp= BBToU( BB_BPAWN_ATK );
      ais_pawn[0] = ibb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ibb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ibb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ibb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ibb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ibb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ibb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ibb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ibb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBToU( bb_target ) )
	{
	  ito   = LastOne( bb_target );
	  utemp = To2Move(ito);
	  if ( ! ais_pawn[aifile[ito]] && ! IsMateBPawnDrop(__ptree__,ito) )
	    {
	      *pmove++ = utemp|Drop2Move(pawn);
	    }
	  for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp | ahand[i]; }
	  Xor( ito, bb_target );
	}

      while ( ibb_target0b )
	{
	  ito   = last_one0( ibb_target0b );
	  utemp = To2Move(ito);
	  if ( ! ais_pawn[aifile[ito]] && ! IsMateBPawnDrop(__ptree__,ito) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	  ibb_target0b ^= abb_mask[ito].p[0];
	}
    }
  else {
    while ( BBToU( bb_target ) )
      {
	ito   = LastOne( bb_target );
	utemp = To2Move(ito);
	for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	Xor( ito, bb_target );
      }

    while ( ibb_target0b )
      {
	ito = last_one0( ibb_target0b );
	utemp = To2Move(ito);
	for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[ i ]; }
	ibb_target0b ^= abb_mask[ ito ].p[0];
      }
  }

  while ( ibb_target0a )
    {
      ito = last_one0( ibb_target0a );
      utemp = To2Move(ito);
      for ( i = nolance; i < nhand; i++ ) { *pmove++ = utemp|ahand[ i ]; }
      ibb_target0a ^= abb_mask[ ito ].p[0];
    }

  return pmove;
}


unsigned int * CONV
w_gen_drop( tree_t * restrict __ptree__, unsigned int * restrict pmove )
{
  const tree_t * restrict ptree = __ptree__;
  bitboard_t bb_target;
  unsigned int ihand, ibb_target2a, ibb_target2b, ibb_pawn_cmp, utemp;
  unsigned int ais_pawn[nfile];
  int nhand, ito, i, nolance, noknight;
  int ahand[6];

  if ( ! HAND_W ) { return pmove; }     /* return! */
  ihand = HAND_W;
  nhand = 0;
  if ( IsHandKnight( ihand ) ) { ahand[ nhand++ ] = Drop2Move(knight); }
  noknight = nhand;
  if ( IsHandLance( ihand ) )  { ahand[ nhand++ ] = Drop2Move(lance); }
  nolance  = nhand;
  if ( IsHandSilver( ihand ) ) { ahand[ nhand++ ] = Drop2Move(silver); }
  if ( IsHandGold( ihand ) )   { ahand[ nhand++ ] = Drop2Move(gold); }
  if ( IsHandBishop( ihand ) ) { ahand[ nhand++ ] = Drop2Move(bishop); }
  if ( IsHandRook( ihand ) )   { ahand[ nhand++ ] = Drop2Move(rook); }

  BBOr( bb_target, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_target, bb_target );
  ibb_target2a = bb_target.p[2] & 0x00001ffU;
  ibb_target2b = bb_target.p[2] & 0x003fe00U;
  bb_target.p[0] &= 0x7ffffffU;
  bb_target.p[1] &= 0x7ffffffU;
  bb_target.p[2] &= 0x7fc0000U;

  if ( IsHandPawn( ihand ) )
    {
      ibb_pawn_cmp= BBToU( BB_WPAWN_ATK );
      ais_pawn[0] = ibb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ibb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ibb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ibb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ibb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ibb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ibb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ibb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ibb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBToU( bb_target ) )
	{
	  ito   = FirstOne( bb_target );
	  utemp = To2Move(ito);
	  if ( ! ais_pawn[aifile[ito]] && ! IsMateWPawnDrop(__ptree__,ito) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp | ahand[i]; }
	  Xor( ito, bb_target );
	}

      while ( ibb_target2b )
	{
	  ito   = first_one2( ibb_target2b );
	  utemp = To2Move(ito);
	  if ( ! ais_pawn[aifile[ito]] && ! IsMateWPawnDrop(__ptree__,ito) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp | ahand[i]; }
	  ibb_target2b ^= abb_mask[ito].p[2];
	}
    }
  else {
    while ( BBToU( bb_target ) )
      {
	ito   = FirstOne( bb_target );
	utemp = To2Move(ito);
	for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	Xor( ito, bb_target );
      }

    while ( ibb_target2b )
      {
	ito   = first_one2( ibb_target2b );
	utemp = To2Move(ito);
	for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	ibb_target2b ^= abb_mask[ito].p[2];
      }
  }

  while ( ibb_target2a )
    {
      ito   = first_one2( ibb_target2a );
      utemp = To2Move(ito);
      for ( i = nolance; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
      ibb_target2a ^= abb_mask[ito].p[2];
    }

  return pmove;
}
