// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include "shogi.h"

int CONV
gen_next_move( tree_t * restrict ptree, int ply, int turn )
{
  switch ( ptree->anext_move[ply].next_phase )
    {
    case next_move_hash:
      {
	unsigned int * restrict pmove;
	int * restrict psortv = ptree->sort_value;
	unsigned int move, killer1, killer2, move_hash, move_best, move_second;
	int i, j, sortv, n, value_best, value_second, value, remaining;

	ptree->anext_move[ply].phase_done = 0;
	ptree->anext_move[ply].next_phase = next_move_capture;
	ptree->anext_move[ply].move_last  = pmove = ptree->move_last[ply];
	ptree->move_last[ply] = GenCaptures( turn, pmove );

	move_hash  = ptree->amove_hash[ply];
	killer1    = ptree->amove_killer[ply].no1;
	killer2    = ptree->amove_killer[ply].no2;
	remaining  = 0;
	move_best  = move_second  = 0;
	value_best = value_second = 0;
	n = (int)( ptree->move_last[ply] - pmove );
	for ( i = 0; i < n; i++ )
	  {
	    move = pmove[i];
	    sortv = swap( ptree, move, -1, INT_MAX, turn );
	    if ( sortv > value_best )
	      {
		move_second  = move_best;
		value_second = value_best;
		value_best = sortv;
		move_best  = move;
	      }
	    else if ( sortv > value_second )
	      {
		move_second  = move;
		value_second = sortv;
	      }
	    if ( move == move_hash ) { sortv = INT_MIN; }
	    else if ( UToFromToPromo(move) == killer1 )
	      {
		killer1 = 0;
		value = ptree->amove_killer[ply].no1_value
		  + p_value_ex[15U+UToCap(move)];
		if ( sortv < value ) { sortv = value; }
		if ( sortv > -1 ) { remaining++; }
	      }
	    else if ( UToFromToPromo(move) == killer2 )
	      {
		killer2 = 0;
		value = ptree->amove_killer[ply].no2_value
		  + p_value_ex[15U+UToCap(move)];
		if ( sortv < value ) { sortv = value; }
		if ( sortv > -1 ) { remaining++; }
	      }
	    else if ( sortv > -1 ) { remaining++; }
	    psortv[i] = sortv;
	  }

	if ( killer1
	     && killer1 != move_hash
	     && ptree->amove_killer[ply].no1_value > -1
	     && is_move_valid( ptree, killer1, turn ) )
	  {
	    *( ptree->move_last[ply]++ ) = killer1;
	    psortv[n++] = ptree->amove_killer[ply].no1_value;
	    remaining++;
	  }

	if ( killer2
	     && killer2 != move_hash
	     && ptree->amove_killer[ply].no2_value > -1
	     && is_move_valid( ptree, killer2, turn ) )
	  {
	    *( ptree->move_last[ply]++ ) = killer2;
	    psortv[n++] = ptree->amove_killer[ply].no2_value;
	    remaining++;
	  }

	ptree->anext_move[ply].value_cap1 = value_best;
	ptree->anext_move[ply].move_cap1  = move_best;
	ptree->anext_move[ply].value_cap2 = value_second;
	ptree->anext_move[ply].move_cap2  = move_second;
	ptree->anext_move[ply].remaining  = remaining;

	/* insertion sort */
	psortv[n] = INT_MIN;
	for ( i = n-2; i >= 0; i-- )
	  {
	    sortv = psortv[i];  move = pmove[i];
	    for ( j = i+1; psortv[j] > sortv; j++ )
	      {
		psortv[j-1] = psortv[j];  pmove[j-1] = pmove[j];
	      }
	    psortv[j-1] = sortv;  pmove[j-1] = move;
	  }
	if ( psortv[n-1] == INT_MIN ) { ptree->move_last[ply]--; }

#if ! defined(MINIMUM)
	if ( move_hash && ! is_move_valid( ptree, move_hash, turn ) )
	  {
	    out_warning( "An invalid hash move is found!!" );
	    ptree->amove_hash[ply] = move_hash = 0U;
	  }
#endif

	if ( move_hash )
	  {
	    if ( move_hash == move_best )
	      {
		ptree->anext_move[ply].phase_done |= phase_cap1;
	      }

	    if ( UToFromToPromo(move_hash) == killer1 )
	      {
		ptree->anext_move[ply].phase_done |= phase_killer1;
	      }
	    else if ( UToFromToPromo(move_hash) == killer2 )
	      { 
		ptree->anext_move[ply].phase_done |= phase_killer2;
	      }
	    ptree->anext_move[ply].phase_done |= phase_hash;
	    MOVE_CURR = move_hash;
	    return 1;
	  }
      }
      
    case next_move_capture:
      if ( ptree->anext_move[ply].remaining-- )
	{
	  unsigned int move;

	  MOVE_CURR = move = *(ptree->anext_move[ply].move_last++);
	  if ( move == ptree->anext_move[ply].move_cap1 )
	    {
	      ptree->anext_move[ply].phase_done |= phase_cap1;
	    }

	  if ( UToFromToPromo(move) == ptree->amove_killer[ply].no1 )
	    {
	      ptree->anext_move[ply].phase_done |= phase_killer1;
	    }
	  else if ( UToFromToPromo(move) == ptree->amove_killer[ply].no2 )
	    { 
	      ptree->anext_move[ply].phase_done |= phase_killer2;
	    }
	  return 1;
	}

      {
	unsigned int * restrict pmove;
	unsigned int value_best, value, key, good, tried;
	int i, n, ibest;

	value_best =  0;
	ibest      = -1;
	ptree->move_last[ply] = GenNoCaptures( turn, ptree->move_last[ply] );
	ptree->move_last[ply] = GenDrop( turn, ptree->move_last[ply] );

	n = (int)( ptree->move_last[ply] - ptree->anext_move[ply].move_last );
	pmove = ptree->anext_move[ply].move_last;
	for ( i = 0; i < n; i++ )
	  {
	    if ( pmove[i] == ptree->amove_hash[ply]
		 || ( pmove[i] == ptree->amove_killer[ply].no1
		      && ( ptree->anext_move[ply].phase_done
			   & phase_killer1 ) )
		 || ( pmove[i] == ptree->amove_killer[ply].no2
		      && ( ptree->anext_move[ply].phase_done
			   & phase_killer2 ) ) )
	      {
		pmove[i] = 0;
		continue;
	      }

	    if ( UToCap(pmove[i]) ) { continue; }
	    if ( I2IsPromote(pmove[i])
		 && I2PieceMove(pmove[i]) != silver ) { continue; }


	    key   = phash( pmove[i], turn );
	    good  = ptree->hist_good[key]  + 1;
	    tried = ptree->hist_tried[key] + 2;
	    value = ( good * 8192U ) / tried;
	    if ( value > value_best )
	      {
		value_best = value;
		ibest      = i;
	      }
	  }

	if ( ibest >= 0 )
	  {
	    ptree->anext_move[ply].phase_done |= phase_history1;
	    ptree->anext_move[ply].next_phase  = next_move_history2;
	    MOVE_CURR  = pmove[ibest];
	    pmove[ibest] = 0;
	    return 1;
	  }
      }
      
    case next_move_history2:
      {
	unsigned int * restrict pmove;
	unsigned int value_best, value, key, good, tried;
	int ibest, i, n;

	ptree->anext_move[ply].next_phase = next_move_misc;
	value_best = 0;
	ibest      = -1;
	n = (int)( ptree->move_last[ply] - ptree->anext_move[ply].move_last );
	pmove = ptree->anext_move[ply].move_last;

	for ( i = 0; i < n; i++ )
	  {
	    if ( UToCap(pmove[i]) ) { continue; }
	    if ( I2IsPromote(pmove[i])
		 && I2PieceMove(pmove[i]) != silver ) { continue; }

	    key   = phash( pmove[i], turn );
	    good  = ptree->hist_good[key]  + 1;
	    tried = ptree->hist_tried[key] + 2;
	    value = ( good * 8192U ) / tried;
	    if ( value > value_best && pmove[i] )
	      {
		value_best = value;
		ibest      = i;
	      }
	  }

	if ( ibest >= 0 )
	  {
	    ptree->anext_move[ply].phase_done |= phase_history2;
	    MOVE_CURR  = pmove[ibest];
	    pmove[ibest] = 0;
	    return 1;
	  }
      }
      
    default:
      assert( ptree->anext_move[ply].next_phase == next_move_misc );
      while ( ptree->anext_move[ply].move_last < ptree->move_last[ply] )
	{
	  if ( *( ptree->anext_move[ply].move_last ) )
	    {
	      MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	      ptree->anext_move[ply].phase_done |= phase_misc;
	      return 1;
	    }
	  ptree->anext_move[ply].move_last++;
	}
    }
  return 0;
}


int CONV
gen_next_evasion( tree_t * restrict ptree, int ply, int turn )
{
  switch ( ptree->anext_move[ply].next_phase )
    {
    case next_evasion_hash:
      ptree->move_last[ply] = GenEvasion( turn, ptree->move_last[ply] );

      if ( ptree->amove_hash[ply] )
	{
#if ! defined(MINIMUM)
	  unsigned int * restrict p;
	  
	  for ( p = ptree->move_last[ply-1]; p < ptree->move_last[ply]; p++ )
	    if ( *p == ptree->amove_hash[ply] ) { break; }
	  
	  if ( *p != ptree->amove_hash[ply] )
	    {
	      out_warning( "An invalid hash evasion-move is found!!" );
	      out_board( ptree, stdout, 0, 0 );
	      Out( "%c%s\n", ach_turn[turn],
		   str_CSA_move(ptree->amove_hash[ply]) );
	      Out( "hash key = %" PRIu64 ", hand = %u, turn = %d\n",
		   HASH_KEY, HAND_B, turn );
	      ptree->amove_hash[ply] = 0U;
	    }
	  else
#endif
	    {
	      ptree->anext_move[ply].next_phase = next_evasion_genall;
	      ptree->current_move[ply]          = ptree->amove_hash[ply];
	      return 1;
	    }
	}

    case next_evasion_genall:
      {
	unsigned int * restrict pmove;
	int * restrict psortv;
	unsigned int move;
	int n, i, j, sortv;

	ptree->anext_move[ply].next_phase = next_evasion_misc;
	ptree->anext_move[ply].move_last = pmove = ptree->move_last[ply-1];
	n = (int)( ptree->move_last[ply] - pmove );
	psortv = ptree->sort_value;

	for ( i = n-1; i >= 0; i-- )
	  {
	    move = pmove[i];
	    if ( move == ptree->amove_hash[ply] )
	      {
		sortv    = INT_MIN;
		pmove[i] = 0;
	      }
	    else if ( I2PieceMove(move) == king )
	      {
		sortv = p_value_ex[UToCap(move)+15] * 2;
	      }
	    else {
	      sortv  = swap( ptree, move, INT_MIN, INT_MAX, turn );
	      sortv += estimate_score_diff( ptree, move, turn );
	    }
	    psortv[i] = sortv;
	  }

	/* insertion sort */
	psortv[n] = INT_MIN;
	for ( i = n-2; i >= 0; i-- )
	  {
	    sortv = psortv[i];  move = pmove[i];
	    for ( j = i+1; psortv[j] > sortv; j++ )
	      {
		psortv[j-1] = psortv[j];  pmove[j-1] = pmove[j];
	      }
	    psortv[j-1] = sortv;  pmove[j-1] = move;
	  }
      }

    default:
      assert( ptree->anext_move[ply].next_phase == next_evasion_misc );
      while ( ptree->anext_move[ply].move_last < ptree->move_last[ply] )
	{
	  if ( *( ptree->anext_move[ply].move_last ) )
	    {
	      MOVE_CURR = *(ptree->anext_move[ply].move_last++);
	      return 1;
	    }
	  ptree->anext_move[ply].move_last++;
	}
    }
  return 0;
}
