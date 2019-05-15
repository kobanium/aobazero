// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include "shogi.h"


static void CONV add_behind_attacks( bitboard_t * restrict pbb, int idirec,
				     int ik );

unsigned int * CONV
b_gen_checks( tree_t * restrict __ptree__, unsigned int * restrict pmove )
{
  bitboard_t bb_piece, bb_rook_chk, bb_bishop_chk, bb_chk, bb_move_to;
  bitboard_t bb_diag1_chk, bb_diag2_chk, bb_file_chk, bb_drop_to, bb_desti;
  bitboard_t bb_rank_chk;
  const tree_t * restrict ptree = __ptree__;
  unsigned int u0, u1, u2;
  int from, to, sq_wk, idirec;

  sq_wk = SQ_WKING;
  bb_file_chk = AttackFile( sq_wk );
  bb_rank_chk = AttackRank( sq_wk );
  BBOr( bb_rook_chk, bb_file_chk, bb_rank_chk );

  bb_diag1_chk = AttackDiag1( sq_wk );
  bb_diag2_chk = AttackDiag2( sq_wk );
  BBOr( bb_bishop_chk, bb_diag1_chk, bb_diag2_chk );
  BBNot( bb_move_to, BB_BOCCUPY );
  BBOr( bb_drop_to, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop_to, bb_drop_to );

  from  = SQ_BKING;
  idirec = (int)adirec[sq_wk][from];
  if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
    {
      BBIni( bb_chk );
      add_behind_attacks( &bb_chk, idirec, sq_wk );
      BBAnd( bb_chk, bb_chk, abb_king_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );
      
      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(king)
	    | Cap2Move(-BOARD[to]);
	}
    }
  
  
  bb_piece = BB_BDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_wk] );
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      AttackDragon( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(dragon)
	    | Cap2Move(-BOARD[to]);
	}
    }

  bb_piece = BB_BHORSE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_wk] );
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      AttackHorse( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(horse)
	    | Cap2Move(-BOARD[to]);
	}
    }

  u1 = BB_BROOK.p[1];
  u2 = BB_BROOK.p[2];
  while( u1 | u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_rook_chk;
	bb_chk.p[0] |= abb_king_attacks[sq_wk].p[0];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while ( bb_chk.p[0] )
	{
	  to          = last_one0( bb_chk.p[0] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}

      while( bb_chk.p[1] | bb_chk.p[2] )
	{
	  to          = last_one12( bb_chk.p[1], bb_chk.p[2] );
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(-BOARD[to]);
	}
    }

  u0 = BB_BROOK.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];
      
      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_wk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }

  u1 = BB_BBISHOP.p[1];
  u2 = BB_BBISHOP.p[2];
  while( u1 | u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_bishop_chk;
	bb_chk.p[0] |= abb_king_attacks[sq_wk].p[0];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while ( bb_chk.p[0] )
	{
	  to          = last_one0( bb_chk.p[0] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}

      while( bb_chk.p[1] | bb_chk.p[2] )
	{
	  to          = last_one12( bb_chk.p[1], bb_chk.p[2] );
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(-BOARD[to]);
	}
    }

  u0 = BB_BBISHOP.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];
      
      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_wk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }


  bb_piece = BB_BTGOLD;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_w_gold_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_gold_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = ( To2Move(to) | From2Move(from)
		       | Piece2Move(BOARD[from])
		       | Cap2Move(-BOARD[to]) );
	}
    }
  

  u0 = BB_BSILVER.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = abb_w_gold_attacks[sq_wk].p[1];
      bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= bb_move_to.p[0] & abb_b_silver_attacks[from].p[0];
      bb_chk.p[1] &= bb_move_to.p[1] & abb_b_silver_attacks[from].p[1];

      while( bb_chk.p[0] | bb_chk.p[1] )
	{
	  to          = last_one01( bb_chk.p[0], bb_chk.p[1] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u1 = BB_BSILVER.p[1] & 0x7fc0000U;
  while( u1 )
    {
      from = last_one1( u1 );
      u1   ^= abb_mask[from].p[1];
      
      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;
      
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= bb_move_to.p[0] & abb_b_silver_attacks[from].p[0];
      while ( bb_chk.p[0] )
	{
	  to          = last_one0( bb_chk.p[0] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }
  

  bb_piece = BB_BSILVER;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_w_silver_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_silver_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(-BOARD[to]);
	}
    }
  

  u0 = BB_BKNIGHT.p[0];
  u1 = BB_BKNIGHT.p[1] & 0x7fffe00U;
  while( u0 | u1 )
    {
      from = last_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= abb_b_knight_attacks[from].p[0] & bb_move_to.p[0];

      while( bb_chk.p[0] )
	{
	  to          = last_one0( bb_chk.p[0] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(knight)
		       | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u2 = BB_BKNIGHT.p[2];
  u1 = BB_BKNIGHT.p[1] & 0x3ffffU;
  while( u2 | u1 )
    {
      from = last_one12( u1, u2 );
      u2   ^= abb_mask[from].p[2];
      u1   ^= abb_mask[from].p[1];

      bb_chk = abb_w_knight_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_knight_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(knight)
	    | Cap2Move(-BOARD[to]);
	}
    }


  bb_piece = BB_BLANCE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, abb_minus_rays[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(lance)
	    | Cap2Move(-BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u1 = BB_BLANCE.p[1];
  u2 = BB_BLANCE.p[2];
  while( u1| u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      bb_chk = bb_file_chk;
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	  BBAnd( bb_chk, bb_chk, abb_minus_rays[from] );
	}
      else { BBAnd( bb_chk, bb_file_chk, abb_plus_rays[sq_wk] );}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, bb_move_to );
      bb_chk.p[0] = bb_chk.p[0] & 0x1ffU;

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(lance)
	    | Cap2Move(-BOARD[to]);
	}
    }

  BBIni( bb_chk );
  bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
  if ( sq_wk < A2 ) { BBOr( bb_chk, bb_chk, abb_mask[sq_wk+nfile] ); }
  BBAnd( bb_chk, bb_chk, bb_move_to );
  BBAnd( bb_chk, bb_chk, BB_BPAWN_ATK );

  BBAnd( bb_piece, bb_diag1_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }

      bb_desti = AttackDiag1( from );
      if ( BBContract( bb_desti, BB_B_BH ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(-BOARD[to]);
	  if ( from < A5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  BBAnd( bb_piece, bb_diag2_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }

      bb_desti = AttackDiag2( from );
      if ( BBContract( bb_desti, BB_B_BH ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(-BOARD[to]);
	  if ( from < A5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  BBAnd( bb_piece, bb_rank_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }

      bb_desti = AttackRank( from );
      if ( BBContract( bb_desti, BB_B_RD ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(-BOARD[to]);
	  if ( from < A5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  while ( BBTest(bb_chk) )
    {
      to = LastOne( bb_chk );
      Xor( to, bb_chk );

      from = to + nfile;
      *pmove = To2Move(to) | From2Move(from)
	| Piece2Move(pawn) | Cap2Move(-BOARD[to]);
      if ( from < A5 ) { *pmove |= FLAG_PROMO; }
      pmove += 1;
    }


  if ( IsHandGold(HAND_B) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_w_gold_attacks[sq_wk] );
      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | Drop2Move(gold);
	}
    }
  

  if ( IsHandSilver(HAND_B) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_w_silver_attacks[sq_wk] );
      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | Drop2Move(silver);
	}
    }
  

  if ( IsHandKnight(HAND_B) && sq_wk < A2 )
    {
      to = sq_wk + 2*nfile - 1;
      if ( aifile[sq_wk] != file1 && BOARD[to] == empty )
	{
	  *pmove++ = To2Move(to) | Drop2Move(knight);
	}

      to = sq_wk + 2*nfile + 1;
      if ( aifile[sq_wk] != file9 && BOARD[to] == empty )
	{
	  *pmove++ = To2Move(to) | Drop2Move(knight);
	}
    }


  if ( IsHandPawn(HAND_B)
       && sq_wk < A1
       && ! ( BBToU(BB_BPAWN) & ( mask_file1 >> aifile[sq_wk] ) ) )
    {
      to = sq_wk + nfile;
      if ( BOARD[to] == empty && ! is_mate_b_pawn_drop( __ptree__, to ) )
	{
	  *pmove++ = To2Move(to) | Drop2Move(pawn);
	}
    }


  if ( IsHandLance(HAND_B) )
    {
      unsigned int move;
      int dist, min_dist;

      if ( (int)aifile[sq_wk] == file1
	   || (int)aifile[sq_wk] == file9 ) { min_dist = 2; }
      else                                  { min_dist = 3; }

      for ( to = sq_wk+nfile, dist = 1; to < nsquare && BOARD[to] == empty;
	    to += nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(lance);
	  if      ( dist == 1 )       { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  if ( IsHandRook(HAND_B) )
    {
      unsigned int move;
      int file, dist, min_dist;

      if ( (int)aifile[sq_wk] == file1
	   || (int)aifile[sq_wk] == file9 ) { min_dist = 2; }
      else                                  { min_dist = 3; }

      for ( to = sq_wk+nfile, dist = 1; to < nsquare && BOARD[to] == empty;
	    to += nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 )       { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      for ( file = (int)aifile[sq_wk]-1, to = sq_wk-1, dist = 1;
	    file >= file1 && BOARD[to] == empty;
	    file -= 1, to -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 ) { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      if ( sq_wk < A8 || I2 < sq_wk ) { min_dist = 2; }
      else                            { min_dist = 3; }

      for ( file = (int)aifile[sq_wk]+1, to = sq_wk+1, dist = 1;
	    file <= file9 && BOARD[to] == empty;
	    file += 1, to += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 )       { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      for ( to = sq_wk-nfile, dist = 1; to >= 0 && BOARD[to] == empty;
	    to -= nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if ( (int)airank[to] == rank3 ) { move |= MOVE_CHK_CLEAR; }
	  if      ( dist == 1 )           { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist )     { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  if ( IsHandBishop(HAND_B) )
    {
      unsigned int move;
      int file, rank, dist;

      to   = sq_wk;
      file = (int)aifile[sq_wk];
      rank = (int)airank[sq_wk];
      for ( to -= 10, file -= 1, rank -= 1, dist = 1;
	    file >= 0 && rank >= 0 && BOARD[to] == empty;
	    to -= 10, file -= 1, rank -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( rank == rank3 ) { move |= MOVE_CHK_CLEAR; }
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_wk;
      file = (int)aifile[sq_wk];
      rank = (int)airank[sq_wk];
      for ( to -= 8, file += 1, rank -= 1, dist = 1;
	    file <= file9 && rank >= 0 && BOARD[to] == empty;
	    to -= 8, file += 1, rank -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( rank == rank3 ) { move |= MOVE_CHK_CLEAR; }
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_wk;
      file = (int)aifile[sq_wk];
      rank = (int)airank[sq_wk];
      for ( to += 8, file -= 1, rank += 1, dist = 1;
	    file >= 0 && rank <= rank9 && BOARD[to] == empty;
	    to += 8, file -= 1, rank += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_wk;
      file = (int)aifile[sq_wk];
      rank = (int)airank[sq_wk];
      for ( to += 10, file += 1, rank += 1, dist = 1;
	    file <= file9 && rank <= rank9 && BOARD[to] == empty;
	    to += 10, file += 1, rank += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  return pmove;
}


unsigned int * CONV
w_gen_checks( tree_t * restrict __ptree__, unsigned int * restrict pmove )
{
  bitboard_t bb_piece, bb_rook_chk, bb_bishop_chk, bb_chk, bb_move_to;
  bitboard_t bb_diag1_chk, bb_diag2_chk, bb_file_chk, bb_drop_to, bb_desti;
  bitboard_t bb_rank_chk;
  const tree_t * restrict ptree = __ptree__;
  unsigned int u0, u1, u2;
  int from, to, sq_bk, idirec;

  sq_bk = SQ_BKING;
  bb_file_chk = AttackFile( sq_bk );
  bb_rank_chk = AttackRank( sq_bk );
  BBOr( bb_rook_chk, bb_file_chk, bb_rank_chk );

  bb_diag1_chk = AttackDiag1( sq_bk );
  bb_diag2_chk = AttackDiag2( sq_bk );
  BBOr( bb_bishop_chk, bb_diag1_chk, bb_diag2_chk );
  BBNot( bb_move_to, BB_WOCCUPY );
  BBOr( bb_drop_to, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop_to, bb_drop_to );


  from  = SQ_WKING;
  idirec = (int)adirec[sq_bk][from];
  if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
    {
      BBIni( bb_chk );
      add_behind_attacks( &bb_chk, idirec, sq_bk );
      BBAnd( bb_chk, bb_chk, abb_king_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(king)
	    | Cap2Move(BOARD[to]);
	}
    }


  bb_piece = BB_WDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_bk] );
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      AttackDragon( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = LastOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(dragon)
	    | Cap2Move(BOARD[to]);
	}
    }


  bb_piece = BB_WHORSE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_bk] );
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      AttackHorse( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(horse)
	    | Cap2Move(BOARD[to]);
	}
    }

  u0 = BB_WROOK.p[0];
  u1 = BB_WROOK.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_rook_chk;
	bb_chk.p[2] |= abb_king_attacks[sq_bk].p[2];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while ( bb_chk.p[2] )
	{
	  to          = first_one2( bb_chk.p[2] );
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}

      while( bb_chk.p[0] | bb_chk.p[1] )
	{
	  to          = first_one01( bb_chk.p[0], bb_chk.p[1] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(BOARD[to]);
	}
    }

  u2 = BB_WROOK.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];
      
      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_bk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(rook)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }

  u0 = BB_WBISHOP.p[0];
  u1 = BB_WBISHOP.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_bishop_chk;
	bb_chk.p[2] |= abb_king_attacks[sq_bk].p[2];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while ( bb_chk.p[2] )
	{
	  to          = first_one2( bb_chk.p[2] );
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}

      while( bb_chk.p[0] | bb_chk.p[1] )
	{
	  to          = first_one01( bb_chk.p[0], bb_chk.p[1] );
	  bb_chk.p[0] ^= abb_mask[to].p[0];
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(BOARD[to]);
	}
    }

  u2 = BB_WBISHOP.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];
      
      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_bk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(bishop)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }


  bb_piece = BB_WTGOLD;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_b_gold_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_gold_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = ( To2Move(to) | From2Move(from)
		       | Piece2Move(-BOARD[from])
		       | Cap2Move(BOARD[to]) );
	}
    }

  
  u2 = BB_WSILVER.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = abb_b_gold_attacks[sq_bk].p[1];
      bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= bb_move_to.p[2] & abb_w_silver_attacks[from].p[2];
      bb_chk.p[1] &= bb_move_to.p[1] & abb_w_silver_attacks[from].p[1];

      while( bb_chk.p[2] | bb_chk.p[1] )
	{
	  to          = first_one12( bb_chk.p[1], bb_chk.p[2] );
	  bb_chk.p[1] ^= abb_mask[to].p[1];
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u1 = BB_WSILVER.p[1] & 0x1ffU;
  while( u1 )
    {
      from = first_one1( u1 );
      u1   ^= abb_mask[from].p[1];
      
      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;
      
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= bb_move_to.p[2] & abb_w_silver_attacks[from].p[2];
      while ( bb_chk.p[2] )
	{
	  to          = first_one2( bb_chk.p[2] );
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }
  

  bb_piece = BB_WSILVER;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_b_silver_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_silver_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(silver)
	    | Cap2Move(BOARD[to]);
	}
    }

  
  u2 = BB_WKNIGHT.p[2];
  u1 = BB_WKNIGHT.p[1] & 0x3ffffU;
  while( u2 | u1 )
    {
      from = first_one12( u1, u2 );
      u2   ^= abb_mask[from].p[2];
      u1   ^= abb_mask[from].p[1];

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= abb_w_knight_attacks[from].p[2] & bb_move_to.p[2];

      while( bb_chk.p[2] )
	{
	  to          = first_one2( bb_chk.p[2] );
	  bb_chk.p[2] ^= abb_mask[to].p[2];
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(knight)
		       | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u0 = BB_WKNIGHT.p[0];
  u1 = BB_WKNIGHT.p[1] & 0x7fffe00U;
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk = abb_b_knight_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_knight_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(knight)
	    | Cap2Move(BOARD[to]);
	}
    }


  bb_piece = BB_WLANCE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, abb_plus_rays[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(lance)
	    | Cap2Move(BOARD[to]) | FLAG_PROMO;
	}
    }
  

  u0 = BB_WLANCE.p[0];
  u1 = BB_WLANCE.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk = bb_file_chk;
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	  BBAnd( bb_chk, bb_chk, abb_plus_rays[from] );
	}
      else { BBAnd( bb_chk, bb_file_chk, abb_minus_rays[sq_bk] ); }

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, bb_move_to );
      bb_chk.p[2] = bb_chk.p[2] & 0x7fc0000U;

      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(lance)
	    | Cap2Move(BOARD[to]);
	}
    }

  BBIni( bb_chk );
  bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
  if ( sq_bk > I8 ) { BBOr( bb_chk, bb_chk, abb_mask[sq_bk-nfile] ); }
  BBAnd( bb_chk, bb_chk, bb_move_to );
  BBAnd( bb_chk, bb_chk, BB_WPAWN_ATK );

  BBAnd( bb_piece, bb_diag1_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackDiag1( from );
      if ( BBContract( bb_desti, BB_W_BH ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(BOARD[to]);
	  if ( from > I5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  BBAnd( bb_piece, bb_diag2_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackDiag2( from );
      if ( BBContract( bb_desti, BB_W_BH ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(BOARD[to]);
	  if ( from > I5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  BBAnd( bb_piece, bb_rank_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackRank( from );
      if ( BBContract( bb_desti, BB_W_RD ) )
	{
	  BBNotAnd( bb_chk, bb_chk, abb_mask[to] );

	  *pmove = To2Move(to) | From2Move(from)
	    | Piece2Move(pawn) | Cap2Move(BOARD[to]);
	  if ( from > I5 ) { *pmove |= FLAG_PROMO; }
	  pmove += 1;
	}
    }

  while ( BBTest(bb_chk) )
    {
      to = FirstOne( bb_chk );
      Xor( to, bb_chk );

      from = to - nfile;
      *pmove = To2Move(to) | From2Move(from) | Piece2Move(pawn)
	| Cap2Move(BOARD[to]);
      if ( from > I5 ) { *pmove |= FLAG_PROMO; }
      pmove += 1;
    }


  if ( IsHandGold(HAND_W) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_b_gold_attacks[sq_bk] );
      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | Drop2Move(gold);
	}
    }
  

  if ( IsHandSilver(HAND_W) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_b_silver_attacks[sq_bk] );
      while( BBTest( bb_chk ) )
	{
	  to = FirstOne( bb_chk );
	  Xor( to, bb_chk );
	  *pmove++ = To2Move(to) | Drop2Move(silver);
	}
    }
  

  if ( IsHandKnight(HAND_W) && sq_bk > I8 )
    {
      to = sq_bk - 2*nfile - 1;
      if ( aifile[sq_bk] != file1 && BOARD[to] == empty )
	{
	  *pmove++ = To2Move(to) | Drop2Move(knight);
	}

      to = sq_bk - 2*nfile + 1;
      if ( aifile[sq_bk] != file9 && BOARD[to] == empty )
	{
	  *pmove++ = To2Move(to) | Drop2Move(knight);
	}
    }


  if ( IsHandPawn(HAND_W)
       && sq_bk > I9
       && ! ( BBToU(BB_WPAWN) & ( mask_file1 >> aifile[sq_bk] ) ) )
    {
      to = sq_bk - nfile;
      if ( BOARD[to] == empty && ! is_mate_w_pawn_drop( __ptree__, to ) )
	{
	  *pmove++ = To2Move(to) | Drop2Move(pawn);
	}
    }


  if ( IsHandLance(HAND_W) )
    {
      unsigned int move;
      int dist, min_dist;

      if ( (int)aifile[sq_bk] == file1
	   || (int)aifile[sq_bk] == file9 ) { min_dist = 2; }
      else                                  { min_dist = 3; }

      for ( to = sq_bk-nfile, dist = 1; to >= 0 && BOARD[to] == empty;
	    to -= nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(lance);
	  if      ( dist == 1 )       { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  if ( IsHandRook(HAND_W) )
    {
      unsigned int move;
      int file, dist, min_dist;

      if ( (int)aifile[sq_bk] == file1
	   || (int)aifile[sq_bk] == file9 ) { min_dist = 2; }
      else                                  { min_dist = 3; }

      for ( to = sq_bk-nfile, dist = 1; to >= 0 && BOARD[to] == empty;
	    to -= nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 )        { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist )  { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      for ( to = sq_bk+nfile, dist = 1; to < nsquare && BOARD[to] == empty;
	    to += nfile, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if ( (int)airank[to] == rank7 ) { move |= MOVE_CHK_CLEAR; }
	  if      ( dist == 1 )           { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist )     { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}


      if ( sq_bk < A8 || I2 < sq_bk ) { min_dist = 2; }
      else                            { min_dist = 3; }

      for ( file = (int)aifile[sq_bk]+1, to = sq_bk+1, dist = 1;
	    file <= file9 && BOARD[to] == empty;
	    file += 1, to += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 )       { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      for ( file = (int)aifile[sq_bk]-1, to = sq_bk-1, dist = 1;
	    file >= file1 && BOARD[to] == empty;
	    file -= 1, to -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(rook);
	  if      ( dist == 1 )           { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > min_dist )     { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  if ( IsHandBishop(HAND_W) )
    {
      unsigned int move;
      int file, rank, dist;

      to   = sq_bk;
      file = (int)aifile[sq_bk];
      rank = (int)airank[sq_bk];
      for ( to += 10, file += 1, rank += 1, dist = 1;
	    file <= file9 && rank <= rank9 && BOARD[to] == empty;
	    to += 10, file += 1, rank += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( rank == rank7 ) { move |= MOVE_CHK_CLEAR; }
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_bk;
      file = (int)aifile[sq_bk];
      rank = (int)airank[sq_bk];
      for ( to += 8, file -= 1, rank += 1, dist = 1;
	    file >= 0 && rank <= rank9 && BOARD[to] == empty;
	    to += 8, file -= 1, rank += 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( rank == rank7 ) { move |= MOVE_CHK_CLEAR; }
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_bk;
      file = (int)aifile[sq_bk];
      rank = (int)airank[sq_bk];
      for ( to -= 8, file += 1, rank -= 1, dist = 1;
	    file <= file9 && rank >= 0 && BOARD[to] == empty;
	    to -= 8, file += 1, rank -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}

      to   = sq_bk;
      file = (int)aifile[sq_bk];
      rank = (int)airank[sq_bk];
      for ( to -= 10, file -= 1, rank -= 1, dist = 1;
	    file >= 0 && rank >= 0 && BOARD[to] == empty;
	    to -= 10, file -= 1, rank -= 1, dist += 1 )
	{
	  move = To2Move(to) | Drop2Move(bishop);
	  if ( dist == 1 )     { move |= MOVE_CHK_CLEAR; }
	  else if ( dist > 2 ) { move |= MOVE_CHK_SET; }
	  *pmove++ = move;
	}
    }


  return pmove;
}


int CONV b_have_checks( tree_t * restrict __ptree__ )
{
  bitboard_t bb_piece, bb_rook_chk, bb_bishop_chk, bb_chk, bb_move_to;
  bitboard_t bb_diag1_chk, bb_diag2_chk, bb_file_chk, bb_drop_to, bb_desti;
  bitboard_t bb_rank_chk;
  const tree_t * restrict ptree = __ptree__;
  unsigned int u0, u1, u2;
  int from, to, sq_wk, idirec;

  sq_wk = SQ_WKING;
  BBOr( bb_drop_to, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop_to, bb_drop_to );

  if ( IsHandGold(HAND_B) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_w_gold_attacks[sq_wk] );
      if ( BBTest( bb_chk ) ) { return 1; }
    }

  if ( IsHandSilver(HAND_B) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_w_silver_attacks[sq_wk] );
      if ( BBTest( bb_chk ) ) { return 1; }
    }

  if ( IsHandKnight(HAND_B) && sq_wk < A2 )
    {
      if ( aifile[sq_wk] != file1
	   && BOARD[sq_wk + 2*nfile - 1] == empty ) { return 1; }

      if ( aifile[sq_wk] != file9
	   && BOARD[sq_wk + 2*nfile + 1] == empty ) { return 1;	}
    }

  if ( IsHandLance(HAND_B)
       && sq_wk + nfile < nsquare
       && BOARD[sq_wk + nfile] == empty ) { return 1; }

  if ( IsHandRook(HAND_B) )
    {
      if ( sq_wk + nfile < nsquare
	   && BOARD[sq_wk + nfile] == empty ) { return 1; }

      if ( file1 < (int)aifile[sq_wk]
	   && BOARD[sq_wk - 1] == empty ) { return 1; }

      if ( (int)aifile[sq_wk] < file9
	   && BOARD[sq_wk + 1] == empty ) { return 1; }

      if ( 0 <= sq_wk - nfile
	   && BOARD[sq_wk - nfile] == empty ) { return 1; }
    }

  if ( IsHandBishop(HAND_B) )
    {
      if ( 0 < (int)aifile[sq_wk]
	   && 0 < (int)airank[sq_wk]
	   && BOARD[sq_wk - 10] == empty ) { return 1; }

      if ( (int)aifile[sq_wk] < file9
	   && 0 < (int)airank[sq_wk]
	   && BOARD[sq_wk - 8] == empty ) { return 1; }

      if ( 0 < (int)aifile[sq_wk]
	   && (int)airank[sq_wk] < rank9
	   && BOARD[sq_wk + 8] == empty ) { return 1; }

      if ( (int)aifile[sq_wk] < file9
	   && (int)airank[sq_wk] < rank9
	   && BOARD[sq_wk + 10] == empty ) { return 1; }
    }

  if ( IsHandPawn(HAND_B)
       && sq_wk < A1
       && BOARD[sq_wk+nfile] == empty
       && ! ( BBToU(BB_BPAWN) & ( mask_file1 >> aifile[sq_wk] ) )
       && ! is_mate_b_pawn_drop( __ptree__, sq_wk+nfile ) )
    {
      return 1;
    }
  
  
  bb_file_chk = AttackFile( sq_wk );
  bb_rank_chk = AttackRank( sq_wk );
  BBOr( bb_rook_chk, bb_file_chk, bb_rank_chk );

  bb_diag1_chk = AttackDiag1( sq_wk );
  bb_diag2_chk = AttackDiag2( sq_wk );
  BBOr( bb_bishop_chk, bb_diag1_chk, bb_diag2_chk );
  BBNot( bb_move_to, BB_BOCCUPY );

  from  = SQ_BKING;
  idirec = (int)adirec[sq_wk][from];
  if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
    {
      BBIni( bb_chk );
      add_behind_attacks( &bb_chk, idirec, sq_wk );
      BBAnd( bb_chk, bb_chk, abb_king_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_BDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_wk] );
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      AttackDragon( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  bb_piece = BB_BHORSE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_wk] );
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      AttackHorse( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u1 = BB_BROOK.p[1];
  u2 = BB_BROOK.p[2];
  while( u1 | u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_rook_chk;
	bb_chk.p[0] |= abb_king_attacks[sq_wk].p[0];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u0 = BB_BROOK.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];
      
      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_wk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u1 = BB_BBISHOP.p[1];
  u2 = BB_BBISHOP.p[2];
  while( u1 | u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_bishop_chk;
	bb_chk.p[0] |= abb_king_attacks[sq_wk].p[0];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u0 = BB_BBISHOP.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];
      
      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_wk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_BTGOLD;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_w_gold_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_gold_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }
  

  u0 = BB_BSILVER.p[0];
  while( u0 )
    {
      from = last_one0( u0 );
      u0   ^= abb_mask[from].p[0];

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = abb_w_gold_attacks[sq_wk].p[1];
      bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= bb_move_to.p[0] & abb_b_silver_attacks[from].p[0];
      bb_chk.p[1] &= bb_move_to.p[1] & abb_b_silver_attacks[from].p[1];

      if ( bb_chk.p[0] | bb_chk.p[1] ) { return 1; }
    }
  

  u1 = BB_BSILVER.p[1] & 0x7fc0000U;
  while( u1 )
    {
      from = last_one1( u1 );
      u1   ^= abb_mask[from].p[1];
      
      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;
      
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= bb_move_to.p[0] & abb_b_silver_attacks[from].p[0];

      if ( bb_chk.p[0] ) { return 1; }
    }
  

  bb_piece = BB_BSILVER;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_w_silver_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_silver_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }
  

  u0 = BB_BKNIGHT.p[0];
  u1 = BB_BKNIGHT.p[1] & 0x7fffe00U;
  while( u0 | u1 )
    {
      from = last_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      bb_chk.p[0] &= abb_b_knight_attacks[from].p[0] & bb_move_to.p[0];

      if ( bb_chk.p[0] ) { return 1; }
    }
  

  u2 = BB_BKNIGHT.p[2];
  u1 = BB_BKNIGHT.p[1] & 0x3ffffU;
  while( u2 | u1 )
    {
      from = last_one12( u1, u2 );
      u2   ^= abb_mask[from].p[2];
      u1   ^= abb_mask[from].p[1];

      bb_chk = abb_w_knight_attacks[sq_wk];

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, abb_b_knight_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_BLANCE;
  while( BBTest( bb_piece ) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
      bb_chk.p[1] = bb_chk.p[2] = 0;

      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, abb_minus_rays[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }
  

  u1 = BB_BLANCE.p[1];
  u2 = BB_BLANCE.p[2];
  while( u1| u2 )
    {
      from = last_one12( u1, u2 );
      u1   ^= abb_mask[from].p[1];
      u2   ^= abb_mask[from].p[2];

      bb_chk = bb_file_chk;
      idirec = (int)adirec[sq_wk][from];
      if ( idirec && is_pinned_on_white_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_wk );
	  BBAnd( bb_chk, bb_chk, abb_minus_rays[from] );
	}
      else { BBAnd( bb_chk, bb_file_chk, abb_plus_rays[sq_wk] );}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, bb_move_to );
      bb_chk.p[0] = bb_chk.p[0] & 0x1ffU;

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  BBIni( bb_chk );
  bb_chk.p[0] = abb_w_gold_attacks[sq_wk].p[0];
  if ( sq_wk < A2 ) { BBOr( bb_chk, bb_chk, abb_mask[sq_wk+nfile] ); };
  BBAnd( bb_chk, bb_chk, bb_move_to );
  BBAnd( bb_chk, bb_chk, BB_BPAWN_ATK );
  if ( BBTest(bb_chk) ) { return 1; }

  BBAnd( bb_piece, bb_diag1_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }

      bb_desti = AttackDiag1( from );

      if ( BBContract( bb_desti, BB_B_BH ) ) { return 1; }
    }

  BBAnd( bb_piece, bb_diag2_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }

      bb_desti = AttackDiag2( from );

      if ( BBContract( bb_desti, BB_B_BH ) ) { return 1; }
    }
  
  BBAnd( bb_piece, bb_rank_chk, BB_BPAWN );
  while ( BBTest(bb_piece) )
    {
      from = LastOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from - nfile;
      if ( BOARD[to] > 0 ) { continue; }
      
      bb_desti = AttackRank( from );
      if ( BBContract( bb_desti, BB_B_RD ) ) { return 1; }
    }

  return 0;
}


int CONV w_have_checks( tree_t * restrict __ptree__ )
{
  bitboard_t bb_piece, bb_rook_chk, bb_bishop_chk, bb_chk, bb_move_to;
  bitboard_t bb_diag1_chk, bb_diag2_chk, bb_file_chk, bb_drop_to, bb_desti;
  bitboard_t bb_rank_chk;
  const tree_t * restrict ptree = __ptree__;
  unsigned int u0, u1, u2;
  int from, to, sq_bk, idirec;

  sq_bk = SQ_BKING;
  BBOr( bb_drop_to, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop_to, bb_drop_to );

  if ( IsHandGold(HAND_W) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_b_gold_attacks[sq_bk] );
      if ( BBTest( bb_chk ) ) { return 1; }
    }

  if ( IsHandSilver(HAND_W) )
    {
      BBAnd( bb_chk, bb_drop_to, abb_b_silver_attacks[sq_bk] );
      if ( BBTest( bb_chk ) ) { return 1; }
    }

  if ( IsHandKnight(HAND_W) && sq_bk > I8 )
    {
      if ( aifile[sq_bk] != file1
	   && BOARD[sq_bk - 2*nfile - 1] == empty ) { return 1; }

      if ( aifile[sq_bk] != file9
	   && BOARD[sq_bk - 2*nfile + 1] == empty ) { return 1;	}
    }

  if ( IsHandLance(HAND_W)
       && 0 <= sq_bk - nfile
       && BOARD[sq_bk - nfile] == empty ) { return 1; }

  if ( IsHandRook(HAND_W) )
    {
      if ( sq_bk + nfile < nsquare
	   && BOARD[sq_bk + nfile] == empty ) { return 1; }

      if ( file1 < (int)aifile[sq_bk]
	   && BOARD[sq_bk - 1] == empty ) { return 1; }

      if ( (int)aifile[sq_bk] < file9
	   && BOARD[sq_bk + 1] == empty ) { return 1; }

      if ( 0 <= sq_bk - nfile
	   && BOARD[sq_bk - nfile] == empty ) { return 1; }
    }

  if ( IsHandBishop(HAND_W) )
    {
      if ( 0 < (int)aifile[sq_bk]
	   && 0 < (int)airank[sq_bk]
	   && BOARD[sq_bk - 10] == empty ) { return 1; }

      if ( (int)aifile[sq_bk] < file9
	   && 0 < (int)airank[sq_bk]
	   && BOARD[sq_bk - 8] == empty ) { return 1; }

      if ( 0 < (int)aifile[sq_bk]
	   && (int)airank[sq_bk] < rank9
	   && BOARD[sq_bk + 8] == empty ) { return 1; }

      if ( (int)aifile[sq_bk] < file9
	   && (int)airank[sq_bk] < rank9
	   && BOARD[sq_bk + 10] == empty ) { return 1; }
    }

  if ( IsHandPawn(HAND_W)
       && sq_bk > I9
       && BOARD[sq_bk - nfile] == empty
       && ! ( BBToU(BB_WPAWN) & ( mask_file1 >> aifile[sq_bk] ) )
       && ! is_mate_w_pawn_drop( __ptree__, sq_bk - nfile ) )
    {
      return 1;
    }


  bb_file_chk = AttackFile( sq_bk );
  bb_rank_chk = AttackRank( sq_bk );
  BBOr( bb_rook_chk, bb_file_chk, bb_rank_chk );

  bb_diag1_chk = AttackDiag1( sq_bk );
  bb_diag2_chk = AttackDiag2( sq_bk );
  BBOr( bb_bishop_chk, bb_diag1_chk, bb_diag2_chk );
  BBNot( bb_move_to, BB_WOCCUPY );


  from  = SQ_WKING;
  idirec = (int)adirec[sq_bk][from];
  if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
    {
      BBIni( bb_chk );
      add_behind_attacks( &bb_chk, idirec, sq_bk );
      BBAnd( bb_chk, bb_chk, abb_king_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_WDRAGON;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_bk] );
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      AttackDragon( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_WHORSE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_bk] );
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      AttackHorse( bb_desti, from );
      BBAnd( bb_chk, bb_chk, bb_desti );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u0 = BB_WROOK.p[0];
  u1 = BB_WROOK.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_rook_chk;
	bb_chk.p[2] |= abb_king_attacks[sq_bk].p[2];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u2 = BB_WROOK.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];
      
      AttackRook( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_rook_chk, abb_king_attacks[sq_bk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u0 = BB_WBISHOP.p[0];
  u1 = BB_WBISHOP.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	bb_chk       = bb_bishop_chk;
	bb_chk.p[2] |= abb_king_attacks[sq_bk].p[2];
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  u2 = BB_WBISHOP.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];
      
      AttackBishop( bb_desti, from );

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  BBAnd( bb_chk, bb_desti, bb_move_to );
	}
      else {
	BBOr( bb_chk, bb_bishop_chk, abb_king_attacks[sq_bk] );
	BBAnd( bb_chk, bb_chk, bb_desti );
	BBAnd( bb_chk, bb_chk, bb_move_to );
      }

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_WTGOLD;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_b_gold_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_gold_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  
  u2 = BB_WSILVER.p[2];
  while( u2 )
    {
      from = first_one2( u2 );
      u2   ^= abb_mask[from].p[2];

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = abb_b_gold_attacks[sq_bk].p[1];
      bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= bb_move_to.p[2] & abb_w_silver_attacks[from].p[2];
      bb_chk.p[1] &= bb_move_to.p[1] & abb_w_silver_attacks[from].p[1];

      if ( bb_chk.p[2] | bb_chk.p[1] ) { return 1; }
    }
  

  u1 = BB_WSILVER.p[1] & 0x1ffU;
  while( u1 )
    {
      from = first_one1( u1 );
      u1   ^= abb_mask[from].p[1];
      
      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;
      
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= bb_move_to.p[2] & abb_w_silver_attacks[from].p[2];

      if ( bb_chk.p[2] ) { return 1; }
    }
  

  bb_piece = BB_WSILVER;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk = abb_b_silver_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_silver_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  
  u2 = BB_WKNIGHT.p[2];
  u1 = BB_WKNIGHT.p[1] & 0x3ffffU;
  while( u2 | u1 )
    {
      from = first_one12( u1, u2 );
      u2   ^= abb_mask[from].p[2];
      u1   ^= abb_mask[from].p[1];

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      bb_chk.p[2] &= abb_w_knight_attacks[from].p[2] & bb_move_to.p[2];

      if ( bb_chk.p[2] ) { return 1; }
    }
  

  u0 = BB_WKNIGHT.p[0];
  u1 = BB_WKNIGHT.p[1] & 0x7fffe00U;
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk = abb_b_knight_attacks[sq_bk];

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, abb_w_knight_attacks[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }


  bb_piece = BB_WLANCE;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );

      bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
      bb_chk.p[1] = bb_chk.p[0] = 0;

      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	}

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, abb_plus_rays[from] );
      BBAnd( bb_chk, bb_chk, bb_move_to );

      if ( BBTest( bb_chk ) ) { return 1; }
    }
  

  u0 = BB_WLANCE.p[0];
  u1 = BB_WLANCE.p[1];
  while( u0 | u1 )
    {
      from = first_one01( u0, u1 );
      u0   ^= abb_mask[from].p[0];
      u1   ^= abb_mask[from].p[1];

      bb_chk = bb_file_chk;
      idirec = (int)adirec[sq_bk][from];
      if ( idirec && is_pinned_on_black_king( ptree, from, idirec ) )
	{
	  add_behind_attacks( &bb_chk, idirec, sq_bk );
	  BBAnd( bb_chk, bb_chk, abb_plus_rays[from] );
	}
      else { BBAnd( bb_chk, bb_file_chk, abb_minus_rays[sq_bk] ); }

      BBAnd( bb_chk, bb_chk, AttackFile( from ) );
      BBAnd( bb_chk, bb_chk, bb_move_to );
      bb_chk.p[2] = bb_chk.p[2] & 0x7fc0000U;

      if ( BBTest( bb_chk ) ) { return 1; }
    }

  BBIni( bb_chk );
  bb_chk.p[2] = abb_b_gold_attacks[sq_bk].p[2];
  if ( sq_bk > I8 ) { BBOr( bb_chk, bb_chk, abb_mask[sq_bk-nfile] ); };
  BBAnd( bb_chk, bb_chk, bb_move_to );
  BBAnd( bb_chk, bb_chk, BB_WPAWN_ATK );
  if ( BBTest( bb_chk ) ) { return 1; }

  BBAnd( bb_piece, bb_diag1_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackDiag1( from );

      if ( BBContract( bb_desti, BB_W_BH ) ) { return 1; }
    }

  BBAnd( bb_piece, bb_diag2_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackDiag2( from );

      if ( BBContract( bb_desti, BB_W_BH ) ) { return 1; }
    }

  BBAnd( bb_piece, bb_rank_chk, BB_WPAWN );
  while ( BBTest(bb_piece) )
    {
      from = FirstOne( bb_piece );
      Xor( from, bb_piece );
      
      to = from + nfile;
      if ( BOARD[to] < 0 ) { continue; }

      bb_desti = AttackRank( from );
      if ( BBContract( bb_desti, BB_W_RD ) ) { return 1; }
    }

  return 0;
}


static void CONV
add_behind_attacks( bitboard_t * restrict pbb, int idirec, int ik )
{
  bitboard_t bb_tmp;

  if ( idirec == direc_diag1 )
    {
      bb_tmp = abb_bishop_attacks_rr45[ik][0];
    }
  else if ( idirec == direc_diag2 )
    {
      bb_tmp = abb_bishop_attacks_rl45[ik][0];
    }
  else if ( idirec == direc_file )
    {
      bb_tmp = abb_file_attacks[ik][0];
    }
  else {
    assert( idirec == direc_rank );
    bb_tmp = abb_rank_attacks[ik][0];
  }
  BBNot( bb_tmp, bb_tmp );
  BBOr( *pbb, *pbb, bb_tmp );
}
