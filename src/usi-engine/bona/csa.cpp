// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "shogi.h"

static void out_CSA_header( const tree_t * restrict ptree, record_t *pr );
static int str2piece( const char *str );
static int skip_comment( record_t *pr );
static int read_char( record_t *pr );
static int read_CSA_line( record_t *pr, char *str );
static int in_CSA_header( tree_t * restrict ptree, record_t *pr, int flag );
static int read_board_rep2( const char *str_line, min_posi_t *pmin_posi );
static int read_board_rep3( const char *str_line, min_posi_t *pmin_posi );

int
read_record( tree_t * restrict ptree, const char *str_file,
	     unsigned int moves, int flag )
{
  record_t record;
  int iret;

  iret = record_open( &record, str_file, mode_read, NULL, NULL );
  if ( iret < 0 ) { return iret; }

  if ( ! moves )
    {
      iret = in_CSA_header( ptree, &record, flag );
      if ( iret < 0 )
	{
	  record_close( &record );
	  return iret;
	}
    }
  else do {
    iret = in_CSA( ptree, &record, NULL, flag );
    if ( iret < 0 )
      {
	record_close( &record );
	return iret;
      }
  } while ( iret != record_next
	    && iret != record_eof
	    && moves > record.moves );
  
  return record_close( &record );
}


int
record_open( record_t *pr, const char *str_file, record_mode_t record_mode,
	     const char *str_name1, const char *str_name2 )
{
  pr->games = pr->moves = pr->lines = 0;
  pr->str_name1[0] = '\0';
  pr->str_name2[0] = '\0';

  if ( str_name1 )
    {
      strncpy( pr->str_name1, str_name1, SIZE_PLAYERNAME-1 );
      pr->str_name1[SIZE_PLAYERNAME-1] = '\0';
    }
  
  if ( str_name2 )
    {
      strncpy( pr->str_name2, str_name2, SIZE_PLAYERNAME-1 );
      pr->str_name2[SIZE_PLAYERNAME-1] = '\0';
    }

  if ( record_mode == mode_write )
    {
      pr->pf = file_open( str_file, "w" );
      if ( pr->pf == NULL ) { return -2; }
    }
  else if ( record_mode == mode_read_write )
    {
      pr->pf = file_open( str_file, "wb+" );
      if ( pr->pf == NULL ) { return -2; }
    }
  else {
    assert( record_mode == mode_read );

    pr->pf = file_open( str_file, "rb" );
    if ( pr->pf == NULL ) { return -2; }
  }

  return 1;
}


int
record_close( record_t *pr )
{
  int iret = file_close( pr->pf );
  pr->pf = NULL;
  return iret;
}


void
out_CSA( tree_t * restrict ptree, record_t *pr, unsigned int move )
{
  const char *str_move;
  unsigned int sec;

  /* print move */
  if ( move == MOVE_RESIGN )
    {
      if ( ! pr->moves ) { out_CSA_header( ptree, pr ); }
      fprintf( pr->pf, "%s\n", str_resign );
      pr->lines++;
    }
  else {
    if ( ! pr->moves )
      {
	root_turn = Flip(root_turn);
	UnMakeMove( root_turn, move, 1 );
	out_CSA_header( ptree, pr );
	MakeMove( root_turn, move, 1 );
	root_turn = Flip(root_turn);
      }
    str_move = str_CSA_move( move );
    fprintf( pr->pf, "%c%s\n", ach_turn[Flip(root_turn)], str_move );
    pr->lines++;
    pr->moves++;
  }

  /* print time */
  sec = root_turn ? sec_b_total : sec_w_total;

  fprintf( pr->pf, "T%-7u,'%03u:%02u \n", sec_elapsed, sec / 60U, sec % 60U );
  pr->lines++;

  /* print repetition or mate status */
  if ( game_status & flag_mated )
    {
      fprintf( pr->pf, "%%TSUMI\n" );
      pr->lines++;
    }
  else if ( game_status & flag_drawn )
    {
      fprintf( pr->pf, "%s\n", str_repetition );
      pr->lines++;
    }

  fflush( pr->pf );
}


int
record_wind( record_t *pr )
{
  char str_line[ SIZE_CSALINE ];
  int iret;
  for (;;)
    {
      iret = read_CSA_line( pr, str_line );
      if ( iret < 0 ) { return iret; }
      if ( ! iret ) { return record_eof; }
      if ( ! strcmp( str_line, "/" ) ) { break; }
    }
  pr->games++;
  pr->moves = 0;
  return record_next;
}


#if ! defined(MINIMUM)
int
record_rewind( record_t *pr )
{
  pr->games = pr->moves = pr->lines = 0;
  if ( fseek( pr->pf, 0, SEEK_SET ) ) { return -2; }

  return 1;
}


int
record_getpos( record_t *pr, rpos_t *prpos )
{
  if ( fgetpos( pr->pf, &prpos->fpos ) )
    {
      str_error = "fgetpos() failed.";
      return -2;
    }
  prpos->games = pr->games;
  prpos->moves = pr->moves;
  prpos->lines = pr->lines;

  return 1;
}


int
record_setpos( record_t *pr, const rpos_t *prpos )
{
  if ( fsetpos( pr->pf, &prpos->fpos ) )
    {
      str_error = "fsetpos() failed.";
      return -2;
    }
  pr->games = prpos->games;
  pr->moves = prpos->moves;
  pr->lines = prpos->lines;

  return 1;
}
#endif /* no MINIMUM */


int
in_CSA( tree_t * restrict ptree, record_t *pr, unsigned int *pmove, int flag )
{
  char str_line[ SIZE_CSALINE ];
  char *ptr;
  unsigned int move;
  long l;
  int iret;
  
  if ( pr->moves == 0 )
    {
      iret = in_CSA_header( ptree, pr, flag );
      if ( iret < 0 ) { return iret; }
    }

  do {
    iret = read_CSA_line( pr, str_line );
    if ( iret < 0 ) { return iret; }
    if ( ! iret ) { return record_eof; }
    if ( ! strcmp( str_line, str_resign ) )
      {
	game_status |= flag_resigned;
	return record_resign;
      }
    if ( ! strcmp( str_line, str_repetition )
	 || ! strcmp( str_line, str_jishogi ) )
      {
	game_status |= flag_drawn;
	return record_drawn;
      }
    if ( ! strcmp( str_line, str_record_error ) )
      {
	return record_error;
      }
  } while ( str_line[0] == 'T' || str_line[0] == '%' );

  if ( ! strcmp( str_line, "/" ) )
    {
      pr->games++;
      pr->moves = 0;
      return record_next;
    }

  if ( game_status & mask_game_end )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_bad_record );
      str_error = str_message;
      return -2;
    }

  iret = interpret_CSA_move( ptree, &move, str_line+1 );
  if ( iret < 0 )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_error );
      str_error = str_message;
      return -2;
    }
  if ( pmove != NULL ) { *pmove = move; }

  /* do time */
  if ( flag & flag_time )
    {
      iret = read_CSA_line( pr, str_line );
      if ( iret < 0 ) { return iret; }
      if ( ! iret )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    pr->lines, str_unexpect_eof );
	  str_error = str_message;
	  return -2;
	}
      if ( str_line[0] != 'T' )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line, pr->lines,
		   "Time spent is not available." );
	  str_error = str_message;
	  return -2;
	}
      l = strtol( str_line+1, &ptr, 0 );
      if ( ptr == str_line+1 || l == LONG_MAX || l < 0 )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    pr->lines, str_bad_record );
	  str_error = str_message;
	  return -2;
	}
    }
  else { l = 0; }
  sec_elapsed = (unsigned int)l;
  if ( root_turn ) { sec_w_total += (unsigned int)l; }
  else             { sec_b_total += (unsigned int)l; }

  iret = make_move_root( ptree, move, flag & ~flag_time );
  if ( iret < 0 )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_error );
      str_error = str_message;
      return iret;
    }

  pr->moves++;

  return record_misc;
}


int
interpret_CSA_move( tree_t * restrict ptree, unsigned int *pmove,
		    const char *str )
{
  int ifrom_file, ifrom_rank, ito_file, ito_rank, ipiece;
  int ifrom, ito;
  unsigned int move;
  unsigned int *pmove_last;
  unsigned int *p;

  ifrom_file = str[0]-'0';
  ifrom_rank = str[1]-'0';
  ito_file   = str[2]-'0';
  ito_rank   = str[3]-'0';

  ito_file   = 9 - ito_file;
  ito_rank   = ito_rank - 1;
  ito        = ito_rank * 9 + ito_file;
  ipiece     = str2piece( str+4 );
  if ( ipiece < 0 )
    {
      str_error = str_illegal_move;
      return -2;
    }

  if ( ! ifrom_file && ! ifrom_rank )
    {
      move  = To2Move(ito) | Drop2Move(ipiece);
      ifrom = nsquare;
    }
  else {
    ifrom_file = 9 - ifrom_file;
    ifrom_rank = ifrom_rank - 1;
    ifrom      = ifrom_rank * 9 + ifrom_file;
    if ( abs(BOARD[ifrom]) + promote == ipiece )
      {
	ipiece -= promote;
	move    = FLAG_PROMO;
      }
    else { move = 0; }

    move |= ( To2Move(ito) | From2Move(ifrom) | Cap2Move(abs(BOARD[ito]))
	      | Piece2Move(ipiece) );
  }

  *pmove = 0;
  pmove_last = ptree->amove;
  pmove_last = GenCaptures(root_turn, pmove_last );
  pmove_last = GenNoCaptures(root_turn, pmove_last );
  pmove_last = GenCapNoProEx2(root_turn, pmove_last );
  pmove_last = GenNoCapNoProEx2(root_turn, pmove_last );
  pmove_last = GenDrop( root_turn, pmove_last );
  for ( p = ptree->amove; p < pmove_last; p++ )
    {
      if ( *p == move )
	{
	  *pmove = move;
	  break;
	}
    }
    
  if ( ! *pmove )
    {
      str_error = str_illegal_move;
      if ( ipiece == pawn
	   && ifrom == nsquare
	   && ! BOARD[ito]
	   && ( root_turn ? IsHandPawn(HAND_W) : IsHandPawn(HAND_B) ) )
	{
	  unsigned int u;

	  if ( root_turn )
	    {
	      u = BBToU( BB_WPAWN_ATK );
	      if ( u & (mask_file1>>ito_file) )
		{
		  str_error = str_double_pawn;
		}
	      else if ( BOARD[ito+nfile] == king )
		{
		  str_error = str_mate_drppawn;
		}
	    }
	  else {
	    u = BBToU( BB_BPAWN_ATK );
	    if ( u & (mask_file1>>ito_file) ) { str_error = str_double_pawn; }
	    else if ( BOARD[ito-nfile] == -king )
	      {
		str_error = str_mate_drppawn;
	      }
	  }
	}
      return -2;
    }
  
  return 1;
}


const char *
str_CSA_move( unsigned int move )
{
  static char str[7];
  std::string csa_str = string_CSA_move( move );
  if ( csa_str.size() >= 7 ) { printf("str_CSA_move() err.\n"); exit(0); }
  strncpy(str, csa_str.c_str(), 7);
  str[7-1] = 0;
  return str;
}

std::string string_CSA_move( unsigned int move ) {
  const int size = 7+8;	// +8 is "-Wformat-truncation" prevention
  char str[size];
  int ifrom, ito, ipiece_move, is_promote;

  is_promote  = (int)I2IsPromote(move);
  ipiece_move = (int)I2PieceMove(move);
  ifrom       = (int)I2From(move);
  ito         = (int)I2To(move);

  if ( is_promote )
    {
      snprintf( str, size, "%d%d%d%d%s",
		9-aifile[ifrom], airank[ifrom]+1,
		9-aifile[ito],   airank[ito]  +1,
		astr_table_piece[ ipiece_move + promote ] );
    }
  else if ( ifrom < nsquare )
    {
      snprintf( str, size, "%d%d%d%d%s",
		9-aifile[ifrom], airank[ifrom]+1,
		9-aifile[ito],   airank[ito]  +1,
		astr_table_piece[ ipiece_move ] );
    }
  else {
    snprintf( str, size, "00%d%d%s",
	      9-aifile[ito], airank[ito]+1,
	      astr_table_piece[ From2Drop(ifrom) ] );
  }

  std::string csa_str = str;
  return csa_str;
}

int
read_board_rep1( const char *str_line, min_posi_t *pmin_posi )
{
  const char *p;
  char str_piece[3];
  int piece, ifile, irank, isquare;
  signed char board[nsquare];

  memcpy( board, &min_posi_no_handicap.asquare, nsquare );

  for ( p = str_line + 2; p[0] != '\0'; p += 4 )
    {
      if ( p[1] == '\0' || p[2] == '\0' || p[3] == '\0' )
	{
	  str_error = str_bad_board;
	  return -2;
	}
      str_piece[0] = p[2];
      str_piece[1] = p[3];
      str_piece[2] = '\0';
      piece        = str2piece( str_piece );
      ifile        = p[0]-'0';
      irank        = p[1]-'0';
      ifile        = 9 - ifile;
      irank        = irank - 1;
      isquare      = irank * nfile + ifile;
      if ( piece == -2 || ifile < file1 || ifile > file9 || irank < rank1
	   || irank > rank9 || abs(board[isquare]) != piece )
	{
	  str_error = str_bad_board;
	  return -2;
	}
      board[isquare] = empty;
    }
  
  for ( isquare = 0; isquare < nsquare; isquare++ ) if ( board[isquare] )
    {
      if ( pmin_posi->asquare[isquare] )
	{
	  str_error = str_bad_board;
	  return -2;
	}
      pmin_posi->asquare[isquare] = board[isquare];
    }
  
  return 1;
}


#if defined(USI)
int CONV
usi2csa( const tree_t * restrict ptree, const char *str_usi, char *str_csa )
{
  int sq_file, sq_rank, sq, pc;

  if ( '1' <= str_usi[0] && str_usi[0] <= '9'
       && 'a' <= str_usi[1] && str_usi[1] <= 'i'
       && '1' <= str_usi[2] && str_usi[2] <= '9'
       && 'a' <= str_usi[3] && str_usi[3] <= 'i' )
    {
      str_csa[0] = str_usi[0];
      str_csa[1] = (char)( str_usi[1] + '1' - 'a' );
      str_csa[2] = str_usi[2];
      str_csa[3] = (char)( str_usi[3] + '1' - 'a' );

      sq_file = str_csa[0]-'0';
      sq_file = 9 - sq_file;
      sq_rank = str_csa[1]-'0';
      sq_rank = sq_rank - 1;
      sq      = sq_rank * 9 + sq_file;
      pc      = abs(BOARD[sq]);
      if ( str_usi[4] == '+' ) { pc += promote; }

      str_csa[4] = astr_table_piece[pc][0];
      str_csa[5] = astr_table_piece[pc][1];
      str_csa[6] = '\0';

      return 1;
    }

  if ( isascii( (int)str_usi[0] )
       && isalpha( (int)str_usi[0] )
       && str_usi[1] == '*'
       && '1' <= str_usi[2] && str_usi[2] <= '9'
       && 'a' <= str_usi[3] && str_usi[3] <= 'i' )
    {
      str_csa[0] = '0';
      str_csa[1] = '0';
      str_csa[2] = str_usi[2];
      str_csa[3] = (char)( str_usi[3] - 'a' + '1' );
      
      switch ( str_usi[0] )
	{
	case 'P':  pc = pawn;    break;
	case 'L':  pc = lance;   break;
	case 'N':  pc = knight;  break;
	case 'S':  pc = silver;  break;
	case 'G':  pc = gold;    break;
	case 'B':  pc = bishop;  break;
	case 'R':  pc = rook;    break;
	default:   return -1;
	}

      str_csa[4] = astr_table_piece[pc][0];
      str_csa[5] = astr_table_piece[pc][1];
      str_csa[6] = '\0';

      return 1;
    }

  snprintf( str_message, SIZE_MESSAGE, "%s: %s", str_illegal_move, str_usi );
  str_error = str_message;
  return -1;
}


int CONV
csa2usi( const tree_t * restrict ptree, const char *str_csa, char *str_usi )
{
  if ( str_csa[0] == '0' && str_csa[1] == '0'
       && '1' <= str_csa[2] && str_csa[2] <= '9'
       && '1' <= str_csa[3] && str_csa[3] <= '9'
       && 'A' <= str_csa[4] && str_csa[4] <= 'Z'
       && 'A' <= str_csa[5] && str_csa[5] <= 'Z' )
    {
      switch ( str2piece( str_csa + 4 ) )
	{
	case pawn:    str_usi[0] = 'P';  break;
	case lance:   str_usi[0] = 'L';  break;
	case knight:  str_usi[0] = 'N';  break;
	case silver:  str_usi[0] = 'S';  break;
	case gold:    str_usi[0] = 'G';  break;
	case bishop:  str_usi[0] = 'B';  break;
	case rook:    str_usi[0] = 'R';  break;
	default:  return -1;  break;
	}

      str_usi[1] = '*';
      str_usi[2] = str_csa[2];
      str_usi[3] = (char)( str_csa[3] + 'a' - '1' );
      str_usi[4] = '\0';

      return 1;
    }


  if ( '1' <= str_csa[0] && str_csa[0] <= '9'
       && '1' <= str_csa[1] && str_csa[1] <= '9'
       && '1' <= str_csa[2] && str_csa[2] <= '9'
       && '1' <= str_csa[3] && str_csa[3] <= '9'
       && 'A' <= str_csa[4] && str_csa[4] <= 'Z'
       && 'A' <= str_csa[5] && str_csa[5] <= 'Z' )
    {
      int sq_file, sq_rank, sq, pc;


      str_usi[0] = str_csa[0];
      str_usi[1] = (char)( str_csa[1] + 'a' - '1' );
      str_usi[2] = str_csa[2];
      str_usi[3] = (char)( str_csa[3] + 'a' - '1' );

      sq_file = str_csa[0]-'0';
      sq_file = 9 - sq_file;

      sq_rank = str_csa[1]-'0';
      sq_rank = sq_rank - 1;
      sq      = sq_rank * 9 + sq_file;
      pc      = abs(BOARD[sq]);

      if ( pc + promote == str2piece( str_csa + 4 ) )
	{
	  str_usi[4] = '+';
	  str_usi[5] = '\0';
	}
      else { str_usi[4] = '\0'; }


      return 1;
    }

  str_usi[0] = '*';
  str_usi[1] = '\0';
  return 1;
}
#endif


static void
out_CSA_header( const tree_t * restrict ptree, record_t *pr )
{
  time_t t;

  fprintf( pr->pf, "'" BNZ_NAME " version " BNZ_VER "\n" );

  if ( pr->str_name1[0] != '\0' )
    {
      fprintf( pr->pf, "N+%s\n", pr->str_name1 );
    }

  if ( pr->str_name2[0] != '\0' )
    {
      fprintf( pr->pf, "N-%s\n", pr->str_name2 );
    }

  t = time( NULL );
  if ( t == (time_t)-1 ) { out_warning( "%s time() faild." ); }
  else {
#if defined(_MSC_VER)
    struct tm tm;
    localtime_s( &tm, &t );
    fprintf( pr->pf, "$START_TIME:%4d/%02d/%02d %02d:%02d:%02d\n",
	     tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
	     tm.tm_hour, tm.tm_min, tm.tm_sec );
#else
    struct tm *ptm;
    ptm = localtime( &t );
    fprintf( pr->pf, "$START_TIME:%4d/%02d/%02d %02d:%02d:%02d\n",
	     ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
	     ptm->tm_hour, ptm->tm_min, ptm->tm_sec );
#endif
  }

  if ( ! memcmp( BOARD, min_posi_no_handicap.asquare, nsquare )
       && min_posi_no_handicap.turn_to_move == root_turn
       && min_posi_no_handicap.hand_black   == HAND_B
       && min_posi_no_handicap.hand_white   == HAND_W )
    {
      fprintf( pr->pf, "PI\n" );
      pr->lines++;
    }
  else {
    out_board( ptree, pr->pf, 0, 1 );
    pr->lines += 10;
  }
  
  if ( root_turn ) { fprintf( pr->pf, "-\n" ); }
  else             { fprintf( pr->pf, "+\n" ); }
  pr->lines++;
}


static int
in_CSA_header( tree_t * restrict ptree, record_t *pr, int flag )
{
  min_posi_t min_posi;
  const char *str_name1, *str_name2;
  char str_line[ SIZE_CSALINE ];
  int iret, is_rep1_done, is_rep2_done, is_all_done, i, j;

  for ( i = 0; i < MAX_ANSWER; i++ ) { pr->info.str_move[i][0] = '\0'; }
  str_name1 = str_name2 = NULL;

  /* version and info */
  for ( ;; )
    {
      iret = read_CSA_line( pr, str_line );
      if ( iret < 0 ) { return iret; }

      if ( str_line[0] != 'N'
	   && str_line[0] != 'V'
	   && str_line[0] != '$' ) { break; }

      if ( ! memcmp( str_line, "$ANSWER:", 8 ) )
	{
	  for ( i = 0; i < MAX_ANSWER; i++ )
	    {
	      for ( j = 0; j < 8; j++ )
		{
		  pr->info.str_move[i][j] = str_line[8+i*8+j];
		}
	      pr->info.str_move[i][7] = '\0';
	      if ( str_line[8+i*8+7] == '\0' ) { break; }
	    }
	  if ( i == MAX_ANSWER )
	    {
	      snprintf( str_message, SIZE_MESSAGE, str_fmt_line, pr->lines,
		       "The number of answers reached MAX_ANSWER." );
	      str_error = str_message;
	      return -2;
	    }
	}
      else if ( ! memcmp( str_line, "N+", 2 ) )
	{
	  strncpy( pr->str_name1, str_line+2, SIZE_PLAYERNAME-1 );
	  pr->str_name1[SIZE_PLAYERNAME-1] = '\0';
	  str_name1 = pr->str_name1;
	}
      else if ( ! memcmp( str_line, "N-", 2 ) )
	{
	  strncpy( pr->str_name2, str_line+2, SIZE_PLAYERNAME-1 );
	  pr->str_name2[SIZE_PLAYERNAME-1] = '\0';
	  str_name2 = pr->str_name2;
	}
    }
  if ( ! iret )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_unexpect_eof );
      str_error = str_message;
      return -2;
    }

  /* board representation */
  memset( &min_posi.asquare, empty, nsquare );
  min_posi.hand_black = min_posi.hand_white = 0;
  is_rep1_done = is_rep2_done = is_all_done = 0;
  while ( str_line[0] == 'P' )
    {
      if ( str_line[1] == 'I' && ! is_rep2_done && ! is_all_done )
	{
	  is_rep1_done = 1;
	  iret = read_board_rep1( str_line, &min_posi );
	}
      else if ( isdigit( (int)str_line[1] ) && str_line[1] != '0'
		&& ! is_rep1_done && ! is_all_done )
	{
	  is_rep2_done = 1;
	  iret = read_board_rep2( str_line, &min_posi );
	}
      else if ( str_line[1] == '+' || str_line[1] == '-' )
	{
	  is_all_done = iret = read_board_rep3( str_line, &min_posi );
	}
      else { break; }
      if ( iret < 0 )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    pr->lines, str_error );
	  str_error = str_message;
	  return iret;
	}

      iret = read_CSA_line( pr, str_line );
      if ( iret < 0 ) { return iret; }
      if ( ! iret )
	{
	  snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		    pr->lines, str_unexpect_eof );
	  str_error = str_message;
	  return -2;
	}
    }
  
  /* turn to move */
  if ( strcmp( str_line, "+" ) && strcmp( str_line, "-" ) )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_bad_record );
      str_error = str_message;
      return -2;
    }
  min_posi.turn_to_move = (char)( ( str_line[0] == '+' ) ? black : white );

  return ini_game( ptree, &min_posi, flag, str_name1, str_name2 );
}


static int
read_board_rep3( const char *str_line, min_posi_t *pmin_posi )
{
  int is_all_done, irank, ifile, isquare, piece, n, color;
  int npawn, nlance, nknight, nsilver, ngold, nbishop, nrook;
  unsigned int handv, hand_white, hand_black;
  char str_piece[3];

  is_all_done = 0;
  str_piece[2] = '\0';

  color = str_line[1] == '+' ? black : white;
  for ( n = 2; str_line[n] != '\0'; n += 4 ) {
    if ( str_line[n+1] == '\0' || str_line[n+2] == '\0'
	 || str_line[n+3] == '\0' || is_all_done )
      {
	str_error = str_bad_board;
	return -2;
      }
    if ( str_line[n] == '0' && str_line[n+1] == '0'
	 && str_line[n+2] == 'A' && str_line[n+3] == 'L' ) {
      hand_black = pmin_posi->hand_black;
      hand_white = pmin_posi->hand_white;
      npawn   = (int)(I2HandPawn(hand_black)   + I2HandPawn(hand_white));
      nlance  = (int)(I2HandLance(hand_black)  + I2HandLance(hand_white));
      nknight = (int)(I2HandKnight(hand_black) + I2HandKnight(hand_white));
      nsilver = (int)(I2HandSilver(hand_black) + I2HandSilver(hand_white));
      ngold   = (int)(I2HandGold(hand_black)   + I2HandGold(hand_white));
      nbishop = (int)(I2HandBishop(hand_black) + I2HandBishop(hand_white));
      nrook   = (int)(I2HandRook(hand_black)   + I2HandRook(hand_white));
      for ( isquare = 0; isquare < nsquare; isquare++ )
	switch ( abs( pmin_posi->asquare[isquare] ) )
	  {
	  case pawn:    case pro_pawn:    npawn++;    break;
	  case lance:   case pro_lance:   nlance++;   break;
	  case knight:  case pro_knight:  nknight++;  break;
	  case silver:  case pro_silver:  nsilver++;  break;
	  case gold:                      ngold++;    break;
	  case bishop:  case horse:       nbishop++;  break;
	  case rook:    case dragon:      nrook++;    break;
	  default:
	    assert( pmin_posi->asquare[isquare] == empty );
	    break;
	  }
      handv  = flag_hand_pawn   * ( npawn_max   -npawn );
      handv += flag_hand_lance  * ( nlance_max  -nlance );
      handv += flag_hand_knight * ( nknight_max -nknight );
      handv += flag_hand_silver * ( nsilver_max -nsilver );
      handv += flag_hand_gold   * ( ngold_max   -ngold );
      handv += flag_hand_bishop * ( nbishop_max -nbishop );
      handv += flag_hand_rook   * ( nrook_max   -nrook );
      if ( color ) { pmin_posi->hand_white += handv; }
      else         { pmin_posi->hand_black += handv; }
      is_all_done = 1;
      continue;
    }
    
    ifile        = str_line[n+0]-'0';
    irank        = str_line[n+1]-'0';
    str_piece[0] = str_line[n+2];
    str_piece[1] = str_line[n+3];
    piece        = str2piece( str_piece );
    
    /* hand */
    if ( ifile == 0 && ifile == 0 )
      {
	switch ( piece )
	  {
	  case pawn:    handv = flag_hand_pawn;    break;
	  case lance:   handv = flag_hand_lance;   break;
	  case knight:  handv = flag_hand_knight;  break;
	  case silver:  handv = flag_hand_silver;  break;
	  case gold:    handv = flag_hand_gold;    break;
	  case bishop:  handv = flag_hand_bishop;  break;
	  case rook:    handv = flag_hand_rook;    break;
	  default:
	    str_error = str_bad_board;
	    return -2;
	  }
	if ( color ) { pmin_posi->hand_white += handv; }
	else         { pmin_posi->hand_black += handv; }
      }
    /* board */
    else {
      ifile   = 9 - ifile;
      irank   = irank - 1;
      isquare = irank * nfile + ifile;
      if ( piece == -2 || ifile < file1 || ifile > file9
	   || irank < rank1 || irank > rank9 || pmin_posi->asquare[isquare] )
	{
	  str_error = str_bad_board;
	  return -2;
	}
      pmin_posi->asquare[isquare] = (signed char)( color ? -piece : piece );
    }
  }
  
  return is_all_done;
}


static int
read_board_rep2( const char * str_line, min_posi_t *pmin_posi )
{
  int irank, ifile, piece;
  char str_piece[3];

  str_piece[2] = '\0';
  
  irank = str_line[1] - '1';

  for ( ifile = 0; ifile < nfile; ifile++ )
    if ( str_line[2+ifile*3] == '+' || str_line[2+ifile*3] == '-' )
      {
	str_piece[0] = str_line[2+ifile*3+1];
	str_piece[1] = str_line[2+ifile*3+2];
	piece = str2piece( str_piece );
	if ( piece < 0 || pmin_posi->asquare[ irank*nfile + ifile ] )
	  {
	    str_error = str_bad_board;
	    return -2;
	  }
	pmin_posi->asquare[ irank*nfile + ifile ]
	  = (signed char)( str_line[ 2 + ifile*3 ] == '-' ? -piece : piece );
      }
    else { pmin_posi->asquare[ irank*nfile + ifile ] = empty; }

  return 1;
}


static int
str2piece( const char *str )
{
  int i;

  for ( i = 0; i < 16; i++ )
    {
      if ( ! strcmp( astr_table_piece[i], str ) ) { break; }
    }
  if ( i == 0 || i == piece_null || i == 16 ) { i = -2; }

  return i;
}


/* reads a csa line in str, trancates trailing spases.
 * return value:
 *   0  EOF, no line is read.
 *   1  a csa line is read in str
 *  -2  buffer overflow
 */
static int
read_CSA_line( record_t *pr, char *str )
{
  int i, c, do_comma;

  for ( ;; )
    {
      c = skip_comment( pr );
      if ( isgraph( c ) || c == EOF ) { break; }
    }
  if ( c == EOF ) { return 0; }
  
  do_comma = ( c == 'N' || c == '$' ) ? 0 : 1;

  for ( i = 0; i < SIZE_CSALINE-1; i++ )
    {
      if ( c == EOF || c == '\n' || ( do_comma && c == ',' ) ) { break; }
      str[i] = (char)c;
      c = read_char( pr );
    }

  if ( i == SIZE_CSALINE-1 )
    {
      snprintf( str_message, SIZE_MESSAGE, str_fmt_line,
		pr->lines, str_ovrflw_line );
      return -2;
    }

  i--;
  while ( isascii( (int)str[i] ) && isspace( (int)str[i] ) ) { i--; }
  str[i+1] = '\0';

  return 1;
}


static int
skip_comment( record_t *pr )
{
  int c;

  c = read_char( pr );
  for ( ;; )
    {
      if ( c != '\'' ) { break; }
      for ( ;; )
	{
	  c = read_char( pr );
	  if ( c == EOF || c == '\n' ) { break; }
	}
    }
  
  return c;
}


static int
read_char( record_t *pr )
{
  int c;

  c = fgetc( pr->pf );
  if ( c == '\n' ) { pr->lines++; }

  return c;
}
