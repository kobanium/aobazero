// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "shogi.h"


int CONV
is_move_valid( tree_t * restrict __ptree__, unsigned int move, int turn )
{
  tree_t * restrict ptree = __ptree__;
  int from = (int)I2From(move);
  int to   = (int)I2To(move);
  int piece_move;
  unsigned int u;
  bitboard_t bb;

  if ( from < nsquare )
    {
      piece_move = (int)I2PieceMove(move);
      if ( turn )
	{
	  if ( BOARD[from] != -piece_move )       { return 0; }
	  if ( BOARD[to]   != (int)UToCap(move) ) { return 0; }
	}
      else {
	if ( BOARD[from] !=  piece_move )        { return 0; }
	if ( BOARD[to]   != -(int)UToCap(move) ) { return 0; }
      }
      
      switch ( piece_move )
	{
	case 0:  return 0;

	case lance:  case bishop:  case horse:  case rook:  case dragon:
	  BBOr( bb, BB_BOCCUPY, BB_WOCCUPY );
	  BBAnd( bb, bb, abb_obstacle[from][to] );
	  if ( BBTest( bb ) ) { return 0; }
	  break;
	}

      return 1;
    }

  if ( BOARD[to] ) { return 0; }
  else {
    u = turn ? HAND_W : HAND_B;
    switch ( From2Drop(from) )
      {
      case pawn:
	if ( ! IsHandPawn(u) ) { return 0; }
	{
	  if ( turn )
	    {
	      u = BBToU( BB_WPAWN_ATK );
	      if ( ( mask_file1 >> aifile[to] ) & u ) { return 0; }
	      if ( IsMateWPawnDrop(__ptree__, to) )   { return 0; }
	    }
	  else {
	    u = BBToU( BB_BPAWN_ATK );
	    if ( ( mask_file1 >> aifile[to] ) & u ) { return 0; }
	    if ( IsMateBPawnDrop(__ptree__, to) )   { return 0; }
	  }
	  return 1;
	}
      case lance:   if ( IsHandLance(u) )  { return 1; }  break;
      case knight:  if ( IsHandKnight(u) ) { return 1; }  break;
      case silver:  if ( IsHandSilver(u) ) { return 1; }  break;
      case gold:    if ( IsHandGold(u) )   { return 1; }  break;
      case bishop:  if ( IsHandBishop(u) ) { return 1; }  break;
      default:      assert( From2Drop(from) == rook );
                    if ( IsHandRook(u) )   { return 1; }  break;
      }
  }
  return 0;
}


#define NpchkReturn(piece) if ( (n ## piece) > (n ## piece ## _max) ) {  \
                             str_error = "too many " # piece "s";        \
                             return -2; }

int CONV
exam_tree( const tree_t * restrict ptree )
{
  int npawn, nlance, nknight, nsilver, ngold, nbishop, nrook;
  int nwking, nbking, isquare, ifile, irank, wcounter, bcounter;

  /* total number of each piece */
  npawn   = (int)(I2HandPawn( HAND_B )   + I2HandPawn( HAND_W ));
  nlance  = (int)(I2HandLance( HAND_B )  + I2HandLance( HAND_W ));
  nknight = (int)(I2HandKnight( HAND_B ) + I2HandKnight( HAND_W ));
  nsilver = (int)(I2HandSilver( HAND_B ) + I2HandSilver( HAND_W ));
  ngold   = (int)(I2HandGold( HAND_B )   + I2HandGold( HAND_W ));
  nbishop = (int)(I2HandBishop( HAND_B ) + I2HandBishop( HAND_W ));
  nrook   = (int)(I2HandRook( HAND_B )   + I2HandRook( HAND_W ));
  nwking = nbking = 0;

  for ( isquare = 0; isquare < nsquare; isquare++ )
    switch ( abs( BOARD[isquare] ) )
      {
      case empty:                                 break;
      case pawn:    case pro_pawn:    npawn++;    break;
      case lance:   case pro_lance:   nlance++;   break;
      case knight:  case pro_knight:  nknight++;  break;
      case silver:  case pro_silver:  nsilver++;  break;
      case gold:                      ngold++;    break;
      case bishop:  case horse:       nbishop++;  break;
      case rook:    case dragon:      nrook++;    break;
      case king:
	if ( BOARD[isquare] == king ) { nbking++; }
	else                            { nwking++; }
	break;
      }
  NpchkReturn( pawn );    NpchkReturn( lance );
  NpchkReturn( knight );  NpchkReturn( silver );
  NpchkReturn( gold );    NpchkReturn( bishop );
  NpchkReturn( rook );
  if ( nbking != 1 || nwking != 1 )
    {
      str_error = "invalid number of kings";
      return -2;
    }

  /* double pawns */
  for ( ifile = 0; ifile < 9; ifile++ )
    {
      bcounter = wcounter = 0;
      for ( irank = 0; irank < 9; irank++ )
	{
	  if ( BOARD[ irank*nfile+ifile ] ==  pawn ) { bcounter++; }
	  if ( BOARD[ irank*nfile+ifile ] == -pawn ) { wcounter++; }
	}
      if ( bcounter > 1 )
	{
	  str_error = "two black pawns at a file";
	  return -2;
	}
      if ( wcounter > 1 )
	{
	  str_error="two white pawns at a file";
	  return -2;
	}
    }

  /* pieces can not move */
  for ( isquare = 0; isquare < 9; isquare++ )
    {
      if ( BOARD[ isquare ] == pawn )
	{
	  str_error = "black pawns in rank 1";
	  return -2;
	}
      if ( BOARD[ isquare ] == lance )
	{
	  str_error = "black lances in rank 1";
	  return -2;
	}
    }
  for ( isquare = 0; isquare < 18; isquare++ )
    if ( BOARD[ isquare ] == knight )
      {
	str_error = "black knights in rank 1-2";
	return -2;
      }

  for ( isquare = 72; isquare < 81; isquare++ )
    {
      if ( BOARD[ isquare ] == -pawn )
	{
	  str_error = "white pawns in rank 9";
	  return -2;
	}
      if ( BOARD[ isquare ] == -lance )
	{
	  str_error = "white lances in rank 9";
	  return -2;
	}
    }
  for ( isquare = 63; isquare < 81; isquare++ )
    if ( BOARD[ isquare ] == -knight )
      {
	str_error = "white knights in rank 8-9";
	return -2;
      }

  if ( InCheck( Flip(root_turn) ) )
    {
      str_error = str_king_hang;
      return -2;
    }

  assert( exam_bb( ptree ) );

  return 1;
}

#undef NpchkReturn
