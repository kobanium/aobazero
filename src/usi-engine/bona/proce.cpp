// 2019 Team AobaZero
// This source code is in the public domain.
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shogi.h"

#include <string>
#include "yss_var.h"
#include "yss_dcnn.h"


/* unacceptable when the program is thinking, or quit pondering */
#define AbortDifficultCommand                                              \
	  if ( game_status & flag_thinking )                               \
	    {                                                              \
	      str_error = str_busy_think;                                  \
	      return -2;                                                   \
	    }                                                              \
	  else if ( game_status & ( flag_pondering | flag_puzzling ) )     \
	    {                                                              \
	      game_status |= flag_quit_ponder;                             \
	      return 2;                                                    \
	    }

#if defined(MINIMUM)
#  define CmdBook(x,y) cmd_book(y);
static int CONV cmd_book( char **lasts );
#else
#  define CmdBook(x,y) cmd_book(x,y);
static int CONV cmd_learn( tree_t * restrict ptree, char **lasts );
static int CONV cmd_book( tree_t * restrict ptree, char **lasts );
#endif

#if ! defined(NO_STDOUT)
static int CONV cmd_stress( char **lasts );
#endif

#if defined(CSA_LAN)
static int CONV proce_csalan( tree_t * restrict ptree );
static int CONV cmd_connect( tree_t * restrict ptree, char **lasts );
static int CONV cmd_sendpv( char **lasts );
#endif

#if defined(MNJ_LAN)
static int CONV proce_mnj( tree_t * restrict ptree );
static int CONV cmd_mnjignore( tree_t *restrict ptree, char **lasts );
static int CONV cmd_mnj( char **lasts );
static int CONV cmd_mnjmove( tree_t * restrict ptree, char **lasts,
			     int num_alter );
#endif

#if defined(USI)
static int CONV proce_usi( tree_t * restrict ptree );
static int CONV usi_posi( tree_t * restrict ptree, char **lasts );
static int CONV usi_go( tree_t * restrict ptree, char **lasts );
static int CONV usi_ignore( tree_t * restrict ptree, char **lasts );
#endif

#if defined(TLP)
static int CONV cmd_thread( char **lasts );
#endif

#if defined(MPV)
static int CONV cmd_mpv( char **lasts );
#endif

#if defined(DFPN)
static int CONV cmd_dfpn( tree_t * restrict ptree, char **lasts );
#endif

#if defined(DFPN_CLIENT)
static int CONV cmd_dfpn_client( tree_t * restrict ptree, char **lasts );
#endif

static int CONV proce_cui( tree_t * restrict ptree );
static int CONV cmd_usrmove( tree_t * restrict ptree, const char *str_move,
			     char **last );
static int CONV cmd_outmove( tree_t * restrict ptree );
static int CONV cmd_move_now( void );
static int CONV cmd_ponder( char **lasts );
static int CONV cmd_limit( char **lasts );
static int CONV cmd_quit( void );
static int CONV cmd_beep( char **lasts );
static int CONV cmd_peek( char **lasts );
static int CONV cmd_stdout( char **lasts );
static int CONV cmd_newlog( char **lasts );
static int CONV cmd_hash( char **lasts );
static int CONV cmd_ping( void );
static int CONV cmd_suspend( void );
static int CONV cmd_problem( tree_t * restrict ptree, char **lasts );
static int CONV cmd_display( tree_t * restrict ptree, char **lasts );
static int CONV cmd_move( tree_t * restrict ptree, char **lasts );
static int CONV cmd_new( tree_t * restrict ptree, char **lasts );
static int CONV cmd_read( tree_t * restrict ptree, char **lasts );
static int CONV cmd_resign( tree_t * restrict ptree, char **lasts );
static int CONV cmd_time( char **lasts );


int CONV is_move( const char *str )
{
  if ( isdigit( (int)str[0] ) && isdigit( (int)str[1] )
       && isdigit( (int)str[2] ) && isdigit( (int)str[3] )
       && isupper( (int)str[4] ) && isupper( (int)str[5] )
       && str[6] == '\0' ) { return 1; }

  return 0;
}


int CONV
procedure( tree_t * restrict ptree )
{
#if defined(CSA_LAN)
  if ( sckt_csa != SCKT_NULL ) { return proce_csalan( ptree ); }
#endif

#if defined(MNJ_LAN)
  if ( sckt_mnj != SCKT_NULL ) { return proce_mnj( ptree ); }
#endif

#if defined(USI)
  if ( usi_mode != usi_off ) { return proce_usi( ptree ); }
#endif

  return proce_cui( ptree );
}


static int CONV proce_cui( tree_t * restrict ptree )
{
  const char *token;
  char *last;

  token = strtok_r( str_cmdline, str_delimiters, &last );

  if ( token == NULL || *token == '#' ) { return 1; }
  if ( is_move( token ) ) { return cmd_usrmove( ptree, token, &last ); }
  if ( ! strcmp( token, "s" ) )         { return cmd_move_now(); }
  if ( ! strcmp( token, "beep" ) )      { return cmd_beep( &last); }
  if ( ! strcmp( token, "book" ) )      { return CmdBook( ptree, &last ); }
  if ( ! strcmp( token, "display" ) )   { return cmd_display( ptree, &last ); }
  if ( ! strcmp( token, "hash" ) )      { return cmd_hash( &last ); }
  if ( ! strcmp( token, "limit" ) )     { return cmd_limit( &last ); }
  if ( ! strcmp( token, "move" ) )      { return cmd_move( ptree, &last ); }
  if ( ! strcmp( token, "new" ) )       { return cmd_new( ptree, &last ); }
  if ( ! strcmp( token, "outmove" ) )   { return cmd_outmove( ptree ); }
  if ( ! strcmp( token, "peek" ) )      { return cmd_peek( &last ); }
  if ( ! strcmp( token, "stdout" ) )    { return cmd_stdout( &last ); }
  if ( ! strcmp( token, "ping" ) )      { return cmd_ping(); }
  if ( ! strcmp( token, "ponder" ) )    { return cmd_ponder( &last ); }
  if ( ! strcmp( token, "problem" ) )   { return cmd_problem( ptree, &last ); }
  if ( ! strcmp( token, "quit" ) )      { return cmd_quit(); }
  if ( ! strcmp( token, "read" ) )      { return cmd_read( ptree, &last ); }
  if ( ! strcmp( token, "resign" ) )    { return cmd_resign( ptree, &last ); }
  if ( ! strcmp( token, "suspend" ) )   { return cmd_suspend(); }
  if ( ! strcmp( token, "time" ) )      { return cmd_time( &last ); }
  if ( ! strcmp( token, "newlog" ) )    { return cmd_newlog( &last ); }
#if defined(CSA_LAN)
  if ( ! strcmp( token, "connect" ) )   { return cmd_connect( ptree, &last ); }
  if ( ! strcmp( token, "sendpv" ) )    { return cmd_sendpv( &last ); }
#endif
#if defined(MNJ_LAN)
  if ( ! strcmp( token, "mnj" ) )       { return cmd_mnj( &last ); }
#endif
#if defined(MPV)
  if ( ! strcmp( token, "mpv" ) )       { return cmd_mpv( &last ); }
#endif
#if defined(DFPN)
  if ( ! strcmp( token, "dfpn" ) )      { return cmd_dfpn( ptree, &last ); }
#endif
#if defined(DFPN_CLIENT)
  if ( ! strcmp( token, "dfpn_client")) { return cmd_dfpn_client( ptree,
								  &last ); }
#endif
#if defined(TLP)
  if ( ! strcmp( token, "tlp" ) )       { return cmd_thread( &last ); }
#endif
#if ! defined(NO_STDOUT)
  if ( ! strcmp( token, "stress" ) )    { return cmd_stress( &last ); }
#endif
#if ! defined(MINIMUM)
  if ( ! strcmp( token, "learn" ) )     { return cmd_learn( ptree, &last ); }
#endif

  str_error = str_bad_cmdline;
  return -2;
}


#if defined(CSA_LAN)
static int CONV proce_csalan( tree_t * restrict ptree )
{
  const char *token;
  char *last;

  token = strtok_r( str_cmdline, str_delimiters, &last );
    
  if ( token == NULL ) { return 1; }
  if ( *token == ach_turn[client_turn] && is_move( token+1 ) )
    {
      char *ptr;
      long l;

      token = strtok_r( NULL, str_delimiters, &last );
      if ( token == NULL || *token != 'T' )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}
      
      l = strtol( token+1, &ptr, 0 );
      if ( token+1 == ptr || l == LONG_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}

      adjust_time( (unsigned int)l, client_turn );
      Out( "  elapsed: b%u, w%u\n", sec_b_total, sec_w_total );
      return 1;
    }
  if ( *token == ach_turn[Flip(client_turn)] && is_move( token+1 ) )
    {
      return cmd_usrmove( ptree, token+1, &last );
    }
  if ( ! strcmp( token, str_resign ) ) { return cmd_resign( ptree, &last ); }
  if ( ! strcmp( token, "#WIN" )
       || ! strcmp( token, "#LOSE" )
       || ! strcmp( token, "#DRAW" )
       || ! strcmp( token, "#CHUDAN" ) )
    {
      if ( game_status & ( flag_thinking | flag_pondering | flag_puzzling ) )
	{
	  game_status |= flag_suspend;
	  return 2;
	}

      if ( sckt_out( sckt_csa, "LOGOUT\n" ) < 0 ) { return -1; }
      if ( sckt_recv_all( sckt_csa )        < 0 ) { return -1; }

      ShutdownAll();
      
      if ( client_ngame == client_max_game ) { return cmd_quit(); }

      return client_next_game( ptree, client_str_addr, (int)client_port );
    }
  
  return 1;
}
#endif


#if defined(MNJ_LAN)
static int CONV proce_mnj( tree_t * restrict ptree )
{
  const char *token;
  char *last;
  int iret;

  token = strtok_r( str_cmdline, str_delimiters, &last );
  if ( token == NULL ) { return 1; }

  if ( ! strcmp( token, "new" ) )
    {
      iret = cmd_suspend();
      if ( iret != 1 ) { return iret; }

      mnj_posi_id = 0;
      iret = cmd_new( ptree, &last );
      if ( iret < 0 ) { return iret; }

      moves_ignore[0] = MOVE_NA;
      return analyze( ptree );
    }
  if ( ! strcmp( token, "ignore" ) ) { return cmd_mnjignore( ptree, &last ); }
  if ( ! strcmp( token, "idle" ) )   { return cmd_suspend(); }
  if ( ! strcmp( token, "alter" ) )  { return cmd_mnjmove( ptree, &last, 1 ); }
  if ( ! strcmp( token, "retract" ) )
    {
      long l;
      char *ptr;
      const char *str = strtok_r( NULL, str_delimiters, &last );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || (long)NUM_UNMAKE < l )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}
      
      return cmd_mnjmove( ptree, &last, (int)l );
    }
  if ( ! strcmp( token, "move" ) )  { return cmd_mnjmove( ptree, &last, 0 ); }

  str_error = str_bad_cmdline;
  return -2;
}


static int CONV
cmd_mnjignore( tree_t *restrict ptree, char **lasts )
{
  const char *token;
  char *ptr;
  int i;
  unsigned int move;
  long lid;


  token = strtok_r( NULL, str_delimiters, lasts );
  if ( token == NULL )
    {
      str_error = str_bad_cmdline;
      return -1;
    }
  lid = strtol( token, &ptr, 0 );
  if ( ptr == token || lid == LONG_MAX || lid < 1 )
    {
      str_error = str_bad_cmdline;
      return -1;
    }

  AbortDifficultCommand;

  for ( i = 0; ; i += 1 )
    {
      token = strtok_r( NULL, str_delimiters, lasts );
      if ( token == NULL ) { break; }

      if ( interpret_CSA_move( ptree, &move, token ) < 0 ) { return -1; }

      moves_ignore[i] = move;
    }
  if ( i == 0 )
    {
      str_error = str_bad_cmdline;
      return -1;
    }
  mnj_posi_id     = (int)lid;
  moves_ignore[i] = MOVE_NA;

  return analyze( ptree );
}


static int CONV
cmd_mnjmove( tree_t * restrict ptree, char **lasts, int num_alter )
{
  const char *str1 = strtok_r( NULL, str_delimiters, lasts );
  const char *str2 = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long lid;
  unsigned int move;
  int iret;

  if ( sckt_mnj == SCKT_NULL ||  str1 == NULL || str2 == NULL )
    {
      str_error = str_bad_cmdline;
      return -1;
    }

  lid = strtol( str2, &ptr, 0 );
  if ( ptr == str2 || lid == LONG_MAX || lid < 1 )
    {
      str_error = str_bad_cmdline;
      return -1;
    }

  AbortDifficultCommand;
 
  while ( num_alter )
    {
      iret = unmake_move_root( ptree );
      if ( iret < 0 ) { return iret; }

      num_alter -= 1;
    }

  iret = interpret_CSA_move( ptree, &move, str1 );
  if ( iret < 0 ) { return iret; }
    
  iret = get_elapsed( &time_turn_start );
  if ( iret < 0 ) { return iret; }

  mnj_posi_id = (int)lid;

  iret = make_move_root( ptree, move, ( flag_time | flag_rep
					| flag_detect_hang ) );
  if ( iret < 0 ) { return iret; }
  
#  if ! defined(NO_STDOUT)
  iret = out_board( ptree, stdout, 0, 0 );
  if ( iret < 0 ) { return iret; }
#  endif

  moves_ignore[0] = MOVE_NA;
  return analyze( ptree );
}
#endif


#if defined(USI)
static int CONV proce_usi( tree_t * restrict ptree )
{
  const char *token;
  char *lasts;
  int iret;

  token = strtok_r( str_cmdline, str_delimiters, &lasts );
  if ( token == NULL ) { return 1; }

  if ( ! strcmp( token, "usi" ) )
    {
      USIOut( "id name %s\n", str_myname );
      USIOut( "id author Kunihito Hoki, Hiroshi Yamashita, Yuki Kobayashi\n" );
      USIOut( "id settings %s\n", get_cmd_line_ptr() );
      USIOut( "usiok\n" );
      return 1;
    }

  if ( ! strcmp( token, "isready" ) )
    {
      USIOut( "readyok\n", str_myname );
      return 1;
    }

  if ( ! strcmp( token, "echo" ) )
    {
      USIOut( "%s\n", lasts );
      return 1;
    }

  if ( ! strcmp( token, "ignore_moves" ) )
    {
      return usi_ignore( ptree, &lasts );
    }

  if ( ! strcmp( token, "genmove_probability" ) )
    {
      if ( get_elapsed( &time_start ) < 0 ) { return -1; }
      return usi_root_list( ptree );
    }

  if ( ! strncmp( token, "go", 2 ) ) {
    usi_go_count++;
    iret = usi_go( ptree, &lasts );
    moves_ignore[0] = MOVE_NA;
    return iret;
  }

//if ( ! strcmp( token, "stop" ) )     { return cmd_move_now(); }
  if ( ! strcmp( token, "stop" ) ) {
    if ( is_ignore_stop()==0 ) {
      send_latest_bestmove();
    }
    return 1;
  }
  if ( ! strcmp( token,"usinewgame") ) {
    usi_go_count       = 0;
    usi_bestmove_count = 0;
    usi_newgame();
    return 1;
  }

  if ( ! strcmp( token, "position" ) ) { return usi_posi( ptree, &lasts ); }
  if ( ! strcmp( token, "quit" ) )     { return cmd_quit(); }
  if ( ! strcmp( token, "d" ) ) {
/*
    fprintf(stderr,"print board\n");
    int i;
    for (i=0;i<nsquare;i++) {
      fprintf(stderr,"%3d,",ptree->posi.asquare[i]);
      if ( ((i+1)%9)==0 ) fprintf(stderr,"\n");
	}
*/
	print_board(ptree);
    return 1;
  }
  if ( 1 ) {
	fprintf(stderr,"usi illegal command\n");
	return 1;
  }

  str_error = str_bad_cmdline;
  return -1;
}


static int CONV
usi_ignore( tree_t * restrict ptree, char **lasts )
{
  const char *token;
  char str_buf[7];
  int i;
  unsigned int move;

  AbortDifficultCommand;

  for ( i = 0; ; i += 1 )
    {
      token = strtok_r( NULL, str_delimiters, lasts );
      if ( token == NULL ) { break; }
      
      if ( usi2csa( ptree, token, str_buf ) < 0 )            { return -1; }
      if ( interpret_CSA_move( ptree, &move, str_buf ) < 0 ) { return -1; }

      moves_ignore[i] = move;
    }

  moves_ignore[i] = MOVE_NA;

  return 1;
}


static int CONV
usi_go( tree_t * restrict ptree, char **lasts )
{
  const char *token;
//  char *ptr;
  int iret;
//  long l;

  AbortDifficultCommand;
/*
  if ( game_status & mask_game_end )
    {
      str_error = str_game_ended;
      return -1;
    }
*/
  token = strtok_r( NULL, str_delimiters, lasts );
  if ( ! token ) {
    fprintf(stderr,"token is null. go infinite\n");
    token = "infinite";
  }

#if defined(YSS_ZERO)
  fUSIMoveCount = 0;
  if ( ! strcmp( token, "visit" ) ) {
     fUSIMoveCount = 1;
  }
#endif

/*
  if ( ! strcmp( token, "book" ) )
    {
      AbortDifficultCommand;
      if ( usi_book( ptree ) < 0 ) { return -1; }

      return 1;
    }

  if ( ! strcmp( token, "infinite" ) )
    {
      usi_byoyomi     = UINT_MAX;
      depth_limit     = PLY_MAX;
      node_limit      = UINT64_MAX;
      sec_limit_depth = UINT_MAX;
    }
  else if ( ! strcmp( token, "byoyomi" ) )
    {
      token = strtok_r( NULL, str_delimiters, lasts );
      if ( token == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}

      l = strtol( token, &ptr, 0 );
      if ( ptr == token || (unsigned long)l > UINT_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -1;
	}
      
      usi_byoyomi     = (unsigned int)l;
      depth_limit     = PLY_MAX;
      node_limit      = UINT64_MAX;
      sec_limit_depth = UINT_MAX;
    }
  else {
    str_error = str_bad_cmdline;
    return -1;
  }

      
  if ( get_elapsed( &time_turn_start ) < 0 ) { return -1; }
*/
//  iret = com_turn_start( ptree, 0 );
  iret = YssZero_com_turn_start( ptree );
  if ( iret < 0 ) {
    DEBUG_PRT("Err\n");
//  if ( str_error == str_no_legal_move ) { USIOut( "bestmove resign\n" ); }
//  else                                  { return -1; }
  }
  
  return 1;
}


static int CONV
usi_posi( tree_t * restrict ptree, char **lasts )
{
  const char *token;
  char str_buf[7];
  unsigned int move;
    
  AbortDifficultCommand;
    
  moves_ignore[0] = MOVE_NA;

  token = strtok_r( NULL, str_delimiters, lasts );
  if ( strcmp( token, "startpos" ) )
    {
      str_error = str_bad_cmdline;
      return -1;
    }
    
  if ( ini_game( ptree, &min_posi_no_handicap,
		 flag_history, NULL, NULL ) < 0 ) { return -1; }
    
  token = strtok_r( NULL, str_delimiters, lasts );
  if ( token == NULL ) { return 1; }

  if ( strcmp( token, "moves" ) )
    {
      str_error = str_bad_cmdline;
      return -1;
    }
    
  for ( ;; )  {

    token = strtok_r( NULL, str_delimiters, lasts );
    if ( token == NULL ) { break; }
      
    if ( usi2csa( ptree, token, str_buf ) < 0 )            { return -1; }
    if ( interpret_CSA_move( ptree, &move, str_buf ) < 0 ) { return -1; }
#if defined(YSS_ZERO)
    if ( make_move_root( ptree, move, 0 ) < 0 )
#else
    if ( make_move_root( ptree, move, ( flag_history | flag_time
					| flag_rep
					| flag_detect_hang ) ) < 0 )
#endif
      {
	return -1;
      }
  }
    
  if ( get_elapsed( &time_turn_start ) < 0 ) { return -1; }
  return 1;
}

#endif


static int CONV cmd_move_now( void )
{
  if ( game_status & flag_thinking ) { game_status |= flag_move_now; }

  return 1;
}


static int CONV
cmd_usrmove( tree_t * restrict ptree, const char *str_move, char **lasts )
{
  const char *str;
  char *ptr;
  long lelapsed;
  unsigned int move;
  int iret;

  if ( game_status & mask_game_end )
    {
      str_error = str_game_ended;
      return -2;
    }
  
  if ( game_status & flag_thinking )
    {
      str_error = str_busy_think;
      return -2;
    }

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( str == NULL ) { lelapsed = 0; }
  else {
    if ( *str != 'T' )
      {
	str_error = str_bad_cmdline;
	return -2;
      }
    str += 1;
    lelapsed = strtol( str, &ptr, 0 );
    if ( ptr == str || lelapsed == LONG_MAX || lelapsed < 1 )
      {
	str_error = str_bad_cmdline;
	return -2;
      }
  }

  if ( game_status & ( flag_pondering | flag_puzzling ) )
    {
      int i;

      for ( i = 0; i < ponder_nmove; i++ )
	{
	  if ( ! strcmp( str_move, str_CSA_move(ponder_move_list[i]) ) )
	    {
	      break;
	    }
	}
      if ( i == ponder_nmove )
	{
#if defined(CSA_LAN)
	  if ( sckt_csa != SCKT_NULL ) { AbortDifficultCommand; }
#endif

#if defined(CSASHOGI)
	  AbortDifficultCommand;
#else
	  str_error = str_illegal_move;
	  return -2;
#endif
	}

      if ( ( game_status & flag_puzzling )
	   || strcmp( str_move, str_CSA_move(ponder_move) ) )
	{
	  ponder_move  = MOVE_PONDER_FAILED;
	  game_status |= flag_quit_ponder;
	  return 2;
	}
      else {
	iret = update_time( Flip(root_turn) );
	if ( iret < 0 ) { return iret; }
	if ( lelapsed )
	  {
	    adjust_time( (unsigned int)lelapsed, Flip(root_turn) );
	  }

	out_CSA( ptree, &record_game, ponder_move );

	game_status      &= ~flag_pondering;
	game_status      |= flag_thinking;
	set_search_limit_time( root_turn );

	OutCsaShogi( "info ponder end\n" );

	str = str_time_symple( time_turn_start - time_start );
	Out( "    %6s          MOVE PREDICTION HIT\n"
	     "  elapsed: b%u, w%u\n", str, sec_b_total, sec_w_total );
	return 1;
      }
    }

  iret = interpret_CSA_move( ptree, &move, str_move );
  if ( iret < 0 ) { return iret; }
  move_evasion_pchk = 0;
  iret = make_move_root( ptree, move, ( flag_rep | flag_history | flag_time
					| flag_detect_hang ) );
  if ( iret < 0 )
      {

#if defined(CSA_LAN)
	if ( sckt_csa != SCKT_NULL )
	  {
	    if ( move_evasion_pchk )
	      {
		str  = str_CSA_move( move_evasion_pchk );
		iret = sckt_out( sckt_csa, "%c%s\n",
				 ach_turn[Flip(root_turn)], str );
		if ( iret < 0 ) { return iret; }
	      }
	    return cmd_suspend();
	  }
#endif

	if ( move_evasion_pchk )
	  {
	    str = str_CSA_move( move_evasion_pchk );
#if defined(CSASHOGI)
	    OutCsaShogi( "move%s\n", str );
	    return cmd_suspend();
#else
	    snprintf( str_message, SIZE_MESSAGE, "perpetual check (%c%s)",
		      ach_turn[Flip(root_turn)], str );
	    str_error = str_message;
	    return -2;
#endif
	  }

	return iret;
      }

  if ( lelapsed ) { adjust_time( (unsigned int)lelapsed, Flip(root_turn) ); }
  Out( "  elapsed: b%u, w%u\n", sec_b_total, sec_w_total );

#if defined(CSA_LAN)
  if ( sckt_csa != SCKT_NULL && ( game_status & flag_mated ) )
    {
      iret = sckt_out( sckt_csa, "%%TORYO\n" );
      if ( iret < 0 ) { return iret; }
    }
#endif

  if ( ! ( game_status & mask_game_end ) )
    {
      iret = com_turn_start( ptree, 0 );
      if ( iret < 0 ) { return iret; }
    }

  return 1;
}


static int CONV cmd_beep( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on )  ) {  game_status &= ~flag_nobeep; }
  else if ( ! strcmp( str, str_off ) ) {  game_status |=  flag_nobeep; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


static int CONV cmd_peek( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on )  ) {  game_status &= ~flag_nopeek; }
  else if ( ! strcmp( str, str_off ) ) {  game_status |=  flag_nopeek; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


static int CONV cmd_stdout( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on )  ) {  game_status &= ~flag_nostdout; }
  else if ( ! strcmp( str, str_off ) ) {  game_status |=  flag_nostdout; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


static int CONV cmd_newlog( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on )  ) { game_status &= ~flag_nonewlog; }
  else if ( ! strcmp( str, str_off ) ) { game_status |=  flag_nonewlog; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


static int CONV cmd_ponder( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on )  ) { game_status &= ~flag_noponder; }
  else if ( ! strcmp( str, str_off ) )
    {
      if ( game_status & ( flag_pondering | flag_puzzling ) )
	{
	  game_status |= flag_quit_ponder;
	}
      game_status |= flag_noponder;
    }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


#if ! defined(NO_STDOUT)
static int CONV cmd_stress( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_on  ) ) { game_status &= ~flag_nostress; }
  else if ( ! strcmp( str, str_off ) ) { game_status |= flag_nostress; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}
#endif


static int CONV
#if defined(MINIMUM)
cmd_book( char **lasts )
#else
cmd_book( tree_t * restrict ptree, char **lasts )
#endif
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  int iret = 1;

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  if      ( ! strcmp( str, str_on ) )   { iret = book_on(); }
  else if ( ! strcmp( str, str_off ) )  { iret = book_off(); }
  else if ( ! strcmp( str, "narrow" ) ) { game_status |= flag_narrow_book; }
  else if ( ! strcmp( str, "wide" ) )   { game_status &= ~flag_narrow_book; }
#if ! defined(MINIMUM)
  else if ( ! strcmp( str, "create" ) )
    {
      AbortDifficultCommand;

      iret = book_create( ptree );
      if ( iret < 0 ) { return iret; }

      iret = ini_game( ptree, &min_posi_no_handicap, flag_history,
		       NULL, NULL );
      if ( iret < 0 ) { return iret; }

      iret = get_elapsed( &time_turn_start );
    }
#endif
  else {
    str_error = str_bad_cmdline;
    iret = -2;
  }

  return iret;
}


static int CONV cmd_display( tree_t * restrict ptree, char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;
  int iret;

  if ( str != NULL )
    {
      l = strtol( str, &ptr, 0 );
      if ( ptr == str )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      if      ( l == 1 ) { game_status &= ~flag_reverse; }
      else if ( l == 2 ) { game_status |= flag_reverse; }
      else {
	str_error = str_bad_cmdline;
	return -2;
      }
    }
  
  Out( "\n" );
  iret = out_board( ptree, stdout, 0, 0 );
  if ( iret < 0 ) { return iret; }
#if ! defined(NO_LOGGING)
  iret = out_board( ptree, pf_log, 0, 0 );
  if ( iret < 0 ) { return iret; }
#endif
  Out( "\n" );

  return 1;
}


static int CONV cmd_ping( void )
{
  OutCsaShogi( "pong\n" );
  Out( "pong\n" );
  return 1;
}


static int CONV cmd_hash( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  l = strtol( str, &ptr, 0 );
  if ( ptr == str || l == LONG_MAX || l < 1 || l > 31 )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  
  AbortDifficultCommand;
  
  log2_ntrans_table = (int)l;
  memory_free( (void *)ptrans_table_orig );
  return ini_trans_table();
}


static int CONV cmd_limit( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l1, l2, l3;

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  AbortDifficultCommand;

  if ( ! strcmp( str, "depth" ) )
    {
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l1 = strtol( str, &ptr, 0 );
      if ( ptr == str || l1 == LONG_MAX || l1 < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      sec_limit_up = UINT_MAX;
      node_limit   = UINT64_MAX;
      depth_limit  = (int)l1;
    }
  else if ( ! strcmp( str, "nodes" ) )
    {
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l1 = strtol( str, &ptr, 0 );
      if ( ptr == str || l1 == LONG_MAX || l1 < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      sec_limit_up = UINT_MAX;
      depth_limit  = PLY_MAX;
      node_limit   = (uint64_t)l1;
    }
  else if ( ! strcmp( str, "time" ) )
    {
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      if ( ! strcmp( str, "extendable" ) )
	{
	  game_status |= flag_time_extendable;
	}
      else if ( ! strcmp( str, "strict" ) )
	{
	  game_status &= ~flag_time_extendable;
	}
      else {
	l1 = strtol( str, &ptr, 0 );
	if ( ptr == str || l1 == LONG_MAX || l1 < 0 )
	  {
	    str_error = str_bad_cmdline;
	    return -2;
	  }

	str = strtok_r( NULL, str_delimiters, lasts );
	if ( str == NULL )
	  {
	    str_error = str_bad_cmdline;
	    return -2;
	  }
	l2 = strtol( str, &ptr, 0 );
	if ( ptr == str || l2 == LONG_MAX || l2 < 0 )
	  {
	    str_error = str_bad_cmdline;
	    return -2;
	  }

	str = strtok_r( NULL, str_delimiters, lasts );
	if ( ! str ) { l3 = -1; }
	else {
	  l3 = strtol( str, &ptr, 0 );
	  if ( ptr == str || l3 >= PLY_MAX || l3 < -1 )
	    {
	      str_error = str_bad_cmdline;
	      return -2;
	    }
	}

	if ( ! ( l1 | l2 ) ) { l2 = 1; }

	depth_limit  = PLY_MAX;
	node_limit   = UINT64_MAX;
	sec_limit    = (unsigned int)l1 * 60U;
	sec_limit_up = (unsigned int)l2;
	if ( l3 == -1 ) { sec_limit_depth = UINT_MAX; }
	else            { sec_limit_depth = (unsigned int)l3; }
      }
    }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}


static int CONV
cmd_read( tree_t * restrict ptree, char **lasts )
{
  const char *str1 = strtok_r( NULL, str_delimiters, lasts );
  const char *str2 = strtok_r( NULL, str_delimiters, lasts );
  const char *str3 = strtok_r( NULL, str_delimiters, lasts );
  const char *str_tmp;
  FILE *pf_src, *pf_dest;
  char str_file[SIZE_FILENAME];
  char *ptr;
  unsigned int moves;
  long l;
  int iret, flag, c;

  flag    = flag_history | flag_rep | flag_detect_hang;
  moves   = UINT_MAX;
  str_tmp = NULL;

  if ( str1 == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if ( str2 != NULL )
    {
      if ( ! strcmp( str2, "t" ) ) { flag |= flag_time; }
      else if ( strcmp( str2, "nil" ) )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
    }

  if ( str3 != NULL )
    {
      l = strtol( str3, &ptr, 0 );
      if ( ptr == str3 || l == LONG_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      moves = (unsigned int)l - 1U;
    }

  AbortDifficultCommand;

  if ( ! strcmp( str1, "." ) )
    {
      str_tmp = "game.cs_";

#if defined(NO_LOGGING)
      strncpy( str_file, "game.csa", SIZE_FILENAME-1 );
#else
      snprintf( str_file, SIZE_FILENAME, "%s/game%03d.csa",
		str_dir_logs, record_num );
#endif
      pf_dest = file_open( str_tmp, "w" );
      if ( pf_dest == NULL ) { return -2; }

      pf_src = file_open( str_file, "r" );
      if ( pf_src == NULL )
	{
	  file_close( pf_dest );
	  return -2;
	}

      while ( ( c = getc(pf_src) ) != EOF ) { putc( c, pf_dest ); }

      iret = file_close( pf_src );
      if ( iret < 0 )
	{
	  file_close( pf_dest );
	  return iret;
	}

      iret = file_close( pf_dest );
      if ( iret < 0 ) { return iret; }

      flag |= flag_time;
      str1  = str_tmp;
    }

  iret = read_record( ptree, str1, moves, flag );
  if ( iret < 0 ) { return iret; }

  iret = get_elapsed( &time_turn_start );
  if ( iret < 0 ) { return iret; }

  if ( str_tmp && remove( str_tmp ) )
    {
      out_warning( "remove() failed." );
      return -2;
    }

  return 1;
}


static int CONV cmd_resign( tree_t * restrict ptree, char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;

  if ( str == NULL || *str == 'T' )
    {
      AbortDifficultCommand;

      if ( game_status & mask_game_end ) { return 1; }

      game_status |= flag_resigned;
      update_time( root_turn );
      out_CSA( ptree, &record_game, MOVE_RESIGN );
    }
  else {
    l = strtol( str, &ptr, 0 );
    if ( ptr == str || l == LONG_MAX || l < MT_CAP_PAWN )
      {
	str_error = str_bad_cmdline;
	return -2;
      }
    resign_threshold = (int)l;
  }

  return 1;
}


static int CONV cmd_move( tree_t * restrict ptree, char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;
  unsigned int move;
  int iret, i;

  if ( game_status & mask_game_end )
    {
      str_error = str_game_ended;
      return -2;
    }
  
  AbortDifficultCommand;

  if ( str == NULL )
    {
      iret = get_elapsed( &time_turn_start );
      if ( iret < 0 ) { return iret; }
      
      return com_turn_start( ptree, 0 );
    }

  l = strtol( str, &ptr, 0 );
  if ( str != ptr && l != LONG_MAX && l >= 1 && *ptr == '\0' )
    {
      for ( i = 0; i < l; i += 1 )
	{
	  if ( game_status & ( flag_move_now | mask_game_end ) ) { break; }

	  iret = get_elapsed( &time_turn_start );
	  if ( iret < 0 ) { return iret; }
	
	  iret = com_turn_start( ptree, 0 );
	  if ( iret < 0 ) { return iret; }
	}

      return 1;
    }

  do {
    iret = interpret_CSA_move( ptree, &move, str );
    if ( iret < 0 ) { return iret; }
    
    iret = get_elapsed( &time_turn_start );
    if ( iret < 0 ) { return iret; }
    
    iret = make_move_root( ptree, move,
			   ( flag_history | flag_time | flag_rep
			     | flag_detect_hang ) );
    if ( iret < 0 ) { return iret; }
    
    str = strtok_r( NULL, str_delimiters, lasts );

  } while ( str != NULL );
  
  return 1;
}


static int CONV cmd_new( tree_t * restrict ptree, char **lasts )
{
  const char *str1 = strtok_r( NULL, str_delimiters, lasts );
  const char *str2 = strtok_r( NULL, str_delimiters, lasts );
  const min_posi_t *pmp;
  min_posi_t min_posi;
  int iret;

  AbortDifficultCommand;

  if ( str1 != NULL )
    {
      memset( &min_posi.asquare, empty, nsquare );
      min_posi.hand_black = min_posi.hand_white = 0;
      iret = read_board_rep1( str1, &min_posi );
      if ( iret < 0 ) { return iret; }

      if ( str2 != NULL )
	{
	  if      ( ! strcmp( str2, "-" ) ) { min_posi.turn_to_move = white; }
	  else if ( ! strcmp( str2, "+" ) ) { min_posi.turn_to_move = black; }
	  else {
	    str_error = str_bad_cmdline;
	    return -2;
	  }
	}
      else { min_posi.turn_to_move = black; }

      pmp = &min_posi;
    }
  else { pmp = &min_posi_no_handicap; }

  iret = ini_game( ptree, pmp, flag_history, NULL, NULL );
  if ( iret < 0 ) { return iret; }

  return get_elapsed( &time_turn_start );
}


static int CONV cmd_outmove( tree_t * restrict ptree )
{
  const char *str_move;
  char buffer[256];
  unsigned int move_list[ MAX_LEGAL_MOVES ];
  int i, c, n;

  AbortDifficultCommand;

  if ( game_status & mask_game_end )
    {
      Out( "NO LEGAL MOVE\n" );
      DFPNOut( "NO LEGAL MOVE\n" );
      return 1;
    }

  n = gen_legal_moves( ptree, move_list, 0 );

  buffer[0]='\0';
  for ( c = i = 0; i < n; i += 1 )
    {
      str_move = str_CSA_move(move_list[i]);

      if ( i && ( i % 10 ) == 0 )
        {
          Out( "%s\n", buffer );
          DFPNOut( "%s ", buffer );
          memcpy( buffer, str_move, 6 );
          c = 6;
        }
      else if ( i )
        {
          buffer[c] = ' ';
          memcpy( buffer + c + 1, str_move, 6 );
          c += 7;
        }
      else {
        memcpy( buffer + c, str_move, 6 );
        c += 6;
      }
      buffer[c] = '\0';
    }
  Out( "%s\n", buffer );
  DFPNOut( "%s\n", buffer );

  return 1;
}


static int CONV cmd_problem( tree_t * restrict ptree, char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;
  unsigned int nposition;
  int iret;
#if defined(DFPN)
  int is_mate;
#endif

  AbortDifficultCommand;


#if defined(DFPN)
  is_mate = 0;
  if ( str != NULL && ! strcmp( str, "mate" ) )
    {
      is_mate = 1;
      str     = strtok_r( NULL, str_delimiters, lasts );
    }
#endif

  if ( str != NULL )
    {
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      nposition = (unsigned int)l;
    }
  else { nposition = UINT_MAX; }

  
  iret = record_open( &record_problems, "problem.csa", mode_read, NULL, NULL );
  if ( iret < 0 ) { return iret; }

#if defined(DFPN)
  iret = is_mate ? solve_mate_problems( ptree, nposition )
                 : solve_problems( ptree, nposition );
#else
  iret = solve_problems( ptree, nposition );
#endif

  if ( iret < 0 )
    {
      record_close( &record_problems );
      return iret;
    }

  iret = record_close( &record_problems );
  if ( iret < 0 ) { return iret; }

  return get_elapsed( &time_turn_start );
}


static int CONV cmd_quit( void )
{
  game_status |= flag_quit;
  return 1;
}


static int CONV cmd_suspend( void )
{
  if ( game_status & ( flag_pondering | flag_puzzling ) )
    {
      game_status |= flag_quit_ponder;
      return 2;
    }
  
  game_status |= flag_suspend;
  return 1;
}


static int CONV cmd_time( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  else if ( ! strcmp( str, "response" ) )
    {
      long l;
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 0 || l > 1000 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      time_response = (unsigned int)l;
      return 1;
    }
  else if ( ! strcmp( str, "remain" ) )
    {
      long l1, l2;
      
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l1 = strtol( str, &ptr, 0 );
      if ( ptr == str || l1 == LONG_MAX || l1 < 0 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l2 = strtol( str, &ptr, 0 );
      if ( ptr == str || l2 == LONG_MAX || l2 < 0 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      if ( sec_limit_up == UINT_MAX )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      return reset_time( (unsigned int)l1, (unsigned int)l2 );
    }

  str_error = str_bad_cmdline;
  return -2;
}


#if !defined(MINIMUM)
/* learn (ini|no-ini) steps games iterations tlp1 tlp2 */
static int CONV cmd_learn( tree_t * restrict ptree, char **lasts )
{
  const char *str1 = strtok_r( NULL, str_delimiters, lasts );
  const char *str2 = strtok_r( NULL, str_delimiters, lasts );
  const char *str3 = strtok_r( NULL, str_delimiters, lasts );
  const char *str4 = strtok_r( NULL, str_delimiters, lasts );
#  if defined(TLP)
  const char *str5 = strtok_r( NULL, str_delimiters, lasts );
  const char *str6 = strtok_r( NULL, str_delimiters, lasts );
#  endif
  char *ptr;
  long l;
  unsigned int max_games;
  int is_ini, nsteps, max_iterations, nworker1, nworker2, iret;

  if ( str1 == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  if      ( ! strcmp( str1, "ini" ) )    { is_ini = 1; }
  else if ( ! strcmp( str1, "no-ini" ) ) { is_ini = 0; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  max_games      = UINT_MAX;
  max_iterations = INT_MAX;
  nworker1 = nworker2 = nsteps = 1;

  if ( str2 != NULL )
    {
      l = strtol( str2, &ptr, 0 );
      if ( ptr == str2 || l == LONG_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      nsteps = (int)l;
    }

  if ( str3 != NULL )
    {
      l = strtol( str3, &ptr, 0 );
      if ( ptr == str3 || l == LONG_MAX || l == LONG_MIN )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      if ( l > 0 ) { max_games = (unsigned int)l; }
    }

  if ( str4 != NULL )
    {
      l = strtol( str4, &ptr, 0 );
      if ( ptr == str4 || l == LONG_MAX || l == LONG_MIN )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      if ( l > 0 ) { max_iterations = (int)l; }
    }

#  if defined(TLP)
  if ( str5 != NULL )
    {
      l = strtol( str5, &ptr, 0 );
      if ( ptr == str5 || l > TLP_MAX_THREADS || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      nworker1 = (int)l;
    }

  if ( str6 != NULL )
    {
      l = strtol( str6, &ptr, 0 );
      if ( ptr == str6 || l > TLP_MAX_THREADS || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      nworker2 = (int)l;
    }
#  endif

  AbortDifficultCommand;

  log2_ntrans_table = 12;

  memory_free( (void *)ptrans_table_orig );

  iret = ini_trans_table();
  if ( iret < 0 ) { return iret; }

  iret = learn( ptree, is_ini, nsteps, max_games, max_iterations,
		nworker1, nworker2 );
  if ( iret < 0 ) { return -1; }

  iret = ini_game( ptree, &min_posi_no_handicap, flag_history, NULL, NULL );
  if ( iret < 0 ) { return -1; }

  iret = get_elapsed( &time_turn_start );
  if ( iret < 0 ) { return iret; }

  return 1;
}
#endif /* MINIMUM */


#if defined(MPV)
static int CONV cmd_mpv( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );
  char *ptr;
  long l;

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  else if ( ! strcmp( str, "num" ) )
    {
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 1 || l > MPV_MAX_PV )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      AbortDifficultCommand;

      mpv_num = (int)l;

      return 1;
    }
  else if ( ! strcmp( str, "width" ) )
    {
      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < MT_CAP_PAWN )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      AbortDifficultCommand;

      mpv_width = (int)l;

      return 1;
    }

  str_error = str_bad_cmdline;
  return -2;
}
#endif


#if defined(DFPN)
static int CONV cmd_dfpn( tree_t * restrict ptree, char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  else if ( ! strcmp( str, "hash" ) )
    {
      char *ptr;
      long l;

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 1 )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      AbortDifficultCommand;

      dfpn_hash_log2 = (unsigned int)l;
      return dfpn_ini_hash();
    }
  else if ( ! strcmp( str, "go" ) )
    {
      AbortDifficultCommand;

      return dfpn( ptree, root_turn, 1 );
    }
  else if ( ! strcmp( str, "connect" ) )
    {
      char str_addr[256];
      char str_id[256];
      char *ptr;
      long l;
      int port;

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( ! str || ! strcmp( str, "." ) ) { str = "127.0.0.1"; }
      strncpy( str_addr, str, 255 );
      str_addr[255] = '\0';

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( ! str || ! strcmp( str, "." ) ) { str = "4083"; }
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 0 || l > USHRT_MAX )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      port = (int)l;

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( ! str || ! strcmp( str, "." ) ) { str = "bonanza1"; }
      strncpy( str_id, str, 255 );
      str_id[255] = '\0';

      AbortDifficultCommand;
      
      dfpn_sckt = sckt_connect( str_addr, port );
      if ( dfpn_sckt == SCKT_NULL ) { return -2; }

      str_buffer_cmdline[0] = '\0';
      DFPNOut( "Worker: %s\n", str_id );

      return 1;
    }

  str_error = str_bad_cmdline;
  return -2;
}
#endif


#if defined(TLP)
static int CONV cmd_thread( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  else if ( ! strcmp( str, "num" ) )
    {
      char *ptr;
      long l;

      str = strtok_r( NULL, str_delimiters, lasts );
      if ( str == NULL )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}
      l = strtol( str, &ptr, 0 );
      if ( ptr == str || l == LONG_MAX || l < 1 || l > TLP_MAX_THREADS )
	{
	  str_error = str_bad_cmdline;
	  return -2;
	}

      TlpEnd();

      tlp_max = (int)l;

      if ( game_status & ( flag_thinking | flag_pondering | flag_puzzling ) )
	{
	  return tlp_start();
	}
      return 1;
    }

  str_error = str_bad_cmdline;
  return -2;
}
#endif


#if defined(DFPN_CLIENT)
static int CONV cmd_dfpn_client( tree_t * restrict ptree, char **lasts )
{
  const char *str;
  char *ptr;
  int iret;

  AbortDifficultCommand;

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "127.0.0.1"; }
  strncpy( dfpn_client_str_addr, str, 255 );
  dfpn_client_str_addr[255] = '\0';

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "4083"; }
  dfpn_client_port = strtol( str, &ptr, 0 );
  if ( ptr == str || dfpn_client_port == LONG_MAX || dfpn_client_port < 0
       || dfpn_client_port > USHRT_MAX )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  Out( "DFPN Server: %s %d\n", dfpn_client_str_addr, dfpn_client_port );

  iret = ini_game( ptree, &min_posi_no_handicap, flag_history, NULL, NULL );
  if ( iret < 0 ) { return iret; }

  if ( dfpn_client_sckt == SCKT_NULL )
    {
      str_error = "Check network status.";
      return -1;
    }

  return get_elapsed( &time_turn_start );
}
#endif


#if defined(CSA_LAN)
static int CONV cmd_connect( tree_t * restrict ptree, char **lasts )
{
  const char *str;
  char *ptr;
  long max_games;

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "gserver.computer-shogi.org"; }
  strncpy( client_str_addr, str, 255 );
  client_str_addr[255] = '\0';

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "4081"; }
  client_port = strtol( str, &ptr, 0 );
  if ( ptr == str || client_port == LONG_MAX || client_port < 0
       || client_port > USHRT_MAX )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "bonanza_test"; }
  strncpy( client_str_id, str, 255 );
  client_str_id[255] = '\0';

  str = strtok_r( NULL, " \t", lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "bonanza_test"; }
  strncpy( client_str_pwd, str, 255 );
  client_str_pwd[255] = '\0';

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { client_max_game = INT_MAX; }
  else {
    max_games = strtol( str, &ptr, 0 );
    if ( ptr == str || max_games == LONG_MAX || max_games < 1 )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
    client_max_game = max_games;
  }

  AbortDifficultCommand;

  client_ngame          = 0;

  return client_next_game( ptree, client_str_addr, (int)client_port );
}


static int CONV cmd_sendpv( char **lasts )
{
  const char *str = strtok_r( NULL, str_delimiters, lasts );

  if ( str == NULL )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  if      ( ! strcmp( str, str_off ) ) {  game_status &= ~flag_sendpv; }
  else if ( ! strcmp( str, str_on ) )  {  game_status |=  flag_sendpv; }
  else {
    str_error = str_bad_cmdline;
    return -2;
  }

  return 1;
}
#endif


#if defined(MNJ_LAN)
/* mnj sd seed addr port name factor stable_depth */
static int CONV cmd_mnj( char **lasts )
{
  char client_str_addr[256];
  char client_str_id[256];
  const char *str;
  char *ptr;
  unsigned int seed;
  long l;
  int client_port, sd;
  double factor;

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  l = strtol( str, &ptr, 0 );
  if ( ptr == str || l == LONG_MAX || l < 0 )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  sd = (int)l;


  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  l = strtol( str, &ptr, 0 );
  if ( ptr == str || l == LONG_MAX || l < 0 )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  seed = (unsigned int)l;


  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "localhost"; }
  strncpy( client_str_addr, str, 255 );
  client_str_addr[255] = '\0';


  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "4082"; }
  l = strtol( str, &ptr, 0 );
  if ( ptr == str || l == LONG_MAX || l < 0 || l > USHRT_MAX )
    {
      str_error = str_bad_cmdline;
      return -2;
    }
  client_port = (int)l;


  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "bonanza1"; }
  strncpy( client_str_id, str, 255 );
  client_str_id[255] = '\0';

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { str = "1.0"; }
  factor = strtod( str, &ptr );
  if ( ptr == str || factor < 0.0 )
    {
      str_error = str_bad_cmdline;
      return -2;
    }

  str = strtok_r( NULL, str_delimiters, lasts );
  if ( ! str || ! strcmp( str, "." ) ) { l = -1; }
  else {
    l = strtol( str, &ptr, 0 );
    if ( ptr == str || l == LONG_MAX )
      {
	str_error = str_bad_cmdline;
	return -2;
      }
  }
  if ( l <= 0 ) { mnj_depth_stable = INT_MAX; }
  else          { mnj_depth_stable = (int)l; }

  AbortDifficultCommand;

  resign_threshold  = 65535;
  game_status      |= ( flag_noponder | flag_noprompt );
  if ( mnj_reset_tbl( sd, seed ) < 0 ) { return -1; }

  sckt_mnj = sckt_connect( client_str_addr, (int)client_port );
  if ( sckt_mnj == SCKT_NULL ) { return -2; }

  str_buffer_cmdline[0] = '\0';

  Out( "Sending my name %s", client_str_id );
  MnjOut( "%s %g final%s\n", client_str_id, factor,
	  ( mnj_depth_stable == INT_MAX ) ? "" : " stable" );

  return cmd_suspend();
}
#endif


