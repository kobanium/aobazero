// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include "shogi.h"

static void CONV adjust_fmg( void );
static int CONV set_root_alpha( int nfail_low, int root_alpha_old );
static int CONV set_root_beta( int nfail_high, int root_beta_old );
static int CONV is_answer_right( unsigned int move );
static int CONV rep_book_prob( tree_t * restrict ptree );
static const char * CONV str_fail_high( int turn, int nfail_high );


int CONV
iterate( tree_t * restrict ptree )
{
  int value, iret, ply;
  unsigned int cpu_start;
  int right_answer_made;

  /* probe the opening book */
  if ( pf_book != NULL
#if defined(USI) || defined(MNJ_LAN)
       && moves_ignore[0] == MOVE_NA
#endif
       && ! rep_book_prob( ptree ) )
    {
      int is_book_hit, i;
      unsigned int elapsed;
      
      is_book_hit = book_probe( ptree );
      if ( is_book_hit < 0 ) { return is_book_hit; }
      
      iret = get_elapsed( &elapsed );
      if ( iret < 0 ) { return iret; }

      Out( "- opening book is probed. (%ss)\n",
	   str_time_symple( elapsed - time_start ) );
      if ( is_book_hit )
	{
	  pv_close( ptree, 2, book_hit );
	  last_pv         = ptree->pv[1];
	  last_root_value = 0;
	  if ( ! ( game_status & flag_puzzling ) )
	    {
	      for ( i = 0; i < (int)HIST_SIZE; i++ )
		{
		  ptree->hist_good[i]  /= 256U;
		  ptree->hist_tried[i] /= 256U;
		}
	    }
	  MnjOut( "pid=%d move=%s n=0 v=0e final%s\n",
		  mnj_posi_id, str_CSA_move(ptree->pv[1].a[1]),
		  ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );


#if defined(USI)
	  if ( usi_mode != usi_off )
	    {
	      char str_usi[6];
	      csa2usi( ptree, str_CSA_move(ptree->pv[1].a[1]), str_usi );
	      USIOut( "info depth 1 score cp 0 nodes 0 pv %s\n", str_usi );
	    }		      
#endif

	  return 1;
	}
    }

  /* detect inaniwa tactics */
#if defined(INANIWA_SHIFT)
  if ( ! inaniwa_flag && 19 < ptree->nrep )
    {
      if ( root_turn == white
	   && ( BOARD[A7]==-pawn || BOARD[A6]==-pawn || BOARD[A5]==-pawn )
	   && BOARD[B7] == -pawn && BOARD[C7] == -pawn
	   && BOARD[D7] == -pawn && BOARD[E7] == -pawn
	   && BOARD[F7] == -pawn && BOARD[G7] == -pawn
	   && BOARD[H7] == -pawn
	   && ( BOARD[I7]==-pawn || BOARD[I6]==-pawn || BOARD[I5]==-pawn ) )
	{
	  Out( "\nINANIWA SHIFT TURNED ON (BLACK)\n\n" );
  	  inaniwa_flag = 1;
	  ehash_clear();
	  if ( ini_trans_table() < 0 ) { return -1; }
	}

      if ( root_turn == black
	   && ( BOARD[A3]==pawn || BOARD[A4]==pawn || BOARD[A5] == pawn )
	   && BOARD[B3] == pawn && BOARD[C3] == pawn
	   && BOARD[D3] == pawn && BOARD[E3] == pawn
	   && BOARD[F3] == pawn && BOARD[G3] == pawn
	   && BOARD[H3] == pawn
	   && ( BOARD[I3]==pawn || BOARD[I4]==pawn || BOARD[I5]==pawn ) )
	{
	  Out( "\nINANIWA SHIFT TURNED ON (WHITE)\n\n" );
	  inaniwa_flag = 2;
	  ehash_clear();
	  if ( ini_trans_table() < 0 ) { return -1; }
	}
    }
#endif
  

  /* initialize variables */
  if ( get_cputime( &cpu_start ) < 0 ) { return -1; }

  ptree->node_searched         =  0;
  ptree->null_pruning_done     =  0;
  ptree->null_pruning_tried    =  0;
  ptree->check_extension_done  =  0;
  ptree->recap_extension_done  =  0;
  ptree->onerp_extension_done  =  0;
  ptree->nfour_fold_rep        =  0;
  ptree->nperpetual_check      =  0;
  ptree->nsuperior_rep         =  0;
  ptree->nrep_tried            =  0;
  ptree->neval_called          =  0;
  ptree->nquies_called         =  0;
  ptree->ntrans_always_hit     =  0;
  ptree->ntrans_prefer_hit     =  0;
  ptree->ntrans_probe          =  0;
  ptree->ntrans_exact          =  0;
  ptree->ntrans_lower          =  0;
  ptree->ntrans_upper          =  0;
  ptree->ntrans_superior_hit   =  0;
  ptree->ntrans_inferior_hit   =  0;
  ptree->fail_high             =  0;
  ptree->fail_high_first       =  0;
  ptree->current_move[0]       =  0;
  ptree->save_eval[0]          =  INT_MAX;
  ptree->save_eval[1]          =  INT_MAX;
  ptree->pv[0].a[0]            =  0;
  ptree->pv[0].a[1]            =  0;
  ptree->pv[0].depth           =  0;
  ptree->pv[0].length          =  0;
  iteration_depth              =  0;
  easy_value                   =  0;
  easy_abs                     =  0;
  right_answer_made            =  0;
  root_abort                   =  0;
  root_nmove                   =  0;
  root_value                   = -score_bound;
  root_alpha                   = -score_bound;
  root_beta                    =  score_bound;
  root_index                   =  0;
  root_move_list[0].status     = flag_first;
  node_last_check              =  0;
  time_last_check              =  time_start;
  game_status                 &= ~( flag_move_now | flag_suspend
				    | flag_quit_ponder | flag_search_error );
#if defined(DBG_EASY)
  easy_move                    =  0;
#endif

#if defined(USI)
  usi_time_out_last            =  time_start;
#endif

#if defined(TLP)
  ptree->tlp_abort             = 0;
  tlp_nsplit                   = 0;
  tlp_nabort                   = 0;
  tlp_nslot                    = 0;
#endif

#if defined(MPV)
  if ( ! ( game_status & flag_puzzling ) && mpv_num > 1 )
    {
      int i;

      for ( i = 0; i < 2*mpv_num+1; i++ ) { mpv_pv[i].length = 0; }
      
      last_pv.a[0]    = 0;
      last_pv.a[1]    = 0;
      last_pv.depth   = 0;
      last_pv.length  = 0;
      last_root_value = 0;

      root_mpv = 1;
    }
  else { root_mpv = 0; }
#endif

#if defined(DFPN_CLIENT)
  dfpn_client_rresult_unlocked = dfpn_client_na;
  dfpn_client_move_unlocked    = MOVE_NA;
  dfpn_client_best_move        = MOVE_NA;
  dfpn_client_cresult_index    = 0;
#endif

  for ( ply = 0; ply < PLY_MAX; ply++ )
    {
      ptree->amove_killer[ply].no1 = ptree->amove_killer[ply].no2 = 0U;
      ptree->killers[ply].no1 = ptree->killers[ply].no2 = 0x0U;
    }

  {
    unsigned int u =  node_per_second / 16U;
    if      ( u > TIME_CHECK_MAX_NODE ) { u = TIME_CHECK_MAX_NODE; }
    else if ( u < TIME_CHECK_MIN_NODE ) { u = TIME_CHECK_MIN_NODE; }
    node_next_signal = u;
  }

  set_search_limit_time( root_turn );
  adjust_fmg();

  Out( "nsuc_check: [0]=%d [1]=%d\n",
       (int)ptree->nsuc_check[0], (int)ptree->nsuc_check[1] );

  /* look up last pv. */
  if ( last_pv.length )
    {
      Out( "- a pv was found in the previous search result.\n" );

      iteration_depth   = ( iteration_depth < 1 ) ? 0 : last_pv.depth - 1;
      ptree->pv[0]      = last_pv;
      ptree->pv[0].type = prev_solution;
      root_value        = root_turn ? -last_root_value : last_root_value;
      out_pv( ptree, root_value, root_turn, 0 );
      Out( "\n" );
    }

  /* probe the transposition table, since last pv is not available.  */
  if ( ! last_pv.length
#if defined(MPV)
       && ! root_mpv
#endif
       )
    {
      unsigned int value_type, dummy;
      int alpha, beta;
    
      value = INT_MIN;
      for ( ply = 1; ply < PLY_MAX - 10; ply++ )
	{
	  dummy = 0;
	  alpha = -score_bound;
	  beta  =  score_bound;
	  
	  value_type = hash_probe( ptree, 1, ply*PLY_INC+PLY_INC/2,
				   root_turn, alpha, beta, &dummy );
	  if ( value_type != value_exact )   { break; }
	  value = HASH_VALUE;
      }
      
      if ( -score_bound < value )
	{
	  Out( "- a pv was peeked through the transposition table.\n" );
	  iteration_depth     = ply-1;
	  ptree->pv[0].depth  = (unsigned char)(ply-1);
	  ptree->pv[0].type   = hash_hit;
	  root_value          = value;
	  out_pv( ptree, value, root_turn, 0 );
	  Out( "\n" );
	  
	  if ( ! ptree->pv[0].length )
	    {
	      iteration_depth    = 0;
	      ptree->pv[0].depth = 0;
	      root_value         = -score_bound;
#if ! defined(MINIMUM)
	      out_warning( "PEEK FAILED!!!" );
#endif
	    }
	}
    }

  /* root move generation */
  {
    unsigned int elapsed;
    
    Out( "- root move generation" );
    value = make_root_move_list( ptree );

    if ( ! root_nmove )
      {
#if defined(MNJ_LAN) || defined(USI)
	if ( moves_ignore[0] != MOVE_NA )
	  {
	    MnjOut( "pid=%d move=%%TORYO v=%de n=0 final%s\n",
		    mnj_posi_id, -score_bound,
		    ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );

	    return 1;
	  }
#endif
	str_error = str_no_legal_move;
	return -2;
      }

    if ( ! ptree->pv[0].length || ptree->pv[0].a[1] != root_move_list[0].move )
      {
	iteration_depth     = 0;
	ptree->pv[0].a[1]   = root_move_list[0].move;
	ptree->pv[0].length = 1;
	ptree->pv[0].depth  = 1;
	ptree->pv[0].type   = no_rep;
	root_value          = value;
	root_index          = 0;
      }

#if defined(MPV)
    if ( root_mpv )
      {
	if ( root_nmove == 1 ) { root_mpv = 0; }
	easy_abs = 0;
      }
#endif

    if ( get_elapsed( &elapsed ) < 0 ) { return -1; }
    Out( " ... done (%d moves, %ss)\n",
	 root_nmove, str_time_symple( elapsed - time_start ) );
  }


#if defined(DFPN_CLIENT)
  /* probe results from DFPN searver */
  dfpn_client_check_results();
  if ( dfpn_client_move_unlocked != MOVE_NA )
    {
      ptree->pv[0].a[1]   = dfpn_client_move_unlocked;
      ptree->pv[0].length = 1;
      ptree->pv[0].depth  = 0;
      ptree->pv[0].type   = no_rep;
      root_value          = score_matelong;
    }
#endif


  /* save preliminary result */
  assert( root_value != -score_bound );
  last_root_value = root_turn ? -root_value : root_value;
  last_pv         = ptree->pv[0];


#if defined(DFPN_CLIENT)
  /* send the best move to DFPN server */
  if ( dfpn_client_sckt != SCKT_NULL
       && dfpn_client_best_move != ptree->pv[0].a[1] )
    {
      dfpn_client_best_move = ptree->pv[0].a[1];
      lock( &dfpn_client_lock );
      dfpn_client_out( "BEST MOVE %s\n", str_CSA_move(dfpn_client_best_move) );
      unlock( &dfpn_client_lock );
    }
#endif


#if 0 && defined(MNJ_LAN)
  /* send the best move to parallel server */
  if ( sckt_mnj != SCKT_NULL ) {

    if ( moves_ignore[0] == MOVE_NA && root_nmove == 1 ) {
    /* only one replay */
      MnjOut( "pid=%d move=%s v=%de n=%" PRIu64 " final%s\n",
	      mnj_posi_id, str_CSA_move(ptree->pv[0].a[1]), root_value,
	      ptree->node_searched,
	      ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );
      
      return 1;
    }

    MnjOut( "pid=%d move=%s v=%de n=%" PRIu64 "\n",
	    mnj_posi_id, str_CSA_move(ptree->pv[0].a[1]), root_value,
	    ptree->node_searched );
  }
#endif
  MnjOut( "pid=%d move=%s v=%de n=%" PRIu64 "\n",
	  mnj_posi_id, str_CSA_move(ptree->pv[0].a[1]), root_value,
	  ptree->node_searched );

#if defined(USI)
  if ( usi_mode != usi_off )
    {
      char str_usi[6];
      csa2usi( ptree, str_CSA_move(ptree->pv[0].a[1]), str_usi );
      USIOut( "info depth %d score cp %d nodes %" PRIu64 " pv %s\n",
	      iteration_depth, root_value, ptree->node_searched, str_usi );
    }
#endif


  /* return, if previous pv is long enough */
  if ( abs(root_value) > score_max_eval
       || iteration_depth >= depth_limit
       || ( ( game_status & flag_puzzling )
	    && ( root_nmove == 1 || ptree->pv[0].depth > 4 ) ) )
    {
      return 1;
    }


  /* iterative deepening search */
#if defined(TLP)
  iret = tlp_start();
  if ( iret < 0 ) { return iret; }
#endif
  iteration_depth += 1;
  root_beta        = set_root_beta(  0, root_value );
  root_alpha       = set_root_alpha( 0, root_value );
  root_value       = root_alpha;

  Out( "- drive an iterative deepening search starting from depth %d\n",
       iteration_depth );

  for ( ; iteration_depth < 30/*PLY_MAX-10*/; iteration_depth++ ) {

    if ( get_elapsed( &time_last_search ) < 0 ) { return -1; }
    
#if defined(MPV)
    if ( root_mpv )
      {
	int i;
	i = ( root_nmove < mpv_num ) ? root_nmove : mpv_num;
	for ( ; i < mpv_num*2; i++ ) { mpv_pv[i].length = 0; }
      }
#endif
    
    {
      unsigned int move;
      int tt, i, n;

      tt = root_turn;
      for ( ply = 1; ply <= ptree->pv[0].length; ply++ )
	{
	  move = ptree->pv[0].a[ply];
	  if ( ! is_move_valid( ptree, move, tt ) )
	    {
#if ! defined(MINIMUM)
	      out_warning( "Old pv has an illegal move!  ply=%d, move=%s",
			   ply, str_CSA_move(move) );
#endif
	      break;
	    }
	  MakeMove( tt, move, ply );
	  if ( InCheck(tt) )
	    {
#if ! defined(MINIMUM)
	      out_warning( "Old pv has an illegal evasion!  ply=%d, move=%s",
			   ply, str_CSA_move(move) );
#endif
	      UnMakeMove( tt, move, ply );
	      break;
	    }
	  tt = Flip(tt);
	}
      for ( ply--; ply > 0; ply-- )
	{
	  tt   = Flip(tt);
	  move = ptree->pv[0].a[ply];
	  UnMakeMove( tt, move, ply );
	  hash_store_pv( ptree, move, tt );
	}

      root_nfail_high = 0;
      root_nfail_low  = 0;

      n = root_nmove;
      root_move_list[0].status = flag_first;
      for ( i = 1; i < n; i++ ) { root_move_list[i].status = 0; }
    }

    /*  a trial of searches  */
    for ( ;; ) {
      value = searchr( ptree, root_alpha, root_beta, root_turn,
		       iteration_depth*PLY_INC + PLY_INC/2 );
      if ( game_status & flag_search_error ) { return -1; }
      if ( root_abort )                      { break; }

      assert( abs(value) < score_foul );

      if ( root_beta <= value )
	{
	  const char *str_move;
	  const char *str;
	  double dvalue;
	  
	  root_move_list[0].status &= ~flag_searched;
	  dvalue = (double)( root_turn ? -root_beta : root_beta );

	  do { root_beta  = set_root_beta( ++root_nfail_high, root_beta ); }
	  while ( value >= root_beta );

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
	  MnjOut( "pid=%d move=%s v=%dl n=%" PRIu64 "%s\n",
		  mnj_posi_id, str_CSA_move(ptree->pv[1].a[1]),
		  root_beta, ptree->node_searched,
		  ( mnj_depth_stable <= iteration_depth ) ? " stable" : "" );

#if defined(USI)
	  if ( usi_mode != usi_off )
	    {
	      char str_usi[6];
	      csa2usi( ptree, str_CSA_move(ptree->pv[1].a[1]), str_usi );
	      USIOut( "info depth %d score cp %d nodes %" PRIu64 " pv %s\n",
		      iteration_depth, root_beta, ptree->node_searched,
		      str_usi );
	    }
#endif

	  str = str_time_symple( time_last_result - time_start );
	  if ( root_move_list[0].status & flag_first )
	    {
	      Out( "(%2d)%6s %7.2f ", iteration_depth, str, dvalue / 100.0 );
	    }
	  else { Out( "    %6s %7.2f ", str, dvalue / 100.0 ); }

	  str      = str_fail_high( root_turn, root_nfail_high );
	  str_move = str_CSA_move(ptree->pv[1].a[1]);
	  Out( " 1.%c%s [%s!]\n", ach_turn[root_turn], str_move, str );
	  
	  
	  if ( game_status & flag_pondering )
	    {
	      OutCsaShogi( "info%+.2f %c%s %c%s [%s!]\n",
			   dvalue / 100.0, ach_turn[Flip(root_turn)],
			   str_CSA_move(ponder_move),
			   ach_turn[root_turn], str_move, str );
	    }
	  else {
	    OutCsaShogi( "info%+.2f %c%s [%s!]\n", dvalue / 100.0,
			 ach_turn[root_turn], str_move, str );
	  }
	}
      else if ( value <= root_alpha )
	{
	  const char *str_move;
	  const char *str;
	  unsigned int time_elapsed;
	  double dvalue;

	  if ( ! ( root_move_list[0].status & flag_first ) )
	    {
	      root_value = root_alpha;
	      break;
	    }

	  root_move_list[0].status &= ~flag_searched;
	  dvalue = (double)( root_turn ? -root_alpha : root_alpha );

	  if ( get_elapsed( &time_elapsed ) < 0 ) { return -1; }

	  do { root_alpha = set_root_alpha( ++root_nfail_low, root_alpha ); }
	  while ( value <= root_alpha );
	  root_value = root_alpha;
	  str = str_time_symple( time_elapsed - time_start );
	  Out( "(%2d)%6s %7.2f ", iteration_depth, str, dvalue / 100.0 );

	  str      = str_fail_high( Flip(root_turn), root_nfail_low );
	  str_move = str_CSA_move(root_move_list[0].move);
	  Out( " 1.%c%s [%s?]\n", ach_turn[root_turn], str_move, str );
	  if ( game_status & flag_pondering )
	    {
	      OutCsaShogi( "info%+.2f %c%s %c%s [%s?]\n",
			   dvalue / 100.0, ach_turn[Flip(root_turn)],
			   str_CSA_move(ponder_move),
			   ach_turn[root_turn], str_move, str );
	    }
	  else {
	    OutCsaShogi( "info%+.2f %c%s [%s?]\n", dvalue / 100.0,
			 ach_turn[root_turn], str_move, str );
	  }
	}
      else { break; }
    }

    /* the trial of search is finished */
    if ( root_alpha < root_value && root_value < root_beta )
      {
	last_root_value = root_turn ? - root_value : root_value;
	last_pv         = ptree->pv[0];
      }

    if ( root_abort ) { break; }

    if ( root_alpha < root_value && root_value < root_beta )
      {
#if ! defined(MINIMUM)
	{
	  int i, n;
	  n = root_nmove;
	  for ( i = 0; i < n; i++ )
	    {
	      if ( root_move_list[i].status & flag_searched ) { continue; }
	      out_warning( "A root move %s is ignored\n",
			   str_CSA_move(root_move_list[i].move) );
	    }
	}
#endif

	if ( ( game_status & flag_problem ) && depth_limit == PLY_MAX )
	  {
	    if ( is_answer_right( ptree->pv[0].a[1] ) )
	      {
		if ( right_answer_made > 1 && iteration_depth > 3 ) { break; }
		right_answer_made++;
	      }
	    else { right_answer_made = 0; }
	  }
	
	if ( abs(value)      >  score_max_eval ) { break; }
	if ( iteration_depth >= depth_limit )    { break; }
	
	root_beta  = set_root_beta(  0, value );
	root_alpha = set_root_alpha( 0, value );
	root_value = root_alpha;
      }
#if ! defined(MINIMUM)
    else { out_warning(( "SEARCH INSTABILITY IS DETECTED!!!" )); }
#endif

    /* shell sort */
    {
      root_move_t root_move_swap;
      const int n = root_nmove;
      uint64_t sortv;
      int i, j, k, h;
      
      for ( k = SHELL_H_LEN - 1; k >= 0; k-- )
	{
	  h = ashell_h[k];
	  for ( i = n-h-1; i > 0; i-- )
	    {
	      root_move_swap = root_move_list[i];
	      sortv          = root_move_list[i].nodes;
	      for ( j = i+h; j < n && root_move_list[j].nodes > sortv; j += h )
		{
		  root_move_list[j-h] = root_move_list[j];
		}
	      root_move_list[j-h] = root_move_swap;
	    }
	}
    }
  }

  /* iteration finished */
  if ( game_status & flag_problem )
    {
      if ( is_answer_right( ptree->pv[0].a[1] ) )
	{
	  right_answer_made = 1;
	}
      else { right_answer_made = 0; }
    }

  {
    int i;
    
    for ( i = 0; i < (int)HIST_SIZE; i++ )
      {
	ptree->hist_good[i]  /= 256U;
	ptree->hist_tried[i] /= 256U;
      }
  }
  /* prunings and extentions-statistics */
  {
    double drep, dhash, dnull, dfh1st;

    drep    = (double)ptree->nperpetual_check;
    drep   += (double)ptree->nfour_fold_rep;
    drep   += (double)ptree->nsuperior_rep;
    drep   *= 100.0 / (double)( ptree->nrep_tried + 1 );

    dhash   = (double)ptree->ntrans_exact;
    dhash  += (double)ptree->ntrans_inferior_hit;
    dhash  += (double)ptree->ntrans_superior_hit;
    dhash  += (double)ptree->ntrans_upper;
    dhash  += (double)ptree->ntrans_lower;
    dhash  *= 100.0 / (double)( ptree->ntrans_probe + 1 );

    dnull   = 100.0 * (double)ptree->null_pruning_done;
    dnull  /= (double)( ptree->null_pruning_tried + 1 );

    dfh1st  = 100.0 * (double)ptree->fail_high_first;
    dfh1st /= (double)( ptree->fail_high + 1 );

    Out( "    pruning  -> rep=%4.3f%%  hash=%2.0f%%  null=%2.0f%%"
	 "  fh1st=%4.1f%%\n", drep, dhash, dnull, dfh1st );
    
    Out( "    extension-> chk=%u recap=%u 1rep=%u\n",
	 ptree->check_extension_done, ptree->recap_extension_done,
	 ptree->onerp_extension_done );
  }

  /* futility threashold */
#if ! ( defined(NO_STDOUT) && defined(NO_LOGGING) )
  {
    int misc   = fmg_misc;
    int drop   = fmg_drop;
    int cap    = fmg_cap;
    int mt     = fmg_mt;
    int misc_k = fmg_misc_king;
    int cap_k  = fmg_cap_king;
    Out( "    futility -> misc=%d drop=%d cap=%d mt=%d misc(k)=%d cap(k)=%d\n",
	 misc, drop, cap, mt, misc_k, cap_k );
  }
#endif

  /* hashing-statistics */
  {
    double dalways, dprefer, dsupe, dinfe;
    double dlower, dupper, dsat;
    uint64_t word2;
    int ntrans_table, i, n;

    ntrans_table = 1 << log2_ntrans_table;
    if ( ntrans_table > 8192 ) { ntrans_table = 8192; }
    
    for ( i = 0, n = 0; i < ntrans_table; i++ )
      {
	word2 = ptrans_table[i].prefer.word2;
	SignKey( word2, ptrans_table[i].prefer.word1 );
	if ( trans_table_age == ( 7 & (int)word2 ) ) { n++; }

	word2 = ptrans_table[i].always[0].word2;
	SignKey( word2, ptrans_table[i].always[0].word1 );
	if ( trans_table_age == ( 7 & (int)word2 ) ) { n++; }

	word2 = ptrans_table[i].always[1].word2;
	SignKey( word2, ptrans_table[i].always[1].word1 );
	if ( trans_table_age == ( 7 & (int)word2 ) ) { n++; }
      }

    dalways  = 100.0 * (double)ptree->ntrans_always_hit;
    dalways /= (double)( ptree->ntrans_probe + 1 );

    dprefer  = 100.0 * (double)ptree->ntrans_prefer_hit;
    dprefer /= (double)( ptree->ntrans_probe + 1 );

    dsupe    = 100.0 * (double)ptree->ntrans_superior_hit;
    dsupe   /= (double)( ptree->ntrans_probe + 1 );

    dinfe    = 100.0 * (double)ptree->ntrans_inferior_hit;
    dinfe   /= (double)( ptree->ntrans_probe + 1 );

    Out( "    hashing  -> always=%2.0f%% prefer=%2.0f%% supe=%4.2f%% "
	 "infe=%4.2f%%\n", dalways, dprefer, dsupe, dinfe );

    dlower  = 100.0 * (double)ptree->ntrans_lower;
    dlower /= (double)( ptree->ntrans_probe + 1 );

    dupper  = 100.0 * (double)ptree->ntrans_upper;
    dupper /= (double)( ptree->ntrans_probe + 1 );

    dsat    = 100.0 * (double)n;
    dsat   /= (double)( 3 * ntrans_table );

    OutCsaShogi( "statsatu=%.0f", dsat );
    Out( "    hashing  -> "
	 "exact=%d lower=%2.0f%% upper=%4.2f%% sat=%2.0f%% age=%d\n",
	 ptree->ntrans_exact, dlower, dupper, dsat, trans_table_age );
    if ( dsat > 9.0 ) { trans_table_age  = ( trans_table_age + 1 ) & 0x7; }
  }

#if defined(TLP)
  if ( tlp_max > 1 )
    {
      Out( "    threading-> split=%d abort=%d slot=%d\n",
	   tlp_nsplit, tlp_nabort, tlp_nslot+1 );
      if ( tlp_nslot+1 == TLP_NUM_WORK )
	{
	  out_warning( "THREAD WORK AREA IS USED UP!!!" );
	}
    }
#endif

#if defined(DFPN_CLIENT)
  if ( dfpn_client_sckt != SCKT_NULL )
    {
      int asum[4] = { 0, 0, 0, 0 };
      int i, n;

      n = root_nmove;
      for ( i = 0; i < n; i += 1 )
	{
	  asum[ root_move_list[i].dfpn_cresult ] += 1;
	}

      switch ( dfpn_client_rresult_unlocked )
	{
	case dfpn_client_na:
	  Out( "    dfpn     -> n/a " );
	  break;

	case dfpn_client_win:
	  if ( dfpn_client_move_unlocked != MOVE_NA )
	    {
	      Out( "    dfpn     -> win %s ",
		   str_CSA_move(dfpn_client_move_unlocked) );
	    }
	  else { Out( "    dfpn     -> win ignore " ); }
	  break;

	case dfpn_client_lose:
	  Out( "    dfpn     -> lose " );
	  break;

	default:
	  assert( dfpn_client_misc == dfpn_client_rresult_unlocked );
	  Out( "    dfpn     -> misc " );
	  break;
	}
	  
      Out( "(children win %d, lose %d, misc %d, n/a %d)\n",
	   asum[dfpn_client_win], asum[dfpn_client_lose],
	   asum[dfpn_client_misc], asum[dfpn_client_na] );
    }
  if ( dfpn_client_move_unlocked )
    {
      last_root_value = root_turn ? -score_matelong : score_matelong;
      last_pv.a[1]   = dfpn_client_move_unlocked;
      last_pv.length = 1;
      last_pv.depth  = 0;
      last_pv.type   = no_rep;
      MnjOut( "pid=%d move=%s n=0 v=%de final%s\n", mnj_posi_id,
	      str_CSA_move(last_pv.a[1]), score_matelong,
	      ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );
    }
#endif

  {
    double dcpu_percent, dnps, dmat;
    unsigned int cpu, elapsed;

    Out( "    n=%" PRIu64 " quies=%u eval=%u rep=%u %u(chk) %u(supe)\n",
	 ptree->node_searched, ptree->nquies_called, ptree->neval_called,
	 ptree->nfour_fold_rep, ptree->nperpetual_check,
	 ptree->nsuperior_rep );

    if ( get_cputime( &cpu )     < 0 ) { return -1; }
    if ( get_elapsed( &elapsed ) < 0 ) { return -1; }

    cpu             -= cpu_start;
    elapsed         -= time_start;

    dcpu_percent     = 100.0 * (double)cpu;
    dcpu_percent    /= (double)( elapsed + 1U );

    dnps             = 1000.0 * (double)ptree->node_searched;
    dnps            /= (double)( elapsed + 1U );

#if defined(TLP)
    {
      double n = (double)tlp_max;
      node_per_second  = (unsigned int)( ( dnps + 0.5 ) / n );
    }
#else
    node_per_second  = (unsigned int)( dnps + 0.5 );
#endif

    dmat  = (double)MATERIAL;
    dmat /= (double)MT_CAP_PAWN;

    OutCsaShogi( " cpu=%.0f nps=%.2f\n", dcpu_percent, dnps / 1e3 );
    Out( "    time=%s  ", str_time_symple( elapsed ) );
    Out( "cpu=%3.0f%%  mat=%.1f  nps=%.2fK\n", dcpu_percent, dmat, dnps/1e3 );
  }

  if ( ( game_status & flag_problem ) && ! right_answer_made ) { iret = 0; }
  else                                                         { iret = 1; }

  return iret;
}


#if defined(USI)
int CONV
usi_root_list( tree_t * restrict ptree )
{
#define SIZE_STR ( MAX_LEGAL_MOVES * 6 + 200 )
  char str[ SIZE_STR ];
  char str_usi[6];
  unsigned int elapsed;
  int i, istr;

  ptree->node_searched         =  0;
  ptree->current_move[0]       =  MOVE_NA;
  ptree->save_eval[0]          =  INT_MAX;
  ptree->save_eval[1]          =  INT_MAX;
  ptree->pv[0].a[0]            =  0;
  ptree->pv[0].a[1]            =  0;
  ptree->pv[0].depth           =  0;
  ptree->pv[0].length          =  0;
  easy_value                   =  0;
  easy_abs                     =  0;
  root_abort                   =  0;
  root_nmove                   =  0;
  root_value                   = -score_bound;
  root_alpha                   = -score_bound;
  root_beta                    =  score_bound;
  node_last_check              =  0;
  game_status                 &= ~( flag_move_now | flag_suspend
				    | flag_quit_ponder | flag_search_error );

  Out( "- root move generation" );
  make_root_move_list( ptree );

  if ( game_status & flag_search_error ) { return -1; }
  if ( game_status & ( flag_quit | flag_quit_ponder | flag_suspend ) )
    {
      return 1;
    }

  if ( ! root_nmove )
    {
      str_error = "No legal moves to search";
      return -1;
    }

  if ( get_elapsed( &elapsed ) < 0 ) { return -1; }
  Out( " ... done (%d moves, %ss)\n",
       root_nmove, str_time_symple( elapsed - time_start ) );

  istr = snprintf( str, SIZE_STR, "genmove_probability" );
  for ( i = 0; i < root_nmove; i++ )
    {
      csa2usi( ptree, str_CSA_move(root_move_list[i].move), str_usi );
      istr += snprintf( str + istr, SIZE_STR - istr, " %s %d",
			str_usi, i + 1 );
    }

  USIOut( "%s\n", str );
  return 1;
}


int CONV
usi_book( tree_t * restrict ptree )
{
  char str_usi[6];
  int is_book_hit;

  if ( pf_book == NULL || rep_book_prob( ptree  ) )
    {
      USIOut( "bestmove pass\n" );
      return 1;
    }

  is_book_hit = book_probe( ptree );
  if ( is_book_hit < 0 ) { return is_book_hit; }

  if ( ! is_book_hit )
    {
      USIOut( "bestmove pass\n" );
      return 1;
    }

  csa2usi( ptree, str_CSA_move(ptree->current_move[1]), str_usi );
  USIOut( "bestmove %s\n", str_usi );
  return 1;
}
#endif


#if defined(DFPN_CLIENT)
void CONV
dfpn_client_check_results( void )
{
  int i, n, index, num;

  n = root_nmove;
  lock( &dfpn_client_lock );

  if ( dfpn_client_rresult_unlocked == dfpn_client_na )
    {
      dfpn_client_rresult_unlocked = dfpn_client_rresult;
      if ( dfpn_client_rresult == dfpn_client_win
	   && dfpn_client_move_unlocked == MOVE_NA )
	{
	  for ( i = 0; i < n; i += 1 )
	    if ( ! strcmp( (const char *)dfpn_client_str_move,
			   str_CSA_move(root_move_list[i].move) ) )
	      {
		dfpn_client_move_unlocked = root_move_list[i].move;
		break;
	      }
	}
    }

  num = dfpn_client_num_cresult;
  for ( index = dfpn_client_cresult_index; index < num; index += 1 )
    {
      for ( i = 0; i < n; i += 1 ) {
	
	if ( root_move_list[i].dfpn_cresult != dfpn_client_na ) { continue; }
	
	if ( ! strcmp( (const char *)dfpn_client_cresult[index].str_move,
		       str_CSA_move(root_move_list[i].move) ) ) {

	  root_move_list[i].dfpn_cresult
	    = (int)dfpn_client_cresult[index].result;
	  break;
	}
      }
    }

  dfpn_client_cresult_index = index;
  dfpn_client_flag_read     = 0;
  unlock( &dfpn_client_lock );
}
#endif


static int CONV rep_book_prob( tree_t * restrict ptree )
{
  int i;

  for ( i = ptree->nrep - 2; i >= 0; i -= 2 )
    if ( ptree->rep_board_list[i] == HASH_KEY
	 && ptree->rep_hand_list[i] == HAND_B )
      {
	Out( "- book is ignored due to a repetition.\n" );
	return 1;
      }

  return 0;
}


static void CONV adjust_fmg( void )
{
  int misc, cap, drop, mt, misc_king, cap_king;

  misc      = fmg_misc      - FMG_MG      / 2;
  cap       = fmg_cap       - FMG_MG      / 2;
  drop      = fmg_drop      - FMG_MG      / 2;
  misc_king = fmg_misc_king - FMG_MG_KING / 2;
  cap_king  = fmg_cap_king  - FMG_MG_KING / 2;
  mt        = fmg_mt        - FMG_MG_MT   / 2;

  fmg_misc      = ( misc      < FMG_MISC      ) ? FMG_MISC      : misc;
  fmg_cap       = ( cap       < FMG_CAP       ) ? FMG_CAP       : cap;
  fmg_drop      = ( drop      < FMG_DROP      ) ? FMG_DROP      : drop;
  fmg_misc_king = ( misc_king < FMG_MISC_KING ) ? FMG_MISC_KING : misc_king;
  fmg_cap_king  = ( cap_king  < FMG_CAP_KING  ) ? FMG_CAP_KING  : cap_king;
  fmg_mt        = ( mt        < FMG_MT        ) ? FMG_MT        : mt;
}


static int CONV set_root_beta( int nfail_high, int root_beta_old )
{
  int aspiration_hwdth, aspiration_fail1;

  if ( time_max_limit != time_limit )
    {
      aspiration_hwdth = MT_CAP_DRAGON / 8;
      aspiration_fail1 = MT_CAP_DRAGON / 2;
    }
  else {
    aspiration_hwdth = MT_CAP_DRAGON / 4;
    aspiration_fail1 = ( MT_CAP_DRAGON * 3 ) / 4;
  }

  switch ( nfail_high )
    {
    case 0:  root_beta_old += aspiration_hwdth;                     break;
    case 1:  root_beta_old += aspiration_fail1 - aspiration_hwdth;  break;
    case 2:  root_beta_old  = score_bound;                          break;
    default:
      out_error( "Error at set_root_beta!" );
      exit(1);
    }
  if ( root_beta_old > score_max_eval ) { root_beta_old = score_bound; }

  return root_beta_old;
}


static int CONV set_root_alpha( int nfail_low, int root_alpha_old )
{
  int aspiration_hwdth, aspiration_fail1;

  if ( time_max_limit != time_limit )
    {
      aspiration_hwdth = MT_CAP_DRAGON / 8;
      aspiration_fail1 = MT_CAP_DRAGON / 2;
    }
  else {
    aspiration_hwdth = MT_CAP_DRAGON / 4;
    aspiration_fail1 = ( MT_CAP_DRAGON * 3 ) / 4;
  }

  switch ( nfail_low )
    {
    case 0:  root_alpha_old -= aspiration_hwdth;                     break;
    case 1:  root_alpha_old -= aspiration_fail1 - aspiration_hwdth;  break;
    case 2:  root_alpha_old  = -score_bound;                         break;
    default:
      out_error( "Error at set_root_alpha!" );
      exit(1);
    }
  if ( root_alpha_old < -score_max_eval ) { root_alpha_old = -score_bound; }

  return root_alpha_old;
}


static const char * CONV str_fail_high( int turn, int nfail_high )
{
  const char *str;

  if ( time_max_limit != time_limit )
    {
      if ( nfail_high == 1 ) { str = turn ? "-1" : "+1"; }
      else                   { str = turn ? "-4" : "+4"; }
    }
  else {
    if ( nfail_high == 1 ) { str = turn ? "-2" : "+2"; }
    else                   { str = turn ? "-6" : "+6"; }
  }
  return str;
}


static int CONV is_answer_right( unsigned int move )
{
  const char *str_anser;
  const char *str_move;
  int ianser, iret;
  
  iret     = 0;
  str_move = str_CSA_move( move );

  for ( ianser = 0; ianser < MAX_ANSWER; ianser++ )
    {
      str_anser = &( record_problems.info.str_move[ianser][0] );
      if ( str_anser[0] == '\0' ) { break; }
      if ( ! strcmp( str_anser+1, str_move ) )
	{
	  iret = 1;
	  break;
	}
    }

  return iret;
}
