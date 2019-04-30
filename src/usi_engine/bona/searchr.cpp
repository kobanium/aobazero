// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "shogi.h"

static int CONV save_result( tree_t * restrict ptree, int value, int beta,
			     int turn );

#if defined(MPV)
static int CONV mpv_set_bound( int alpha );
static int CONV mpv_find_min( int *pnum );
static int CONV mpv_add_result( tree_t * restrict ptree, int value );
static void CONV mpv_sub_result( unsigned int move );
static void CONV mpv_out( tree_t * restrict ptree, int turn,
			  unsigned int time );
#endif

#if defined(NO_STDOUT) && defined(NO_LOGGING)
#  define NextRootMove(a,b) next_root_move(a)
static int CONV next_root_move( tree_t * restrict ptree );
#else
#  define NextRootMove(a,b) next_root_move(a,b)
static int CONV next_root_move( tree_t * restrict ptree, int turn );
#endif

static int CONV
search_wrapper( tree_t * restrict ptree, int alpha, int beta, int turn,
		int depth, int ply, unsigned int state_node )
{
  int value;

#if defined(DFPN_CLIENT)
  /* try beta cut using results from DFPN server */
  if ( root_move_list[root_index].dfpn_cresult == dfpn_client_win )
    {
      if ( ! root_index ) { Out( "- best move is proofed, skip.\n" ); }
      ptree->pv[1].a[1]   = MOVE_LAST;
      ptree->pv[1].length = 1;
      ptree->pv[1].depth  = 0;
      ptree->pv[1].type   = no_rep;
      MOVE_CURR           = MOVE_NA;
      return score_matelong;
    }
#endif
  
  value = search( ptree, alpha, beta, turn, depth, ply, state_node );

#if defined(DFPN_CLIENT)
  /* catch a signal, i.e., the first move has to be ignored */
  if ( game_status & flag_skip_root_move )
    {
      if ( root_index )
	{
	  Out( "- %s is proofed while searching.\n", str_CSA_move(MOVE_LAST) );
	}
      else {
	Out( "- %s (best move) is proofed while searching.\n",
	     str_CSA_move(MOVE_LAST) );
      }

      game_status &= ~flag_skip_root_move;
      root_abort          = 0;

      ptree->pv[1].a[1]   = MOVE_LAST;
      ptree->pv[1].length = 1;
      ptree->pv[1].depth  = 0;
      ptree->pv[1].type   = no_rep;
      MOVE_CURR           = MOVE_NA;
      return score_matelong;
    }
#endif

  return value;
}


int CONV
searchr( tree_t * restrict ptree, int alpha, int beta, int turn, int depth )
{
  uint64_t root_nodes_start;
  int value, first_move;
  int new_depth, extension, ply, state_node_new;
#if defined(MPV)
  int bound = INT_MIN;
#endif
#if defined(USI)
  char str_usi[6];
#endif

  first_move          = 1;
  ply                 = 1;
  ptree->move_last[1] = ptree->move_last[0];

  while( NextRootMove( ptree, turn ) )
    {
      root_nodes_start = ptree->node_searched;
#if defined(USI)
      if ( usi_mode != usi_off )
	{
	  csa2usi( ptree, str_CSA_move(MOVE_CURR), str_usi );
	}
#endif

      MakeMove( turn, MOVE_CURR, 1 );

      assert( ! InCheck( turn ) );

      state_node_new = ( node_do_mate | node_do_null | node_do_futile
			 | node_do_recap | node_do_recursion
			 | node_do_hashcut );
      if ( InCheck(Flip(turn)) )
	{
	  ptree->check_extension_done++;
	  ptree->nsuc_check[2] = (unsigned char)( ptree->nsuc_check[0] + 1U );
	  extension            = EXT_CHECK - PLY_INC;
	}
      else {
	extension            = - PLY_INC;
	ptree->nsuc_check[2] = 0;
      }
      
      if ( first_move )
	{
#if defined(MPV)
	  if ( root_mpv ) { bound = alpha; }
#endif
	  new_depth = depth + extension;
	  value = -search_wrapper( ptree, -beta, -alpha, Flip(turn),
				   new_depth, 2, state_node_new );
	  if ( root_abort )
	    {
	      UnMakeMove( turn, MOVE_CURR, 1 );
	      return 0;
	    }

	  if ( value <= alpha )
	    {
	      UnMakeMove( turn, MOVE_CURR, 1 );
	      return value;
	    }

	  first_move = 0;
	}
#if defined(MPV)
      else if ( root_mpv )
	{
	  bound = mpv_set_bound( alpha );
	  if ( depth + extension >= PLY_INC || ptree->nsuc_check[2] )
	    {
	      new_depth = depth + extension;
	      
	      value = -search_wrapper( ptree, -bound-1, -bound, Flip(turn),
				       new_depth, 2, state_node_new );
	      if ( ! root_abort && bound < value )
		{
		  new_depth = depth + extension;
		  value = -search_wrapper( ptree, -beta, -bound, Flip(turn),
					   new_depth, 2, state_node_new );
		}
	      if ( root_abort )
		{
		  UnMakeMove( turn, MOVE_CURR, 1 );
		  return 0;
		}
	    }
	  else {
	    value = -search_quies( ptree, -beta, -bound, Flip(turn), 2, 1 );
	  }
	}
#endif /* MPV */
      else {
        int depth_reduced = 0;
        new_depth = depth + extension;

        /* reductions */
        if ( 2*PLY_INC <= new_depth
             && ! ptree->nsuc_check[ply]
             && ! UToCap(MOVE_CURR)
             && ! ( I2IsPromote(MOVE_CURR)
                    && I2PieceMove(MOVE_CURR) != silver ) )
          {
            unsigned int key     = phash( MOVE_CURR, turn );
            unsigned int good    = ptree->hist_good[key]  + 1;
            unsigned int triedx8 = ( ptree->hist_tried[key] + 2 ) * 8U;

            if      ( good *160U < triedx8 ) { depth_reduced = PLY_INC * 3/2; }
            else if ( good * 50U < triedx8 ) { depth_reduced = PLY_INC * 2/2; }
            else if ( good * 19U < triedx8 ) { depth_reduced = PLY_INC * 1/2; }

            depth_reduced = PLY_INC;
            new_depth    -= depth_reduced;
          }

        value = -search_wrapper( ptree, -alpha-1, -alpha, Flip(turn),
				 new_depth, 2, state_node_new );
        if ( ! root_abort && alpha < value && depth_reduced )
          {
            new_depth += depth_reduced;
            value = -search_wrapper( ptree, -alpha-1, -alpha, Flip(turn),
				     new_depth, 2, state_node_new );
          }
	if ( root_abort )
	  {
	    UnMakeMove( turn, MOVE_CURR, 1 );
	    return 0;
	  }

	if ( alpha < value )
	  {
#if defined(DFPN_CLIENT)
	    if ( dfpn_client_sckt != SCKT_NULL
		 && 4 < iteration_depth
		 && dfpn_client_best_move != MOVE_CURR )
	      {
		dfpn_client_best_move = MOVE_CURR;
		lock( &dfpn_client_lock );
		dfpn_client_out( "BEST MOVE %s\n",
				 str_CSA_move(dfpn_client_best_move) );
		unlock( &dfpn_client_lock );
	      }
#endif
	    MnjOut( "pid=%d move=%s v=%dl n=% " PRIu64 "%s\n",
		    mnj_posi_id, str_CSA_move(MOVE_CURR), alpha+1,
		    ptree->node_searched,
		    ( mnj_depth_stable <= iteration_depth ) ? " stable" : "" );

#if defined(USI)
	    if ( usi_mode != usi_off )
	      {
		USIOut( "info depth %d score cp %d nodes %" PRIu64 " pv %s\n",
			iteration_depth, alpha+1, ptree->node_searched,
			str_usi );
	      }
#endif

	    new_depth = depth + extension;
	    easy_abs  = 0;
	    value = -search_wrapper( ptree, -beta, -alpha, Flip(turn),
				     new_depth, 2, state_node_new );
	    if ( root_abort )
	      {
		const char *str;
		double dvalue;
		char ch;
		
		UnMakeMove( turn, MOVE_CURR, 1 );
		pv_close( ptree, 2, pv_fail_high );
		time_last_result     = time_last_check;
		root_value           = alpha+1;
		ptree->pv[0]         = ptree->pv[1];
		if ( turn )
		  {
		    dvalue = -(double)alpha;
		    ch     = '-';
		  }
		else {
		  dvalue = alpha;
		  ch     = '+';
		}
		str = str_CSA_move(MOVE_CURR);
		Out( "           %7.2f  1.%c%s [%c0!]\n",
		     dvalue / 100.0, ch, str, ch );
		if ( game_status & flag_pondering )
		  {
		    OutCsaShogi( "info%+.2f %c%s %c%s [%c0!]\n",
				 dvalue / 100.0, ach_turn[Flip(turn)],
				 str_CSA_move(ponder_move),
				 ch, str, ch );
		  }
		else {
		  OutCsaShogi( "info%+.2f %c%s [%c0!]\n",
			       dvalue / 100.0, ch, str, ch );
		}
		return 0;
	      }
	  }
      }

      UnMakeMove( turn, MOVE_CURR, 1 );

      root_move_list[root_index].nodes
	= ptree->node_searched - root_nodes_start;

#if defined(MPV)
      if ( root_mpv && value < beta )
	{
	  mpv_sub_result( MOVE_CURR );
	  if ( bound < value && mpv_add_result( ptree, value ) < 0 )
	    {
	      game_status |= flag_search_error;
	      return 0;
	    }
	}
#endif
      
      if ( alpha < value )
	{
	  if ( save_result( ptree, value, beta, turn ) < 0 )
	    {
	      game_status |= flag_search_error;
	      return 0;
	    }
	  if ( beta <= value ) { return value; }
	  alpha = value;
	}
    }
  if ( root_abort ) { return 0; }

  return alpha;
}


void CONV
out_pv( tree_t * restrict ptree, int value, int turn, unsigned int time )
{
#if defined(USI)
  char str_pv[256];
  int ipv;
#endif
  const char *str;
  double dvalue;
  int ply, tt, is_out;

  tt     = turn;
  is_out = ( 4 < iteration_depth || abs(value) > score_max_eval ) ? 1 : 0;

#if defined(MPV)
  if ( root_mpv ) { is_out = 0; }
#endif

#if defined(USI)
  if ( usi_mode != usi_off )
    {
      is_out    = 1;
      ipv       = 0;
      str_pv[0] = '\0';
    }
#endif

  if ( is_out )
    {
      str    = str_time_symple( time );
      dvalue = (double)( turn ? -value : value ) / 100.0;
      OutCsaShogi( "info%+.2f", dvalue );
      if ( game_status & flag_pondering )
	{
	  OutCsaShogi( " %c%s", ach_turn[Flip(turn)],
		       str_CSA_move(ponder_move) );
	}

      if ( ptree->pv[0].length )
	{
	  if ( root_move_list[root_index].status & flag_first )
	    {
	      Out( " %2d %6s %7.2f ", iteration_depth, str, dvalue );
	    }
	  else { Out( "    %6s %7.2f ", str, dvalue ); }
	}
    }

  for ( ply = 1; ply <= ptree->pv[0].length; ply++ )
    {
      if ( is_out )
	{
	  if ( ply > 1 && ! ( (ply-1) % 5 ) )
	    {
	      Out( "\n                   " );
	    }
	  str = str_CSA_move( ptree->pv[0].a[ply] );
	  OutCsaShogi( " %c%s", ach_turn[tt], str );
	  Out( "%2d.%c%-7s", ply, ach_turn[tt], str );

#if defined(USI)
	  if ( usi_mode != usi_off && ply <= 4 )
	    {
	      char str_usi[6];
	      csa2usi( ptree, str_CSA_move(ptree->pv[0].a[ply]), str_usi );
	      ipv += snprintf( str_pv + ipv, 256 - ipv, " %s", str_usi );
	    }
#endif
	}

      MakeMove( tt, ptree->pv[0].a[ply], ply );
      tt    = Flip(tt);
      value = -value;
    }

  if ( ptree->pv[0].type == hash_hit )
    {
      unsigned int dummy;
      int i, value_type;

      for ( ; ply < PLY_MAX; ply++ )
	{
	  dummy = 0;
	  ptree->amove_hash[ply] = 0;
	  value_type = hash_probe( ptree, ply, 0, tt, -score_bound,
				   score_bound, &dummy );
	  if ( ! ( value_type == value_exact
		   && value   == HASH_VALUE
		   && is_move_valid( ptree, ptree->amove_hash[ply], tt ) ) )
	    {
	      break;
	    }
	  ptree->pv[0].a[ply] = ptree->amove_hash[ply];
	  for ( i = 1; i < ply; i++ )
	    if ( ptree->pv[0].a[i] == ptree->pv[0].a[ply] ) { goto rep_esc; }
	  
	  if ( is_out )
	    {
	      if ( ply > 1 && ! ( (ply-1) % 5 ) )
		{
		  Out( "\n                   " );
		}
	      str = str_CSA_move(ptree->pv[0].a[ply]);
	      OutCsaShogi( " %c%s", ach_turn[tt], str );
	      Out( "%2d:%c%-7s", ply, ach_turn[tt], str );

#if defined(USI)
	      if ( usi_mode != usi_off && ply <= 4 )
		{
		  char str_usi[6];
		  csa2usi( ptree, str_CSA_move(ptree->pv[0].a[ply]), str_usi );
		  ipv += snprintf( str_pv + ipv, 256 - ipv, " %s", str_usi );
		}
#endif
	    }

	  MakeMove( tt, ptree->pv[0].a[ply], ply );
	  if ( InCheck(tt) )
	    {
	      UnMakeMove( tt, ptree->amove_hash[ply], ply );
	      break;
	    }
	  ptree->pv[0].length++;
	  tt    = Flip(tt);
	  value = -value;
	}
    }
 rep_esc:

  if ( is_out && ptree->pv[0].type != no_rep )
    {
      if ( (((ply-1) % 5) == 0) && (ply != 1) )
	{
	  Out( "\n                   " );
	}
      str = NULL;
      switch ( ptree->pv[0].type )
	{
	case perpetual_check:  str = "PER. CHECK";     break;
	case four_fold_rep:    str = "REPETITION";     break;
	case black_superi_rep:
	case white_superi_rep: str = "SUPERI. POSI.";  break;
	case prev_solution:    str = "PREV. SEARCH";   break;
	case hash_hit:         str = "HASH HIT";       break;
	case book_hit:         str = "BOOK HIT";       break;
	case pv_fail_high:     str = "FAIL HIGH";      break;
	case mate_search:      str = "MATE SEARCH";    break;
	}
      if ( str != NULL ) { Out( " <%s>", str ); }
    }
  for ( ply--; ply >= 1; ply-- )
    {
      tt    = Flip(tt);
      value = -value;
      UnMakeMove( tt, ptree->pv[0].a[ply], ply );
    }

  if ( is_out )
    {
      OutCsaShogi( "\n" );
      Out( "\n" );
      USIOut( "info depth %d score cp %d nodes %" PRIu64 " pv%s\n",
	      iteration_depth, value, ptree->node_searched, str_pv );
    }
} 


static int CONV
save_result( tree_t * restrict ptree, int value, int beta, int turn )
{
  root_move_t root_move_temp;
  int index;

  if ( get_elapsed( &time_last_result ) < 0 ) { return -1; }

  index = root_index;
  if ( index )
    {
      root_move_temp = root_move_list[index];
      for ( ; index > 0; index-- )
	{
	  root_move_list[index] = root_move_list[index-1];
	}
      root_move_list[0] = root_move_temp;
    }
  root_index = 0;
  if ( beta <= value ) { pv_close( ptree, 2, pv_fail_high ); }

  ptree->pv[0] = ptree->pv[1];
  root_value   = value;

  if ( value < beta )
    {
#if defined(DFPN_CLIENT)
      if ( dfpn_client_sckt != SCKT_NULL
	   && 4 < iteration_depth
	   && dfpn_client_best_move != ptree->pv[1].a[1] )
	{
	  dfpn_client_best_move = ptree->pv[1].a[1];
	  lock( &dfpn_client_lock );
	  dfpn_client_out( "BEST MOVE %s\n",
			   str_CSA_move(dfpn_client_best_move) );
	  unlock( &dfpn_client_lock );
	}
#endif
      MnjOut( "pid=%d move=%s v=%de n=%" PRIu64 "%s\n",
	      mnj_posi_id, str_CSA_move(ptree->pv[1].a[1]), value,
	      ptree->node_searched,
	      ( mnj_depth_stable <= iteration_depth ) ? " stable" : "" );

      out_pv( ptree, value, turn, time_last_result - time_start );
    }

  return 1;
}


static int CONV
#if defined(NO_STDOUT) && defined(NO_LOGGING)
next_root_move( tree_t * restrict ptree )
#else
next_root_move( tree_t * restrict ptree, int turn )
#endif
{
  int i, n;

  n = root_nmove;
  for ( i = 0; i < n; i++ )
    {
      if ( root_move_list[i].status & flag_searched ) { continue; }
      root_move_list[i].status |= flag_searched;
      ptree->current_move[1]    = root_move_list[i].move;
      root_index                = i;

#if ! ( defined(NO_STDOUT) && defined(NO_LOGGING) )
      if ( iteration_depth > 5 )
	{
	  const char *str_move;
	  char str[9];

	  str_move = str_CSA_move(ptree->current_move[1]);
	  snprintf( str, 9, "%d/%d", i+1, root_nmove );
	  if ( root_move_list[i].status & flag_first )
	    {
	      Out( "(%2d)       %7s* 1.%c%s     \r",
		   iteration_depth, str, ach_turn[turn], str_move );
	    }
	  else {
	    Out( "           %7s* 1.%c%s     \r",
		 str, ach_turn[turn], str_move );
	  }
	}
#endif
      
      return 1;
    }
  
  return 0;
}


#if defined(MPV)
static void CONV
mpv_out( tree_t * restrict ptree, int turn, unsigned int time )
{
  int mpv_out, ipv, best;

  best    = (int)mpv_pv[0].a[0] - 32768;
  mpv_out = ( iteration_depth > 3 || abs(best) > score_max_eval ) ? 1 : 0;

  for ( ipv = 0; mpv_pv[ipv].length; ipv++ )
    {
      const char *str;
      double dvalue;
      int tt, is_out, value, ply;

      assert( ipv < mpv_num*2 );
      tt     = turn;
      value  = (int)mpv_pv[ipv].a[0] - 32768;
      if ( mpv_out && value > best - mpv_width && ipv < mpv_num )
	{
	  is_out = 1;
	}
      else { is_out = 0; }

      if ( is_out )
	{
	  dvalue = (double)( turn ? -value : value ) / 100.0;
	  if ( is_out && ! ipv ) { OutCsaShogi( "info" ); }
	  if ( is_out && ipv )   { OutCsaShogi( ":" ); }

	  OutCsaShogi( "%+.2f", dvalue );
	  if ( game_status & flag_pondering )
	    {
	      OutCsaShogi( " %c%s", ach_turn[Flip(turn)],
			   str_CSA_move(ponder_move) );
	    }

	  str = str_time_symple( time );
	  ply = mpv_pv[ipv].depth;
	  if ( ! ipv ) { Out( "o%2d %6s %7.2f ", ply, str, dvalue ); }
	  else         { Out( " %2d        %7.2f ", ply, dvalue ); }
	}

      for ( ply = 1; ply <= mpv_pv[ipv].length; ply++ )
	{
	  if ( is_out )
	    {
	      if ( ply > 1 && ! ( (ply-1) % 5 ) )
		{
		  Out( "\n                   " );
		}
	      str = str_CSA_move(mpv_pv[ipv].a[ply]);
	      OutCsaShogi( " %c%s", ach_turn[tt], str );
	      Out( "%2d.%c%-11s", ply, ach_turn[tt], str );
	    }
	  
	  assert( is_move_valid( ptree, mpv_pv[ipv].a[ply], tt ) );
	  MakeMove( tt, mpv_pv[ipv].a[ply], ply );
	  tt    = Flip(tt);
	  value = -value;
	}

      if ( mpv_pv[ipv].type == hash_hit )
	{
	  unsigned int dummy;
	  int i, value_type;

	  for ( ; ply < PLY_MAX; ply++ )
	    {
	      dummy = 0;
	      ptree->amove_hash[ply] = 0;
	      value_type = hash_probe( ptree, ply, 0, tt, -score_bound,
				       score_bound, &dummy );
	      if ( ! ( value_type == value_exact
		       && value   == HASH_VALUE
		       && is_move_valid(ptree,ptree->amove_hash[ply],tt) ) )
		{
		  break;
		}
	      mpv_pv[ipv].a[ply] = ptree->amove_hash[ply];
	      for ( i = 1; i < ply; i++ )
		if ( mpv_pv[ipv].a[i]
		     == mpv_pv[ipv].a[ply] ) { goto rep_esc; }
	      
	      if ( is_out )
		{
		  if ( ply > 1 && ! ( (ply-1) % 5 ) )
		    {
		      Out( "\n                   " );
		    }
		  str = str_CSA_move(mpv_pv[ipv].a[ply]);
		  OutCsaShogi( " %c%s", ach_turn[tt], str );
		  Out( "%2d:%c%-11s", ply, ach_turn[tt], str );
		}

	      MakeMove( tt, mpv_pv[ipv].a[ply], ply );
	      if ( InCheck(tt) )
		{
		  UnMakeMove( tt, ptree->amove_hash[ply], ply );
		  break;
		}
	      mpv_pv[ipv].length++;
	      tt    = Flip(tt);
	      value = -value;
	    }
	}
    rep_esc:

      if ( is_out && mpv_pv[ipv].type != no_rep )
	{
	  if ( (((ply-1) % 5) == 0) && (ply != 1) )
	    {
	      Out( "\n                   " );
	    }
	  str = NULL;
	  switch ( mpv_pv[ipv].type )
	    {
	    case perpetual_check:  str = "PER. CHECK";     break;
	    case four_fold_rep:    str = "REPETITION";     break;
	    case black_superi_rep:
	    case white_superi_rep: str = "SUPERI. POSI.";  break;
	    case prev_solution:    str = "PREV. SEARCH";   break;
	    case hash_hit:         str = "HASH HIT";       break;
	    case book_hit:         str = "BOOK HIT";       break;
	    case pv_fail_high:     str = "FAIL HIGH";      break;
	    case mate_search:      str = "MATE SEARCH";    break;
	    }
	  if ( str != NULL ) { Out( " <%s>", str ); }
	}
      for ( ply--; ply >= 1; ply-- )
	{
	  tt = Flip(tt);
	  UnMakeMove( tt, mpv_pv[ipv].a[ply], ply );
	}

      if ( is_out ) { Out( "\n" ); }
    }

  if ( mpv_out ) { OutCsaShogi( "\n" ); }
} 


static void CONV
mpv_sub_result( unsigned int move )
{
  int i;

  for ( i = 0; mpv_pv[i].length; i++ )
    {
      assert( i < mpv_num*2 );
      assert( i < root_nmove*2 );
      assert( mpv_pv[i].depth <= iteration_depth );
      assert( -score_bound < (int)mpv_pv[i].a[0]-32768 );
      assert( (int)mpv_pv[i].a[0]-32768 < score_bound );

      if ( mpv_pv[i].a[1] == move ) { break; }
    }

  for ( ; mpv_pv[i].length; i++ )
    {
      assert( i < mpv_num*2 );
      assert( i < root_nmove*2 );
      assert( mpv_pv[i].depth <= iteration_depth );
      assert( -score_bound < (int)mpv_pv[i].a[0]-32768 );
      assert( (int)mpv_pv[i].a[0]-32768 < score_bound );

      mpv_pv[i] = mpv_pv[i+1];
    }
}


static int CONV
mpv_add_result( tree_t * restrict ptree, int value )
{
  pv_t pv_tmp, pv;
  unsigned int time;
  int i, vmin, num;

  vmin = mpv_find_min( &num );
  assert( num <= mpv_num );
  assert( num < root_nmove );
  assert( -score_bound < value );
  assert( value < score_bound  );

  /* remove the weakest pv if all of slots are full */
  if ( num == mpv_num )
    {
      for ( i = 0; mpv_pv[i].length; i++ )
	{
	  assert( i < mpv_num*2 );
	  assert( i < root_nmove*2 );
	  assert( mpv_pv[i].depth <= iteration_depth );
	  assert( -score_bound < (int)mpv_pv[i].a[0]-32768 );
	  assert( (int)mpv_pv[i].a[0]-32768 < score_bound );

	  if ( mpv_pv[i].depth == iteration_depth
	       && mpv_pv[i].a[0] == (unsigned int)(vmin+32768) ) { break; }
	}
      assert( i != mpv_num*2 );
      assert( mpv_pv[i].length );
      do {
	assert( i < mpv_num*2 );
	assert( i < root_nmove*2 );
	mpv_pv[i] = mpv_pv[i+1];
	i++;
      } while ( mpv_pv[i].length );
    }

  /* add a pv */
  for ( i = 0; mpv_pv[i].length; i++ )
    {
      assert( i < mpv_num*2 );
      assert( i < root_nmove*2 );
      assert( -score_bound < (int)mpv_pv[i].a[0]-32768 );
      assert( (int)mpv_pv[i].a[0]-32768 < score_bound );

      if ( mpv_pv[i].a[0] < (unsigned int)(value+32768) ) { break; }
    }

  pv      = ptree->pv[1];
  pv.a[0] = (unsigned int)(value+32768);
  do {
    assert( i < mpv_num*2 );
    assert( i < root_nmove*2 );
    pv_tmp      = mpv_pv[i];
    mpv_pv[i] = pv;
    pv          = pv_tmp;
    i          += 1;
  } while ( pv.length );

  if ( get_elapsed( &time ) < 0 ) { return -1; }

  mpv_out( ptree, root_turn, time - time_start );

  return 1;
}


static int CONV
mpv_set_bound( int alpha )
{
  int bound, num, value;

  bound = alpha - mpv_width;
  if ( bound < -score_bound ) { bound = -score_bound; }

  value = mpv_find_min( &num );
  assert( num <= mpv_num );
  assert( num < root_nmove );
  assert( num );
  assert( -score_bound < value );
  assert( value < score_bound  );
  if ( num == mpv_num && bound < value ) { bound = value; }

  return bound;
}


static int CONV
mpv_find_min( int *pnum )
{
  int i, num;
  unsigned int a[ MPV_MAX_PV+1 ], u, utemp;

  a[0] = 0;
  num  = 0;
  for ( i = 0; mpv_pv[i].length; i++ )
    {
      assert( i < mpv_num*2 );
      assert( i < root_nmove*2 );
      assert( mpv_pv[i].depth <= iteration_depth );

      if ( mpv_pv[i].depth == iteration_depth )
	{
	  u = mpv_pv[i].a[0];
	  assert( -score_bound < (int)u-32768 );
	  assert( (int)u-32768 < score_bound );

	  for ( num = 0; u <= a[num]; num++ );
	  do {
	    assert( num < mpv_num );
	    assert( num < root_nmove );
	    utemp  = a[num];
	    a[num] = u;
	    u      = utemp;
	    num   += 1;
	  } while ( u );
	  a[num] = 0;
	}
    }

  if ( pnum ) { *pnum = num; }

  if ( num ) { return (int)a[num-1] - 32768; }

  return 0;
}
#endif
