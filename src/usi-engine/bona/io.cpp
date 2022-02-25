// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#if defined(_WIN32)
#  include <io.h>
#  include <conio.h>
#else
#  include <sys/time.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif
#include "shogi.h"

#if defined(_MSC_VER)
#  include <Share.h>
#  define fopen( file, mode ) _fsopen( file, mode, _SH_DENYNO )
#endif

#if defined(NO_STDOUT) || defined(WIN32_PIPE)
static int out_board0( FILE *pf, int piece, int i, int ito, int ifrom );
#  define OutBoard0(a,b,c,d,e,f) out_board0(a,b,c,d,e)
#else
static int out_board0( FILE *pf, int piece, int i, int ito, int ifrom,
		       int is_promote );
#  define OutBoard0(a,b,c,d,e,f) out_board0(a,b,c,d,e,f)
#endif

static void CONV out_file( FILE *pf, const char *format, ... );
static int CONV check_input_buffer( void );
static int CONV read_command( char **pstr_line_end );
static void CONV out_hand( FILE *pf, unsigned int hand,
			   const char *str_prefix );
static void CONV out_hand0( FILE *pf, int n, const char *str_prefix,
			    const char *str );

#if ! ( defined(NO_STDOUT) && defined(NO_LOGGING) )
void
out( const char *format, ... )
{
  va_list arg;

#  if ! defined(NO_STDOUT)
  if ( !( game_status & flag_nostdout ) )
    {
      va_start( arg, format );
      vprintf( format, arg );
      va_end( arg );
      fflush( stdout );
    }
#  endif

#  if ! defined(NO_LOGGING)
  if ( ( strchr( format, '\n' ) != NULL || strchr( format, '\r' ) == NULL )
       && pf_log != NULL )
    {
      va_start( arg, format );
      vfprintf( pf_log, format, arg ); 
      va_end( arg );
      fflush( pf_log );
    }
#  endif
}
#endif


#if defined(USI)
void CONV
usi_out( const char *format, ... )
{
  va_list arg;

  va_start( arg, format );
  vprintf( format, arg );
  va_end( arg );
  fflush( stdout );

#  if ! defined(NO_LOGGING)
  if ( ( strchr( format, '\n' ) != NULL || strchr( format, '\r' ) == NULL )
       && pf_log != NULL )
    {
      fprintf( pf_log, "OUT: " );
      va_start( arg, format );
      vfprintf( pf_log, format, arg ); 
      va_end( arg );
      fflush( pf_log );
    }
#  endif
}
#endif


#if defined(CSASHOGI)
void
out_csashogi( const char *format, ... )
{
  va_list arg;

  va_start( arg, format );
  vprintf( format, arg );
  va_end( arg );

  fflush( stdout );
}
#endif


void
out_warning( const char *format, ... )
{
  va_list arg;

#if defined(TLP) || defined(DFPN_CLIENT)
  lock( &io_lock );
#endif

  if ( !( game_status & flag_nostdout ) )
    {
      fprintf( stderr, "\n%s", str_warning );
      va_start( arg, format );
      vfprintf( stderr, format, arg );
      va_end( arg );
      fprintf( stderr, "\n\n" );
      fflush( stderr );
    }

#if ! defined(NO_LOGGING)
  if ( pf_log != NULL )
    {
      fprintf( pf_log, "\n%s", str_warning );
      va_start( arg, format );
      vfprintf( pf_log, format, arg ); 
      va_end( arg );
      fprintf( pf_log, "\n\n" );
      fflush( pf_log );
    }
#endif

#if defined(TLP) || defined(DFPN_CLIENT)
  unlock( &io_lock );
#endif
}


void
out_error( const char *format, ... )
{
  va_list arg;
  
  if ( !( game_status & flag_nostdout ) )
    {
      fprintf( stderr, "\nERROR: " );
      va_start( arg, format );
      vfprintf( stderr, format, arg );
      va_end( arg );
      fprintf( stderr, "\n\n" );
      fflush( stderr );
    }

#if ! defined(NO_LOGGING)
  if ( pf_log != NULL )
    {
      fprintf( pf_log, "\nERROR: " );
      va_start( arg, format );
      vfprintf( pf_log, format, arg );
      va_end( arg );
      fprintf( pf_log, "\n\n" );
      fflush( pf_log );
    }
#endif
  
}


FILE *
file_open( const char *str_file, const char *str_mode )
{
  FILE *pf;

  pf = fopen( str_file, str_mode );
  if ( pf == NULL )
    {
      snprintf( str_message, SIZE_MESSAGE,
		"%s, %s", str_fopen_error, str_file );
      str_error = str_message;
      return NULL;
    }
  
  return pf;
}


int
file_close( FILE *pf )
{
  if ( pf == NULL ) { return 1; }
  if ( ferror( pf ) )
    {
      fclose( pf );
      str_error = str_io_error;
      return -2;
    }
  if ( fclose( pf ) )
    {
      str_error = str_io_error;
      return -2;
    }

  return 1;
}


void
show_prompt( void )
{
  if ( game_status & flag_noprompt ) { return; }
  
  if ( game_status & flag_drawn ) { Out( "Drawn> " ); }
  else if ( game_status & flag_mated )
    {
      if ( root_turn ) { Out( "Black Mated> " ); }
      else             { Out( "White Mated> " ); }
    }
  else if ( game_status & flag_resigned )
    {
      if ( root_turn ) { Out( "White Resigned> " ); }
      else             { Out( "Black Resigned> " ); }
    }
  else if ( game_status & flag_suspend )
    {
      if ( root_turn ) { Out( "White Suspend> " ); }
      else             { Out( "Black Suspend> " ); }
    }
  else if ( root_turn ) { Out( "White %d> ", record_game.moves+1 ); }
  else                  { Out( "Black %d> ", record_game.moves+1 ); }
}


int
open_history( const char *str_name1, const char *str_name2 )
{
#if defined(YSS_ZERO)
  return 1;
#endif
#if defined(NO_LOGGING)
//  char str_file[SIZE_FILENAME];
  int iret;

  iret = record_close( &record_game );
  if ( iret < 0 ) { return -1; }

  iret = record_open( &record_game, "game.csa", mode_read_write,
		      str_name1, str_name2 );
  if ( iret < 0 ) { return -1; }

  return 1;
#else
  FILE *pf;
  int i, iret;
  char str_file[SIZE_FILENAME];
  
  if ( record_game.pf != NULL && ! record_game.moves )
    {
      record_game.str_name1[0] = '\0';
      record_game.str_name2[0] = '\0';

      if ( str_name1 )
	{
	  strncpy( record_game.str_name1, str_name1, SIZE_PLAYERNAME-1 );
	  record_game.str_name1[SIZE_PLAYERNAME-1] = '\0';
	}
      
      if ( str_name2 )
	{
	  strncpy( record_game.str_name2, str_name2, SIZE_PLAYERNAME-1 );
	  record_game.str_name2[SIZE_PLAYERNAME-1] = '\0';
	}
      return 1;
    }

  if ( ( ( game_status & flag_nonewlog )
#  if defined(USI)
	 ||  usi_mode != usi_off
#  endif
	 ) && 0 <= record_num )
    {
      iret = record_close( &record_game );
      if ( iret < 0 ) { return -1; }
      
      snprintf( str_file, SIZE_FILENAME, "%s/game%03d.csa",
		str_dir_logs, record_num );
      iret = record_open( &record_game, str_file, mode_read_write,
			  str_name1, str_name2 );
      if ( iret < 0 ) { return -1; }
    }
  else
    {
      iret = file_close( pf_log );
      if ( iret < 0 ) { return -1; }
      
      iret = record_close( &record_game );
      if ( iret < 0 ) { return -1; }
      
      for ( i = 0; i < 999; i++ )
	{
	  snprintf( str_file, SIZE_FILENAME, "%s/game%03d.csa",
		    str_dir_logs, i );
	  pf = file_open( str_file, "r" );
	  if ( pf == NULL ) { break; }
	  iret = file_close( pf );
	  if ( iret < 0 ) { return -1; }
	}
      record_num = i;
      
      snprintf( str_file, SIZE_FILENAME, "%s/n%03d.log",
		str_dir_logs, i );
      pf_log = file_open( str_file, "w" );
      if ( pf_log == NULL ) { return -1; }
      
      snprintf( str_file, SIZE_FILENAME, "%s/game%03d.csa",
		str_dir_logs, i );
      iret = record_open( &record_game, str_file, mode_read_write,
			  str_name1, str_name2 );
      if ( iret < 0 ) { return -1; }
    }
  
  return 1;
#endif
}


int
out_board( const tree_t * restrict ptree, FILE *pf, unsigned int move,
	   int is_strict )
{
  int irank, ifile, i, iret, ito, ifrom;

#if ! defined(WIN32_PIPE)
  int is_promote;
#endif

  if ( game_status & flag_nostdout ) { return 1; }

  if ( ! is_strict && move )
    {
      ito        = I2To( move );
      ifrom      = I2From( move );
#if ! defined(NO_STDOUT) && ! defined(WIN32_PIPE)
      is_promote = I2IsPromote( move );
#endif
    }
  else {
    ito = ifrom = nsquare;
#if ! defined(NO_STDOUT) && ! defined(WIN32_PIPE)
    is_promote = 0;
#endif
  }
  
  if ( ( game_status & flag_reverse ) && ! is_strict )
    {
      fprintf( pf, "          <reversed>        \n" );
      fprintf( pf, "'  1  2  3  4  5  6  7  8  9\n" );

      for ( irank = rank9; irank >= rank1; irank-- )
	{
	  fprintf( pf, "P%d", irank + 1 );
	  
	  for ( ifile = file9; ifile >= file1; ifile-- )
	    {
	      i = irank * nfile + ifile;
	      iret = OutBoard0( pf, BOARD[i], i, ito, ifrom, is_promote );
	      if ( iret < 0 ) { return iret; }
	    }
	  fprintf( pf, "\n" );
	}
    }
  else {
    fprintf( pf, "'  9  8  7  6  5  4  3  2  1\n" );

    for ( irank = rank1; irank <= rank9; irank++ )
      {
	fprintf( pf, "P%d", irank + 1 );
	
	for ( ifile = file1; ifile <= file9; ifile++ )
	  {
	    i = irank * nfile + ifile;
	    iret = OutBoard0( pf, BOARD[i], i, ito, ifrom, is_promote );
	    if ( iret < 0 ) { return iret; }
	  }
	fprintf( pf, "\n" );
      }
  }
    
  out_hand( pf, HAND_B, "P+" );
  out_hand( pf, HAND_W, "P-" );
#if defined(YSS_ZERO)
//  record_t *pr = &record_game;	// ’Êí‚Í‹ó‚Á‚ÛH
//  fprintf( pf, "moves=%d, pr->games=%d,moves=%d,lines=%d\n",ptree->nrep, pr->games,pr->moves,pr->lines );
  fprintf( pf, "moves=%3d(%d), turn %c, nHandicap=%d, seq_hash=%016" PRIx64 ,ptree->nrep+sfen_current_move_number,sfen_current_move_number, ach_turn[(root_turn)&1], nHandicap, ptree->sequence_hash );
  if ( ptree->nrep > 0 ) {
    const min_posi_t *p0 = &ptree->record_plus_ply_min_posi[ptree->nrep-0];
    const min_posi_t *p1 = &ptree->record_plus_ply_min_posi[ptree->nrep-1];
	int x,y,n=0,diff[2],d[2];
	diff[0] = diff[1] = 0;
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int z = y*9 + x;
//		fprintf( pf, "%d,",p0->asquare[z]);
		if ( p0->asquare[z] == p1->asquare[z] ) continue;
		if ( n == 2 ) continue;
		d[n] = (y+1) + (10 - (x+1))*10;
		diff[n++] = z;	// •ÏX‚ª1‚©Š‚È‚ç‹î‘Å
	}
	int bz = diff[0];
	int az = diff[1];
	int b0 = d[0];
	int a0 = d[1];
	if ( n==2 ) {
		if ( p0->asquare[bz] != 0 ) {
//			bz = diff[1];
			az = diff[0];
			b0 = d[1];
			a0 = d[0];
		}
	} else {
		b0 = 0;
		a0 = d[0];
		az = diff[0];
	}
//	fprintf( pf, "%d,02d%d%s\n",n,b0,a0,astr_table_piece[abs(p0->asquare[az])]);
	fprintf( pf, ", %c%02d%d%s\n",ach_turn[(root_turn+1)&1],b0,a0,astr_table_piece[abs(p0->asquare[az])]);
  }
  fprintf( pf, "\n");
#endif
  fflush( pf );

  return 1;
}

#if defined(YSS_ZERO)
void print_board(const tree_t * restrict ptree)
{
    game_status &= game_status & (~flag_nostdout);
    out_board( ptree, stderr, 0, 0 );
    game_status |= flag_nostdout;
}
#endif

int
next_cmdline( int is_wait )
{
  char *str_line_end;
  size_t size;
  int iret;

  str_line_end = strchr( str_buffer_cmdline, '\n' );

  if ( ! str_line_end )
    {
      if ( is_wait )
	{
	  do {
	    iret = read_command( & str_line_end );
	    if ( iret < 0 ) { return iret; }
	  } while ( ! str_line_end && iret );
	  if ( ! iret )
	    {
	      game_status |= flag_quit;
	      return 1;
	    }
	}
      else {
	iret = check_input_buffer();
	if ( iret <= 0 ) { return iret; }

	iret = read_command( & str_line_end );
	if ( iret < 0 ) { return iret; }
	if ( ! iret )
	  {
	    game_status |= flag_quit;
	    return 1;
	  }
	if ( ! str_line_end ) { return 0; }
      }
    }
  
  if ( str_line_end - str_buffer_cmdline + 1 >= SIZE_CMDLINE )
    {
      str_error = str_ovrflw_line;
      memmove( str_buffer_cmdline, str_line_end + 1,
	       strlen( str_line_end + 1 ) + 1 );
      return -2;
    }
  
  size = str_line_end - str_buffer_cmdline;
  memcpy( str_cmdline, str_buffer_cmdline, size );
  *( str_cmdline + size ) = '\0';

#if defined(USI)
  // ShogiDokoro sends "\r\n"
//  fprintf(stderr,"hoge=%d'%s'\n",size,str_cmdline);
  if ( size >= 1 ) {
    char *p = str_cmdline + size;
    if ( *(p-1) == '\r' ) {
      *(p-1) = 0;
    }
  }
#endif
  
  if ( is_wait )
    {
      out_file( NULL, "%s\n", str_cmdline );
      memmove( str_buffer_cmdline, str_line_end + 1,
	       strlen( str_line_end + 1 ) + 1 );
    }

  return 1;
}


static int
#if defined(NO_STDOUT) || defined(WIN32_PIPE)
out_board0( FILE *pf, int piece, int i, int ito, int ifrom )
#else
out_board0( FILE *pf, int piece, int i, int ito, int ifrom, int is_promote )
#endif
{
  int ch;
#if ! ( defined(NO_STDOUT) || defined(WIN32_PIPE) )
  int iret;
#endif

  if ( piece )
    {
      ch = piece < 0 ? '-' : '+';
#if ! ( defined(NO_STDOUT) || defined(WIN32_PIPE) )
      if ( i == ito && pf == stdout && ! ( game_status & flag_nostress ) )
	{
	  iret = StdoutStress( is_promote, ifrom );
	  if ( iret < 0 )
	    {
	      fprintf( pf, "\n" );
	      return iret;
	    }
	}
#endif
      fprintf( pf, "%c%s", ch, astr_table_piece[ abs( piece ) ] );
#if ! ( defined(NO_STDOUT) || defined(WIN32_PIPE) )
      if ( i == ito && pf == stdout && ! ( game_status & flag_nostress ) )
	{
	  iret = StdoutNormal();
	  if ( iret < 0 )
	    {
	      fprintf( pf, "\n" );
	      return iret;
	    }
	}
#endif
    }
  else {
    if ( ifrom < nsquare
	 && ( ifrom == i
	      || ( adirec[ito][ifrom]
		   && adirec[ito][ifrom] == adirec[ito][i]
		   && ( ( ito < i && i <= ifrom )
			|| ( ito > i && i >= ifrom ) ) ) ) )
      {
	fprintf( pf, "   " );
      }
    else { fprintf( pf, " * " ); }
  }

  return 1;
}


#if ! defined(NO_STDOUT) && ! defined(WIN32_PIPE)

void
out_beep( void )
{
  if ( game_status & flag_nobeep ) { return; }

#  if defined(_WIN32)
  if ( ! MessageBeep( MB_OK ) )
    {
      out_warning( "Beep is not available." );
    }
#  else
  printf( "\007" );
#  endif
}


int
stdout_normal( void )
{
#  if defined(_WIN32)
  HANDLE hStdout;
  WORD wAttributes;
  
  hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
  if ( hStdout == INVALID_HANDLE_VALUE )
    {
      str_error = "GetStdHandle() faild";
      return -1;
    }

  wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
  if ( ! SetConsoleTextAttribute( hStdout, wAttributes ) )
    {
      str_error = "SetConsoleTextAttribute() faild";
      return -1;
    }

#  else
  printf( "\033[0m" );
#  endif
  return 1;
}


int
stdout_stress( int is_promote, int ifrom )
{
#  if defined(_WIN32)
  HANDLE hStdout;
  WORD wAttributes;

  hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
  if ( hStdout == INVALID_HANDLE_VALUE )
    {
      str_error = "GetStdHandle() faild";
      return -1;
    }

  if ( is_promote )
    {
      wAttributes = BACKGROUND_RED | BACKGROUND_INTENSITY;
    }
  else if ( ifrom >= nsquare )
    {
      wAttributes = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
    }
  else {
    wAttributes = ( BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE
		    | BACKGROUND_INTENSITY );
  }
  if ( ! SetConsoleTextAttribute( hStdout, wAttributes ) )
    {
      str_error = "SetConsoleTextAttribute() faild";
      return -1;
    }
#  else
  if      ( is_promote )       { printf( "\033[7;31m" ); }
  else if ( ifrom >= nsquare ) { printf( "\033[7;34m" ); }
  else                         { printf( "\033[7m" ); }
#  endif

  return 1;
}

#endif /* no NO_STDOUT and no WIN32_PIPE */


static void CONV
out_hand( FILE *pf, unsigned int hand, const char *str_prefix )
{
  out_hand0( pf, (int)I2HandPawn(hand),   str_prefix, "00FU" );
  out_hand0( pf, (int)I2HandLance(hand),  str_prefix, "00KY" );
  out_hand0( pf, (int)I2HandKnight(hand), str_prefix, "00KE" );
  out_hand0( pf, (int)I2HandSilver(hand), str_prefix, "00GI" );
  out_hand0( pf, (int)I2HandGold(hand),   str_prefix, "00KI" );
  out_hand0( pf, (int)I2HandBishop(hand), str_prefix, "00KA" );
  out_hand0( pf, (int)I2HandRook(hand),   str_prefix, "00HI" );
}


static void CONV
out_hand0( FILE *pf, int n, const char *str_prefix, const char *str )
{
  int i;

  if ( n > 0 )
    {
      fprintf( pf, "%s", str_prefix );
      for ( i = 0; i < n; i++ ) { fprintf( pf, "%s", str ); }
      fprintf( pf, "\n" );
    }
}


static int CONV
read_command( char ** pstr_line_end )
{
  char *str_end;
  int count_byte, count_cmdbuff;

  count_cmdbuff = (int)strlen( str_buffer_cmdline );
  str_end       = str_buffer_cmdline + count_cmdbuff;

#if defined(CSA_LAN)
  if ( sckt_csa != SCKT_NULL )
    {
      count_byte = sckt_in( sckt_csa, str_end, SIZE_CMDLINE-1-count_cmdbuff );
      if ( count_byte < 0 ) { return count_byte; }
      goto tag;
    }
#endif

#if defined(MNJ_LAN)
  if ( sckt_mnj != SCKT_NULL )
    {
      count_byte = sckt_in( sckt_mnj, str_end, SIZE_CMDLINE-1-count_cmdbuff );
      if ( count_byte < 0 ) { return count_byte; }
      goto tag;
    }
#endif

#if defined(DFPN)
  if ( dfpn_sckt != SCKT_NULL )
    {
      count_byte = sckt_in( dfpn_sckt, str_end, SIZE_CMDLINE-1-count_cmdbuff );
      if ( count_byte < 0 ) { return count_byte; }
      goto tag;
    }
#endif

  do { count_byte = (int)read( 0, str_end, SIZE_CMDBUFFER-1-count_cmdbuff ); }
  while ( count_byte < 0 && errno == EINTR );
  
  if ( count_byte < 0 )
    {
      str_error = "read() faild.";
      return -1;
    }
  *( str_end + count_byte ) = '\0';

#if defined(CSA_LAN) || defined(MNJ_LAN) || defined(DFPN)
 tag:
#endif

#if defined(USI)
  if ( usi_mode != usi_off ) { Out( "IN: %s[END]\n", str_end );}
#endif

  *pstr_line_end = strchr( str_buffer_cmdline, '\n' );
  if ( *pstr_line_end == NULL
       && count_byte + count_cmdbuff + 1 >= SIZE_CMDLINE )
    {
      *str_buffer_cmdline = '\0';
      str_error = str_ovrflw_line;
      return -2;
    }

  return count_byte;
}


static int CONV
check_input_buffer( void )
{
#if defined(CSA_LAN)
  if ( sckt_csa != SCKT_NULL ) { return sckt_check( sckt_csa ); }
#endif
  
#if defined(MNJ_LAN)
  if ( sckt_mnj != SCKT_NULL ) { return sckt_check( sckt_mnj ); }
#endif

#if defined(DFPN)
  if ( dfpn_sckt != SCKT_NULL ) { return sckt_check( dfpn_sckt ); }
#endif

  {
#if defined(_WIN32) && defined(WIN32_PIPE)
    BOOL bSuccess;
    HANDLE hHandle;
    DWORD dwBytesRead, dwTotalBytesAvail, dwBytesLeftThisMessage;
    char buf[1];

    hHandle = GetStdHandle( STD_INPUT_HANDLE );
    if ( hHandle == INVALID_HANDLE_VALUE )
      {
	str_error = "GetStdHandle() faild.";
	return -1;
      }
    bSuccess = PeekNamedPipe( hHandle, buf, 1, &dwBytesRead,
			      &dwTotalBytesAvail, &dwBytesLeftThisMessage );
    if ( ! bSuccess )
      {
	str_error = "PeekNamedPipe() faild.";
	return -1;
      }
    if ( dwBytesRead ) { return 1; }
    return 0;

#elif defined(_WIN32)

    return _kbhit();

#else

    fd_set readfds;
    struct timeval tv;
    int iret;

#  if defined(__ICC)
#    pragma warning(disable:279)
#    pragma warning(disable:593)
#    pragma warning(disable:1469)
#  endif
    
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    tv.tv_sec  = 0;
    tv.tv_usec = 0;
    iret       = select( 1, &readfds, NULL, NULL, &tv );
    if ( iret == -1 )
      {
	str_error = "select() faild.";
	return -1;
      }
    return iret;
    
#  if defined(__ICC)
#    pragma warning(default:279)
#    pragma warning(default:593)
#    pragma warning(default:1469)
#  endif
#endif /* no _WIN32 */
  }
}


static void CONV
out_file( FILE *pf, const char *format, ... )
{
  va_list arg;

  if ( pf != NULL )
    {
      va_start( arg, format );
      vfprintf( pf, format, arg ); 
      va_end( arg );
      fflush( pf );
    }
  
#if ! defined(NO_LOGGING)
  if ( pf_log != NULL )
    {
      va_start( arg, format );
      vfprintf( pf_log, format, arg ); 
      va_end( arg );
      fflush( pf_log );
    }
#endif

}
