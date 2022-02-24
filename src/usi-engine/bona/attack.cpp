// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <stdlib.h>
#include "shogi.h"

unsigned int CONV
is_pinned_on_white_king( const tree_t * restrict ptree, int isquare,
			int idirec )
{
  bitboard_t bb_attacks, bb_attacker;

  switch ( idirec )
    {
    case direc_rank:
      bb_attacks = AttackRank( isquare );
      if ( BBContract( bb_attacks, BB_WKING ) )
	{
	  return BBContract( bb_attacks, BB_B_RD );
	}
      break;

    case direc_file:
      bb_attacks = AttackFile( isquare );
      if ( BBContract( bb_attacks, BB_WKING ) )
	{
	  BBAnd( bb_attacker, BB_BLANCE, abb_plus_rays[isquare] );
	  BBOr( bb_attacker, bb_attacker, BB_B_RD );
	  return BBContract( bb_attacks, bb_attacker );  /* return! */
	}
      break;

    case direc_diag1:
      bb_attacks = AttackDiag1( isquare );
      if ( BBContract( bb_attacks, BB_WKING ) )
	{
	  return BBContract( bb_attacks, BB_B_BH );      /* return! */
	}
      break;

    default:
      assert( idirec == direc_diag2 );
      bb_attacks = AttackDiag2( isquare );
      if ( BBContract( bb_attacks, BB_WKING ) )
	{
	  return BBContract( bb_attacks, BB_B_BH );      /* return! */
	}
      break;
    }
  
  return 0;
}


unsigned int CONV
is_pinned_on_black_king( const tree_t * restrict ptree, int isquare,
			int idirec )
{
  bitboard_t bb_attacks, bb_attacker;

  switch ( idirec )
    {
    case direc_rank:
      bb_attacks = AttackRank( isquare );
      if ( BBContract( bb_attacks, BB_BKING ) )
	{
	  return BBContract( bb_attacks, BB_W_RD );
	}
      break;

    case direc_file:
      bb_attacks = AttackFile( isquare );
      if ( BBContract( bb_attacks, BB_BKING ) )
	{
	  BBAnd( bb_attacker, BB_WLANCE, abb_minus_rays[isquare] );
	  BBOr( bb_attacker, bb_attacker, BB_W_RD );
	  return BBContract( bb_attacks, bb_attacker );      /* return! */
	}
      break;

    case direc_diag1:
      bb_attacks = AttackDiag1( isquare );
      if ( BBContract( bb_attacks, BB_BKING ) )
	{
	  return BBContract( bb_attacks, BB_W_BH );          /* return! */
	}
      break;

    default:
      assert( idirec == direc_diag2 );
      bb_attacks = AttackDiag2( isquare );
      if ( BBContract( bb_attacks, BB_BKING ) )
	{
	  return BBContract( bb_attacks, BB_W_BH );          /* return! */
	}
      break;
    }
  return 0;
}


/* perpetual check detections are omitted. */
int CONV
is_mate_b_pawn_drop( tree_t * restrict ptree, int sq_drop )
{
  bitboard_t bb, bb_sum, bb_move;
  int iwk, ito, iret, ifrom, idirec;

  BBAnd( bb_sum, BB_WKNIGHT, abb_b_knight_attacks[sq_drop] );

  BBAndOr( bb_sum, BB_WSILVER, abb_b_silver_attacks[sq_drop] );
  BBAndOr( bb_sum, BB_WTGOLD, abb_b_gold_attacks[sq_drop] );

  AttackBishop( bb, sq_drop );
  BBAndOr( bb_sum, BB_W_BH, bb );

  AttackRook( bb, sq_drop );
  BBAndOr( bb_sum, BB_W_RD, bb );

  BBOr( bb, BB_WHORSE, BB_WDRAGON );
  BBAndOr( bb_sum, bb, abb_king_attacks[sq_drop] );

  while ( BBTest( bb_sum ) )
    {
      ifrom  = FirstOne( bb_sum );
      Xor( ifrom, bb_sum );

      if ( IsDiscoverWK( ifrom, sq_drop ) ) { continue; }
      return 0;
    }

  iwk  = SQ_WKING;
  iret = 1;
  Xor( sq_drop, BB_BOCCUPY );
  XorFile( sq_drop, OCCUPIED_FILE );
  XorDiag2( sq_drop, OCCUPIED_DIAG2 );
  XorDiag1( sq_drop, OCCUPIED_DIAG1 );
  
  BBNotAnd( bb_move, abb_king_attacks[iwk], BB_WOCCUPY );
  while ( BBTest( bb_move ) )
    {
      ito = FirstOne( bb_move );
      if ( ! is_white_attacked( ptree, ito ) )
	{
	  iret = 0;
	  break;
	}
      Xor( ito, bb_move );
    }

  Xor( sq_drop, BB_BOCCUPY );
  XorFile( sq_drop, OCCUPIED_FILE );
  XorDiag2( sq_drop, OCCUPIED_DIAG2 );
  XorDiag1( sq_drop, OCCUPIED_DIAG1 );

  return iret;
}


int CONV
is_mate_w_pawn_drop( tree_t * restrict ptree, int sq_drop )
{
  bitboard_t bb, bb_sum, bb_move;
  int ibk, ito, ifrom, iret, idirec;

  BBAnd( bb_sum, BB_BKNIGHT, abb_w_knight_attacks[sq_drop] );

  BBAndOr( bb_sum, BB_BSILVER, abb_w_silver_attacks[sq_drop] );
  BBAndOr( bb_sum, BB_BTGOLD,  abb_w_gold_attacks[sq_drop] );

  AttackBishop( bb, sq_drop );
  BBAndOr( bb_sum, BB_B_BH, bb );

  AttackRook( bb, sq_drop );
  BBAndOr( bb_sum, BB_B_RD, bb );

  BBOr( bb, BB_BHORSE, BB_BDRAGON );
  BBAndOr( bb_sum, bb, abb_king_attacks[sq_drop] );

  while ( BBTest( bb_sum ) )
    {
      ifrom  = FirstOne( bb_sum );
      Xor( ifrom, bb_sum );

      if ( IsDiscoverBK( ifrom, sq_drop ) ) { continue; }
      return 0;
    }

  ibk  = SQ_BKING;
  iret = 1;
  Xor( sq_drop, BB_WOCCUPY );
  XorFile( sq_drop, OCCUPIED_FILE );
  XorDiag2( sq_drop, OCCUPIED_DIAG2 );
  XorDiag1( sq_drop, OCCUPIED_DIAG1 );
  
  BBNotAnd( bb_move, abb_king_attacks[ibk], BB_BOCCUPY );
  while ( BBTest( bb_move ) )
    {
      ito = FirstOne( bb_move );
      if ( ! is_black_attacked( ptree, ito ) )
	{
	  iret = 0;
	  break;
	}
      Xor( ito, bb_move );
    }

  Xor( sq_drop, BB_WOCCUPY );
  XorFile( sq_drop, OCCUPIED_FILE );
  XorDiag2( sq_drop, OCCUPIED_DIAG2 );
  XorDiag1( sq_drop, OCCUPIED_DIAG1 );

  return iret;
}


int CONV
is_move_check_b( const tree_t * restrict ptree, unsigned int move )
{
  const int from = (int)I2From(move);
  const int to   = (int)I2To(move);
  int ipiece_move, idirec;
  bitboard_t bb;

  if ( from >= nsquare ) { ipiece_move = From2Drop(from); }
  else {
    ipiece_move = (int)I2PieceMove(move);
    if ( I2IsPromote(move) ) { ipiece_move += promote; }
    
    idirec = (int)adirec[SQ_WKING][from];
    if ( idirec && idirec != (int)adirec[SQ_WKING][to]
	 && is_pinned_on_white_king( ptree, from, idirec ) ) { return 1; }
  }
  
  switch ( ipiece_move )
    {
    case pawn:
      return BOARD[to-nfile] == -king;

    case lance:
      AttackBLance( bb, to );
      return BBContract( bb, BB_WKING );
      
    case knight:
      return BBContract( abb_b_knight_attacks[to], BB_WKING );
      
    case silver:
      return BBContract( abb_b_silver_attacks[to], BB_WKING );
      
    case bishop:
      AttackBishop( bb, to );
      return BBContract( bb, BB_WKING );
      
    case rook:
      AttackRook( bb, to );
      return BBContract( bb, BB_WKING );
      
    case king:
      return 0;

    case horse:
      AttackHorse( bb, to );
      return BBContract( bb, BB_WKING );
      
    case dragon:
      assert( ipiece_move == dragon );
      AttackDragon( bb, to );
      return BBContract( bb, BB_WKING );
    }
  /*
    case gold:	       case pro_pawn:
    case pro_lance:    case pro_knight:
    case pro_silver:
  */
  return BBContract( abb_b_gold_attacks[to], BB_WKING );
}


int CONV
is_move_check_w( const tree_t * restrict ptree, unsigned int move )
{
  const int from = (int)I2From(move);
  const int to   = (int)I2To(move);
  int ipiece_move, idirec;
  bitboard_t bb;

  if ( from >= nsquare ) { ipiece_move = From2Drop(from); }
  else {
    ipiece_move = (int)I2PieceMove(move);
    if ( I2IsPromote(move) ) { ipiece_move += promote; }
    
    idirec = (int)adirec[SQ_BKING][from];
    if ( idirec && idirec != (int)adirec[SQ_BKING][to]
	 && is_pinned_on_black_king( ptree, from, idirec ) ) { return 1; }
  }
  
  switch ( ipiece_move )
    {
    case pawn:
      return BOARD[to+nfile] == king;
      
    case lance:
      AttackWLance( bb, to );
      return BBContract( bb, BB_BKING );
      
    case knight:
      return BBContract( abb_w_knight_attacks[to], BB_BKING );
      
    case silver:
      return BBContract( abb_w_silver_attacks[to], BB_BKING );
      
    case bishop:
      AttackBishop( bb, to );
      return BBContract( bb, BB_BKING );
      
    case rook:
      AttackRook( bb, to );
      return BBContract( bb, BB_BKING );

    case king:
      return 0;

    case horse:
      AttackHorse( bb, to );
      return BBContract( bb, BB_BKING );
      
    case dragon:
      AttackDragon( bb, to );
      return BBContract( bb, BB_BKING );
    }

  /*
    case gold:        case pro_pawn:
    case pro_lance:   case pro_knight:
    case pro_silver:
  */
  return BBContract( abb_w_gold_attacks[to], BB_BKING );
}


bitboard_t CONV
attacks_to_piece( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb_ret, bb_attacks, bb;

  BBIni( bb_ret );
  if ( sq < rank9*nfile && BOARD[sq+nfile] == pawn )
    {
      bb_ret = abb_mask[sq+nfile];
    }
  if ( sq >= nfile && BOARD[sq-nfile] == -pawn )
    {
      BBOr( bb_ret, bb_ret, abb_mask[sq-nfile] );
    }

  BBAndOr( bb_ret, BB_BKNIGHT, abb_w_knight_attacks[sq] );
  BBAndOr( bb_ret, BB_WKNIGHT, abb_b_knight_attacks[sq] );
  BBAndOr( bb_ret, BB_BSILVER, abb_w_silver_attacks[sq] );
  BBAndOr( bb_ret, BB_WSILVER, abb_b_silver_attacks[sq] );
  BBAndOr( bb_ret, BB_BTGOLD,  abb_w_gold_attacks[sq] );
  BBAndOr( bb_ret, BB_WTGOLD,  abb_b_gold_attacks[sq] );

  BBOr( bb, BB_B_HDK, BB_W_HDK );
  BBAndOr( bb_ret, bb, abb_king_attacks[sq] );

  BBOr( bb, BB_B_BH, BB_W_BH );
  AttackBishop( bb_attacks, sq );
  BBAndOr( bb_ret, bb, bb_attacks );

  BBOr( bb, BB_B_RD, BB_W_RD );
  BBAndOr( bb_ret, bb, AttackRank( sq ) );
  BBAndOr( bb, BB_BLANCE, abb_plus_rays[sq] );
  BBAndOr( bb, BB_WLANCE, abb_minus_rays[sq] );
  BBAndOr( bb_ret, bb, AttackFile( sq ) );
  
  return bb_ret;
}


bitboard_t CONV
b_attacks_to_piece( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb_ret, bb_attacks, bb;

  BBIni( bb_ret );
  if ( sq < rank9*nfile && BOARD[sq+nfile] == pawn )
    {
      bb_ret = abb_mask[sq+nfile];
    }

  BBAndOr( bb_ret, BB_BKNIGHT, abb_w_knight_attacks[sq] );
  BBAndOr( bb_ret, BB_BSILVER, abb_w_silver_attacks[sq] );
  BBAndOr( bb_ret, BB_BTGOLD,  abb_w_gold_attacks[sq] );
  BBAndOr( bb_ret, BB_B_HDK,   abb_king_attacks[sq] );

  AttackBishop( bb_attacks, sq );
  BBAndOr( bb_ret, BB_B_BH, bb_attacks );

  bb = BB_B_RD;
  BBAndOr( bb_ret, bb, AttackRank(sq) );
  BBAndOr( bb, BB_BLANCE, abb_plus_rays[sq] );
  BBAndOr( bb_ret, bb, AttackFile(sq) );
  
  return bb_ret;
}


bitboard_t CONV
w_attacks_to_piece( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb_ret, bb_attacks, bb;

  BBIni( bb_ret );
  if ( nfile <= sq && BOARD[sq-nfile] == -pawn )
    {
      bb_ret = abb_mask[sq-nfile];
    }

  BBAndOr( bb_ret, BB_WKNIGHT, abb_b_knight_attacks[sq] );
  BBAndOr( bb_ret, BB_WSILVER, abb_b_silver_attacks[sq] );
  BBAndOr( bb_ret, BB_WTGOLD,  abb_b_gold_attacks[sq] );
  BBAndOr( bb_ret, BB_W_HDK,   abb_king_attacks[sq] );

  AttackBishop( bb_attacks, sq );
  BBAndOr( bb_ret, BB_W_BH, bb_attacks );

  bb = BB_W_RD;
  BBAndOr( bb_ret, bb, AttackRank(sq) );
  BBAndOr( bb, BB_WLANCE, abb_minus_rays[sq] );
  BBAndOr( bb_ret, bb, AttackFile(sq) );
  
  return bb_ret;
}


unsigned int CONV
is_white_attacked( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb, bb1, bb_atk;

  BBAnd  ( bb, BB_BPAWN_ATK, abb_mask[sq] );
  BBAndOr( bb, BB_BKNIGHT,   abb_w_knight_attacks[sq] );
  BBAndOr( bb, BB_BSILVER,   abb_w_silver_attacks[sq] );
  BBAndOr( bb, BB_BTGOLD,    abb_w_gold_attacks[sq] );
  BBAndOr( bb, BB_B_HDK,     abb_king_attacks[sq] );

  AttackBishop( bb_atk, sq );
  BBAndOr( bb, BB_B_BH, bb_atk );

  bb1 = BB_B_RD;
  BBAndOr( bb1, BB_BLANCE, abb_plus_rays[sq] );
  BBAndOr( bb, bb1, AttackFile( sq ) );
  BBAndOr( bb, BB_B_RD, AttackRank( sq ) );

  return BBToU(bb);
}


unsigned int CONV
is_black_attacked( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb, bb1, bb_atk;

  BBAnd  ( bb, BB_WPAWN_ATK, abb_mask[sq] );
  BBAndOr( bb, BB_WKNIGHT,   abb_b_knight_attacks[sq] );
  BBAndOr( bb, BB_WSILVER,   abb_b_silver_attacks[sq] );
  BBAndOr( bb, BB_WTGOLD,    abb_b_gold_attacks[sq] );
  BBAndOr( bb, BB_W_HDK,     abb_king_attacks[sq] );

  AttackBishop( bb_atk, sq );
  BBAndOr( bb, BB_W_BH, bb_atk );

  bb1 = BB_W_RD;
  BBAndOr( bb1, BB_WLANCE, abb_minus_rays[sq] );
  BBAndOr( bb, bb1, AttackFile( sq ) );
  BBAndOr( bb, BB_W_RD, AttackRank( sq ) );

  return BBTest(bb);
}

/*
int count_white_attacked( const tree_t * restrict ptree, int sq )
{
  bitboard_t bb, bb1, bb_atk;
  int sum = 0;

  BBAnd  ( bb, BB_BPAWN_ATK, abb_mask[sq] );              sum += (BBToU(bb) != 0);
  BBAnd  ( bb, BB_BKNIGHT,   abb_w_knight_attacks[sq] );  sum += (BBToU(bb) != 0);	// Œj”n2–‡“¯Žž‚ÉŒvŽZ‚Æ‚©‚Åƒ_ƒ‚Å‚·‚Ë
  BBAnd  ( bb, BB_BSILVER,   abb_w_silver_attacks[sq] );  sum += (BBToU(bb) != 0);
  BBAnd  ( bb, BB_BTGOLD,    abb_w_gold_attacks[sq] );    sum += (BBToU(bb) != 0);
  BBAnd  ( bb, BB_B_HDK,     abb_king_attacks[sq] );      sum += (BBToU(bb) != 0);

  AttackBishop( bb_atk, sq );
  BBAnd  ( bb, BB_B_BH, bb_atk );                         sum += (BBToU(bb) != 0);

  bb1 = BB_B_RD;
  BBAnd  ( bb1, BB_BLANCE, abb_plus_rays[sq] );           sum += (BBToU(bb) != 0);
  BBAnd  ( bb, bb1, AttackFile( sq ) );                   sum += (BBToU(bb) != 0);
  BBAnd  ( bb, BB_B_RD, AttackRank( sq ) );               sum += (BBToU(bb) != 0);

  return sum;
}
*/
