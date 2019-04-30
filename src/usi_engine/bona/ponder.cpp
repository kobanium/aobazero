// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <limits.h>
#include "shogi.h"


int
ponder( tree_t * restrict ptree )
{
  const char *str;
  unsigned int move;
  int iret;

  if ( ( game_status & ( mask_game_end | flag_noponder | flag_nopeek ) )
       || abs( last_root_value ) > score_max_eval
       || ! record_game.moves
       || sec_limit_up == UINT_MAX ) { return 1; }

  ponder_nmove = gen_legal_moves( ptree, ponder_move_list, 1 );

  if ( get_elapsed( &time_start ) < 0 ) { return -1; }

  Out( "\nSearch a move to ponder\n\n" );
  OutCsaShogi( "info ponder start\n" );

  game_status |= flag_puzzling;
  iret         = iterate( ptree );
  game_status &= ~flag_puzzling;
  if ( iret < 0 ) { return iret; }

  if ( game_status & ( flag_quit | flag_quit_ponder | flag_suspend ) )
    {
      OutCsaShogi( "info ponder end\n" );
      return 1;
    }

  if ( abs(last_root_value) > score_max_eval )
    {
      OutCsaShogi( "info ponder end\n" );
      return 1;
    }

  ponder_move = move = last_pv.a[1];
  str = str_CSA_move( move );
  Out( "\nPonder on %c%s (%+.2f)\n\n",
       ach_turn[root_turn], str, (double)last_root_value / 100.0 );

  iret = make_move_root( ptree, move, flag_rep );
  if ( iret < 0 )
    {
      OutCsaShogi( "info ponder end\n" );
      return iret;
    }
  
  if ( game_status & mask_game_end )
    {
      OutCsaShogi( "info ponder end\n" );
      return unmake_move_root( ptree );
    }
  
  if ( get_elapsed( &time_start ) < 0 ) { return -1; }

  game_status |= flag_pondering;

  iret = iterate( ptree );
  if ( game_status & flag_thinking )
    {
      game_status &= ~flag_thinking;
      if ( iret < 0 ) { return iret; }

      iret = com_turn_start( ptree, flag_from_ponder );
      if ( iret < 0 ) { return iret; }
      
      return 2;
    }
  OutCsaShogi( "info ponder end\n" );
  game_status &= ~flag_pondering;
  return unmake_move_root( ptree );
}


#if defined(MNJ_LAN) || defined(USI)
int
analyze( tree_t * restrict ptree )
{
  int iret;

#if defined(MNJ_LAN)
  if ( game_status & mask_game_end )
    {
      MnjOut( "pid=%d move=%%TORYO v=%de n=0 final%s\n",
	      mnj_posi_id, -score_bound,
	      ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );
      return 1;
    }
#endif

  iret = get_elapsed( &time_start );
  if ( iret < 0 ) { return iret; }

  game_status |= flag_pondering;
  iret = iterate( ptree );
  game_status &= ~flag_pondering;

  if ( abs(last_root_value) > score_max_eval )
    {
      MnjOut( "pid=%d final%s\n", mnj_posi_id,
	      ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );
    }

  return iret;
}
#endif
