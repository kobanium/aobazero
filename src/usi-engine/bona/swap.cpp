// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "shogi.h"

int CONV
swap( const tree_t * restrict ptree, unsigned int move, int root_alpha,
      int root_beta, int turn )
{
  bitboard_t bb, bb_temp, bb_attack;
  int attacked_piece, from, to, nc;
  int piece_cap, piece, value, xvalue, alpha, beta, is_promo;

  from = (int)I2From( move );
  to   = (int)I2To( move );

  if ( from >= nsquare )
    {
      value          = 0;
      piece          = From2Drop( from );
      attacked_piece = p_value_ex[15+piece];
    }
  else {
    piece     = (int)I2PieceMove( move );
    piece_cap = (int)UToCap( move );
    is_promo  = (int)I2IsPromote( move );
    
    value          = p_value_ex[15+piece_cap];
    attacked_piece = p_value_ex[15+piece];
    if ( is_promo )
      {
	value          += p_value_pm[7+piece];
	attacked_piece += p_value_pm[7+piece];
      }
    xvalue = value - attacked_piece - MT_PRO_PAWN;
    if ( xvalue >= root_beta ) { return xvalue; }
  }

  bb_attack = attacks_to_piece( ptree, to );
  alpha     = INT_MIN;
  beta      = value;
  for ( nc = 1;; nc++ )
    {
      /* remove an attacker, and add a hidden piece to bb_attack */
      if ( from < nsquare )
	{
	  Xor( from, bb_attack );
	  switch ( adirec[to][from] )
	    {
	    case direc_rank:
	      bb = AttackRank( from );
	      if ( from > to ) { BBAnd( bb, bb, abb_plus_rays[from] ); }
	      else             { BBAnd( bb, bb, abb_minus_rays[from] ); }
	      BBOr( bb_temp, BB_B_RD, BB_W_RD );
	      BBAndOr( bb_attack, bb, bb_temp );
	      break;

	    case direc_file:
	      bb = AttackFile( from );
	      BBOr( bb_temp, BB_B_RD, BB_W_RD );
	      if ( from > to )
		{
		  BBOr( bb_temp, bb_temp, BB_BLANCE );
		  BBAnd( bb, bb, abb_plus_rays[from] );
		}
	      else {
		BBOr( bb_temp, bb_temp, BB_WLANCE );
		BBAnd( bb, bb, abb_minus_rays[from] );
	      }
	      BBAnd( bb, bb, bb_temp );
	      BBOr( bb_attack, bb_attack, bb );
	      break;
	      
	    case direc_diag1:
	      bb = AttackDiag1( from );
	      if ( from > to ) { BBAnd( bb, bb, abb_plus_rays[from] ); }
	      else             { BBAnd( bb, bb, abb_minus_rays[from] ); }
	      BBOr( bb_temp, BB_B_BH, BB_W_BH );
	      BBAnd( bb, bb, bb_temp );
	      BBOr( bb_attack, bb_attack, bb );
	      break;
	      
	    case direc_diag2:
	      bb = AttackDiag2( from );
	      if ( from > to ) { BBAnd( bb, bb, abb_plus_rays[from] ); }
	      else               { BBAnd( bb, bb, abb_minus_rays[from] ); }
	      BBOr( bb_temp, BB_B_BH, BB_W_BH );
	      BBAnd( bb, bb, bb_temp );
	      BBOr( bb_attack, bb_attack, bb );
	      break;
	      
	    }
	}

      /* find the cheapest piece attacks the target */
      if ( turn ) {

	if ( ! BBContract( bb_attack, BB_BOCCUPY ) ) { break; }
	value = attacked_piece - value;

	if( BBContract( bb_attack, BB_BPAWN ) )
	  {
	    from           = to+nfile;
	    attacked_piece = MT_CAP_PAWN;
	    if ( to < 27 )
	      {
		value          += MT_PRO_PAWN;
		attacked_piece += MT_PRO_PAWN;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BLANCE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_LANCE;
	    if ( to < 27 )
	      {
		value          += MT_PRO_LANCE;
		attacked_piece += MT_PRO_LANCE;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BKNIGHT, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_KNIGHT;
	    if ( to < 27 )
	      {
		value          += MT_PRO_KNIGHT;
		attacked_piece += MT_PRO_KNIGHT;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BPRO_PAWN, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_PAWN;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BPRO_LANCE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_LANCE;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BPRO_KNIGHT, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_KNIGHT;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BSILVER, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_SILVER;
	    if ( from < 27 || to < 27 )
	      {
		value          += MT_PRO_SILVER;
		attacked_piece += MT_PRO_SILVER;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BPRO_SILVER, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_SILVER;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BGOLD, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_GOLD;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BBISHOP, bb_attack );
	if( BBTest( bb ) )
	  {
	    from          = FirstOne( bb );
	    attacked_piece = MT_CAP_BISHOP;
	    if ( from < 27 || to < 27 )
	      {
		value          += MT_PRO_BISHOP;
		attacked_piece += MT_PRO_BISHOP;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BHORSE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_HORSE;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BROOK, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_ROOK;
	    if ( from < 27 || to < 27 )
	      {
		value          += MT_PRO_ROOK;
		attacked_piece += MT_PRO_ROOK;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_BDRAGON, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_DRAGON;
	    goto ab_cut;
	  }
	
	assert( BBContract( bb_attack, BB_BKING ) );
	
	from           = SQ_BKING;
	attacked_piece = MT_CAP_KING;
	
      } else {
	
	if ( ! BBContract( bb_attack, BB_WOCCUPY ) ) { break; }
	
	value = attacked_piece - value;

	if( BBContract( bb_attack, BB_WPAWN ) )
	  {
	    from           = to-nfile;
	    attacked_piece = MT_CAP_PAWN;
	    if ( to > 53 )
	      {
		value          += MT_PRO_PAWN;
		attacked_piece += MT_PRO_PAWN;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WLANCE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_LANCE;
	    if ( to > 53 )
	      {
		value          += MT_PRO_LANCE;
		attacked_piece += MT_PRO_LANCE;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WKNIGHT, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_KNIGHT;
	    if ( to > 53 )
	      {
		value          += MT_PRO_KNIGHT;
		attacked_piece += MT_PRO_KNIGHT;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WPRO_PAWN, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_PAWN;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WPRO_LANCE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_LANCE;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WPRO_KNIGHT, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_KNIGHT;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WSILVER, bb_attack );
	if( BBTest( bb ) )
	  {
	    from          = FirstOne( bb );
	    attacked_piece = MT_CAP_SILVER;
	    if ( from > 53 || to > 53 )
	      {
		value          += MT_PRO_SILVER;
		attacked_piece += MT_PRO_SILVER;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WPRO_SILVER, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_PRO_SILVER;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WGOLD, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_GOLD;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WBISHOP, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_BISHOP;
	    if ( from > 53 || to > 53 )
	      {
		value          += MT_PRO_BISHOP;
		attacked_piece += MT_PRO_BISHOP;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WHORSE, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_HORSE;
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WROOK, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_ROOK;
	    if ( from > 53 || to > 53 )
	      {
		value          += MT_PRO_ROOK;
		attacked_piece += MT_PRO_ROOK;
	      }
	    goto ab_cut;
	  }
	BBAnd( bb, BB_WDRAGON, bb_attack );
	if( BBTest( bb ) )
	  {
	    from           = FirstOne( bb );
	    attacked_piece = MT_CAP_DRAGON;
	    goto ab_cut;
	  }
	
	assert( BBContract( bb_attack, BB_WKING ) );
	from           = SQ_WKING;
	attacked_piece = MT_CAP_KING;
      }

    ab_cut:
      turn = Flip( turn );
      if ( nc & 1 )
	{
	  if ( -value > alpha )
	    {
	      if ( -value >= beta )      { return beta; }
	      if ( -value >= root_beta ) { return -value; }
	      alpha = -value;
	    }
	  else {
	    xvalue = attacked_piece + MT_PRO_PAWN - value;
	    if ( xvalue <= alpha )      { return alpha; }
	    if ( xvalue <= root_alpha ) { return xvalue; }
	  }
	}
      else {
	if ( value < beta )
	  {
	    if ( value <= alpha )      { return alpha; }
	    if ( value <= root_alpha ) { return value; }
	    beta = value;
	  }
	else {
	  xvalue = value - attacked_piece - MT_PRO_PAWN;
	  if ( xvalue >= beta )      { return beta; }
	  if ( xvalue >= root_beta ) { return xvalue; }
	}
      }
    }
  
  if ( nc & 1 ) { return beta; }
  else          { return alpha; }
}
