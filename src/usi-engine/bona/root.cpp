// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <limits.h>
#include "shogi.h"

#if defined(USI) || defined(MNJ_LAN)
static int is_move_ignore( unsigned int move );
#endif

int
make_root_move_list( tree_t * restrict ptree )
{
  unsigned int * restrict pmove;
  int asort[ MAX_LEGAL_MOVES ];
  unsigned int move;
  int i, j, k, h, value, num_root_move, iret, value_pre_pv;
  int value_best;

  pmove = ptree->move_last[0];
  ptree->move_last[1] = GenCaptures( root_turn, pmove );
  ptree->move_last[1] = GenNoCaptures( root_turn, ptree->move_last[1] );
  ptree->move_last[1] = GenDrop( root_turn, ptree->move_last[1] );
  num_root_move = (int)( ptree->move_last[1] - pmove );

  value_pre_pv = INT_MIN;
  value_best   = 0;
  for ( i = 0; i < num_root_move; i++ )
    {
      value = INT_MIN;
      move  = pmove[i];

      MakeMove( root_turn, move, 1 );
      if ( ! InCheck( root_turn )
#if defined(USI) || defined(MNJ_LAN)
	   && ! is_move_ignore( move )
#endif
	   )
	{
	  iret = no_rep;
	  if ( InCheck(Flip(root_turn)) )
	    {
	      ptree->nsuc_check[2]
		= (unsigned char)( ptree->nsuc_check[0] + 1U );
	      if ( ptree->nsuc_check[2] >= 2 * 2 )
		{
		  iret = detect_repetition( ptree, 2, Flip(root_turn), 2 );
		}
	    }

	  if ( iret == perpetual_check ) { value = INT_MIN; }
	  else {
	    ptree->current_move[1] = move;
	    value = -search_quies( ptree, -score_bound, score_bound,
				   Flip(root_turn), 2, 1 );

	    if ( value > value_best ) { value_best = value; }
	    if ( I2IsPromote(move) ) { value++; }
	    if ( move == ptree->pv[0].a[1] )
	      {
		value_pre_pv = value;
		value        = INT_MAX;
	      }
	  }
	}
      UnMakeMove( root_turn, move, 1 );
      asort[i] = value;
    }

  /* shell sort */
  for ( k = SHELL_H_LEN - 1; k >= 0; k-- )
    {
      h = ashell_h[k];
      for ( i = num_root_move-h-1; i >= 0; i-- )
	{
	  value = asort[i];
	  move  = pmove[i];
	  for ( j = i+h; j < num_root_move && asort[j] > value; j += h )
	    {
	      asort[j-h] = asort[j];
	      pmove[j-h] = pmove[j];
	    }
	  asort[j-h] = value;
	  pmove[j-h] = move;
	}
    }

  /* discard all of moves cause mate or perpetual check */
  if ( asort[0] >= -score_max_eval )
    {
      for ( ; num_root_move; num_root_move-- )
	{
	  if ( asort[num_root_move-1] >= -score_max_eval ) { break; }
	}
    }

  /* discard perpetual checks */
  else for ( ; num_root_move; num_root_move-- )
    {
      if ( asort[num_root_move-1] != INT_MIN ) { break; }
    }

  for ( i = 0; i < num_root_move; i++ )
    {
      root_move_list[i].move   = pmove[i];
      root_move_list[i].nodes  = 0;
      root_move_list[i].status = 0;
#if defined(DFPN_CLIENT)
      root_move_list[i].dfpn_cresult = dfpn_client_na;
#endif
    }
  if ( value_pre_pv != INT_MIN ) { asort[0] = value_pre_pv; }
  root_nmove = num_root_move;

  if ( num_root_move > 1 && ! ( game_status & flag_puzzling ) )
    {
      int id_easy_move = 0;

      if ( asort[0] > asort[1] + ( MT_CAP_DRAGON * 3 ) / 8 )
	{
	  id_easy_move = 3;
	  easy_min     = - ( MT_CAP_DRAGON *  4 ) / 16;
	  easy_max     =   ( MT_CAP_DRAGON * 32 ) / 16;
	  easy_abs     =   ( MT_CAP_DRAGON * 19 ) / 16;
	}
      else if ( asort[0] > asort[1] + ( MT_CAP_DRAGON * 2 ) / 8 )
	{
	  id_easy_move = 2;
	  easy_min     = - ( MT_CAP_DRAGON *  3 ) / 16;
	  easy_max     =   ( MT_CAP_DRAGON *  6 ) / 16;
	  easy_abs     =   ( MT_CAP_DRAGON *  9 ) / 16;
	}
      else if ( asort[0] > asort[1] + MT_CAP_DRAGON / 8
		&& asort[0] > - MT_CAP_DRAGON / 8
		&& I2From(pmove[0]) < nsquare )
	{
	  id_easy_move = 1;
	  easy_min     = - ( MT_CAP_DRAGON *  2 ) / 16;
	  easy_max     =   ( MT_CAP_DRAGON *  4 ) / 16;
	  easy_abs     =   ( MT_CAP_DRAGON *  6 ) / 16;
	}

      if ( easy_abs )
	{
	  Out( "\n    the root move %s looks easy (type %d).\n",
	       str_CSA_move(pmove[0]), id_easy_move );
	  Out( "    evasion:%d, capture:%d, promotion:%d, drop:%d, "
	       "value:%5d - %5d\n",
	       ptree->nsuc_check[1]        ? 1 : 0,
	       UToCap(pmove[0])            ? 1 : 0,
	       I2IsPromote(pmove[0])       ? 1 : 0,
	       I2From(pmove[0]) >= nsquare ? 1 : 0,
	       asort[0], asort[1] );
	  easy_value = asort[0];
	}
    }

  return asort[0];
}


#if defined(USI) || defined(MNJ_LAN)
static int CONV
is_move_ignore( unsigned int move )
{
  int i;

  for ( i = 0; moves_ignore[i] != MOVE_NA; i += 1 )
    {
      if ( move == moves_ignore[i] ) { return 1; }
    }
  
  return 0;
}
#endif
