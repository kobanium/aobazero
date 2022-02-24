// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "shogi.h"

enum { mate_king_cap_checker = 0,
       mate_cap_checker_gen,
       mate_cap_checker,
       mate_king_cap_gen,
       mate_king_cap,
       mate_king_move_gen,
       mate_king_move,
       mate_intercept_move_gen,
       mate_intercept_move,
       mate_intercept_weak_move,
       mate_intercept_drop_sup };

static int CONV mate3_and( tree_t * restrict ptree, int turn, int ply,
			   int flag );
static void CONV checker( const tree_t * restrict ptree, char *psq, int turn );
static unsigned int CONV gen_king_cap_checker( const tree_t * restrict ptree,
					       int to, int turn );
static int CONV mate_weak_or( tree_t * restrict ptree, int turn, int ply,
			      int from, int to );
static unsigned int * CONV gen_move_to( const tree_t * restrict ptree, int sq,
					int turn,
					unsigned int * restrict pmove );
static unsigned int * CONV gen_king_move( const tree_t * restrict ptree,
					  const char *psq, int turn,
					  int is_capture,
					  unsigned int * restrict pmove );
static unsigned int * CONV gen_intercept( tree_t * restrict __ptree__,
					  int sq_checker, int ply, int turn,
					  int * restrict premaining,
					  unsigned int * restrict pmove,
					  int flag );
static int CONV gen_next_evasion_mate( tree_t * restrict ptree,
				       const char *psq, int ply, int turn,
				       int flag );
/*
static uint64_t mate3_hash_tbl[ MATE3_MASK + 1 ] = {0};

static int CONV
mhash_probe( tree_t * restrict ptree, int turn, int ply )
{
  uint64_t key_current, key, word;
  unsigned int move;

  word  = mate3_hash_tbl[ (unsigned int)HASH_KEY & MATE3_MASK ];
#if ! defined(__x86_64__)
  word ^= word << 32;
#endif

  key          = word     & ~(uint64_t)0x7ffffU;
  key_current  = HASH_KEY & ~(uint64_t)0x7ffffU;
  key_current ^= (uint64_t)HAND_B << 42;
  key_current ^= (uint64_t)turn << 63;

  if ( key != key_current ) { return 0; }

  move = (unsigned int)word & 0x7ffffU;
  if ( move != MOVE_NA )
    {
      move |= turn ? Cap2Move( BOARD[I2To(move)])
                   : Cap2Move(-BOARD[I2To(move)]);
    }

  MOVE_CURR = move;

  return 1;
}


static void CONV
mhash_store( const tree_t * restrict ptree, int turn, unsigned int move )
{
  uint64_t word;

  word  = HASH_KEY & ~(uint64_t)0x7ffffU;
  word |= (uint64_t)( move & 0x7ffffU );
  word ^= (uint64_t)HAND_B << 42;
  word ^= (uint64_t)turn << 63;
  
#if ! defined(__x86_64__)
  word ^= word << 32;
#endif
  mate3_hash_tbl[ (unsigned int)HASH_KEY & MATE3_MASK ] = word;
}
*/

unsigned int CONV
is_mate_in3ply( tree_t * restrict ptree, int turn, int ply )
{
  int value, flag_skip;

/*
  if ( mhash_probe( ptree, turn, ply ) )
    {
      if ( MOVE_CURR == MOVE_NA ) { return 0; }
      else                        { return 1; }
    }
*/

  if ( ply >= PLY_MAX-2 ) { return 0; }

  flag_skip = 0;

#if defined(YSS_ZERO)
  ptree->anext_move[ply].move_last = ptree->move_last[0];
  ptree->move_last[ply] = GenCheck( turn, ptree->move_last[0] );
#else
  ptree->anext_move[ply].move_last = ptree->move_last[ply-1];	// only move_last[0] is set in AobaZero.
  ptree->move_last[ply] = GenCheck( turn, ptree->move_last[ply-1] );
#endif

  while ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
    {
      MOVE_CURR = *ptree->anext_move[ply].move_last++;

      if ( MOVE_CURR & MOVE_CHK_CLEAR ) { flag_skip = 0; }
      if ( flag_skip ) { continue; }

      assert( is_move_valid( ptree, MOVE_CURR, turn ) );
//    PRT("%2d:%s(%d):\n",ply,str_CSA_move(MOVE_CURR),MOVE_CURR);
      MakeMove( turn, MOVE_CURR, ply );
      if ( InCheck(turn) )
	{
	  UnMakeMove( turn, MOVE_CURR, ply );
	  continue;
	}

      value = mate3_and( ptree, Flip(turn), ply+1, 0 );
      
      UnMakeMove( turn, MOVE_CURR, ply );

      if ( value )
	{
//	  mhash_store( ptree, turn, MOVE_CURR );
	  return 1;
	}

      if ( ( MOVE_CURR & MOVE_CHK_SET )
	   && I2To(MOVE_CURR) != I2To(ptree->current_move[ply+1]) )
	{
	  flag_skip = 1;
	}
    }

//mhash_store( ptree, turn, MOVE_NA );
  return 0;
}


static int CONV
mate3_and( tree_t * restrict ptree, int turn, int ply, int flag )
{
  unsigned int move;
  char asq[2];

  assert( InCheck(turn) );

  ptree->anext_move[ply].next_phase = mate_king_cap_checker;
  checker( ptree, asq, turn );

  while ( gen_next_evasion_mate( ptree, asq, ply, turn, flag ) ) {

    if ( ptree->anext_move[ply].next_phase == mate_intercept_drop_sup )
      {
	return 0;
      }

    MakeMove( turn, MOVE_CURR, ply );
    assert( ! InCheck(turn) );

    if ( InCheck( Flip(turn) ) ) { move = 0; }
    else                         { move = IsMateIn1Ply( Flip(turn) ); }

    if ( ! move
	 && ptree->anext_move[ply].next_phase == mate_intercept_weak_move )
      {
	assert( asq[1] == nsquare );
	move = (unsigned int)mate_weak_or( ptree, Flip(turn), ply+1, asq[0],
					   I2To(MOVE_CURR) );
      }

    UnMakeMove( turn, MOVE_CURR, ply );
      
    if ( ! move ) { return 0; }
  }
  
  return 1;
}


static int CONV
mate_weak_or( tree_t * restrict ptree, int turn, int ply, int from,
	      int to )
{
  int idirec, pc, pc_cap, value, flag;

  if ( ply >= PLY_MAX-2 ) { return 0; }
  
  if ( turn )
    {
      if ( IsDiscoverWK( from, to ) ) { return 0; }

      pc     = -BOARD[from];
      pc_cap =  BOARD[to];
      MOVE_CURR = ( To2Move(to) | From2Move(from)
		      | Piece2Move(pc) | Cap2Move(pc_cap) );
      if ( ( pc == bishop || pc == rook )
	   && ( to > I4 || from > I4 ) ) { MOVE_CURR |= FLAG_PROMO; }
    }
  else {
    if ( IsDiscoverBK( from, to ) ) { return 0; }
    
    pc     =  BOARD[from];
    pc_cap = -BOARD[to];
    MOVE_CURR = ( To2Move(to) | From2Move(from) | Piece2Move(pc)
		  | Cap2Move(pc_cap) );
    if ( ( pc == bishop || pc == rook )
	 && ( to < A6 || from < A6 ) ) { MOVE_CURR |= FLAG_PROMO; }
  }

  MakeMove( turn, MOVE_CURR, ply );
  if ( I2From(MOVE_LAST) < nsquare )
    {
      if ( InCheck(turn) )
	{
	  UnMakeMove( turn, MOVE_CURR, ply );
	  return 0;
	}
      flag = 1;
    }
  else {
    assert( ! InCheck(turn) );
    flag = 2;
  }
  
  ptree->move_last[ply] = ptree->move_last[ply-1];
  value = mate3_and( ptree, Flip(turn), ply+1, flag );
  
  UnMakeMove( turn, MOVE_CURR, ply );

  return value;
}


static int CONV
gen_next_evasion_mate( tree_t * restrict ptree, const char *psq, int ply,
		       int turn, int flag )
{
  switch ( ptree->anext_move[ply].next_phase )
    {
    case mate_king_cap_checker:
      ptree->anext_move[ply].next_phase = mate_cap_checker_gen;
      MOVE_CURR = gen_king_cap_checker( ptree, psq[0], turn );
      if ( MOVE_CURR ) { return 1; }

    case mate_cap_checker_gen:
      ptree->anext_move[ply].next_phase = mate_cap_checker;
      ptree->anext_move[ply].move_last	= ptree->move_last[ply-1];
      ptree->move_last[ply]             = ptree->move_last[ply-1];
      if ( psq[1] == nsquare )
	{
	  ptree->move_last[ply]
	    = gen_move_to( ptree, psq[0], turn, ptree->move_last[ply-1] );
	}

    case mate_cap_checker:
      if ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
	{
	  MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	  return 1;
	}

    case mate_king_cap_gen:
      ptree->anext_move[ply].next_phase = mate_king_cap;
      ptree->anext_move[ply].move_last  = ptree->move_last[ply-1];
      ptree->move_last[ply]
	= gen_king_move( ptree, psq, turn, 1, ptree->move_last[ply-1] );

    case mate_king_cap:
      if ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
	{
	  MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	  return 1;
	}

    case mate_king_move_gen:
      ptree->anext_move[ply].next_phase = mate_king_move;
      ptree->anext_move[ply].move_last  = ptree->move_last[ply-1];
      ptree->move_last[ply]
	= gen_king_move( ptree, psq, turn, 0, ptree->move_last[ply-1] );

    case mate_king_move:
      if ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
	{
	  MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	  return 1;
	}

    case mate_intercept_move_gen:
      ptree->anext_move[ply].remaining  = 0;
      ptree->anext_move[ply].next_phase = mate_intercept_move;
      ptree->anext_move[ply].move_last  = ptree->move_last[ply-1];
      ptree->move_last[ply]             = ptree->move_last[ply-1];
      if ( psq[1] == nsquare && abs(BOARD[(int)psq[0]]) != knight  )
	{
	  int n;
	  ptree->move_last[ply] = gen_intercept( ptree, psq[0], ply, turn, &n,
						 ptree->move_last[ply-1],
						 flag );
	  if ( n < 0 )
	    {
	      ptree->anext_move[ply].next_phase = mate_intercept_drop_sup;
	      ptree->anext_move[ply].remaining  = 0;
	      MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	      return 1;
	    }

	  ptree->anext_move[ply].remaining = n;
	}

    case mate_intercept_move:
      if ( ptree->anext_move[ply].remaining-- )
	{
	  MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	  return 1;
	}
      ptree->anext_move[ply].next_phase = mate_intercept_weak_move;

    case mate_intercept_weak_move:
      if ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
	{
	  MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	  return 1;
	}
      break;

    default:
      assert( 0 );
    }

  return 0;
}


static void CONV
checker( const tree_t * restrict ptree, char *psq, int turn )
{
  bitboard_t bb;
  int n, sq0, sq1, sq_king;

  if ( turn )
    {
      sq_king = SQ_WKING;
      bb = b_attacks_to_piece( ptree, sq_king );
    }
  else {
    sq_king = SQ_BKING;
    bb = w_attacks_to_piece( ptree, sq_king );
  }


  assert( BBTest(bb) );
  sq0 = LastOne( bb );
  sq1 = nsquare;

  Xor( sq0, bb );
  if ( BBTest( bb ) )
    {
      sq1 = LastOne( bb );
      if ( BBContract( abb_king_attacks[sq_king], abb_mask[sq1] ) )
	{
	  n   = sq0;
	  sq0 = sq1;
	  sq1 = n;
	}
    }

  psq[0] = (char)sq0;
  psq[1] = (char)sq1;
}


static unsigned int CONV
gen_king_cap_checker( const tree_t * restrict ptree, int to, int turn )
{
  unsigned int move;
  int from;

  if ( turn )
    {
      from = SQ_WKING;
      if ( ! BBContract( abb_king_attacks[from],
			 abb_mask[to] ) )   { return 0;}
      if ( is_white_attacked( ptree, to ) ) { return 0; }
      move = Cap2Move(BOARD[to]);
    }
  else {
    from = SQ_BKING;
    if ( ! BBContract( abb_king_attacks[from],
		       abb_mask[to] ) )   { return 0;}
    if ( is_black_attacked( ptree, to ) ) { return 0; }
    move = Cap2Move(-BOARD[to]);
  }
  move |= To2Move(to) | From2Move(from) | Piece2Move(king);

  return move;
}


static unsigned int * CONV
gen_move_to( const tree_t * restrict ptree, int to, int turn,
	     unsigned int * restrict pmove )
{
  bitboard_t bb;
  int direc, from, pc, flag_promo, flag_unpromo;

  if ( turn )
    {
      bb = w_attacks_to_piece( ptree, to );
      BBNotAnd( bb, bb, abb_mask[SQ_WKING] );
      while ( BBTest(bb) )
	{
	  from = LastOne( bb );
	  Xor( from, bb );

	  direc = (int)adirec[SQ_WKING][from];
	  if ( direc && is_pinned_on_white_king( ptree, from, direc ) )
	    {
	      continue;
	    }

	  flag_promo   = 0;
	  flag_unpromo = 1;
	  pc           = -BOARD[from];
	  switch ( pc )
	    {
	    case pawn:
	      if ( to > I4 ) { flag_promo = 1;  flag_unpromo = 0; }
	      break;

	    case lance:	 case knight:
	      if      ( to > I3 ) { flag_promo = 1;  flag_unpromo = 0; }
	      else if ( to > I4 ) { flag_promo = 1; }
	      break;

	    case silver:
	      if ( to > I4 || from > I4 ) { flag_promo = 1; }
	      break;

	    case bishop:  case rook:
	      if ( to > I4
		   || from > I4 ) { flag_promo = 1;  flag_unpromo = 0; }
	      break;

	    default:
	      break;
	    }
	  assert( flag_promo || flag_unpromo );
	  if ( flag_promo )
	    {
	      *pmove++ = ( From2Move(from) | To2Move(to) | FLAG_PROMO
			   | Piece2Move(pc) | Cap2Move(BOARD[to]) );
	    }
	  if ( flag_unpromo )
	    {
	      *pmove++ = ( From2Move(from) | To2Move(to)
			   | Piece2Move(pc) | Cap2Move(BOARD[to]) );
	    }
	}
    }
  else {
    bb = b_attacks_to_piece( ptree, to );
    BBNotAnd( bb, bb, abb_mask[SQ_BKING] );
    while ( BBTest(bb) )
      {
	from = FirstOne( bb );
	Xor( from, bb );
	
	direc = (int)adirec[SQ_BKING][from];
	if ( direc && is_pinned_on_black_king( ptree, from, direc ) )
	  {
	    continue;
	  }

	flag_promo   = 0;
	flag_unpromo = 1;
	pc           = BOARD[from];
	switch ( pc )
	  {
	  case pawn:
	    if ( to < A6 ) { flag_promo = 1;  flag_unpromo = 0; }
	    break;
	    
	  case lance:  case knight:
	    if      ( to < A7 ) { flag_promo = 1;  flag_unpromo = 0; }
	    else if ( to < A6 ) { flag_promo = 1; }
	    break;
	    
	  case silver:
	    if ( to < A6 || from < A6 ) { flag_promo = 1; }
	    break;
	    
	  case bishop:  case rook:
	    if ( to < A6
		 || from < A6 ) { flag_promo = 1;  flag_unpromo = 0; }
	    break;
	    
	  default:
	    break;
	  }
	assert( flag_promo || flag_unpromo );
	if ( flag_promo )
	  {
	    *pmove++ = ( From2Move(from) | To2Move(to) | FLAG_PROMO
			 | Piece2Move(pc) | Cap2Move(-BOARD[to]) );
	  }
	if ( flag_unpromo )
	  {
	    *pmove++ = ( From2Move(from) | To2Move(to)
			 | Piece2Move(pc) | Cap2Move(-BOARD[to]) );
	  }
      }
  }

  return pmove;
}


static unsigned int * CONV
gen_king_move( const tree_t * restrict ptree, const char *psq, int turn,
	       int is_capture, unsigned int * restrict pmove )
{
  bitboard_t bb;
  int to, from;

  if ( turn )
    {
      from = SQ_WKING;
      bb   = abb_king_attacks[from];
      if ( is_capture )
	{
	  BBAnd( bb, bb, BB_BOCCUPY );
	  BBNotAnd( bb, bb, abb_mask[(int)psq[0]] );
	}
      else { BBNotAnd( bb, bb, BB_BOCCUPY ); }
      BBNotAnd( bb, bb, BB_WOCCUPY );
    }
  else {
    from = SQ_BKING;
    bb   = abb_king_attacks[from];
    if ( is_capture )
      {
	BBAnd( bb, bb, BB_WOCCUPY );
	BBNotAnd( bb, bb, abb_mask[(int)psq[0]] );
      }
    else { BBNotAnd( bb, bb, BB_WOCCUPY ); }
    BBNotAnd( bb, bb, BB_BOCCUPY );
  }
  
  while ( BBTest(bb) )
    {
      to = LastOne( bb );
      Xor( to, bb );

      if ( psq[1] != nsquare
	   && ( adirec[from][(int)psq[1]]
		== adirec[from][to] ) ) { continue; }

      if ( psq[0] != to
	   && adirec[from][(int)psq[0]] == adirec[from][to] ) {
	  if ( adirec[from][(int)psq[0]] & flag_cross )
	    {
	      if ( abs(BOARD[(int)psq[0]]) == lance
		   || abs(BOARD[(int)psq[0]]) == rook
		   || abs(BOARD[(int)psq[0]]) == dragon ) { continue; }
	    }
	  else if ( ( adirec[from][(int)psq[0]] & flag_diag )
		    && ( abs(BOARD[(int)psq[0]]) == bishop
			 || abs(BOARD[(int)psq[0]]) == horse ) ){ continue; }
	}

      if ( turn )
	{
	  if ( is_white_attacked( ptree, to ) ) { continue; }

	  *pmove++ = ( From2Move(from) | To2Move(to)
		       | Piece2Move(king) | Cap2Move(BOARD[to]) );
	}
      else {
	if ( is_black_attacked( ptree, to ) ) { continue; }

	*pmove++ = ( From2Move(from) | To2Move(to)
		     | Piece2Move(king) | Cap2Move(-BOARD[to]) );
      }
    }

  return pmove;
}


static unsigned int * CONV
gen_intercept( tree_t * restrict __ptree__, int sq_checker, int ply, int turn,
	       int * restrict premaining, unsigned int * restrict pmove,
	       int flag )
{
#define Drop(pc) ( To2Move(to) | Drop2Move(pc) )

  const tree_t * restrict ptree = __ptree__;
  bitboard_t bb_atk, bb_defender, bb;
  unsigned int amove[16];
  unsigned int hand;
  int n0, n1, inc, pc, sq_k, to, from, direc, nmove, nsup, i, min_chuai, itemp;
  int dist, flag_promo, flag_unpromo;

  n0 = n1 = 0;
  if ( turn )
    {
      sq_k        = SQ_WKING;
      bb_defender = BB_WOCCUPY;
      BBNotAnd( bb_defender, bb_defender, abb_mask[sq_k] );
    }
  else {
    sq_k        = SQ_BKING;
    bb_defender = BB_BOCCUPY;
    BBNotAnd( bb_defender, bb_defender, abb_mask[sq_k] );
  }

  switch ( adirec[sq_k][sq_checker] )
    {
    case direc_rank:
      min_chuai = ( sq_k < A8 || I2 < sq_k ) ? 2 : 4;
      inc       = 1;
      break;

    case direc_diag1:
      min_chuai = 3;
      inc       = 8;
      break;

    case direc_file:
      itemp     = (int)aifile[sq_k];
      min_chuai = ( itemp == file1 || itemp == file9 ) ? 2 : 4;
      inc = 9;
      break;

    default:
      assert( (int)adirec[sq_k][sq_checker] == direc_diag2 );
      min_chuai = 3;
      inc       = 10;
    }
  if ( sq_k > sq_checker ) { inc = -inc; }
  
  for ( dist = 1, to = sq_k + inc;
	to != sq_checker;
	dist += 1, to += inc ) {

    assert( 0 <= to && to < nsquare && BOARD[to] == empty );

    nmove  = 0;
    bb_atk = attacks_to_piece( ptree, to );
    BBAnd( bb, bb_defender, bb_atk );
    while ( BBTest(bb) )
      {
	from = LastOne( bb );
	Xor( from, bb );
	
	direc        = (int)adirec[sq_k][from];
	flag_promo   = 0;
	flag_unpromo = 1;
	if ( turn )
	  {
	    if ( direc && is_pinned_on_white_king( ptree, from, direc ) )
	      {
		continue;
	      }
	    pc = -BOARD[from];
	    switch ( pc )
	      {
	      case pawn:
		if ( to > I4 ) { flag_promo = 1;  flag_unpromo = 0; }
		break;
		
	      case lance:  case knight:
		if      ( to > I3 ) { flag_promo = 1;  flag_unpromo = 0; }
		else if ( to > I4 ) { flag_promo = 1; }
		break;
		
	      case silver:
		if ( to > I4 || from > I4 ) { flag_promo = 1; }
		break;
		
	      case bishop:  case rook:
		if ( to > I4
		     || from > I4 ) { flag_promo = 1;  flag_unpromo = 0; }
		break;
		
	      default:
		break;
	      }
	  }
	else {
	  if ( direc && is_pinned_on_black_king( ptree, from, direc ) )
	    {
	      continue;
	    }
	  pc = BOARD[from];
	  switch ( pc )
	    {
	    case pawn:
	      if ( to < A6 ) { flag_promo = 1;  flag_unpromo = 0; }
	      break;
	      
	    case lance:  case knight:
	      if      ( to < A7 ) { flag_promo = 1;  flag_unpromo = 0; }
	      else if ( to < A6 ) { flag_promo = 1; }
	      break;
	      
	    case silver:
	      if ( to < A6 || from < A6 ) { flag_promo = 1; }
	      break;
	      
	    case bishop:  case rook:
	      if ( to < A6
		   || from < A6 ) { flag_promo = 1;  flag_unpromo = 0; }
	      break;
	      
	    default:
	      break;
	    }
	}
	assert( flag_promo || flag_unpromo );
	if ( flag_promo )
	  {
	    amove[nmove++] = ( From2Move(from) | To2Move(to)
			       | FLAG_PROMO | Piece2Move(pc) );
	  }
	if ( flag_unpromo )
	  {
	    amove[nmove++] = ( From2Move(from) | To2Move(to)
			       | Piece2Move(pc) );
	  }
      }
    
    nsup = ( to == sq_k + inc ) ? nmove + 1 : nmove;
    if ( nsup > 1 )
      {
	for ( i = n0 + n1 - 1; i >= n0; i-- ) { pmove[i+nmove] = pmove[i]; }
	for ( i = 0; i < nmove; i++ ) { pmove[n0++] = amove[i]; }
      }
    else if ( nmove ) { pmove[n0 + n1++] = amove[0]; }
    
    if ( ! nsup )
      {
	/* - tentative assumption - */
	/* no recursive drops at non-supported square. */
	if ( flag == 2  ) { continue; }
	
	/* -tentative assumption- */
	/* no intercept-drop at non-supported square. */
	if ( (int)I2To(MOVE_LAST) == sq_checker
	     && dist > min_chuai ) { continue; }
      }
    
    nmove = 0;
    
    if ( turn ) {
      
      hand = HAND_W;
      
      if ( nsup ) {
	
	if ( IsHandRook(hand) ) { amove[nmove++] = Drop(rook); }
	else if ( IsHandLance(hand) && to < A1 )
	  {
	    amove[nmove++] = Drop(lance);
	  }
	else if ( IsHandPawn(hand)
		  && to < A1
		  && ! ( BBToU(BB_WPAWN) & ( mask_file1 >> aifile[to] ) )
		  && ! IsMateWPawnDrop( __ptree__, to ) )
	  {
	    amove[nmove++] = Drop(pawn);
	  }
	
      } else {
	
	if ( IsHandPawn(hand)
	     && to < A1
	     && ! ( BBToU(BB_WPAWN) & ( mask_file1 >> aifile[to] ) )
	     && ! IsMateWPawnDrop( __ptree__, to ) )
	  {
	    amove[nmove++] = Drop(pawn);
	  }
	if ( IsHandLance(hand) && to < A1 ) { amove[nmove++] = Drop(lance); }
	if ( IsHandRook(hand) )             { amove[nmove++] = Drop(rook); }
      }
      
      if ( IsHandKnight(hand) && to < A2 ) { amove[nmove++] = Drop(knight); }
      
    } else {
      
      hand = HAND_B;
      
      if ( nsup ) {
	
	if ( IsHandRook(hand) ) { amove[nmove++] = Drop(rook); }
	else if ( IsHandLance(hand) && to > I9 )
	  {
	    amove[nmove++] = Drop(lance);
	  }
	else if ( IsHandPawn(hand)
		  && to > I9
		  && ! ( BBToU(BB_BPAWN) & ( mask_file1 >> aifile[to] ) )
		  && ! IsMateBPawnDrop( __ptree__, to ) )
	  {
	    amove[nmove++] = Drop(pawn);
	  }
	
      } else {
	
	if ( IsHandPawn(hand)
	     && to > I9
	     && ! ( BBToU(BB_BPAWN) & ( mask_file1 >> aifile[to] ) )
	     && ! IsMateBPawnDrop( __ptree__, to ) )
	  {
	    amove[nmove++] = Drop(pawn);
	  }
	if ( IsHandLance(hand) && to > I9 ) { amove[nmove++] = Drop(lance); }
	if ( IsHandRook(hand) )             { amove[nmove++] = Drop(rook); }
      }
      
      if ( IsHandKnight(hand) && to > I8 ) { amove[nmove++] = Drop(knight); }
    }
    
    if ( IsHandSilver(hand) ) { amove[nmove++] = Drop(silver); }
    if ( IsHandGold(hand) )   { amove[nmove++] = Drop(gold); }
    if ( IsHandBishop(hand) ) { amove[nmove++] = Drop(bishop); }
    
    if ( nsup )
      {
	/* - tentative assumption - */
	/* a supported intercepter saves the king for two plies at least. */
	if ( nmove && flag == 0 && dist > min_chuai
	     && I2From(MOVE_LAST) >= nsquare )
	    {
	      *premaining = -1;
	      pmove[0]    = amove[0];
	      return pmove + 1;
	    }

	for ( i = n0 + n1 - 1; i >= n0; i-- ) { pmove[i+nmove] = pmove[i]; }
	for ( i = 0; i < nmove; i++ ) { pmove[n0++] = amove[i]; }
      }
    else for ( i = 0; i < nmove; i++ ) { pmove[n0 + n1++] = amove[i]; }
  }
  
  *premaining = n0;
  return pmove + n0 + n1;

#undef Drop
}
