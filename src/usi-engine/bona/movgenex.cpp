// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "shogi.h"


#define BAddMoveCap(piece)                                             \
            utemp = From2Move(from) | Piece2Move(piece);               \
            while ( BBTest( bb_move ) ) {                              \
	      to       = LastOne( bb_move );                           \
	      *pmove++ = To2Move(to) | Cap2Move(-BOARD[to]) | utemp;   \
	      Xor( to, bb_move ); }

#define BAddMove(piece) utemp = From2Move(from) | Piece2Move(piece);   \
                        while ( BBTest( bb_move ) ) {                  \
                        to       = LastOne( bb_move );                 \
                        *pmove++ = To2Move(to) | utemp;                \
	                Xor( to, bb_move ); }


#define WAddMoveCap(piece)                                             \
            utemp = From2Move(from) | Piece2Move(piece);               \
            while ( BBTest( bb_move ) ) {                              \
	      to      = FirstOne( bb_move );                           \
	      *pmove++ = To2Move(to) | Cap2Move(BOARD[to]) | utemp;    \
	      Xor( to, bb_move ); }

#define WAddMove(piece) utemp = From2Move(from) | Piece2Move(piece);   \
                         while ( BBTest( bb_move ) ) {                 \
	                   to       = FirstOne( bb_move );             \
	                   *pmove++ = To2Move(to) | utemp;             \
	                   Xor( to, bb_move ); }


unsigned int * CONV
b_gen_cap_nopro_ex2( const tree_t * restrict ptree,
		     unsigned int * restrict pmove )
{
  int from, to;
  unsigned int utemp, ubb_piece0, ubb_piece1, ubb_piece2, ubb_move0;
  bitboard_t bb_target, bb_move, bb_piece;

  bb_target = BB_WOCCUPY;

  ubb_move0 = BB_BPAWN_ATK.p[0] & bb_target.p[0] & 0x003ffffU;
  while( ubb_move0 )
    {
      to   = last_one0( ubb_move0 );
      from = to + 9;
      *pmove++ = To2Move(to) | From2Move(from)
	| Cap2Move(-BOARD[to]) | Piece2Move(pawn);
      ubb_move0 ^= abb_mask[to].p[0];
    }

  ubb_piece1 = BB_BBISHOP.p[1];
  ubb_piece2 = BB_BBISHOP.p[2];
  while( ubb_piece1 | ubb_piece2 )
    {
      from     = last_one12( ubb_piece1, ubb_piece2 );
      ubb_move0 = BishopAttack0(from) & bb_target.p[0];
      utemp     = From2Move(from) | Piece2Move(bishop);
      while ( ubb_move0 )
	{
	  to      = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | Cap2Move(-BOARD[to]) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      ubb_piece1 ^= abb_mask[from].p[1];
      ubb_piece2 ^= abb_mask[from].p[2];
    }
  ubb_piece0 = BB_BBISHOP.p[0];
  while( ubb_piece0 )
    {
      from = last_one0( ubb_piece0 );
      AttackBishop( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      BAddMoveCap( bishop );
      ubb_piece0 ^= abb_mask[from].p[0];
    }

  ubb_piece1 = BB_BROOK.p[1];
  ubb_piece2 = BB_BROOK.p[2];
  while( ubb_piece1 | ubb_piece2 )
    {
      from     = last_one12( ubb_piece1, ubb_piece2 );
      AttackRook( bb_move, from );
      ubb_move0 = bb_move.p[0] & bb_target.p[0];
      utemp = From2Move(from) | Piece2Move(rook);
      while ( ubb_move0 )
	{
	  to      = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | Cap2Move(-BOARD[to]) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      ubb_piece1 ^= abb_mask[from].p[1];
      ubb_piece2 ^= abb_mask[from].p[2];
    }
  ubb_piece0 = BB_BROOK.p[0];
  while( ubb_piece0 )
    {
      from     = last_one0( ubb_piece0 );
      AttackRook( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      BAddMoveCap( rook );
      ubb_piece0 ^= abb_mask[from].p[0];
    }

  bb_piece = BB_BLANCE;
  bb_target.p[0] &= 0x3fe00;
  while( BBTest( bb_piece ) )
    {
      from     = LastOne( bb_piece );
      ubb_move0 = AttackFile(from).p[0]
	& abb_minus_rays[from].p[0] & bb_target.p[0];
      utemp = From2Move(from) | Piece2Move(lance);
      while ( ubb_move0 )
	{
	  to      = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | Cap2Move(-BOARD[to]) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      Xor( from, bb_piece );
    }

  return pmove;
}


unsigned int * CONV
b_gen_nocap_nopro_ex2( const tree_t * restrict ptree,
		       unsigned int * restrict pmove )
{
  bitboard_t bb_target, bb_move, bb_piece;
  unsigned int ubb_piece0, ubb_piece1, ubb_piece2, ubb_move0, ubb_target0;
  unsigned int utemp;
  int to, from;

  BBOr( bb_target, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_target, bb_target );

  ubb_move0 = BB_BPAWN_ATK.p[0] & bb_target.p[0] & 0x003ffffU;
  while( ubb_move0 )
    {
      to   = last_one0( ubb_move0 );
      from = to + 9;
      *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(pawn);
      ubb_move0 ^= abb_mask[to].p[0];
    }

  ubb_piece1 = BB_BBISHOP.p[1];
  ubb_piece2 = BB_BBISHOP.p[2];
  while( ubb_piece1 | ubb_piece2 )
    {
      from     = last_one12( ubb_piece1, ubb_piece2 );
      ubb_move0 = BishopAttack0(from) & bb_target.p[0];
      utemp     = From2Move(from) | Piece2Move(bishop);
      while ( ubb_move0 )
	{
	  to      = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      ubb_piece1 ^= abb_mask[from].p[1];
      ubb_piece2 ^= abb_mask[from].p[2];
    }
  ubb_piece0 = BB_BBISHOP.p[0];
  while( ubb_piece0 )
    {
      from = last_one0( ubb_piece0 );
      AttackBishop( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      BAddMove( bishop );
      ubb_piece0 ^= abb_mask[from].p[0];
    }

  ubb_piece1 = BB_BROOK.p[1];
  ubb_piece2 = BB_BROOK.p[2];
  while( ubb_piece1 | ubb_piece2 )
    {
      from     = last_one12( ubb_piece1, ubb_piece2 );
      AttackRook( bb_move, from );
      ubb_move0 = bb_move.p[0] & bb_target.p[0];
      utemp = From2Move(from) | Piece2Move(rook);
      while ( ubb_move0 )
	{
	  to      = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      ubb_piece1 ^= abb_mask[from].p[1];
      ubb_piece2 ^= abb_mask[from].p[2];
    }
  ubb_piece0 = BB_BROOK.p[0];
  while( ubb_piece0 )
    {
      from     = last_one0( ubb_piece0 );
      AttackRook( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      BAddMove( rook );
      ubb_piece0 ^= abb_mask[from].p[0];
    }

  bb_piece = BB_BLANCE;
  ubb_target0 = bb_target.p[0] & 0x3fe00;
  while( BBTest( bb_piece ) )
    {
      from     = LastOne( bb_piece );
      ubb_move0 = AttackFile(from).p[0]
	& abb_minus_rays[from].p[0] & ubb_target0;
      utemp = From2Move(from) | Piece2Move(lance);
      while ( ubb_move0 )
	{
	  to = last_one0( ubb_move0 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move0 ^= abb_mask[to].p[0];
	}
      Xor( from, bb_piece );
    }

  return pmove;
}


unsigned int * CONV
w_gen_cap_nopro_ex2( const tree_t * restrict ptree,
		     unsigned int * restrict pmove )
{
  bitboard_t bb_target, bb_move, bb_piece;
  unsigned int utemp, ubb_piece0, ubb_piece1, ubb_piece2, ubb_move2;
  int from, to;

  bb_target = BB_BOCCUPY;

  ubb_move2 = BB_WPAWN_ATK.p[2] & bb_target.p[2] & 0x7fffe00U;
  while( ubb_move2 )
    {
      to   = first_one2( ubb_move2 );
      from = to - 9;
      *pmove++ = To2Move(to) | From2Move(from)
	| Cap2Move(BOARD[to]) | Piece2Move(pawn);
      ubb_move2 ^= abb_mask[to].p[2];
    }

  ubb_piece0 = BB_WBISHOP.p[0];
  ubb_piece1 = BB_WBISHOP.p[1];
  while( ubb_piece0 | ubb_piece1 )
    {
      from     = first_one01( ubb_piece0, ubb_piece1 );
      ubb_move2 = BishopAttack2(from) & bb_target.p[2];
      utemp     = From2Move(from) | Piece2Move(bishop);
      while ( ubb_move2 )
	{
	  to      = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | Cap2Move(BOARD[to]) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      ubb_piece0 ^= abb_mask[from].p[0];
      ubb_piece1 ^= abb_mask[from].p[1];
    }
  ubb_piece2 = BB_WBISHOP.p[2];
  while( ubb_piece2 )
    {
      from = first_one2( ubb_piece2 );
      AttackBishop( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      WAddMoveCap( bishop );
      ubb_piece2 ^= abb_mask[from].p[2];
    }

  ubb_piece0 = BB_WROOK.p[0];
  ubb_piece1 = BB_WROOK.p[1];
  while( ubb_piece0 | ubb_piece1 )
    {
      from     = first_one01( ubb_piece0, ubb_piece1 );
      AttackRook( bb_move, from );
      ubb_move2 = bb_move.p[2] & bb_target.p[2];
      utemp     = From2Move(from) | Piece2Move(rook);
      while ( ubb_move2 )
	{
	  to      = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | Cap2Move(BOARD[to]) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      ubb_piece0 ^= abb_mask[from].p[0];
      ubb_piece1 ^= abb_mask[from].p[1];
    }
  ubb_piece2 = BB_WROOK.p[2];
  while( ubb_piece2 )
    {
      from     = first_one2( ubb_piece2 );
      AttackRook( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      WAddMoveCap( rook );
      ubb_piece2 ^= abb_mask[from].p[2];
    }

  bb_piece = BB_WLANCE;
  bb_target.p[2] &= 0x3fe00;
  while( BBTest( bb_piece ) )
    {
      from      = FirstOne( bb_piece );
      ubb_move2 = AttackFile(from).p[2]
	& abb_plus_rays[from].p[2] & bb_target.p[2];
      utemp = From2Move(from) | Piece2Move(lance);
      while ( ubb_move2 )
	{
	  to       = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | Cap2Move(BOARD[to]) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      Xor( from, bb_piece );
    }

  return pmove;
}


unsigned int * CONV
w_gen_nocap_nopro_ex2( const tree_t * restrict ptree,
		       unsigned int * restrict pmove )
{
  bitboard_t bb_target, bb_piece, bb_move;
  unsigned int ubb_piece0, ubb_piece1, ubb_piece2, ubb_move2, ubb_target2;
  unsigned int utemp;
  int to, from;

  BBOr( bb_target, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_target, bb_target );

  ubb_move2 = BB_WPAWN_ATK.p[2] & bb_target.p[2] & 0x7fffe00U;;
  while( ubb_move2 )
    {
      to   = first_one2( ubb_move2 );
      from = to - 9;
      *pmove++ = To2Move(to) | From2Move(from) | Piece2Move(pawn);
      ubb_move2 ^= abb_mask[to].p[2];
    }

  ubb_piece0 = BB_WBISHOP.p[0];
  ubb_piece1 = BB_WBISHOP.p[1];
  while( ubb_piece0 | ubb_piece1 )
    {
      from     = first_one01( ubb_piece0, ubb_piece1 );
      ubb_move2 = BishopAttack2(from) & bb_target.p[2];
      utemp     = From2Move(from) | Piece2Move(bishop);
      while ( ubb_move2 )
	{
	  to      = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      ubb_piece0 ^= abb_mask[from].p[0];
      ubb_piece1 ^= abb_mask[from].p[1];
    }
  ubb_piece2 = BB_WBISHOP.p[2];
  while( ubb_piece2 )
    {
      from = first_one2( ubb_piece2 );
      AttackBishop( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      WAddMove( bishop );
      ubb_piece2 ^= abb_mask[from].p[2];
    }

  ubb_piece0 = BB_WROOK.p[0];
  ubb_piece1 = BB_WROOK.p[1];
  while( ubb_piece0 | ubb_piece1 )
    {
      from      = first_one01( ubb_piece0, ubb_piece1 );
      AttackRook( bb_move, from );
      ubb_move2 = bb_move.p[2] & bb_target.p[2];
      utemp     = From2Move(from) | Piece2Move(rook);
      while ( ubb_move2 )
	{
	  to       = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      ubb_piece0 ^= abb_mask[from].p[0];
      ubb_piece1 ^= abb_mask[from].p[1];
    }
  ubb_piece2 = BB_WROOK.p[2];
  while( ubb_piece2 )
    {
      from     = first_one2( ubb_piece2 );
      AttackRook( bb_move, from );
      BBAnd( bb_move, bb_move, bb_target );
      WAddMove( rook );
      ubb_piece2 ^= abb_mask[from].p[2];
    }

  bb_piece = BB_WLANCE;
  ubb_target2 = bb_target.p[2] & 0x3fe00;
  while( BBTest( bb_piece ) )
    {
      from = FirstOne( bb_piece );
      ubb_move2 = AttackFile(from).p[2]
	& abb_plus_rays[from].p[2] & ubb_target2;
      utemp = From2Move(from) | Piece2Move(lance);
      while ( ubb_move2 )
	{
	  to = first_one2( ubb_move2 );
	  *pmove++ = To2Move(to) | utemp;
	  ubb_move2 ^= abb_mask[to].p[2];
	}
      Xor( from, bb_piece );
    }

  return pmove;
}


#undef BAddMoveCap
#undef BAddMove
#undef WAddMoveCap
#undef WAddMove
