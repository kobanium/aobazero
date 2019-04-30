// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <stdlib.h>
#include "shogi.h"

#define DebugOut { static int count = 0; \
                   if ( count++ < 16 ) { out_CSA_posi( ptree, stdout, 0 ); } }

static int CONV can_w_king_escape( tree_t * restrict ptree, int to,
				   const bitboard_t * restrict pbb );
static int CONV can_b_king_escape( tree_t * restrict ptree, int to,
				   const bitboard_t * restrict pbb );
static int CONV can_w_piece_capture( const tree_t * restrict ptree, int to );
static int CONV can_b_piece_capture( const tree_t * restrict ptree, int to );


unsigned int CONV
is_b_mate_in_1ply( tree_t * restrict ptree )
{
  bitboard_t bb, bb_temp, bb_check, bb_check_pro, bb_attacks, bb_drop, bb_move;
  unsigned int ubb;
  int to, from, idirec;

  assert( ! is_black_attacked( ptree, SQ_BKING ) );

  /*  Drops  */
  BBOr( bb_drop, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop, bb_drop );

  if ( IsHandRook(HAND_B) ) {

    BBAnd( bb, abb_w_gold_attacks[SQ_WKING], abb_b_gold_attacks[SQ_WKING] );
    BBAnd( bb, bb, bb_drop );
    while( BBTest(bb) )
      {
	to = FirstOne( bb );
	Xor( to, bb );

	if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
	BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_w_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(rook);
      }

  } else if ( IsHandLance(HAND_B) && SQ_WKING <= I2 ) {

    to = SQ_WKING+nfile;
    if ( ! BOARD[to] && is_white_attacked( ptree, to ) )
      {
	bb_attacks = abb_file_attacks[to][0];
	if ( ! can_w_king_escape( ptree, to, &bb_attacks )
	     && ! can_w_piece_capture( ptree, to ) )
	  {
	    return To2Move(to) | Drop2Move(lance);
	  }
      }
  }

  if ( IsHandBishop(HAND_B) ) {

    BBAnd( bb, abb_w_silver_attacks[SQ_WKING],
	   abb_b_silver_attacks[SQ_WKING] );
    BBAnd( bb, bb, bb_drop );
    while( BBTest(bb) )
      {
	to = FirstOne( bb );
	Xor( to, bb );

	if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
	BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	      abb_bishop_attacks_rl45[to][0] );
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_w_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(bishop);
      }
  }

  if ( IsHandGold(HAND_B) ) {

    if ( IsHandRook(HAND_B) )
      {
	BBAnd( bb, abb_b_gold_attacks[SQ_WKING],
	       abb_b_silver_attacks[SQ_WKING] );
	BBNotAnd( bb, bb_drop, bb );
	BBAnd( bb, bb, abb_w_gold_attacks[SQ_WKING] );
      }
    else { BBAnd( bb, bb_drop, abb_w_gold_attacks[SQ_WKING] ); }

    while ( BBTest(bb) )
      {
	to = FirstOne( bb );
	Xor( to, bb );
	
	if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_b_gold_attacks[to];
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_w_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(gold);
      }
  }
  
  if ( IsHandSilver(HAND_B) ) {
    
    if ( IsHandGold(HAND_B) )
      {
	if ( IsHandBishop(HAND_B) ) { goto b_silver_drop_end; }
	BBNotAnd( bb,
		  abb_w_silver_attacks[SQ_WKING],
		  abb_w_gold_attacks[SQ_WKING]  );
	BBAnd( bb, bb, bb_drop );
      }
    else {
      BBAnd( bb, bb_drop, abb_w_silver_attacks[SQ_WKING] );
      if ( IsHandBishop(HAND_B) )
	{
	  BBAnd( bb, bb, abb_w_gold_attacks[SQ_WKING] );
	}
    }
    
    while ( BBTest(bb) )
      {
	to = FirstOne( bb );
	Xor( to, bb );
	
	if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_b_silver_attacks[to];
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_w_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(silver);
      }
  }
 b_silver_drop_end:
 
  if ( IsHandKnight(HAND_B) ) {
    
    BBAnd( bb, bb_drop, abb_w_knight_attacks[SQ_WKING] );
    while ( BBTest(bb) )
      {
	to = FirstOne( bb );
	Xor( to, bb );
	
	BBIni( bb_attacks );
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_w_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(knight);
      }
  }

  /*  Moves  */
  BBNot( bb_move, BB_BOCCUPY );

  bb = BB_BDRAGON;
  while ( BBTest(bb) ) {
    from = FirstOne( bb );
    Xor( from, bb );

    AttackDragon( bb_attacks, from );
    BBAnd( bb_check, bb_move,  bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_WKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_B_HDK );
    Xor( from, BB_B_RD );
    Xor( from, BB_BOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }

      if ( (int)adirec[SQ_WKING][to] & flag_cross )
	{
	  BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	  BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
	}
      else { AttackDragon( bb_attacks, to ); }

      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_BOCCUPY );
      Xor( from, BB_B_RD );
      Xor( from, BB_B_HDK );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(dragon) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_BOCCUPY );
    Xor( from, BB_B_RD );
    Xor( from, BB_B_HDK );
  }

  bb.p[0] = BB_BROOK.p[0];
  while ( bb.p[0] ) {
    from = last_one0( bb.p[0] );
    bb.p[0] ^= abb_mask[from].p[0];

    AttackRook( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_WKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    BB_B_RD.p[0]    ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      if ( (int)adirec[SQ_WKING][to] & flag_cross )
	{
	  BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	  BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
	}
      else { AttackDragon( bb_attacks, to ); }

      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_B_RD.p[0]    ^= abb_mask[from].p[0];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(-BOARD[to]) | Piece2Move(rook) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_B_RD.p[0]    ^= abb_mask[from].p[0];
  }

  bb.p[1] = BB_BROOK.p[1];
  bb.p[2] = BB_BROOK.p[2];
  while ( bb.p[1] | bb.p[2] ) {
    from = first_one12( bb.p[1], bb.p[2] );
    bb.p[1] ^= abb_mask[from].p[1];
    bb.p[2] ^= abb_mask[from].p[2];

    AttackRook( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    bb_check.p[0] &= abb_king_attacks[SQ_WKING].p[0];
    bb_check.p[1] &= abb_b_gold_attacks[SQ_WKING].p[1];
    bb_check.p[2] &= abb_b_gold_attacks[SQ_WKING].p[2];
    bb_check.p[1] &= abb_w_gold_attacks[SQ_WKING].p[1];
    bb_check.p[2] &= abb_w_gold_attacks[SQ_WKING].p[2];
    if ( ! BBTest(bb_check) ) { continue; }

    BB_B_RD.p[1]    ^= abb_mask[from].p[1];
    BB_B_RD.p[2]    ^= abb_mask[from].p[2];
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      if ( to <= I7 ) {
	if ( (int)adirec[SQ_WKING][to] & flag_cross )
	  {
	    BBOr(bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0]);
	    bb_attacks.p[0] |= abb_king_attacks[to].p[0];
	    bb_attacks.p[1] |= abb_king_attacks[to].p[1];
	  }
	else { AttackDragon( bb_attacks, to ); }

      } else {
	BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
      }
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_B_RD.p[1]    ^= abb_mask[from].p[1];
      BB_B_RD.p[2]    ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from)
	       | ( (to < A6) ? FLAG_PROMO : 0 )
	       | Cap2Move(-BOARD[to]) | Piece2Move(rook) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    BB_B_RD.p[1]    ^= abb_mask[from].p[1];
    BB_B_RD.p[2]    ^= abb_mask[from].p[2];
  }

  bb = BB_BHORSE;
  while ( BBTest(bb) ) {
    from = FirstOne( bb );
    Xor( from, bb );

    AttackHorse( bb_attacks, from );
    BBAnd( bb_check, bb_move,  bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_WKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_B_HDK );
    Xor( from, BB_B_BH );
    Xor( from, BB_BOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_BOCCUPY );
      Xor( from, BB_B_BH );
      Xor( from, BB_B_HDK );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(horse) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_BOCCUPY );
    Xor( from, BB_B_BH );
    Xor( from, BB_B_HDK );
  }

  bb.p[0] = BB_BBISHOP.p[0];
  while ( bb.p[0] ) {
    from = last_one0( bb.p[0] );
    bb.p[0] ^= abb_mask[from].p[0];

    AttackBishop( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_WKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    BB_B_BH.p[0]    ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_B_BH.p[0]    ^= abb_mask[from].p[0];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(-BOARD[to]) | Piece2Move(bishop) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_B_BH.p[0]    ^= abb_mask[from].p[0];
  }

  bb.p[1] = BB_BBISHOP.p[1];
  bb.p[2] = BB_BBISHOP.p[2];
  while ( bb.p[1] | bb.p[2] ) {
    from = first_one12( bb.p[1], bb.p[2] );
    bb.p[1] ^= abb_mask[from].p[1];
    bb.p[2] ^= abb_mask[from].p[2];

    AttackBishop( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    bb_check.p[0] &= abb_king_attacks[SQ_WKING].p[0];
    bb_check.p[1] &= abb_b_silver_attacks[SQ_WKING].p[1];
    bb_check.p[2] &= abb_b_silver_attacks[SQ_WKING].p[2];
    bb_check.p[1] &= abb_w_silver_attacks[SQ_WKING].p[1];
    bb_check.p[2] &= abb_w_silver_attacks[SQ_WKING].p[2];
    if ( ! BBTest(bb_check) ) { continue; }

    BB_B_BH.p[1]    ^= abb_mask[from].p[1];
    BB_B_BH.p[2]    ^= abb_mask[from].p[2];
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      if ( to <= I7 ) {
	bb_attacks.p[0] |= abb_king_attacks[to].p[0];
	bb_attacks.p[1] |= abb_king_attacks[to].p[1];
      }
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_B_BH.p[1]    ^= abb_mask[from].p[1];
      BB_B_BH.p[2]    ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from)
	       | ( (to < A6) ? FLAG_PROMO : 0 )
	       | Cap2Move(-BOARD[to]) | Piece2Move(bishop) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    BB_B_BH.p[1]    ^= abb_mask[from].p[1];
    BB_B_BH.p[2]    ^= abb_mask[from].p[2];
  }

  BBAnd( bb, BB_BTGOLD, b_chk_tbl[SQ_WKING].gold );
  while ( BBTest(bb) ) {
    from = FirstOne( bb );
    Xor( from, bb );

    BBAnd( bb_check, bb_move, abb_b_gold_attacks[from] );
    BBAnd( bb_check, bb_check, abb_w_gold_attacks[SQ_WKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_BTGOLD );
    Xor( from, BB_BOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = FirstOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      bb_attacks = abb_b_gold_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_BOCCUPY );
      Xor( from, BB_BTGOLD );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(BOARD[from]) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_BOCCUPY );
    Xor( from, BB_BTGOLD );
  }

  BBAnd( bb, BB_BSILVER, b_chk_tbl[SQ_WKING].silver );
  while ( bb.p[0] ) {
    from = last_one0( bb.p[0] );
    bb.p[0] ^= abb_mask[from].p[0];

    bb_check_pro.p[0] = bb_move.p[0] & abb_b_silver_attacks[from].p[0]
      & abb_w_gold_attacks[SQ_WKING].p[0];
    bb_check_pro.p[1] = bb_move.p[1] & abb_b_silver_attacks[from].p[1]
      & abb_w_gold_attacks[SQ_WKING].p[1];

    bb_check.p[0] = bb_move.p[0] & abb_b_silver_attacks[from].p[0]
      & abb_w_silver_attacks[SQ_WKING].p[0]
      & ~abb_w_gold_attacks[SQ_WKING].p[0];
    bb_check.p[1] = bb_move.p[1] & abb_b_silver_attacks[from].p[1]
      & abb_w_silver_attacks[SQ_WKING].p[1]
      & ~abb_w_gold_attacks[SQ_WKING].p[1];

    if ( ! ( bb_check_pro.p[0] | bb_check_pro.p[1]
	     | bb_check.p[0]| bb_check.p[1] ) ) { continue; }

    BB_BSILVER.p[0] ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    while ( bb_check_pro.p[0] | bb_check_pro.p[1] ) {
      to = first_one01( bb_check_pro.p[0], bb_check_pro.p[1] );
      bb_check_pro.p[0] ^= abb_mask[to].p[0];
      bb_check_pro.p[1] ^= abb_mask[to].p[1];
      
      if ( ! is_white_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_b_gold_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_BSILVER.p[0] ^= abb_mask[from].p[0];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(-BOARD[to]) | Piece2Move(silver) );
    }

    while ( bb_check.p[0] | bb_check.p[1] ) {
      to = first_one01( bb_check.p[0], bb_check.p[1] );
      bb_check.p[0] ^= abb_mask[to].p[0];
      bb_check.p[1] ^= abb_mask[to].p[1];
      
      if ( ! is_white_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_b_silver_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_BSILVER.p[0] ^= abb_mask[from].p[0];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(silver) );
    }

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_BSILVER.p[0] ^= abb_mask[from].p[0];
  }

  ubb = bb.p[1] & 0x7fc0000U;
  while ( ubb ) {
    from = last_one1( ubb );
    ubb ^= abb_mask[from].p[1];

    bb_check_pro.p[0] = bb_move.p[0] & abb_b_silver_attacks[from].p[0]
      & abb_w_gold_attacks[SQ_WKING].p[0];

    bb_check.p[0] = bb_move.p[0] & abb_b_silver_attacks[from].p[0]
      & abb_w_silver_attacks[SQ_WKING].p[0]
      & ~abb_w_gold_attacks[SQ_WKING].p[0];
    bb_check.p[1] = bb_move.p[1] & abb_b_silver_attacks[from].p[1]
      & abb_w_silver_attacks[SQ_WKING].p[1];

    if ( ! (bb_check_pro.p[0]|bb_check.p[0]|bb_check.p[1]) ) { continue; }

    BB_BSILVER.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    while ( bb_check_pro.p[0] ) {
      to = last_one0( bb_check_pro.p[0] );
      bb_check_pro.p[0] ^= abb_mask[to].p[0];
      
      if ( ! is_white_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_b_gold_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BSILVER.p[1] ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(-BOARD[to]) | Piece2Move(silver) );
    }

    while ( bb_check.p[0] | bb_check.p[1] ) {
      to = first_one01( bb_check.p[0], bb_check.p[1] );
      bb_check.p[0] ^= abb_mask[to].p[0];
      bb_check.p[1] ^= abb_mask[to].p[1];
      
      if ( ! is_white_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_b_silver_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BSILVER.p[1] ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(silver) );
    }

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BSILVER.p[1] ^= abb_mask[from].p[1];
  }

  bb.p[1] &= 0x003ffffU;
  while ( bb.p[1] | bb.p[2] ) {
    from = first_one12( bb.p[1], bb.p[2] );
    bb.p[1] ^= abb_mask[from].p[1];
    bb.p[2] ^= abb_mask[from].p[2];

    bb_check.p[1] = bb_move.p[1] & abb_b_silver_attacks[from].p[1]
      & abb_w_silver_attacks[SQ_WKING].p[1];
    bb_check.p[2] = bb_move.p[2] & abb_b_silver_attacks[from].p[2]
      & abb_w_silver_attacks[SQ_WKING].p[2];
    if ( ! ( bb_check.p[1] | bb_check.p[2] ) ) { continue; }

    BB_BSILVER.p[1] ^= abb_mask[from].p[1];
    BB_BSILVER.p[2] ^= abb_mask[from].p[2];
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = first_one12( bb_check.p[1], bb_check.p[2] );
      bb_check.p[1] ^= abb_mask[to].p[1];
      bb_check.p[2] ^= abb_mask[to].p[2];

      if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
      bb_attacks = abb_b_silver_attacks[to];
      if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverBK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_BSILVER.p[1] ^= abb_mask[from].p[1];
      BB_BSILVER.p[2] ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(silver) );
    } while ( bb_check.p[1] | bb_check.p[2] );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
    BB_BSILVER.p[1] ^= abb_mask[from].p[1];
    BB_BSILVER.p[2] ^= abb_mask[from].p[2];
  }

  BBAnd( bb, BB_BKNIGHT, b_chk_tbl[SQ_WKING].knight );
  while ( BBTest(bb) ) {
    from = FirstOne( bb );
    Xor( from, bb );

    bb_check.p[0] = bb_move.p[0] & abb_b_knight_attacks[from].p[0]
      & abb_w_gold_attacks[SQ_WKING].p[0];

    if ( bb_check.p[0] ) {
      BB_BKNIGHT.p[0] ^= abb_mask[from].p[0];
      BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );

      do {
	to = last_one0( bb_check.p[0] );
	bb_check.p[0] ^= abb_mask[to].p[0];
      
	if ( ! is_white_attacked( ptree, to ) ) { continue; }
      
	bb_attacks = abb_b_gold_attacks[to];
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( IsDiscoverWK( from, to ) );
	else if ( can_w_piece_capture( ptree, to ) )       { continue; }
	if ( IsDiscoverBK( from, to ) )                    { continue; }
      
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
	BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
	BB_BKNIGHT.p[0] ^= abb_mask[from].p[0];
	BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
	return ( To2Move(to) | From2Move(from) | FLAG_PROMO
		 | Cap2Move(-BOARD[to]) | Piece2Move(knight) );
      } while ( bb_check.p[0] );

      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_BKNIGHT.p[0] ^= abb_mask[from].p[0];
      BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
    } else {

      BBAnd( bb_check, bb_move, abb_b_knight_attacks[from] );
      BBAnd( bb_check, bb_check, abb_w_knight_attacks[SQ_WKING] );
      
      if ( BBTest(bb_check) ) {
	BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
	BB_BKNIGHT.p[2] ^= abb_mask[from].p[2];
	BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
	BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	
	do {
	  to = FirstOne( bb_check );
	  Xor( to, bb_check );
      
	  BBIni( bb_attacks );
	  if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	  if ( IsDiscoverWK( from, to ) );
	  else if ( can_w_piece_capture( ptree, to ) )       { continue; }
	  if ( IsDiscoverBK( from, to ) )                    { continue; }
      
	  XorFile( from, OCCUPIED_FILE );
	  XorDiag2( from, OCCUPIED_DIAG2 );
	  XorDiag1( from, OCCUPIED_DIAG1 );
	  BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
	  BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
	  BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
	  BB_BKNIGHT.p[2] ^= abb_mask[from].p[2];
	  return ( To2Move(to) | From2Move(from)
		   | Cap2Move(-BOARD[to]) | Piece2Move(knight) );
	} while ( BBTest(bb_check) );

	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	BB_BOCCUPY.p[1] ^= abb_mask[from].p[1];
	BB_BOCCUPY.p[2] ^= abb_mask[from].p[2];
	BB_BKNIGHT.p[1] ^= abb_mask[from].p[1];
	BB_BKNIGHT.p[2] ^= abb_mask[from].p[2];
      }
    }
  }

  BBAnd( bb, BB_BLANCE, b_chk_tbl[SQ_WKING].lance );
  while ( BBTest(bb) ) {
    from = FirstOne( bb );
    Xor( from, bb );

    bb_attacks = AttackFile(from);
    BBAnd( bb_attacks, bb_attacks, abb_minus_rays[from] );
    BBAnd( bb_attacks, bb_attacks, bb_move );

    BBAnd( bb_check, bb_attacks, abb_mask[SQ_WKING+nfile] );
    bb_check_pro.p[0] = bb_attacks.p[0] & abb_w_gold_attacks[SQ_WKING].p[0];

    if ( ! ( bb_check_pro.p[0] | bb_check.p[0]
	     | bb_check.p[1] | bb_check.p[2] ) ) { continue; }

    Xor( from, BB_BLANCE );
    Xor( from, BB_BOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    bb_check.p[0] &= 0x1ffU;
    if ( BBTest(bb_check) ) {

      to = SQ_WKING+nfile;
      if ( ! is_white_attacked( ptree, to ) ) {
	bb_check.p[0] &= ~abb_mask[to].p[0];
	goto b_lance_next;
      }
      bb_temp = abb_file_attacks[to][0];
      if ( can_w_king_escape( ptree, to, &bb_temp ) ) { goto b_lance_next; }
      if ( IsDiscoverWK( from, to ) );
      else if ( can_w_piece_capture( ptree, to ) )    { goto b_lance_next; }
      if ( IsDiscoverBK( from, to ) )                 { goto b_lance_next; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_BOCCUPY );
      Xor( from, BB_BLANCE );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(lance) );
    }
    
  b_lance_next:
    while ( bb_check_pro.p[0] )
      {
	to = last_one0( bb_check_pro.p[0] );
	bb_check_pro.p[0] ^= abb_mask[to].p[0];
	
	if ( ! is_white_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_b_gold_attacks[to];
	if ( can_w_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( IsDiscoverWK( from, to ) );
	else if ( can_w_piece_capture( ptree, to ) )       { continue; }
	if ( IsDiscoverBK( from, to ) )                    { continue; }
	
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	Xor( from, BB_BOCCUPY );
	Xor( from, BB_BLANCE );
	return ( To2Move(to) | From2Move(from) | FLAG_PROMO
		 | Cap2Move(-BOARD[to]) | Piece2Move(lance) );
      }
    
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_BOCCUPY );
    Xor( from, BB_BLANCE );
  }

  bb_check.p[0] = bb_move.p[0] & BB_BPAWN_ATK.p[0]
    & abb_w_gold_attacks[SQ_WKING].p[0];
  while ( bb_check.p[0] ) {
    to   = last_one0( bb_check.p[0] );
    from = to + nfile;
    bb_check.p[0] ^= abb_mask[to].p[0];

    BB_BPAWN_ATK.p[0] ^= abb_mask[to].p[0];
    BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    BB_BOCCUPY.p[0]   ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    if ( ! is_white_attacked( ptree, to ) )          { goto b_pawn_pro_next; }
    bb_attacks = abb_b_gold_attacks[to];
    if ( can_w_king_escape( ptree,to,&bb_attacks ) ) { goto b_pawn_pro_next; }
    if ( IsDiscoverWK( from, to ) );
    else if ( can_w_piece_capture( ptree, to ) )     { goto b_pawn_pro_next; }
    if ( IsDiscoverBK( from, to ) )                  { goto b_pawn_pro_next; }
    
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[0]   ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
    BB_BPAWN_ATK.p[0] ^= abb_mask[to].p[0];
    BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	     | Cap2Move(-BOARD[to]) | Piece2Move(pawn) );

  b_pawn_pro_next:
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_BOCCUPY.p[0]   ^= abb_mask[from].p[0];
    BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
    BB_BPAWN_ATK.p[0] ^= abb_mask[to].p[0];
    BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
  }

  if ( SQ_WKING >= A7 && SQ_WKING <= I3 ) {
    to   = SQ_WKING + nfile;
    from = to        + nfile;
    if ( BOARD[from] == pawn && BOARD[to] <= 0 ) {

      BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
      BB_BPAWN_ATK.p[2] ^= abb_mask[to].p[2];
      BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2]   ^= abb_mask[from].p[2];
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      
      if ( ! is_white_attacked( ptree, to ) )          { goto b_pawn_end; }
      BBIni( bb_attacks );
      if ( can_w_king_escape( ptree,to,&bb_attacks ) ) { goto b_pawn_end; }
      if ( can_w_piece_capture( ptree, to ) )          { goto b_pawn_end; }
      if ( IsDiscoverBK( from, to ) )                  { goto b_pawn_end; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2]   ^= abb_mask[from].p[2];
      BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
      BB_BPAWN_ATK.p[2] ^= abb_mask[to].p[2];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(-BOARD[to]) | Piece2Move(pawn) );
      
    b_pawn_end:
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_BOCCUPY.p[1]   ^= abb_mask[from].p[1];
      BB_BOCCUPY.p[2]   ^= abb_mask[from].p[2];
      BB_BPAWN_ATK.p[1] ^= abb_mask[to].p[1];
      BB_BPAWN_ATK.p[2] ^= abb_mask[to].p[2];
    }
  }

  return 0;
}


unsigned int CONV
is_w_mate_in_1ply( tree_t * restrict ptree )
{
  bitboard_t bb, bb_temp, bb_check, bb_check_pro, bb_attacks, bb_drop, bb_move;
  unsigned int ubb;
  int to, from, idirec;

  assert( ! is_white_attacked( ptree, SQ_WKING ) );

  /* Drops */
  BBOr( bb_drop, BB_BOCCUPY, BB_WOCCUPY );
  BBNot( bb_drop, bb_drop );

  if ( IsHandRook(HAND_W) ) {

    BBAnd( bb, abb_w_gold_attacks[SQ_BKING],
	   abb_b_gold_attacks[SQ_BKING] );
    BBAnd( bb, bb, bb_drop );
    while( BBTest(bb) )
      {
	to = LastOne( bb );
	Xor( to, bb );

	if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
	BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_b_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(rook);
      }

  } else if ( IsHandLance(HAND_W) && SQ_BKING >= A8 ) {

    to = SQ_BKING-nfile;
    if ( ( ! BOARD[to] ) && is_black_attacked( ptree, to ) )
      {
	bb_attacks = abb_file_attacks[to][0];
	if ( ( ! can_b_king_escape( ptree, to, &bb_attacks ) )
	     && ( ! can_b_piece_capture( ptree, to ) ) )
	  {
	    return To2Move(to) | Drop2Move(lance);
	  }
      }
  }

  if ( IsHandBishop(HAND_W) ) {

    BBAnd( bb, abb_w_silver_attacks[SQ_BKING],
	   abb_b_silver_attacks[SQ_BKING] );
    BBAnd( bb, bb, bb_drop );
    while( BBTest(bb) )
      {
	to = LastOne( bb );
	Xor( to, bb );

	if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
	BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	      abb_bishop_attacks_rl45[to][0] );
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_b_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(bishop);
      }
  }

  if ( IsHandGold(HAND_W) ) {
    
    if ( IsHandRook(HAND_W) )
      {
	BBAnd( bb, abb_w_gold_attacks[SQ_BKING],
	       abb_w_silver_attacks[SQ_BKING] );
	BBNotAnd( bb, bb_drop, bb );
	BBAnd( bb, bb, abb_b_gold_attacks[SQ_BKING] );
      }
    else { BBAnd( bb, bb_drop, abb_b_gold_attacks[SQ_BKING] ); }

    while ( BBTest(bb) )
      {
	to = LastOne( bb );
	Xor( to, bb );
	
	if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_w_gold_attacks[to];
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_b_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(gold);
      }
  }
  
  if ( IsHandSilver(HAND_W) ) {
    
    if ( IsHandGold(HAND_W) )
      {
	if ( IsHandBishop(HAND_W) ) { goto w_silver_drop_end; }
	BBNotAnd( bb,
		  abb_b_silver_attacks[SQ_BKING],
		  abb_b_gold_attacks[SQ_BKING] );
	BBAnd( bb, bb, bb_drop );
      }
    else {
      BBAnd( bb, bb_drop, abb_b_silver_attacks[SQ_BKING] );
      if ( IsHandBishop(HAND_W) )
	{
	  BBAnd( bb, bb, abb_b_gold_attacks[SQ_BKING] );
	}
    }
    
    while ( BBTest(bb) )
      {
	to = LastOne( bb );
	Xor( to, bb );
	
	if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_w_silver_attacks[to];
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_b_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(silver);
      }
  }
 w_silver_drop_end:
  
  if ( IsHandKnight(HAND_W) ) {
    
    BBAnd( bb, bb_drop, abb_b_knight_attacks[SQ_BKING] );
    while ( BBTest(bb) )
      {
	to = LastOne( bb );
	Xor( to, bb );
	
	BBIni( bb_attacks );
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( can_b_piece_capture( ptree, to ) )            { continue; }
	return To2Move(to) | Drop2Move(knight);
      }
  }

  /* Moves */
  BBNot( bb_move, BB_WOCCUPY );

  bb = BB_WDRAGON;
  while ( BBTest(bb) ) {
    from = LastOne( bb );
    Xor( from, bb );

    AttackDragon( bb_attacks, from );
    BBAnd( bb_check, bb_move,  bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_BKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_W_HDK );
    Xor( from, BB_W_RD );
    Xor( from, BB_WOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      if ( (int)adirec[SQ_BKING][to] & flag_cross )
	{
	  BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	  BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
	}
      else { AttackDragon( bb_attacks, to ); }

      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_WOCCUPY );
      Xor( from, BB_W_RD );
      Xor( from, BB_W_HDK );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(dragon) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_WOCCUPY );
    Xor( from, BB_W_RD );
    Xor( from, BB_W_HDK );
  }

  bb.p[2] = BB_WROOK.p[2];
  while ( bb.p[2] ) {
    from = first_one2( bb.p[2] );
    bb.p[2] ^= abb_mask[from].p[2];

    AttackRook( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_BKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    BB_W_RD.p[2]    ^= abb_mask[from].p[2];
    BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      if ( (int)adirec[SQ_BKING][to] & flag_cross )
	{
	  BBOr( bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0] );
	  BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
	}
      else { AttackDragon( bb_attacks, to ); }

      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_W_RD.p[2]    ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(BOARD[to]) | Piece2Move(rook) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
    BB_W_RD.p[2]    ^= abb_mask[from].p[2];
  }

  bb.p[0] = BB_WROOK.p[0];
  bb.p[1] = BB_WROOK.p[1];
  while ( bb.p[0] | bb.p[1] ) {
    from = last_one01( bb.p[0], bb.p[1] );
    bb.p[0] ^= abb_mask[from].p[0];
    bb.p[1] ^= abb_mask[from].p[1];

    AttackRook( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    bb_check.p[0] &= abb_b_gold_attacks[SQ_BKING].p[0];
    bb_check.p[1] &= abb_b_gold_attacks[SQ_BKING].p[1];
    bb_check.p[0] &= abb_w_gold_attacks[SQ_BKING].p[0];
    bb_check.p[1] &= abb_w_gold_attacks[SQ_BKING].p[1];
    bb_check.p[2] &= abb_king_attacks[SQ_BKING].p[2];
    if ( ! BBTest(bb_check) ) { continue; }

    BB_W_RD.p[0]    ^= abb_mask[from].p[0];
    BB_W_RD.p[1]    ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      if ( to >= A3 ) {
	if ( (int)adirec[SQ_BKING][to] & flag_cross )
	  {
	    BBOr(bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0]);
	    bb_attacks.p[1] |= abb_king_attacks[to].p[1];
	    bb_attacks.p[2] |= abb_king_attacks[to].p[2];
	  }
	else { AttackDragon( bb_attacks, to ); }
      } else {
	BBOr(bb_attacks, abb_file_attacks[to][0], abb_rank_attacks[to][0]);
      }
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_W_RD.p[0]    ^= abb_mask[from].p[0];
      BB_W_RD.p[1]    ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from)
	       | ( (to > I4) ? FLAG_PROMO : 0 )
	       | Cap2Move(BOARD[to]) | Piece2Move(rook) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_W_RD.p[0]    ^= abb_mask[from].p[0];
    BB_W_RD.p[1]    ^= abb_mask[from].p[1];
  }

  bb = BB_WHORSE;
  while ( BBTest(bb) ) {
    from = LastOne( bb );
    Xor( from, bb );

    AttackHorse( bb_attacks, from );
    BBAnd( bb_check, bb_move,  bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_BKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_W_HDK );
    Xor( from, BB_W_BH );
    Xor( from, BB_WOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_WOCCUPY );
      Xor( from, BB_W_BH );
      Xor( from, BB_W_HDK );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(horse) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_WOCCUPY );
    Xor( from, BB_W_BH );
    Xor( from, BB_W_HDK );
  }

  bb.p[2] = BB_WBISHOP.p[2];
  while ( bb.p[2] ) {
    from = first_one2( bb.p[2] );
    bb.p[2] ^= abb_mask[from].p[2];

    AttackBishop( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    BBAnd( bb_check, bb_check, abb_king_attacks[SQ_BKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    BB_W_BH.p[2]    ^= abb_mask[from].p[2];
    BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      BBOr( bb_attacks, bb_attacks, abb_king_attacks[to] );
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_W_BH.p[2]    ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(BOARD[to]) | Piece2Move(bishop) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
    BB_W_BH.p[2]    ^= abb_mask[from].p[2];
  }

  bb.p[0] = BB_WBISHOP.p[0];
  bb.p[1] = BB_WBISHOP.p[1];
  while ( bb.p[0] | bb.p[1] ) {
    from = last_one01( bb.p[0], bb.p[1] );
    bb.p[0] ^= abb_mask[from].p[0];
    bb.p[1] ^= abb_mask[from].p[1];

    AttackBishop( bb_attacks, from );
    BBAnd( bb_check, bb_move, bb_attacks );
    bb_check.p[0] &= abb_b_silver_attacks[SQ_BKING].p[0];
    bb_check.p[1] &= abb_b_silver_attacks[SQ_BKING].p[1];
    bb_check.p[0] &= abb_w_silver_attacks[SQ_BKING].p[0];
    bb_check.p[1] &= abb_w_silver_attacks[SQ_BKING].p[1];
    bb_check.p[2] &= abb_king_attacks[SQ_BKING].p[2];
    if ( ! BBTest(bb_check) ) { continue; }

    BB_W_BH.p[0]    ^= abb_mask[from].p[0];
    BB_W_BH.p[1]    ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      BBOr( bb_attacks, abb_bishop_attacks_rr45[to][0],
	    abb_bishop_attacks_rl45[to][0] );
      if ( to >= A3 ) {
	bb_attacks.p[1] |= abb_king_attacks[to].p[1];
	bb_attacks.p[2] |= abb_king_attacks[to].p[2];
      }
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_W_BH.p[0]    ^= abb_mask[from].p[0];
      BB_W_BH.p[1]    ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from)
	       | ( (to > I4) ? FLAG_PROMO : 0 )
	       | Cap2Move(BOARD[to]) | Piece2Move(bishop) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_W_BH.p[0]    ^= abb_mask[from].p[0];
    BB_W_BH.p[1]    ^= abb_mask[from].p[1];
  }

  BBAnd( bb, BB_WTGOLD, w_chk_tbl[SQ_BKING].gold );
  while ( BBTest(bb) ) {
    from = LastOne( bb );
    Xor( from, bb );

    BBAnd( bb_check, bb_move, abb_w_gold_attacks[from] );
    BBAnd( bb_check, bb_check, abb_b_gold_attacks[SQ_BKING] );
    if ( ! BBTest(bb_check) ) { continue; }

    Xor( from, BB_WTGOLD );
    Xor( from, BB_WOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = LastOne( bb_check );
      Xor( to, bb_check );

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      bb_attacks = abb_w_gold_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_WOCCUPY );
      Xor( from, BB_WTGOLD );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(-BOARD[from]) );
    } while ( BBTest(bb_check) );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_WOCCUPY );
    Xor( from, BB_WTGOLD );
  }

  BBAnd( bb, BB_WSILVER, w_chk_tbl[SQ_BKING].silver );
  while ( bb.p[2] ) {
    from = first_one2( bb.p[2] );
    bb.p[2] ^= abb_mask[from].p[2];

    bb_check_pro.p[1] = bb_move.p[1] & abb_w_silver_attacks[from].p[1]
      & abb_b_gold_attacks[SQ_BKING].p[1];
    bb_check_pro.p[2] = bb_move.p[2] & abb_w_silver_attacks[from].p[2]
      & abb_b_gold_attacks[SQ_BKING].p[2];

    bb_check.p[1] = bb_move.p[1] & abb_w_silver_attacks[from].p[1]
      & abb_b_silver_attacks[SQ_BKING].p[1]
      & ~abb_b_gold_attacks[SQ_BKING].p[1];
    bb_check.p[2] = bb_move.p[2] & abb_w_silver_attacks[from].p[2]
      & abb_b_silver_attacks[SQ_BKING].p[2]
      & ~abb_b_gold_attacks[SQ_BKING].p[2];

    if ( ! ( bb_check_pro.p[1] | bb_check_pro.p[2]
	     | bb_check.p[1]| bb_check.p[2] ) ) { continue; }

    BB_WSILVER.p[2] ^= abb_mask[from].p[2];
    BB_WOCCUPY.p[2]  ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    while ( bb_check_pro.p[1] | bb_check_pro.p[2] ) {
      to = last_one12( bb_check_pro.p[1], bb_check_pro.p[2] );
      bb_check_pro.p[1] ^= abb_mask[to].p[1];
      bb_check_pro.p[2] ^= abb_mask[to].p[2];
      
      if ( ! is_black_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_w_gold_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[2]  ^= abb_mask[from].p[2];
      BB_WSILVER.p[2] ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(BOARD[to]) | Piece2Move(silver) );
    }

    while ( bb_check.p[1] | bb_check.p[2] ) {
      to = last_one12( bb_check.p[1], bb_check.p[2] );
      bb_check.p[1] ^= abb_mask[to].p[1];
      bb_check.p[2] ^= abb_mask[to].p[2];
      
      if ( ! is_black_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_w_silver_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[2]  ^= abb_mask[from].p[2];
      BB_WSILVER.p[2] ^= abb_mask[from].p[2];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(silver) );
    }

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[2]  ^= abb_mask[from].p[2];
    BB_WSILVER.p[2] ^= abb_mask[from].p[2];
  }

  ubb = bb.p[1] & 0x1ffU;
  while ( ubb ) {
    from = first_one1( ubb );
    ubb ^= abb_mask[from].p[1];

    bb_check_pro.p[2] = bb_move.p[2] & abb_w_silver_attacks[from].p[2]
      & abb_b_gold_attacks[SQ_BKING].p[2];

    bb_check.p[2] = bb_move.p[2] & abb_w_silver_attacks[from].p[2]
      & abb_b_silver_attacks[SQ_BKING].p[2]
      & ~abb_b_gold_attacks[SQ_BKING].p[2];
    bb_check.p[1] = bb_move.p[1] & abb_w_silver_attacks[from].p[1]
      & abb_b_silver_attacks[SQ_BKING].p[1];

    if ( ! (bb_check_pro.p[2]|bb_check.p[2]|bb_check.p[1]) ) { continue; }

    BB_WSILVER.p[1] ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[1]  ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    while ( bb_check_pro.p[2] ) {
      to = first_one2( bb_check_pro.p[2] );
      bb_check_pro.p[2] ^= abb_mask[to].p[2];
      
      if ( ! is_black_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_w_gold_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_WSILVER.p[1] ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	       | Cap2Move(BOARD[to]) | Piece2Move(silver) );
    }

    while ( bb_check.p[1] | bb_check.p[2] ) {
      to = last_one12( bb_check.p[1], bb_check.p[2] );
      bb_check.p[1] ^= abb_mask[to].p[1];
      bb_check.p[2] ^= abb_mask[to].p[2];
      
      if ( ! is_black_attacked( ptree, to ) ) { continue; }
      
      bb_attacks = abb_w_silver_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_WSILVER.p[1] ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(silver) );
    }

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_WSILVER.p[1] ^= abb_mask[from].p[1];
  }

  bb.p[0] = bb.p[0];
  bb.p[1] = bb.p[1] & 0x7fffe00U;
  while ( bb.p[0] | bb.p[1] ) {
    from = last_one01( bb.p[0], bb.p[1] );
    bb.p[0] ^= abb_mask[from].p[0];
    bb.p[1] ^= abb_mask[from].p[1];

    bb_check.p[0] = bb_move.p[0] & abb_w_silver_attacks[from].p[0]
      & abb_b_silver_attacks[SQ_BKING].p[0];
    bb_check.p[1] = bb_move.p[1] & abb_w_silver_attacks[from].p[1]
      & abb_b_silver_attacks[SQ_BKING].p[1];
    if ( ! ( bb_check.p[0] | bb_check.p[1] ) ) { continue; }

    BB_WSILVER.p[0] ^= abb_mask[from].p[0];
    BB_WSILVER.p[1] ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    do {
      to = last_one01( bb_check.p[0], bb_check.p[1] );
      bb_check.p[0] ^= abb_mask[to].p[0];
      bb_check.p[1] ^= abb_mask[to].p[1];

      if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
      bb_attacks = abb_w_silver_attacks[to];
      if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )       { continue; }
      if ( IsDiscoverWK( from, to ) )                    { continue; }
	
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_WSILVER.p[0] ^= abb_mask[from].p[0];
      BB_WSILVER.p[1] ^= abb_mask[from].p[1];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(silver) );
    } while ( bb_check.p[0] | bb_check.p[1] );

    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
    BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
    BB_WSILVER.p[0] ^= abb_mask[from].p[0];
    BB_WSILVER.p[1] ^= abb_mask[from].p[1];
  }

  BBAnd( bb, BB_WKNIGHT, w_chk_tbl[SQ_BKING].knight );
  while ( BBTest(bb) ) {
    from = LastOne( bb );
    Xor( from, bb );

    bb_check.p[2] = bb_move.p[2] & abb_w_knight_attacks[from].p[2]
      & abb_b_gold_attacks[SQ_BKING].p[2];

    if ( bb_check.p[2] ) {
      BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
      BB_WKNIGHT.p[2] ^= abb_mask[from].p[2];
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );

      do {
	to = first_one2( bb_check.p[2] );
	bb_check.p[2] ^= abb_mask[to].p[2];
      
	if ( ! is_black_attacked( ptree, to ) ) { continue; }
      
	bb_attacks = abb_w_gold_attacks[to];
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( IsDiscoverBK( from, to ) );
	else if ( can_b_piece_capture( ptree, to ) )       { continue; }
	if ( IsDiscoverWK( from, to ) )                    { continue; }
      
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
	BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
	BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
	BB_WKNIGHT.p[2] ^= abb_mask[from].p[2];
	return ( To2Move(to) | From2Move(from) | FLAG_PROMO
		 | Cap2Move(BOARD[to]) | Piece2Move(knight) );
      } while ( bb_check.p[2] );

      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
      BB_WOCCUPY.p[2] ^= abb_mask[from].p[2];
      BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
      BB_WKNIGHT.p[2] ^= abb_mask[from].p[2];
    } else {

      BBAnd( bb_check, bb_move, abb_w_knight_attacks[from] );
      BBAnd( bb_check, bb_check, abb_b_knight_attacks[SQ_BKING] );
      
      if ( BBTest(bb_check) ) {
	BB_WKNIGHT.p[0] ^= abb_mask[from].p[0];
	BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
	BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
	BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	
	do {
	  to = LastOne( bb_check );
	  Xor( to, bb_check );
      
	  BBIni( bb_attacks );
	  if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	  if ( IsDiscoverBK( from, to ) );
	  else if ( can_b_piece_capture( ptree, to ) )       { continue; }
	  if ( IsDiscoverWK( from, to ) )                    { continue; }
      
	  XorFile( from, OCCUPIED_FILE );
	  XorDiag2( from, OCCUPIED_DIAG2 );
	  XorDiag1( from, OCCUPIED_DIAG1 );
	  BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
	  BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
	  BB_WKNIGHT.p[0] ^= abb_mask[from].p[0];
	  BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
	  return ( To2Move(to) | From2Move(from)
		   | Cap2Move(BOARD[to]) | Piece2Move(knight) );
	} while ( BBTest(bb_check) );

	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	BB_WOCCUPY.p[0] ^= abb_mask[from].p[0];
	BB_WOCCUPY.p[1] ^= abb_mask[from].p[1];
	BB_WKNIGHT.p[0] ^= abb_mask[from].p[0];
	BB_WKNIGHT.p[1] ^= abb_mask[from].p[1];
      }
    }
  }

  BBAnd( bb, BB_WLANCE, w_chk_tbl[SQ_BKING].lance );
  while ( BBTest(bb) ) {
    from = LastOne( bb );
    Xor( from, bb );

    bb_attacks = AttackFile(from);
    BBAnd( bb_attacks, bb_attacks, abb_plus_rays[from] );
    BBAnd( bb_attacks, bb_attacks, bb_move );

    BBAnd( bb_check, bb_attacks, abb_mask[SQ_BKING-nfile] );
    bb_check_pro.p[2] = bb_attacks.p[2] & abb_b_gold_attacks[SQ_BKING].p[2];

    if ( ! ( bb_check_pro.p[2] | bb_check.p[0]
	     | bb_check.p[1] | bb_check.p[2] ) ) { continue; }

    Xor( from, BB_WLANCE );
    Xor( from, BB_WOCCUPY );
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    bb_check.p[2] &= 0x7fc0000U;
    if ( BBTest(bb_check) ) {

      to = SQ_BKING-nfile;
      if ( ! is_black_attacked( ptree, to ) ) {
	bb_check.p[2] &= ~abb_mask[to].p[2];
	goto w_lance_next;
      }
      bb_temp = abb_file_attacks[to][0];
      if ( can_b_king_escape( ptree, to, &bb_temp ) ) { goto w_lance_next; }
      if ( IsDiscoverBK( from, to ) );
      else if ( can_b_piece_capture( ptree, to ) )    { goto w_lance_next; }
      if ( IsDiscoverWK( from, to ) )                 { goto w_lance_next; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      Xor( from, BB_WOCCUPY );
      Xor( from, BB_WLANCE );
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(lance) );
    }
    
  w_lance_next:
    while ( bb_check_pro.p[2] )
      {
	to = first_one2( bb_check_pro.p[2] );
	bb_check_pro.p[2] ^= abb_mask[to].p[2];
	
	if ( ! is_black_attacked( ptree, to ) ) { continue; }
	
	bb_attacks = abb_w_gold_attacks[to];
	if ( can_b_king_escape( ptree, to, &bb_attacks ) ) { continue; }
	if ( IsDiscoverBK( from, to ) );
	else if ( can_b_piece_capture( ptree, to ) )       { continue; }
	if ( IsDiscoverWK( from, to ) )                    { continue; }
	
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
	Xor( from, BB_WOCCUPY );
	Xor( from, BB_WLANCE );
	return ( To2Move(to) | From2Move(from) | FLAG_PROMO
		 | Cap2Move(BOARD[to]) | Piece2Move(lance) );
      }
    
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    Xor( from, BB_WOCCUPY );
    Xor( from, BB_WLANCE );
  }

  bb_check.p[2] = bb_move.p[2] & BB_WPAWN_ATK.p[2]
    & abb_b_gold_attacks[SQ_BKING].p[2];
  while ( bb_check.p[2] ) {
    to   = first_one2( bb_check.p[2] );
    from = to - nfile;
    bb_check.p[2] ^= abb_mask[to].p[2];

    BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    BB_WPAWN_ATK.p[2] ^= abb_mask[to].p[2];
    BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[2]   ^= abb_mask[from].p[2];
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );

    if ( ! is_black_attacked( ptree, to ) )          { goto w_pawn_pro_next; }
    bb_attacks = abb_w_gold_attacks[to];
    if ( can_b_king_escape( ptree,to,&bb_attacks ) ) { goto w_pawn_pro_next; }
    if ( IsDiscoverBK( from, to ) );
    else if ( can_b_piece_capture( ptree, to ) )     { goto w_pawn_pro_next; }
    if ( IsDiscoverWK( from, to ) )                  { goto w_pawn_pro_next; }
    
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[2]   ^= abb_mask[from].p[2];
    BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    BB_WPAWN_ATK.p[2] ^= abb_mask[to].p[2];
    return ( To2Move(to) | From2Move(from) | FLAG_PROMO
	     | Cap2Move(BOARD[to]) | Piece2Move(pawn) );

  w_pawn_pro_next:
    XorFile( from, OCCUPIED_FILE );
    XorDiag2( from, OCCUPIED_DIAG2 );
    XorDiag1( from, OCCUPIED_DIAG1 );
    BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
    BB_WOCCUPY.p[2]   ^= abb_mask[from].p[2];
    BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    BB_WPAWN_ATK.p[2] ^= abb_mask[to].p[2];
  }

  if ( SQ_BKING <= I3 && SQ_BKING >= A7 ) {
    to   = SQ_BKING - nfile;
    from = to        - nfile;
    if ( BOARD[from] == -pawn && BOARD[to] >= 0 ) {

      BB_WPAWN_ATK.p[0] ^= abb_mask[to].p[0];
      BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
      BB_WOCCUPY.p[0]   ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      
      if ( ! is_black_attacked( ptree, to ) )          { goto w_pawn_end; }
      BBIni( bb_attacks );
      if ( can_b_king_escape( ptree,to,&bb_attacks ) ) { goto w_pawn_end; }
      if ( can_b_piece_capture( ptree, to ) )          { goto w_pawn_end; }
      if ( IsDiscoverWK( from, to ) )                  { goto w_pawn_end; }
      
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[0]   ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
      BB_WPAWN_ATK.p[0] ^= abb_mask[to].p[0];
      BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
      return ( To2Move(to) | From2Move(from)
	       | Cap2Move(BOARD[to]) | Piece2Move(pawn) );
      
    w_pawn_end:
      XorFile( from, OCCUPIED_FILE );
      XorDiag2( from, OCCUPIED_DIAG2 );
      XorDiag1( from, OCCUPIED_DIAG1 );
      BB_WOCCUPY.p[0]   ^= abb_mask[from].p[0];
      BB_WOCCUPY.p[1]   ^= abb_mask[from].p[1];
      BB_WPAWN_ATK.p[0] ^= abb_mask[to].p[0];
      BB_WPAWN_ATK.p[1] ^= abb_mask[to].p[1];
    }
  }

  return 0;
}


static int CONV
can_w_piece_capture( const tree_t * restrict ptree, int to )
{
  bitboard_t bb_sum, bb, bb_attacks;
  int idirec, from;
  
  from = to-nfile;
  if ( to >= A8 && BOARD[from] == -pawn )
    {
      if ( IsDiscoverWK(from,to) );
      else { return 1; }
    }
  
  BBAnd( bb_sum, BB_WKNIGHT, abb_b_knight_attacks[to] );
  
  BBAndOr( bb_sum, BB_WSILVER, abb_b_silver_attacks[to] );
  BBAndOr( bb_sum, BB_WTGOLD, abb_b_gold_attacks[to] );

  BBOr( bb, BB_WHORSE, BB_WDRAGON );
  BBAndOr( bb_sum, bb, abb_king_attacks[to] );
  
  AttackBishop( bb, to );
  BBAndOr( bb_sum, BB_W_BH, bb );

  BBAndOr( bb_sum, BB_W_RD, AttackRank(to) );
  bb = BB_W_RD;
  BBAndOr( bb, BB_WLANCE, abb_minus_rays[to] );
  bb_attacks = AttackFile( to );
  BBAndOr( bb_sum, bb, bb_attacks );

  while ( BBTest( bb_sum ) )
    {
      from  = FirstOne( bb_sum );
      Xor( from, bb_sum );

      if ( IsDiscoverWK(from,to) ) { continue; }
      return 1;
    }

  return 0;
}


static int CONV
can_b_piece_capture( const tree_t * restrict ptree, int to )
{
  bitboard_t bb_sum, bb;
  int idirec, from;

  from = to+nfile;
  if ( to <= I2 && BOARD[from] == pawn )
    {
      if ( IsDiscoverBK(from,to) );
      else { return 1; }
    }

  BBAnd( bb_sum, BB_BKNIGHT, abb_w_knight_attacks[to] );

  BBAndOr( bb_sum, BB_BSILVER, abb_w_silver_attacks[to] );
  BBAndOr( bb_sum, BB_BTGOLD, abb_w_gold_attacks[to] );

  BBOr( bb, BB_BHORSE, BB_BDRAGON );
  BBAndOr( bb_sum, bb, abb_king_attacks[to] );
  
  AttackBishop( bb, to );
  BBAndOr( bb_sum, bb, BB_B_BH );
  BBAndOr( bb_sum, BB_B_RD, AttackRank(to) );

  bb = BB_B_RD;
  BBAndOr( bb, BB_BLANCE, abb_plus_rays[to] );
  BBAndOr( bb_sum, bb, AttackFile( to ) );

  while ( BBTest( bb_sum ) )
    {
      from  = LastOne( bb_sum );
      Xor( from, bb_sum );

      if ( IsDiscoverBK( from, to ) ) { continue; }
      return 1;
    }

  return 0;
}


static int CONV
can_w_king_escape( tree_t * restrict ptree, int to,
		   const bitboard_t * restrict pbb )
{
  bitboard_t bb = *pbb;
  int iret = 0, iescape;

  if ( !BOARD[to] )
    {
      Xor( to, BB_BOCCUPY );
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
    }
  Xor( SQ_WKING, BB_WOCCUPY );
  XorFile( SQ_WKING, OCCUPIED_FILE );
  XorDiag2( SQ_WKING, OCCUPIED_DIAG2 );
  XorDiag1( SQ_WKING, OCCUPIED_DIAG1 );

  BBOr( bb, bb, abb_mask[to] );
  BBOr( bb, bb, BB_WOCCUPY );
  BBNotAnd( bb, abb_king_attacks[SQ_WKING], bb );
  
  while( BBTest(bb) )
    {
      iescape = FirstOne( bb );
      if ( ! is_white_attacked( ptree, iescape ) )
	{
	  iret = 1;
	  break;
	}
      Xor( iescape, bb );
    }

  XorFile( SQ_WKING, OCCUPIED_FILE );
  XorDiag2( SQ_WKING, OCCUPIED_DIAG2 );
  XorDiag1( SQ_WKING, OCCUPIED_DIAG1 );
  Xor( SQ_WKING, BB_WOCCUPY );
  if ( !BOARD[to] )
    {
      Xor( to, BB_BOCCUPY );
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
    }

  return iret;
}


static int CONV
can_b_king_escape( tree_t * restrict ptree, int to,
		   const bitboard_t * restrict pbb )
{
  bitboard_t bb = *pbb;
  int iret = 0, iescape;

  if ( !BOARD[to] )
    {
      Xor( to, BB_WOCCUPY );
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
    }

  Xor( SQ_BKING, BB_BOCCUPY );
  XorFile( SQ_BKING, OCCUPIED_FILE );
  XorDiag2( SQ_BKING, OCCUPIED_DIAG2 );
  XorDiag1( SQ_BKING, OCCUPIED_DIAG1 );

  BBOr( bb, bb, abb_mask[to] );
  BBOr( bb, bb, BB_BOCCUPY );
  BBNotAnd( bb, abb_king_attacks[SQ_BKING], bb );
  
  while( BBTest(bb) )
    {
      iescape = LastOne( bb );
      if ( ! is_black_attacked( ptree, iescape ) )
	{
	  iret = 1;
	  break;
	}
      Xor( iescape, bb );
    }

  XorFile( SQ_BKING, OCCUPIED_FILE );
  XorDiag2( SQ_BKING, OCCUPIED_DIAG2 );
  XorDiag1( SQ_BKING, OCCUPIED_DIAG1 );
  Xor( SQ_BKING, BB_BOCCUPY );

  if ( !BOARD[to] )
    {
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
      Xor( to, BB_WOCCUPY );
    }

  return iret;
}
