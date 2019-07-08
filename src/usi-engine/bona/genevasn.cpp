// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "shogi.h"


unsigned int * CONV
b_gen_evasion( tree_t * restrict ptree, unsigned int * restrict pmove )
{
  bitboard_t bb_desti, bb_checker, bb_inter, bb_target, bb_piece;
  unsigned int hand, ubb_target0a, ubb_target0b, ubb_pawn_cmp, utemp;
  unsigned ais_pawn[nfile];
  int nchecker, sq_bk, to, sq_check, idirec;
  int nhand, i, nolance, noknight, from;
  int ahand[6];
  
  /* move the king */
  sq_bk = SQ_BKING;
  
  Xor( sq_bk, BB_BOCCUPY );
  XorFile( sq_bk, OCCUPIED_FILE );
  XorDiag2( sq_bk, OCCUPIED_DIAG2 );
  XorDiag1( sq_bk, OCCUPIED_DIAG1 );

  BBNotAnd( bb_desti, abb_king_attacks[sq_bk], BB_BOCCUPY );
  utemp = From2Move(sq_bk) | Piece2Move(king);
  while ( BBTest( bb_desti ) )
    {
      to = LastOne( bb_desti );
      if ( ! is_black_attacked( ptree, to ) )
	{
	  *pmove++ = To2Move(to) | Cap2Move(-BOARD[to]) | utemp;
	}
      Xor( to, bb_desti );
    }
  
  Xor( sq_bk, BB_BOCCUPY );
  XorFile( sq_bk, OCCUPIED_FILE );
  XorDiag2( sq_bk, OCCUPIED_DIAG2 );
  XorDiag1( sq_bk, OCCUPIED_DIAG1 );
  
  bb_checker = w_attacks_to_piece( ptree, sq_bk );
  nchecker = PopuCount( bb_checker );
  if ( nchecker == 2 ) { return pmove; }
  
  sq_check = LastOne( bb_checker );
  bb_inter = abb_obstacle[sq_bk][sq_check];

  /* move other pieces */
  BBOr( bb_target, bb_inter, bb_checker );
  
  BBAnd( bb_desti, bb_target, BB_BPAWN_ATK );
  while ( BBTest( bb_desti ) )
    {
      to = LastOne( bb_desti );
      Xor( to, bb_desti );

      from = to + 9;
      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(pawn)
		    | Cap2Move(-BOARD[to]) );
	  if ( to < A6 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	}
    }

  bb_piece = BB_BLANCE;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_desti = AttackFile( from );
      BBAnd( bb_desti, bb_desti, abb_minus_rays[from] );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  to = LastOne( bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(lance)
		    | Cap2Move(-BOARD[to]) );
	  if ( to <  A6 ) { *pmove++ = utemp | FLAG_PROMO; }
	  if ( to >= A7 ) { *pmove++ = utemp; }
	}
    }

  bb_piece = BB_BKNIGHT;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_b_knight_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(knight)
		    | Cap2Move(-BOARD[to]) );
	  if ( to <  A6 ) { *pmove++ = utemp | FLAG_PROMO; }
	  if ( to >= A7 ) { *pmove++ = utemp; }
	  
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_BSILVER;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      BBAnd( bb_desti, bb_target, abb_b_silver_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );
	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(silver)
		    | Cap2Move(-BOARD[to]) );
	  if ( from < A6 || to < A6 ) { *pmove++ = utemp | FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_BTGOLD;
  while( BBTest( bb_piece ) )
    {
      from  = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_b_gold_attacks[from] );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );
	  *pmove++ = ( To2Move(to) | From2Move(from)
		       | Piece2Move(BOARD[from])
		       | Cap2Move(-BOARD[to]) );
	} while( BBTest( bb_desti ) );
    }

  bb_piece = BB_BBISHOP;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackBishop( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }
      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(bishop)
		    | Cap2Move(-BOARD[to]) );
	  if ( from < A6 || to < A6 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_BROOK;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackRook( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }
      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(rook)
		    | Cap2Move(-BOARD[to]) );
	  if ( from < A6 || to < A6 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_BHORSE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackHorse( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti);
	  *pmove++ = ( To2Move(to) | From2Move(from) | Piece2Move(horse)
		       | Cap2Move(-BOARD[to]) );
	} while ( BBTest( bb_desti ) );
    }
  
  bb_piece = BB_BDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackDragon( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec || ! is_pinned_on_black_king( ptree, from, idirec ) )
	do {
	  to = LastOne( bb_desti );
	  Xor( to, bb_desti );
	  *pmove++ = ( To2Move(to) | From2Move(from) | Piece2Move(dragon)
		       | Cap2Move(-BOARD[to]) );
	} while ( BBTest( bb_desti ) );
    }

  if ( ! HAND_B )          { return pmove; }
  if ( ! BBTest(bb_inter) ) { return pmove; }

  /* drops */
  bb_target = bb_inter;
  ubb_target0a = bb_target.p[0] & 0x7fc0000U;
  ubb_target0b = bb_target.p[0] & 0x003fe00U;
  bb_target.p[0] &= 0x00001ffU;
  bb_target.p[1] &= 0x7ffffffU;
  bb_target.p[2] &= 0x7ffffffU;

  hand = HAND_B;
  nhand = 0;
  if ( IsHandKnight( hand ) ) { ahand[ nhand++ ] = Drop2Move(knight); }
  noknight = nhand;
  if ( IsHandLance( hand ) )  { ahand[ nhand++ ] = Drop2Move(lance); }
  nolance  = nhand;
  if ( IsHandSilver( hand ) ) { ahand[ nhand++ ] = Drop2Move(silver); }
  if ( IsHandGold( hand ) )   { ahand[ nhand++ ] = Drop2Move(gold); }
  if ( IsHandBishop( hand ) ) { ahand[ nhand++ ] = Drop2Move(bishop); }
  if ( IsHandRook( hand ) )   { ahand[ nhand++ ] = Drop2Move(rook); }

  if ( IsHandPawn( hand ) )
    {
      ubb_pawn_cmp= BBToU( BB_BPAWN_ATK );
      ais_pawn[0] = ubb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ubb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ubb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ubb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ubb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ubb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ubb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ubb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ubb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBTest( bb_target ) )
	{
	  to = LastOne( bb_target );
	  utemp = To2Move(to);
	  if ( ! ais_pawn[aifile[to]] && ! IsMateBPawnDrop( ptree, to ) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	  Xor( to, bb_target );
	}

      while ( ubb_target0b )
	{
	  to = last_one0( ubb_target0b );
	  utemp = To2Move(to);
	  if ( ! ais_pawn[aifile[to]] && ! IsMateBPawnDrop( ptree, to ) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	  ubb_target0b ^= abb_mask[ to ].p[0];
	}
    }
  else {
    while ( BBTest( bb_target ) )
      {
	to = LastOne( bb_target );
	utemp = To2Move(to);
	for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	Xor( to, bb_target );
      }

    while ( ubb_target0b )
      {
	to = last_one0( ubb_target0b );
	utemp = To2Move(to);
	for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	ubb_target0b ^= abb_mask[ to ].p[0];
      }
  }

  while ( ubb_target0a )
    {
      to = last_one0( ubb_target0a );
      utemp = To2Move(to);
      for ( i = nolance; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
      ubb_target0a ^= abb_mask[ to ].p[0];
    }

  return pmove;
}


unsigned int * CONV
w_gen_evasion( tree_t * restrict ptree, unsigned int * restrict pmove )
{
  bitboard_t bb_desti, bb_checker, bb_inter, bb_target, bb_piece;
  unsigned int hand, ubb_target2a, ubb_target2b, ubb_pawn_cmp, utemp;
  unsigned int ais_pawn[nfile];
  int nchecker, sq_wk, to, sq_check, idirec;
  int nhand, i, nolance, noknight, from;
  int ahand[6];

  /* move the king */
  sq_wk = SQ_WKING;

  Xor( sq_wk, BB_WOCCUPY );
  XorFile( sq_wk, OCCUPIED_FILE );
  XorDiag2( sq_wk, OCCUPIED_DIAG2 );
  XorDiag1( sq_wk, OCCUPIED_DIAG1 );

  BBNotAnd( bb_desti, abb_king_attacks[sq_wk], BB_WOCCUPY );
  utemp = From2Move(sq_wk) | Piece2Move(king);
  while ( BBTest( bb_desti ) )
    {
      to = FirstOne( bb_desti );
      if ( ! is_white_attacked( ptree, to ) )
	{
	  *pmove++ = To2Move(to) | Cap2Move(BOARD[to]) | utemp;
	}
      Xor( to, bb_desti );
    }

  Xor( sq_wk, BB_WOCCUPY );
  XorFile( sq_wk, OCCUPIED_FILE );
  XorDiag2( sq_wk, OCCUPIED_DIAG2 );
  XorDiag1( sq_wk, OCCUPIED_DIAG1 );

  bb_checker = b_attacks_to_piece( ptree, sq_wk );
  nchecker = PopuCount( bb_checker );
  if ( nchecker == 2 ) { return pmove; }

  sq_check = FirstOne( bb_checker );
  bb_inter = abb_obstacle[sq_wk][sq_check];

  /* move other pieces */
  BBOr( bb_target, bb_inter, bb_checker );

  BBAnd( bb_desti, bb_target, BB_WPAWN_ATK );
  while ( BBTest( bb_desti ) )
    {
      to = FirstOne( bb_desti );
      Xor( to, bb_desti );

      from = to - 9;
      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(pawn)
		    | Cap2Move(BOARD[to]) );
	  if ( to > I4 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	}
    }

  bb_piece = BB_WLANCE;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_desti = AttackFile( from );
      BBAnd( bb_desti, bb_desti, abb_plus_rays[from] );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  to = FirstOne( bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(lance)
		    | Cap2Move(BOARD[to]) );
	  if ( to >  I4 ) { *pmove++ = utemp | FLAG_PROMO; }
	  if ( to <= I3 ) { *pmove++ = utemp; }
	}
    }

  bb_piece = BB_WKNIGHT;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_w_knight_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(knight)
		    | Cap2Move(BOARD[to]) );
	  if ( to >  I4 ) { *pmove++ = utemp | FLAG_PROMO; }
	  if ( to <= I3 ) { *pmove++ = utemp; }
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_WSILVER;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      BBAnd( bb_desti, bb_target, abb_w_silver_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );
	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(silver)
		    | Cap2Move(BOARD[to]) );
	  if ( from > I4 || to > I4 ) { *pmove++ = utemp | FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_WTGOLD;
  while( BBTest( bb_piece ) )
    {
      from  = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_w_gold_attacks[from] );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );
	  *pmove++ = ( To2Move(to) | From2Move(from)
		       | Piece2Move(-BOARD[from])
		       | Cap2Move(BOARD[to]) );
	} while( BBTest( bb_desti ) );
    }

  bb_piece = BB_WBISHOP;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackBishop( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(bishop)
		    | Cap2Move(BOARD[to]) );
	  if ( from > I4 || to > I4 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_WROOK;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackRook( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }
      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );

	  utemp = ( To2Move(to) | From2Move(from) | Piece2Move(rook)
		    | Cap2Move(BOARD[to]) );
	  if ( from > I4 || to > I4 ) { utemp |= FLAG_PROMO; }
	  *pmove++ = utemp;
	} while ( BBTest( bb_desti ) );
    }

  bb_piece = BB_WHORSE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackHorse( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti);
	  *pmove++ = ( To2Move(to) | From2Move(from) | Piece2Move(horse)
		       | Cap2Move(BOARD[to]) );
	} while ( BBTest( bb_desti ) );
    }
  
  bb_piece = BB_WDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackDragon( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec || ! is_pinned_on_white_king( ptree, from, idirec ) )
	do {
	  to = FirstOne( bb_desti );
	  Xor( to, bb_desti );
	  *pmove++ = ( To2Move(to) | From2Move(from) | Piece2Move(dragon)
		       | Cap2Move(BOARD[to]) );
	} while ( BBTest( bb_desti ) );
    }

  if ( ! HAND_W )          { return pmove; }
  if ( ! BBTest(bb_inter) ) { return pmove; }

  /* drop */
  bb_target = bb_inter;
  ubb_target2a = bb_target.p[2] & 0x00001ffU;
  ubb_target2b = bb_target.p[2] & 0x003fe00U;
  bb_target.p[0] &= 0x7ffffffU;
  bb_target.p[1] &= 0x7ffffffU;
  bb_target.p[2] &= 0x7fc0000U;

  hand = HAND_W;
  nhand = 0;
  if ( IsHandKnight( hand ) ) { ahand[ nhand++ ] = Drop2Move(knight); }
  noknight = nhand;
  if ( IsHandLance( hand ) )  { ahand[ nhand++ ] = Drop2Move(lance); }
  nolance  = nhand;
  if ( IsHandSilver( hand ) ) { ahand[ nhand++ ] = Drop2Move(silver); }
  if ( IsHandGold( hand ) )   { ahand[ nhand++ ] = Drop2Move(gold); }
  if ( IsHandBishop( hand ) ) { ahand[ nhand++ ] = Drop2Move(bishop); }
  if ( IsHandRook( hand ) )   { ahand[ nhand++ ] = Drop2Move(rook); }

  if ( IsHandPawn( hand ) )
    {
      ubb_pawn_cmp= BBToU( BB_WPAWN_ATK );
      ais_pawn[0] = ubb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ubb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ubb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ubb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ubb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ubb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ubb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ubb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ubb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBTest( bb_target ) )
	{
	  to = FirstOne( bb_target );
	  utemp = To2Move(to);
	  if ( ! ais_pawn[aifile[to]] && ! IsMateWPawnDrop( ptree, to ) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	  Xor( to, bb_target );
	}

      while ( ubb_target2b )
	{
	  to = first_one2( ubb_target2b );
	  utemp = To2Move(to);
	  if ( ! ais_pawn[aifile[to]] && ! IsMateWPawnDrop( ptree, to ) )
	    {
	      *pmove++ = utemp | Drop2Move(pawn);
	    }
	  for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	  ubb_target2b ^= abb_mask[ to ].p[2];
	}
    }
  else {
    while ( BBTest( bb_target ) )
      {
	to = FirstOne( bb_target );
	utemp = To2Move(to);
	for ( i = 0; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	Xor( to, bb_target );
      }

    while ( ubb_target2b )
      {
	to = first_one2( ubb_target2b );
	utemp = To2Move(to);
	for ( i = noknight; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
	ubb_target2b ^= abb_mask[ to ].p[2];
      }
  }

  while ( ubb_target2a )
    {
      to = first_one2( ubb_target2a );
      utemp = To2Move(to);
      for ( i = nolance; i < nhand; i++ ) { *pmove++ = utemp|ahand[i]; }
      ubb_target2a ^= abb_mask[ to ].p[2];
    }

  return pmove;
}


int CONV b_have_evasion( tree_t * restrict ptree )
{
  bitboard_t bb_desti, bb_checker, bb_inter, bb_target, bb_piece;
  unsigned int ubb_pawn_cmp;
  unsigned int ais_pawn[nfile];
  int nchecker, sq_bk, to, sq_check, idirec, flag, from;
  
  /* move the king */
  flag  = 0;
  sq_bk = SQ_BKING;
  
  Xor( sq_bk, BB_BOCCUPY );
  XorFile( sq_bk, OCCUPIED_FILE );
  XorDiag2( sq_bk, OCCUPIED_DIAG2 );
  XorDiag1( sq_bk, OCCUPIED_DIAG1 );

  BBNotAnd( bb_desti, abb_king_attacks[sq_bk], BB_BOCCUPY );

  while ( BBTest( bb_desti ) )
    {
      to = LastOne( bb_desti );
      Xor( to, bb_desti );

      if ( ! is_black_attacked( ptree, to ) )
	{
	  flag = 1;
	  break;
	}
    }

  Xor( sq_bk, BB_BOCCUPY );
  XorFile( sq_bk, OCCUPIED_FILE );
  XorDiag2( sq_bk, OCCUPIED_DIAG2 );
  XorDiag1( sq_bk, OCCUPIED_DIAG1 );
  
  if ( flag ) { return 1; }


  bb_checker = w_attacks_to_piece( ptree, sq_bk );
  nchecker   = PopuCount( bb_checker );
  if ( nchecker == 2 ) { return 0; }
  
  sq_check = LastOne( bb_checker );
  bb_inter = abb_obstacle[sq_bk][sq_check];

  /* move other pieces */
  BBOr( bb_target, bb_inter, bb_checker );
  
  BBAnd( bb_desti, bb_target, BB_BPAWN_ATK );
  while ( BBTest( bb_desti ) )
    {
      to = LastOne( bb_desti );
      Xor( to, bb_desti );

      from = to + 9;
      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BLANCE;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_desti = AttackFile( from );
      BBAnd( bb_desti, bb_desti, abb_minus_rays[from] );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BKNIGHT;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_b_knight_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BSILVER;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      BBAnd( bb_desti, bb_target, abb_b_silver_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BTGOLD;
  while( BBTest( bb_piece ) )
    {
      from  = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_b_gold_attacks[from] );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BBISHOP;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackBishop( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BROOK;
  while ( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackRook( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_BHORSE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackHorse( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }
  
  bb_piece = BB_BDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      AttackDragon( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_bk][from];
      if ( ! idirec
	   || ! is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
    }

  /* drops */
  if ( ! BBTest(bb_inter) ) { return 0; }

  if ( IsHandSGBR(HAND_B) ) { return 1; }

  bb_inter.p[0] &= 0x003ffffU;
  if ( ! BBTest(bb_inter) ) { return 0; }

  if ( IsHandLance(HAND_B) ) { return 1; }

  if ( IsHandKnight(HAND_B) )
    {
      bb_target       = bb_inter;
      bb_target.p[0] &= 0x00001ffU;
      if ( BBTest(bb_target) ) { return 1; }
    }

  if ( IsHandPawn(HAND_B) )
    {
      bb_target = bb_inter;
      ubb_pawn_cmp= BBToU( BB_BPAWN_ATK );
      ais_pawn[0] = ubb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ubb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ubb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ubb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ubb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ubb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ubb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ubb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ubb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBTest( bb_target ) )
	{
	  to = LastOne( bb_target );
	  Xor( to, bb_target );
	  
	  if ( ! ais_pawn[aifile[to]]
	       && ! IsMateBPawnDrop( ptree, to ) ) { return 1; }
	}
    }

  return 0;
}


int CONV w_have_evasion( tree_t * restrict ptree )
{
  bitboard_t bb_desti, bb_checker, bb_inter, bb_target, bb_piece;
  unsigned int ubb_pawn_cmp;
  unsigned int ais_pawn[nfile];
  int nchecker, sq_wk, to, sq_check, idirec, flag, from;

  /* move the king */
  flag  = 0;
  sq_wk = SQ_WKING;

  Xor( sq_wk, BB_WOCCUPY );
  XorFile( sq_wk, OCCUPIED_FILE );
  XorDiag2( sq_wk, OCCUPIED_DIAG2 );
  XorDiag1( sq_wk, OCCUPIED_DIAG1 );

  BBNotAnd( bb_desti, abb_king_attacks[sq_wk], BB_WOCCUPY );
  while ( BBTest( bb_desti ) )
    {
      to = FirstOne( bb_desti );
      Xor( to, bb_desti );

      if ( ! is_white_attacked( ptree, to ) )
	{
	  flag = 1;
	  break;
	}
    }

  Xor( sq_wk, BB_WOCCUPY );
  XorFile( sq_wk, OCCUPIED_FILE );
  XorDiag2( sq_wk, OCCUPIED_DIAG2 );
  XorDiag1( sq_wk, OCCUPIED_DIAG1 );

  if ( flag ) { return 1; }


  bb_checker = b_attacks_to_piece( ptree, sq_wk );
  nchecker   = PopuCount( bb_checker );
  if ( nchecker == 2 ) { return 0; }

  sq_check = FirstOne( bb_checker );
  bb_inter = abb_obstacle[sq_wk][sq_check];

  /* move other pieces */
  BBOr( bb_target, bb_inter, bb_checker );

  BBAnd( bb_desti, bb_target, BB_WPAWN_ATK );
  while ( BBTest( bb_desti ) )
    {
      to = FirstOne( bb_desti );
      Xor( to, bb_desti );

      from = to - 9;
      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WLANCE;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_desti = AttackFile( from );
      BBAnd( bb_desti, bb_desti, abb_plus_rays[from] );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WKNIGHT;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_w_knight_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WSILVER;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      BBAnd( bb_desti, bb_target, abb_w_silver_attacks[from] );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WTGOLD;
  while( BBTest( bb_piece ) )
    {
      from  = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBAnd( bb_desti, bb_target, abb_w_gold_attacks[from] );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WBISHOP;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackBishop( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WROOK;
  while ( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackRook( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest( bb_desti ) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  bb_piece = BB_WHORSE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackHorse( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }
  
  bb_piece = BB_WDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      AttackDragon( bb_desti, from );
      BBAnd( bb_desti, bb_desti, bb_target );
      if ( ! BBTest(bb_desti) ) { continue; }

      idirec = (int)adirec[sq_wk][from];
      if ( ! idirec
	   || ! is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
    }

  /* drop */
  if ( ! BBTest(bb_inter) ) { return 0; }

  if ( IsHandSGBR(HAND_W) ) { return 1; }

  bb_inter.p[2] &= 0x7fffe00U;
  if ( ! BBTest(bb_inter) ) { return 0; }

  if ( IsHandLance(HAND_W) ) { return 1; }

  if ( IsHandKnight(HAND_W) )
    {
      bb_target       = bb_inter;
      bb_target.p[2] &= 0x7fc0000U;
      if ( BBTest(bb_target) ) { return 1; }
    }

  if ( IsHandPawn(HAND_W) )
    {
      bb_target = bb_inter;
      ubb_pawn_cmp= BBToU( BB_WPAWN_ATK );
      ais_pawn[0] = ubb_pawn_cmp & ( mask_file1 >> 0 );
      ais_pawn[1] = ubb_pawn_cmp & ( mask_file1 >> 1 );
      ais_pawn[2] = ubb_pawn_cmp & ( mask_file1 >> 2 );
      ais_pawn[3] = ubb_pawn_cmp & ( mask_file1 >> 3 );
      ais_pawn[4] = ubb_pawn_cmp & ( mask_file1 >> 4 );
      ais_pawn[5] = ubb_pawn_cmp & ( mask_file1 >> 5 );
      ais_pawn[6] = ubb_pawn_cmp & ( mask_file1 >> 6 );
      ais_pawn[7] = ubb_pawn_cmp & ( mask_file1 >> 7 );
      ais_pawn[8] = ubb_pawn_cmp & ( mask_file1 >> 8 );
 
      while ( BBTest( bb_target ) )
	{
	  to = FirstOne( bb_target );
	  Xor( to, bb_target );

	  if ( ! ais_pawn[aifile[to]]
	       && ! IsMateWPawnDrop( ptree, to ) ) { return 1; }
	}
    }

  return 0;
}
