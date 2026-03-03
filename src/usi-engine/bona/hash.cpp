// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include "shogi.h"

static int CONV eval_supe( unsigned int hand_current, unsigned int hand_hash,
			   int turn_current, int turn_hash,
			   int * restrict pvalue_hash,
			   int * restrict ptype_hash );

int CONV
ini_trans_table( void )
{
  size_t size;
  unsigned int ntrans_table;

  ntrans_table = 1U << log2_ntrans_table;
  size         = sizeof( trans_table_t ) * ntrans_table + 15U;
  ptrans_table_orig = (trans_table_t *)memory_alloc( size );
  if ( ptrans_table_orig == NULL ) { return -1; }
  ptrans_table = (trans_table_t *)( ((ptrdiff_t)ptrans_table_orig+15)
				    & ~(ptrdiff_t)15U );
  hash_mask    = ntrans_table - 1;
  Out( "Trans. Table Entries = %dK (%dMB)\n",
       ( ntrans_table * 3U ) / 1024U, size / (1024U * 1024U ) );

  return clear_trans_table();
}


#define Foo( PIECE, piece )  bb = BB_B ## PIECE;                    \
                             while( BBTest(bb) ) {                  \
                               sq = FirstOne( bb );                 \
                               Xor( sq, bb );                       \
                               key ^= ( b_ ## piece ## _rand )[sq]; \
                             }                                      \
                             bb = BB_W ## PIECE;                    \
                             while( BBTest(bb) ) {                  \
                               sq = FirstOne( bb );                 \
                               Xor( sq, bb );                       \
                               key ^= ( w_ ## piece ## _rand )[sq]; \
                             }

uint64_t CONV
hash_func( const tree_t * restrict ptree )
{
  uint64_t key = 0;
  bitboard_t bb;
  int sq;

  key ^= b_king_rand[SQ_BKING];
  key ^= w_king_rand[SQ_WKING];

  Foo( PAWN,       pawn );
  Foo( LANCE,      lance );
  Foo( KNIGHT,     knight );
  Foo( SILVER,     silver );
  Foo( GOLD,       gold );
  Foo( BISHOP,     bishop );
  Foo( ROOK,       rook );
  Foo( PRO_PAWN,   pro_pawn );
  Foo( PRO_LANCE,  pro_lance );
  Foo( PRO_KNIGHT, pro_knight );
  Foo( PRO_SILVER, pro_silver );
  Foo( HORSE,      horse );
  Foo( DRAGON,     dragon );

  return key;
}

#undef Foo


void rand2_update(tree_t * restrict ptree, int sideToMove, int move) {
	uint64_t r = ptree->rand2_hash;
	const int from = (int)I2From(move);
	const int to   = (int)I2To(move);
	if ( from >= nsquare ) {	// drop
		int k = From2Drop(from);
		if ( sideToMove == black ) {
			int n = get_motigoma(k, HAND_B);
			if ( k <= 0 || k>31 || n<1 || n>18 || to < 0 || to >80 ) DEBUG_PRT("");
			r ^= rand2_hand_table[0][k][n  ];
			r ^= rand2_hand_table[0][k][n-1];
 			r ^= rand2_move_table[k][to];
		} else {
			int n = get_motigoma(k, HAND_W);
			if ( k <= 0 || k+16>31 || n<1 || n>18 || to < 0 || to > 80 ) DEBUG_PRT("");
			r ^= rand2_hand_table[1][k][n  ];
			r ^= rand2_hand_table[1][k][n-1];
 			r ^= rand2_move_table[k+16][to];
		}
	} else {
		int k                 = (int)I2PieceMove(move);
		int k_cap             = (int)UToCap(move);
		const int is_promote  = (int)I2IsPromote(move);

		int new_k = k;
		if ( is_promote ) new_k += promote;
		if ( sideToMove == white ) {
			k     += 16;
			new_k += 16;
		}
	 	r ^= rand2_move_table[    k][from];
	 	r ^= rand2_move_table[new_k][  to];

		if ( k_cap ) {
			int ck = k_cap & 0x07;
			if ( sideToMove == black ) {
				int n = get_motigoma(ck, HAND_B);
				r ^= rand2_hand_table[0][ck][n  ];
				r ^= rand2_hand_table[0][ck][n+1];
				if ( ck < 1 || n > 17 ) DEBUG_PRT("");
			} else {
				int n = get_motigoma(ck, HAND_W);
				r ^= rand2_hand_table[1][ck][n  ];
				r ^= rand2_hand_table[1][ck][n+1];
				if ( ck < 1 || n > 17 ) DEBUG_PRT("");
			}
			if ( sideToMove == black ) k_cap += 16;
	 		r ^= rand2_move_table[k_cap][  to];
			if ( k_cap > 31 ) DEBUG_PRT("");
		}
		if ( k < 0 || k>31 || k_cap < 0 || k_cap >31 || from < 0 || from > 80 || to < 0 || to > 80 || new_k < 0 || new_k > 31 ) DEBUG_PRT("");
//		PRT("nrep=%3d:side=%d,k=%2d,new_k=%2d,from=%2d,to=%2d,k_cap=%2d,is_promote=%d,%016" PRIx64 "\n",ptree->nrep,sideToMove,k,new_k,from,to,k_cap,is_promote,ptree->rand2_hash);
	}
	r = ~r;	// Žè”Ô‚ð”½“]‚Å•\‚·B“¯ˆê‹Ç–Ê‚ÅŽè”Ô‚ªˆÙ‚È‚éA‚Í”½“]‚µ‚Ä‚¢‚é
	ptree->rand2_hash = r;
//	PRT("nrep=%3d:side=%d,from=%2d,to=%2d,move=%06x,%016" PRIx64 "\n",ptree->nrep,sideToMove,from,to,move,r);
}

void set_root_rand2_hash(tree_t * restrict ptree, int sideToMove) {
	uint64_t r = 0;
	for ( int sq = 0; sq < nsquare; sq++ ) {
		int k = BOARD[sq];
		if ( k == 0 ) continue;
		if ( k < 0 ) k = abs(k)+16;	// -1 -> +17(+1+16),  -15 -> +31(+15+16)
		if ( k > 32 ) DEBUG_PRT("");
		r ^= rand2_move_table[k][sq];
	}
	for (int i=1;i<8;i++) {
		int n;
		n = get_motigoma(i, HAND_B);
		if ( n > 18 ) DEBUG_PRT("");
		r ^= rand2_hand_table[0][i][n];
		n = get_motigoma(i, HAND_W);
		if ( n > 18 ) DEBUG_PRT("");
		r ^= rand2_hand_table[1][i][n];
	}
	if ( sideToMove ) r = ~r;
	ptree->rand2_hash = r;
	root_rand2_hash = r;
//	PRT("set root side=%d,%016" PRIx64 "\n",sideToMove,r);
}

void copy_rand2_hash(tree_t * restrict ptree) {
	ptree->rand2_hash = root_rand2_hash;
}


/*
       name    bits  shifts
word1  depth     8     56
       value    16     40
       move     19     21
       hand     21      0
word2  key      57      7
       turn      1      6
       threat    1      5
       type      2      3
       age       3      0
 */

void CONV
hash_store( const tree_t * restrict ptree, int ply, int depth, int turn,
	    int value_type, int value, unsigned int move,
	    unsigned int state_node )
{
  uint64_t word1, word2, hash_word1, hash_word2;
  unsigned int index, slot;
  int depth_hash, age_hash;

#if ! defined(MINIMUM)
  if ( game_status & flag_learning ) { return; }
#endif
  assert( depth <= 0xff );

  if ( depth < 0 ) { depth = 0; }
  if ( abs(value) > score_max_eval )
    {
      if ( abs(value) > score_mate1ply ) { return; }
      if ( value > 0 ) { value += ply-1; }
      else             { value -= ply-1; }
#if ! defined(MINIMUM)
      if ( abs(value) > score_mate1ply )
	{
	  out_warning( "A stored hash value is out of bounce!" );
	}
#endif
    }
  word2 = ( ( HASH_KEY & ~(uint64_t)0x7fU )
	    | (uint64_t)( (turn<<6) | ( state_node & node_mate_threat )
			  | (value_type<<3) | trans_table_age ) );
  word1 = ( ( (uint64_t)( depth<<16 | (value+32768) ) << 40 )
	    | ( (uint64_t)( move & 0x7ffffU ) << 21 )
	    | HAND_B );

  index = (unsigned int)HASH_KEY & hash_mask;
  hash_word1 = ptrans_table[index].prefer.word1;
  hash_word2 = ptrans_table[index].prefer.word2;
  SignKey( hash_word2, hash_word1 );
  age_hash   = (int)((unsigned int)(hash_word2    ) & 0x07U);
  depth_hash = (int)((unsigned int)(hash_word1>>56) & 0xffU);

  SignKey( word2, word1 );

  if ( age_hash != trans_table_age || depth_hash <= depth )
    {
      ptrans_table[index].prefer.word1 = word1;
      ptrans_table[index].prefer.word2 = word2;
    }
  else {
    slot = (unsigned int)HASH_KEY >> 31;
    ptrans_table[index].always[slot].word1 = word1;
    ptrans_table[index].always[slot].word2 = word2;
  }
}


void CONV
hash_store_pv( const tree_t * restrict ptree, unsigned int move, int turn )
{
  uint64_t key_turn_pv, word1, word2;
  unsigned int index;

  key_turn_pv = ( HASH_KEY & ~(uint64_t)0x7fU ) | (unsigned int)( turn << 6 );
  index       = (unsigned int)HASH_KEY & hash_mask;

  word1 = ptrans_table[index].prefer.word1;
  word2 = ptrans_table[index].prefer.word2;
  SignKey( word2, word1 );

  if ( ( (unsigned int)word1 & 0x1fffffU ) == HAND_B
       && ( word2 & ~(uint64_t)0x3fU ) == key_turn_pv )
    {
      if ( ( (unsigned int)(word1>>21) & 0x7ffffU ) != ( move & 0x7ffffU ) )
	{
	  word1 &= ~((uint64_t)0x7ffffU << 21);
	  word1 |= (uint64_t)( move & 0x7ffffU ) << 21;
	  word2 &= ~((uint64_t)0x3U << 3);
	  SignKey( word2, word1 );
	  ptrans_table[index].prefer.word1 = word1;
	  ptrans_table[index].prefer.word2 = word2;
	}
    }
  else {
    unsigned int slot;

    slot = (unsigned int)HASH_KEY >> 31;
    word1 = ptrans_table[index].always[slot].word1;
    word2 = ptrans_table[index].always[slot].word2;
    SignKey( word2, word1 );
    if ( ( (unsigned int)word1 & 0x1fffffU ) == HAND_B
	 && ( word2 & ~(uint64_t)0x3fU ) == key_turn_pv )
      {
	if ( ( (unsigned int)(word1>>21) & 0x7ffffU )
	     != ( move & 0x7ffffU ) )
	  {
	    word1 &= ~((uint64_t)0x7ffffU << 21);
	    word1 |= (uint64_t)( move & 0x7ffffU ) << 21;
	    word2 &= ~((uint64_t)0x3U << 3);
	    SignKey( word2, word1 );
	    ptrans_table[index].always[slot].word1 = word1;
	    ptrans_table[index].always[slot].word2 = word2;
	  }
      }
    else {
      word1  = (uint64_t)32768U << 40;
      word1 |= (uint64_t)( move & 0x7ffffU ) << 21;
      word1 |= HAND_B;
      word2  = key_turn_pv | trans_table_age;
      SignKey( word2, word1 );
      ptrans_table[index].prefer.word1 = word1;
      ptrans_table[index].prefer.word2 = word2;
    }
  }
}


unsigned int CONV
hash_probe( tree_t * restrict ptree, int ply, int depth_current,
	    int turn_current, int alpha, int beta, unsigned int *pstate_node )
{
  uint64_t word1, word2, key_current, key_hash;
  unsigned int hand_hash, move_hash, move_supe, slot, utemp;
  unsigned int state_node_hash, index;
  int null_depth, value_hash, ifrom;
  int turn_hash, depth_hash, type_hash, is_superior;

  ptree->ntrans_probe++;
  move_supe   = 0;

  if ( depth_current < 0 ) { depth_current = 0; }
  null_depth  = NullDepth( depth_current );
  if ( null_depth < PLY_INC ) { null_depth = 0; }

  key_current = HASH_KEY & ~(uint64_t)0x7fU;

  index = (unsigned int)HASH_KEY & hash_mask;
  word1 = ptrans_table[index].prefer.word1;
  word2 = ptrans_table[index].prefer.word2;
  SignKey( word2, word1 );
  key_hash = word2 & ~(uint64_t)0x7fU;

  if ( key_hash == key_current ) {

    ptree->ntrans_prefer_hit++;
    
    depth_hash  = (int)((unsigned int)(word1>>56) & 0x00ffU);
    value_hash  = (int)((unsigned int)(word1>>40) & 0xffffU) - 32768;
    move_hash   = (unsigned int)(word1>>21) & 0x7ffffU;
    hand_hash   = (unsigned int)word1 & 0x1fffffU;
    
    utemp           = (unsigned int)word2;
    state_node_hash = utemp & node_mate_threat;
    turn_hash       = (int)((utemp>>6) & 0x1U);
    type_hash       = (int)((utemp>>3) & 0x3U);
    
    if ( abs(value_hash) > score_max_eval )
      {
	if ( value_hash > 0 ) { value_hash -= ply-1; }
	else                  { value_hash += ply-1; }

#if ! defined(MINIMUM)
	if ( abs(value_hash) > score_mate1ply )
	  {
	    out_warning( "Hash value is out of bounce!!" );
	  }
#endif
      }

    if ( RecursionThreshold <= depth_current
	 && depth_hash < RecursionDepth(depth_current) ) { move_hash = 0; }
    else if ( move_hash )
      {
	move_hash |= turn_current ? Cap2Move( BOARD[I2To(move_hash)])
                                  : Cap2Move(-BOARD[I2To(move_hash)]);
      }

    if ( turn_hash == turn_current && hand_hash == HAND_B ) {

      assert( ! move_hash || is_move_valid( ptree, move_hash, turn_current ) );
      ptree->amove_hash[ply] = move_hash;

      *pstate_node |= state_node_hash;
      
      if ( value_hash <= score_max_eval ) { *pstate_node &= ~node_do_mate; }

      if ( ( type_hash & flag_value_up_exact )
	   && value_hash < beta
	   && null_depth <= depth_hash )
	{
	  *pstate_node &= ~node_do_null;
	}

      if ( ( type_hash & flag_value_up_exact )
	   && value_hash <= alpha
	   && RecursionDepth(depth_current) <= depth_hash )
	{
	  *pstate_node &= ~node_do_recursion;
	}

      if ( type_hash == value_lower
	   && beta <= value_hash
	   && ( depth_hash >= depth_current || value_hash > score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_lower++;
	  return value_lower;
	}

      if ( type_hash == value_upper
	   && value_hash <= alpha
	   && ( depth_hash >= depth_current || value_hash < -score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_upper++;
	  return value_upper;
	}

      if ( type_hash == value_exact
	   && ( depth_hash >= depth_current
		|| abs(value_hash) > score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_upper++;
	  return value_exact;
	}

      if ( ( type_hash & flag_value_low_exact )
	   && ! ptree->nsuc_check[ply]
	   && ! ptree->nsuc_check[ply-1]
	   && ( ( depth_current < 2*PLY_INC
		  && beta+EFUTIL_MG1 <= value_hash )
		|| ( depth_current < 3*PLY_INC
		     && beta+EFUTIL_MG2 <= value_hash ) ) )
	{
	  HASH_VALUE = beta;
	  ptree->ntrans_lower++;
	  return value_lower;
	}

    } else {

      is_superior = eval_supe( HAND_B, hand_hash, turn_current, turn_hash,
			       &value_hash, &type_hash );

      if ( is_superior == 1 ) {

	if ( turn_hash == turn_current ) { move_supe = move_hash; }

	if ( type_hash & flag_value_low_exact )
	  {
	    if ( ! ptree->nsuc_check[ply]
		 && ! ptree->nsuc_check[ply-1]
		 && ( ( depth_current < 2*PLY_INC
			&& beta+EFUTIL_MG1 <= value_hash )
		      || ( depth_current < 3*PLY_INC
			   && beta+EFUTIL_MG2 <= value_hash ) ) )
	      {
		HASH_VALUE = beta;
		ptree->ntrans_lower++;
		return value_lower;
	      }
	    
	    if ( beta <= value_hash
		 && ( depth_current <= depth_hash
		      || score_max_eval < value_hash
		      || ( turn_current != turn_hash
			   && depth_hash >= null_depth
			   && ( *pstate_node & node_do_null ) ) ) )
	      {
		HASH_VALUE = value_hash;
		ptree->ntrans_superior_hit++;
		return value_lower;
	      }
	  }

      }	else if ( is_superior == -1 ) {

	*pstate_node |= state_node_hash;
	
	if ( value_hash <= score_max_eval )
	  {
	    *pstate_node &= ~node_do_mate;
	  }
	
	if ( ( type_hash & flag_value_up_exact )
	     && value_hash <= alpha
	     && RecursionDepth(depth_current) <= depth_hash )
	  {
	    *pstate_node &= ~node_do_recursion;
	  }
	
	if ( type_hash & flag_value_up_exact )
	  {
	    if ( value_hash < beta && null_depth <= depth_hash )
	      {
		*pstate_node &= ~node_do_null;
	      }
	    if ( value_hash <= alpha
		 && ( depth_current <= depth_hash
		      || value_hash < -score_max_eval ) )
	      {
		HASH_VALUE = value_hash;
		ptree->ntrans_inferior_hit++;
		return value_upper;
	      }
	  }
      }
    }
  }
  
  slot  = (unsigned int)HASH_KEY >> 31;
  word1 = ptrans_table[index].always[slot].word1;
  word2 = ptrans_table[index].always[slot].word2;
  
  SignKey( word2, word1 );
  key_hash = word2 & ~(uint64_t)0x7fU;
  
  if ( key_hash == key_current ) {

    ptree->ntrans_always_hit++;

    depth_hash  = (int)((unsigned int)(word1>>56) & 0x00ffU);
    value_hash  = (int)((unsigned int)(word1>>40) & 0xffffU) - 32768;
    move_hash   = (unsigned int)(word1>>21) & 0x7ffffU;
    hand_hash   = (unsigned int)word1 & 0x1fffffU;
    
    utemp           = (unsigned int)word2;
    state_node_hash = utemp & node_mate_threat;
    turn_hash       = (int)((utemp>>6) & 0x1U);
    type_hash       = (int)((utemp>>3) & 0x3U);
    
    if ( abs(value_hash) > score_max_eval )
      {
	if ( value_hash > 0 ) { value_hash -= ply-1; }
	else                  { value_hash += ply-1; }

#if ! defined(MINIMUM)
	if ( abs(value_hash) > score_mate1ply )
	  {
	    out_warning( "Hash value is out of bounce!!" );
	  }
#endif
      }
    
    if ( RecursionThreshold <= depth_current
	 && depth_hash < RecursionDepth(depth_current) ) { move_hash = 0; }
    else if ( move_hash )
      {
	move_hash |= turn_current ? Cap2Move( BOARD[I2To(move_hash)])
                                  : Cap2Move(-BOARD[I2To(move_hash)]);
      }

    if ( turn_hash == turn_current && hand_hash == HAND_B ) {
      
      if ( ! ptree->amove_hash[ply] )
	{
	  assert( ! move_hash
		  || is_move_valid( ptree, move_hash, turn_current ) );
	  ptree->amove_hash[ply] = move_hash;
	}

      *pstate_node |= state_node_hash;

      if ( value_hash <= score_max_eval ) { *pstate_node &= ~node_do_mate; }
      
      if ( ( type_hash & flag_value_up_exact )
	   && value_hash <= alpha
	   && RecursionDepth(depth_current) <= depth_hash )
	{
	  *pstate_node &= ~node_do_recursion;
	}

      if ( ( type_hash & flag_value_up_exact )
	   && value_hash < beta
	   && null_depth <= depth_hash )
	{
	  *pstate_node &= ~node_do_null;
	}

      if ( type_hash == value_lower
	   && value_hash >= beta
	   && ( depth_hash >= depth_current || value_hash > score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_lower++;
	  return value_lower;
	}

      if ( type_hash == value_upper
	   && value_hash <= alpha
	   && ( depth_hash >= depth_current || value_hash < -score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_upper++;
	  return value_upper;
	}

      if ( type_hash == value_exact
	   && ( depth_hash >= depth_current
		|| abs(value_hash) > score_max_eval ) )
	{
	  HASH_VALUE = value_hash;
	  ptree->ntrans_upper++;
	  return value_exact;
	}


      if ( ( type_hash & flag_value_low_exact )
	   && ! ptree->nsuc_check[ply]
	   && ! ptree->nsuc_check[ply-1]
	   && ( ( depth_current < 2*PLY_INC
		  && beta+EFUTIL_MG1 <= value_hash )
		|| ( depth_current < 3*PLY_INC
		     && beta+EFUTIL_MG2 <= value_hash ) ) )
	{
	  HASH_VALUE = beta;
	  ptree->ntrans_lower++;
	  return value_lower;
	}

    } else {

	is_superior = eval_supe( HAND_B, hand_hash, turn_current, turn_hash,
				 &value_hash, &type_hash );

	if ( is_superior == 1 ) {

	  if ( ( turn_hash == turn_current ) && ! move_supe )
	    {
	      move_supe = move_hash;
	    }
	  
	  if ( type_hash & flag_value_low_exact )
	    {
	      if ( ! ptree->nsuc_check[ply]
		   && ! ptree->nsuc_check[ply-1]
		   && ( ( depth_current < 2*PLY_INC
			  && beta+EFUTIL_MG1 <= value_hash )
			|| ( depth_current < 3*PLY_INC
			     && beta+EFUTIL_MG2 <= value_hash ) ) )
		{
		  HASH_VALUE = beta;
		  ptree->ntrans_lower++;
		  return value_lower;
		}
	      
	      if ( value_hash >= beta
		   && ( depth_hash >= depth_current
			|| score_max_eval < value_hash
			|| ( turn_current != turn_hash
			     && depth_hash >= null_depth
			     && ( *pstate_node & node_do_null ) ) ) )
		{
		  HASH_VALUE = value_hash;
		  ptree->ntrans_superior_hit++;
		  return value_lower;
		}
	    }

	} else if ( is_superior == -1 ) {

	  *pstate_node |= state_node_hash;
	  
	  if ( value_hash <= score_max_eval )
	    {
	      *pstate_node &= ~node_do_mate;
	    }
	  
	  if ( ( type_hash & flag_value_up_exact )
	       && value_hash <= alpha
	       && RecursionDepth(depth_current) <= depth_hash )
	    {
	      *pstate_node &= ~node_do_recursion;
	    }
	  
	  if ( type_hash & flag_value_up_exact )
	    {
	      if ( value_hash < beta && null_depth <= depth_hash )
		{
		  *pstate_node &= ~node_do_null;
		}
	      if ( value_hash <= alpha
		   && ( depth_hash >= depth_current
			|| value_hash < -score_max_eval ) )
		{
		  HASH_VALUE = value_hash;
		  ptree->ntrans_inferior_hit++;
		  return value_upper;
		}
	    }
	}
    }
  }
  
  if ( ! ptree->amove_hash[ply] )
    {
      if ( move_supe )
	{
	  ifrom = (int)I2From(move_supe);
	  if ( ifrom >= nsquare )
	    {
	      unsigned int hand = turn_current ? HAND_W : HAND_B;
	      switch( From2Drop(ifrom) )
		{
		case pawn:
		  if ( ! IsHandPawn(hand) ) {
		    move_supe = To2Move(I2To(move_supe));
		    if ( IsHandLance(hand) )
		      {
			move_supe |= Drop2Move(lance);
		      }
		    else if ( IsHandSilver(hand))
		      {
			move_supe |= Drop2Move(silver);
		      }
		    else if ( IsHandGold(hand) )
		      {
			move_supe |= Drop2Move(gold);
		      }
		    else { move_supe |= Drop2Move(rook); }
		  }
		  break;
		
		case lance:
		  if ( ! IsHandLance(hand) )
		    {
		      move_supe = To2Move(I2To(move_supe)) | Drop2Move(rook);
		    }
		  break;
		}
	    }

	  assert( is_move_valid( ptree, move_supe, turn_current ) );
	  ptree->amove_hash[ply] = move_supe;
	}
    }
  
  return value_null;
}


static int CONV
eval_supe( unsigned int hand_current, unsigned int hand_hash,
	   int turn_current, int turn_hash,
	   int * restrict pvalue_hash, int * restrict ptype_hash )
{
  int is_superior;

  if ( hand_current == hand_hash ) { is_superior = 0; }
  else if ( is_hand_eq_supe( hand_current, hand_hash ) )
    {
      is_superior = turn_current ? -1 : 1;
    }
  else if ( is_hand_eq_supe( hand_hash, hand_current ) )
    {
      is_superior = turn_current ? 1 : -1;
    }
  else { return 0; }
  
  if ( turn_hash != turn_current )
    {
      if ( is_superior == -1 ) { is_superior = 0; }
      else {
	is_superior   = 1;
	*pvalue_hash *= -1;
	switch ( *ptype_hash )
	  {
	  case value_lower:  *ptype_hash=value_upper;  break;
	  case value_upper:  *ptype_hash=value_lower;  break;
	  }
      }
    }

  return is_superior;
}


int CONV
clear_trans_table( void )
{
  unsigned int elapsed_start, elapsed_end;
  int ntrans_table, i;

  if ( get_elapsed( &elapsed_start ) < 0 ) { return -1; }

  Out( "cleanning the transposition table ..." );
  
  trans_table_age = 1;
  ntrans_table = 1 << log2_ntrans_table;
  for ( i = 0; i < ntrans_table; i++ )
    {
      ptrans_table[i].prefer.word1    = 0;
      ptrans_table[i].prefer.word2    = 0;
      ptrans_table[i].always[0].word1 = 0;
      ptrans_table[i].always[0].word2 = 0;
      ptrans_table[i].always[1].word1 = 0;
      ptrans_table[i].always[1].word2 = 0;
    }

  if ( get_elapsed( &elapsed_end ) < 0 ) { return -1; }
  Out( " done (%ss)\n", str_time_symple( elapsed_end - elapsed_start ) );

  return 1;
}
