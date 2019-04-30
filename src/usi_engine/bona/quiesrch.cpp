// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "shogi.h"

/* #define DBG_QSEARCH */
#if defined(DBG_QSEARCH)
#  define DOut( ... )  if ( dbg_flag ) { out( __VA_ARGS__ ); }
#else
#  define DOut( ... )
#endif

static int CONV gen_next_quies( tree_t * restrict ptree, int alpha, int turn,
				int ply, int qui_ply );

int CONV
search_quies( tree_t * restrict ptree, int alpha, int beta, int turn, int ply,
	      int qui_ply )
{
  int value, alpha_old, stand_pat;

#if defined(DBG_QSEARCH)
  int dbg_flag = 0;

  if ( iteration_depth == 2 && ply == 4
       && ! strcmp( str_CSA_move(ptree->current_move[1]), "7776FU" )
       && ! strcmp( str_CSA_move(ptree->current_move[2]), "3334FU" )
       && ! strcmp( str_CSA_move(ptree->current_move[3]), "8822UM" ) )
    {
      dbg_flag = 1;
      Out( "qsearch start (alpha=%d beta=%d sp=%d %" PRIu64 ")",
	   alpha, beta, value, ptree->node_searched );
    }
#endif
  

#if defined(TLP)
  if ( ! ptree->tlp_id )
#endif
    {
      node_last_check += 1;
    }
  ptree->node_searched += 1;
  ptree->nquies_called += 1;
  alpha_old             = alpha;
  
  stand_pat = evaluate( ptree, ply, turn );


  if ( alpha < stand_pat )
    {
      if ( beta <= stand_pat )
	{
	  DOut( ", cut by stand-pat\n" );
	  MOVE_CURR = MOVE_PASS;
	  return stand_pat;
	}
      alpha = stand_pat;
    }

  if ( ply >= PLY_MAX-1 )
    {
      if ( alpha_old != alpha ) { pv_close( ptree, ply, no_rep ); }
      MOVE_CURR = MOVE_NA;
      return stand_pat;
    }


  if ( is_mate_in3ply( ptree, turn, ply ) )
    {
      value = score_mate1ply + 1 - ply;
      
      if ( alpha < value
	   && value < beta ) { pv_close( ptree, ply, mate_search ); }

      assert( is_move_valid( ptree, MOVE_CURR, turn ) );
      return value;
    }


  ptree->anext_move[ply].next_phase = next_quies_gencap;
  while ( gen_next_quies( ptree, alpha, turn, ply, qui_ply ) )
    {
      DOut( "\nexpand %s (%" PRIu64 ")",
	    str_CSA_move(MOVE_CURR), ptree->node_searched );

      MakeMove( turn, MOVE_CURR, ply );
      if ( InCheck(turn) )
	{
	  UnMakeMove( turn, MOVE_CURR, ply );
	  continue;
	}

      value = -search_quies( ptree, -beta, -alpha, Flip(turn), ply+1,
			     qui_ply+1 );

      UnMakeMove( turn, MOVE_CURR, ply );

      if ( alpha < value )
	{
	  check_futile_score_quies( ptree, MOVE_CURR, ptree->save_eval[ply],
				    -ptree->save_eval[ply+1], turn );
	  if ( beta <= value )
	    {
	      DOut( ", beta cut (%" PRIu64 ")\n", ptree->node_searched );

	      assert( ! IsMove(MOVE_CURR)
		      || is_move_valid( ptree, MOVE_CURR, turn ) );
	      return value;
	    }

	  DOut( ", renew alpha=%d (%" PRIu64 ")\n",
		value, ptree->node_searched );
	  alpha = value;
	}
    }

  DOut( "\nall searched (%" PRIu64 ")\n", ptree->node_searched );

  if ( alpha_old != alpha )
    {
      if ( alpha == stand_pat ) { pv_close( ptree, ply, no_rep ); }
      else                      { pv_copy( ptree, ply ); }
    }

  return alpha;
}


static int CONV
gen_next_quies( tree_t * restrict ptree, int alpha, int turn, int ply,
		int qui_ply )
{
  switch ( ptree->anext_move[ply].next_phase )
    {
    case next_quies_gencap:
      { 
	unsigned int * restrict pmove;
	int * restrict psortv;
	int i, j, n, nqmove, value, min_score, diff;
	unsigned int move;
	  
	ptree->move_last[ply] = GenCaptures( turn, ptree->move_last[ply-1] );

	/* set sort values */
	pmove  = ptree->move_last[ply-1];
	psortv = ptree->sort_value;
	nqmove = 0;
	n      = (int)( ptree->move_last[ply] - pmove );
	
	for ( i = 0; i < n; i++ )
	  {
	    move = pmove[i];

	    if ( qui_ply >= QUIES_PLY_LIMIT
		 && ( ( UToCap(move) == pawn && ! I2IsPromote(move) )
		      || ( ! UToCap(move) && I2PieceMove(move) != pawn ) ) )
	      {
		continue;
	      }

	    diff      = estimate_score_diff( ptree, move, turn );
	    min_score = eval_max_score( ptree, move, ptree->save_eval[ply],
					turn, diff );

	    if ( alpha < min_score )
	      {
		value = swap( ptree, move, -1, MT_CAP_SILVER, turn );
		if ( -1 < value )
		  {
		    psortv[nqmove]  = value + diff;
		    pmove[nqmove++] = move;
		  }
	      }
	  }
	
	/* insertion sort */
	psortv[nqmove] = INT_MIN;
	for ( i = nqmove-2; i >= 0; i-- )
	  {
	    value = psortv[i];  move = pmove[i];
	    for ( j = i+1; psortv[j] > value; j++ )
	      {
		psortv[j-1] = psortv[j];  pmove[j-1] = pmove[j];
	      }
	    psortv[j-1] = value;  pmove[j-1] = move;
	  }

	ptree->move_last[ply]             = ptree->move_last[ply-1] + nqmove;
	ptree->anext_move[ply].move_last  = pmove;
	ptree->anext_move[ply].next_phase = next_quies_captures;
      }
      
    case next_quies_captures:
      if ( ptree->anext_move[ply].move_last != ptree->move_last[ply] )
	{
	  MOVE_CURR = *ptree->anext_move[ply].move_last++;
	  return 1;
	}
    }

  return 0;
}
