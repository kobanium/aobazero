// 2019 Team AobaZero
// This source code is in the public domain.
#include <math.h>
#include <limits.h>
#include "shogi.h"

int CONV
solve_problems( tree_t * restrict ptree, unsigned int nposition )
{
  const char *str_move;
  uint64_t total_node;
  unsigned int game_status_save, move, uposition, te1, te0;
  int iret, success, failure, ianswer, istatus, iresult;

  success = failure = 0;
  total_node = 0;
  if ( get_elapsed( &te0 ) < 0 ) { return -1; };

  for ( uposition = 0; uposition < nposition; uposition++ )
    {
      istatus = in_CSA( ptree, &record_problems, NULL,
			( flag_nomake_move | flag_detect_hang ) );
      if ( istatus < 0 ) { return istatus; }

      if ( istatus > record_next )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    record_problems.lines, str_bad_record );
	  str_error = str_message;
	  return -2;
	}

      /* examine all of answers */
      Out( "Answers:" );
      for ( ianswer = 0; ianswer < MAX_ANSWER; ianswer++ )
	{
	  str_move = &( record_problems.info.str_move[ianswer][0] );
	  if ( str_move[0] == '\0' ) { break; }
	  if ( ( root_turn && str_move[0] != '-' )
	       || ( ! root_turn && str_move[0] != '+' ) )
	    {
	      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
			record_problems.lines,
			"Answers has invalid sign of turn." );
	      str_error = str_message;
	      return -2;
	    }

	  iret = interpret_CSA_move( ptree, &move, str_move+1 );
	  if ( iret < 0 ) { return iret; }

	  iret = make_move_root( ptree, move, ( flag_detect_hang | flag_rep
						| flag_nomake_move ) );
	  if ( iret < 0 ) { return iret; }

	  Out( "%s ", str_move );
	}
      Out( "\n" );
      if ( ! ianswer )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    record_problems.lines,
		    "No answers in the record" );
	  str_error = str_message;
	  return -2;
	}

      iret = out_board( ptree, stdout, 0, 0 );
      if ( iret < 0 ) { return iret; }

      if ( get_elapsed( &time_start ) < 0 ) { return -1; };
      time_turn_start = time_start;

      game_status_save  = game_status;
      game_status      |= flag_problem | flag_nopeek | flag_thinking;
      iresult           = iterate( ptree );
      game_status       = game_status_save;
      if ( iresult < 0 ) { return iresult; }

      if ( iresult ) { success++; }
      else           { failure++; }

      total_node += ptree->node_searched;

      str_move = str_CSA_move( last_pv.a[1] );
      Out( "problem #%d answer=%s -- %s (correct=%d, incorrect=%d)\n\n",
	   success+failure, str_move, iresult ? "correct" : "incorrect",
	   success, failure );

      if ( istatus == record_eof ) { break; }
      if ( istatus == record_misc )
	{
	  iret = record_wind( &record_problems );
	  if ( iret < 0 )           { return iret; }
	  if ( iret == record_eof ) { break; }
	}
    }

  if ( get_elapsed( &te1 ) < 0 ) { return -1; }

  Out( "Total Nodes:   %" PRIu64 "\n", total_node );
  Out( "Total Elapsed: %.2f\n", (double)( te1 - te0 ) / 1000.0 );

  return 1;
}


#if defined(DFPN)
int CONV
solve_mate_problems( tree_t * restrict ptree, unsigned int nposition )
{
  unsigned int uposition;
  int iret;

  for ( uposition = 0; uposition < nposition; uposition++ )
    {
      int imove, istatus;
      unsigned int record_move;

      for ( imove = 0; ; imove++ )
	{
	  istatus = in_CSA( ptree, &record_problems, &record_move,
			    ( flag_nomake_move | flag_detect_hang
			      | flag_nofmargin ) );
	  if ( istatus < 0 )
	    {
	      str_error = str_illegal_move;
	      return -1;
	    }

	  if ( istatus >= record_eof ) { break; }

	  iret = make_move_root( ptree, record_move, 0 );
	  if ( iret < 0 ) { return iret; }
	}

      Out( "Problem #%d %s\n",
	   uposition, ptree->nsuc_check[1] ? "(in check)" : "" );
	
      iret = dfpn( ptree, root_turn, 1 );
      if ( iret < 0 ) { return iret; }

      if ( istatus == record_eof ) { break; }
      if ( istatus == record_misc )
	{
	  iret = record_wind( &record_problems );
	  if ( iret < 0 )           { return iret; }
	  if ( iret == record_eof ) { break; }
	}
    }

  return 1;
}
#endif
