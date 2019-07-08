// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include "shogi.h"

/*
  Opening Book Data Structure: Index BookData

    Index:         IndexEntry.. (NUM_SECTION times)

      IndexEntry:  SectionPointer SectionSize

    BookData:      Section.. (NUM_SECTION times)

      Section:     [DataEntry]..

        DataEntry: Header Move...


- SectionPointer
  4 byte:  position of the section in character

- SectionSize
  2 byte:  size of the section in character
 
- Header
  1 byte:  number of bytes for the DataEntry
  8 byte:  hash key
  
- Move
  2 byte:  a book move
  2 byte:  frequency
 */

#define BK_SIZE_INDEX     6
#define BK_SIZE_HEADER    9
#define BK_SIZE_MOVE      4
#define BK_MAX_MOVE       32

#if ( BK_SIZE_HEADER + BK_SIZE_MOVE * BK_MAX_MOVE > UCHAR_MAX )
#  error "Maximum size of DataEntry is larger than UCHAR_MAX"
#endif

typedef struct { unsigned short smove, freq; } book_move_t;

typedef struct { int from, to; } ft_t;

static int CONV book_read( uint64_t key, book_move_t *pbook_move,
		      unsigned int *pposition );
static uint64_t CONV book_hash_func( const tree_t * restrict ptree,
				     int *pis_flip );
static unsigned int CONV bm2move( const tree_t * restrict ptree,
				  unsigned int bmove, int is_flip );
static ft_t CONV flip_ft( ft_t ft, int turn, int is_flip );
static int CONV normalize_book_move( book_move_t * restrict pbook_move,
				     int moves );


int CONV
book_on( void )
{
  int iret = file_close( pf_book );
  if ( iret < 0 ) { return iret; }

  pf_book = file_open( str_book, "rb+" );
  if ( pf_book == NULL ) { return -2; }

  return 1;
}


int CONV
book_off( void )
{
  int iret = file_close( pf_book );
  if ( iret < 0 ) { return iret; }

  pf_book = NULL;

  return 1;
}


int CONV
book_probe( tree_t * restrict ptree )
{
  book_move_t abook_move[ BK_MAX_MOVE+1 ];
  uint64_t key;
  double dscore, drand;
  unsigned int move, position, freq_lower_limit;
  int is_flip, i, j, moves, ply;

  key   = book_hash_func( ptree, &is_flip );

  moves = book_read( key, abook_move, &position );
  if ( moves <= 0 ) { return moves; }

#if ! defined(MINIMUM) || ! defined(NDEBUG)
  for ( j = i = 0; i < moves; i++ ) { j += abook_move[i].freq; }
  if ( j != USHRT_MAX )
    {
      str_error = "normalization error (book.bin)";
      return -1;
    }
#endif

  /* decision of book move based on pseudo-random number */
  if ( game_status & flag_puzzling ) { j = 0; }
  else {
    drand = (double)rand64() / (double)UINT64_MAX;

    if ( game_status & flag_narrow_book )
      {
#if defined(BK_ULTRA_NARROW)
	freq_lower_limit = abook_move[0].freq;
#else
	freq_lower_limit = abook_move[0].freq / 2U;
#endif

	for ( i = 1; i < moves; i++ )
	  {
	    if ( abook_move[i].freq < freq_lower_limit ) { break; }
	  }
	moves = i;
	normalize_book_move( abook_move, moves );
      }

    for ( j = moves-1; j > 0; j-- )
      {
	dscore = (double)( abook_move[j].freq ) / (double)USHRT_MAX;
	if ( drand <= dscore ) { break; }
	drand -= dscore;
      }
    if ( ! abook_move[j].freq ) { j = 0; }
  }

  /* show results */
  if ( ! ( game_status & ( flag_pondering | flag_puzzling ) ) )
    {
      Out( "    move     freq\n" );
      OutCsaShogi( "info" );
      for ( i = 0; i < moves; i++ )
	{
	  const char *str;
	  
	  dscore = (double)abook_move[i].freq / (double)USHRT_MAX;
	  move = bm2move( ptree, (unsigned int)abook_move[i].smove, is_flip );
	  str  = str_CSA_move( move );
	  
	  Out( "  %c %s  %5.1f\n", i == j ? '*' : ' ', str, dscore * 100.0 );
	  OutCsaShogi( " %s(%.0f%%)", str, dscore * 100.0 );
	}
      OutCsaShogi( "\n" );
    }

  move = bm2move( ptree, (unsigned int)abook_move[j].smove, is_flip );
  if ( ! is_move_valid( ptree, move, root_turn ) )
    {
      out_warning( "BAD BOOK MOVE!! " );
      return 0;
    }

  ply = record_game.moves;
  if ( game_status & flag_pondering ) { ply++; }

  ptree->current_move[1] = move;

  return 1;
}


static int CONV
book_read( uint64_t key, book_move_t *pbook_move, unsigned int *pposition )
{
  uint64_t book_key;
  const unsigned char *p;
  unsigned int position, size_section, size, u;
  int ibook_section, moves;
  unsigned short s;

  ibook_section = (int)( (unsigned int)key & (unsigned int)( NUM_SECTION-1 ) );

  if ( fseek( pf_book, BK_SIZE_INDEX*ibook_section, SEEK_SET ) == EOF )
    {
      str_error = str_io_error;
      return -2;
    }
  
  if ( fread( &position, sizeof(int), 1, pf_book ) != 1 )
    {
      str_error = str_io_error;
      return -2;
    }
  
  if ( fread( &s, sizeof(unsigned short), 1, pf_book ) != 1 )
    {
      str_error = str_io_error;
      return -2;
    }
  size_section = (unsigned int)s;
  if ( size_section > MAX_SIZE_SECTION )
    {
      str_error = str_book_error;
      return -2;
    }

  if ( fseek( pf_book, (long)position, SEEK_SET ) == EOF )
    {
      str_error = str_io_error;
      return -2;
    }
  if ( fread( book_section, sizeof(unsigned char), (size_t)size_section,
	      pf_book ) != (size_t)size_section )
    {
      str_error = str_io_error;
      return -2;
    }
  
  size       = 0;
  p          = book_section;
  *pposition = position;
  while ( book_section + size_section > p )
    {
      size     = (unsigned int)p[0];
      book_key = *(uint64_t *)( p + 1 );
      if ( book_key == key ) { break; }
      p          += size;
      *pposition += size;
    }
  if ( book_section + size_section <= p ) { return 0; }

  for ( moves = 0, u = BK_SIZE_HEADER; u < size; moves++, u += BK_SIZE_MOVE )
    {
      pbook_move[moves].smove  = *(unsigned short *)(p+u+0);
      pbook_move[moves].freq   = *(unsigned short *)(p+u+2);
    }

  return moves;
}


static ft_t CONV
flip_ft( ft_t ft, int turn, int is_flip )
{
  int ito_rank, ito_file, ifrom_rank, ifrom_file;

  ito_rank = airank[ft.to];
  ito_file = aifile[ft.to];
  if ( ft.from < nsquare )
    {
      ifrom_rank = airank[ft.from];
      ifrom_file = aifile[ft.from];
    }
  else { ifrom_rank = ifrom_file = 0; }

  if ( turn )
    {
      ito_rank = rank9 - ito_rank;
      ito_file = file9 - ito_file;
      if ( ft.from < nsquare )
	{
	  ifrom_rank = rank9 - ifrom_rank;
	  ifrom_file = file9 - ifrom_file;
	}
    }

  if ( is_flip )
    {
      ito_file = file9 - ito_file;
      if ( ft.from < nsquare ) { ifrom_file = file9 - ifrom_file; }
    }

  ft.to = ito_rank * nfile + ito_file;
  if ( ft.from < nsquare ) { ft.from = ifrom_rank * nfile + ifrom_file; }

  return ft;
}


static unsigned int CONV
bm2move( const tree_t * restrict ptree, unsigned int bmove, int is_flip )
{
  ft_t ft;
  unsigned int move;
  int is_promote;

  ft.to      = I2To(bmove);
  ft.from    = I2From(bmove);
  ft         = flip_ft( ft, root_turn, is_flip );
  is_promote = I2IsPromote(bmove);

  move  = (unsigned int)( is_promote | From2Move(ft.from) | ft.to );
  if ( ft.from >= nsquare ) { return move; }

  if ( root_turn )
    {
      move |= Cap2Move(BOARD[ft.to]);
      move |= Piece2Move(-BOARD[ft.from]);
    }
  else {
    move |= Cap2Move(-BOARD[ft.to]);
    move |= Piece2Move(BOARD[ft.from]);
  }

  return move;
}


static uint64_t CONV
book_hash_func( const tree_t * restrict ptree, int *pis_flip )
{
  uint64_t key, key_flip;
  unsigned int hand;
  int i, iflip, irank, ifile, piece;

  key = 0;
  hand = root_turn ? HAND_W : HAND_B;
  i = I2HandPawn(hand);    if ( i ) { key ^= b_hand_pawn_rand[i-1]; }
  i = I2HandLance(hand);   if ( i ) { key ^= b_hand_lance_rand[i-1]; }
  i = I2HandKnight(hand);  if ( i ) { key ^= b_hand_knight_rand[i-1]; }
  i = I2HandSilver(hand);  if ( i ) { key ^= b_hand_silver_rand[i-1]; }
  i = I2HandGold(hand);    if ( i ) { key ^= b_hand_gold_rand[i-1]; }
  i = I2HandBishop(hand);  if ( i ) { key ^= b_hand_bishop_rand[i-1]; }
  i = I2HandRook(hand);    if ( i ) { key ^= b_hand_rook_rand[i-1]; }

  hand = root_turn ? HAND_B : HAND_W;
  i = I2HandPawn(hand);    if ( i ) { key ^= w_hand_pawn_rand[i-1]; }
  i = I2HandLance(hand);   if ( i ) { key ^= w_hand_lance_rand[i-1]; }
  i = I2HandKnight(hand);  if ( i ) { key ^= w_hand_knight_rand[i-1]; }
  i = I2HandSilver(hand);  if ( i ) { key ^= w_hand_silver_rand[i-1]; }
  i = I2HandGold(hand);    if ( i ) { key ^= w_hand_gold_rand[i-1]; }
  i = I2HandBishop(hand);  if ( i ) { key ^= w_hand_bishop_rand[i-1]; }
  i = I2HandRook(hand);    if ( i ) { key ^= w_hand_rook_rand[i-1]; }

  key_flip = key;

  for ( irank = rank1; irank <= rank9; irank++ )
    for ( ifile = file1; ifile <= file9; ifile++ )
      {
	if ( root_turn )
	  {
	    i     = ( rank9 - irank ) * nfile + file9 - ifile;
	    iflip = ( rank9 - irank ) * nfile + ifile;
	    piece = -(int)BOARD[nsquare-i-1];
	  }
	else {
	  i     = irank * nfile + ifile;
	  iflip = irank * nfile + file9 - ifile;
	  piece = (int)BOARD[i];
	}

#define Foo(t_pc)  key      ^= (t_pc ## _rand)[i];     \
                   key_flip ^= (t_pc ## _rand)[iflip];
	switch ( piece )
	  {
	  case  pawn:        Foo( b_pawn );        break;
	  case  lance:       Foo( b_lance );       break;
	  case  knight:      Foo( b_knight );      break;
	  case  silver:      Foo( b_silver );      break;
	  case  gold:        Foo( b_gold );        break;
	  case  bishop:      Foo( b_bishop );      break;
	  case  rook:        Foo( b_rook );        break;
	  case  king:        Foo( b_king );        break;
	  case  pro_pawn:    Foo( b_pro_pawn );    break;
	  case  pro_lance:   Foo( b_pro_lance );   break;
	  case  pro_knight:  Foo( b_pro_knight );  break;
	  case  pro_silver:  Foo( b_pro_silver );  break;
	  case  horse:       Foo( b_horse );       break;
	  case  dragon:      Foo( b_dragon );      break;
	  case -pawn:        Foo( w_pawn );        break;
	  case -lance:       Foo( w_lance );       break;
	  case -knight:      Foo( w_knight );      break;
	  case -silver:      Foo( w_silver );      break;
	  case -gold:        Foo( w_gold );        break;
	  case -bishop:      Foo( w_bishop );      break;
	  case -rook:        Foo( w_rook );        break;
	  case -king:        Foo( w_king );        break;
	  case -pro_pawn:    Foo( w_pro_pawn );    break;
	  case -pro_lance:   Foo( w_pro_lance );   break;
	  case -pro_knight:  Foo( w_pro_knight );  break;
	  case -pro_silver:  Foo( w_pro_silver );  break;
	  case -horse:       Foo( w_horse );       break;
	  case -dragon:      Foo( w_dragon );      break;
	  }
#undef Foo
      }

  if ( key > key_flip )
    {
      key       = key_flip;
      *pis_flip = 1;
    }
  else { *pis_flip = 0; }

  return key;
}


static int CONV
normalize_book_move( book_move_t * restrict pbook_move, int moves )
{
  book_move_t swap;
  double dscale;
  unsigned int u, norm;
  int i, j;

  /* insertion sort by nwin */
  pbook_move[moves].freq = 0;
  for ( i = moves-2; i >= 0; i-- )
    {
      u    = pbook_move[i].freq;
      swap = pbook_move[i];
      for ( j = i+1; pbook_move[j].freq > u; j++ )
	{
	  pbook_move[j-1] = pbook_move[j];
	}
      pbook_move[j-1] = swap;
    }
      
  /* normalization */
  for ( norm = 0, i = 0; i < moves; i++ ) { norm += pbook_move[i].freq; }
  dscale = (double)USHRT_MAX / (double)norm;
  for ( norm = 0, i = 0; i < moves; i++ )
    {
      u = (unsigned int)( (double)pbook_move[i].freq * dscale );
      if ( ! u )           { u = 1U; }
      if ( u > USHRT_MAX ) { u = USHRT_MAX; }
      
      pbook_move[i].freq = (unsigned short)u;
      norm              += u;
    }
  if ( norm > (unsigned int)pbook_move[0].freq + USHRT_MAX )
    {
      str_error = "normalization error";
      return -2;
    }

  pbook_move[0].freq
    = (unsigned short)( pbook_move[0].freq + USHRT_MAX - norm );
  
  return 1;
}


#if ! defined(MINIMUM)

#define MaxNumCell      0x400000
#if defined(BK_SMALL) || defined(BK_TINY)
#  define MaxPlyBook    64
#else
#  define MaxPlyBook    128
#endif

typedef struct {
  unsigned int nwin, ngame, nwin_bnz, ngame_bnz, move;
} record_move_t;

typedef struct {
  uint64_t key;
  unsigned short smove;
  unsigned char result;
} cell_t;

static unsigned int CONV move2bm( unsigned int move, int turn, int is_flip );
static int CONV find_min_cell( const cell_t *pcell, int ntemp );
static int CONV read_a_cell( cell_t *pcell, FILE *pf );
static int compare( const void * p1, const void *p2 );
static int CONV dump_cell( cell_t *pcell, int ncell, int num_tmpfile );
static int CONV examine_game( tree_t * restrict ptree, record_t *pr,
			      int *presult, unsigned int *pmoves );
static int CONV move_selection( const record_move_t *p, int ngame, int nwin );
static int CONV make_cell_csa( tree_t * restrict ptree, record_t *pr,
			       cell_t *pcell, int num_tmpfile );
static int CONV merge_cell( record_move_t *precord_move, FILE **ppf,
			    int num_tmpfile );
static int CONV read_anti_book( tree_t * restrict ptree, record_t * pr );

int CONV
book_create( tree_t * restrict ptree )
{
  record_t record;
  FILE *ppf[101];
  char str_filename[SIZE_FILENAME];
  record_move_t *precord_move;
  cell_t *pcell;
  int iret, num_tmpfile, i, j;


  num_tmpfile = 0;

  pcell = (cell_t*)memory_alloc( sizeof(cell_t) * MaxNumCell );
  if ( pcell == NULL ) { return -2; }

  Out("\n  [book.csa]\n");
  
  iret = record_open( &record, "book.csa", mode_read, NULL, NULL );
  if ( iret < 0 ) { return iret; }

  num_tmpfile = make_cell_csa( ptree, &record, pcell, num_tmpfile );
  if ( num_tmpfile < 0 )
    {
      memory_free( pcell );
      record_close( &record );
      return num_tmpfile;
    }

  iret = record_close( &record );
  if ( iret < 0 )
    {
      memory_free( pcell );
      return iret;
    }

  memory_free( pcell );

  if ( ! num_tmpfile )
    {
      str_error = "No book data";
      return -2;
    }

  if ( num_tmpfile > 100 )
    {
      str_error = "Number of tmp??.bin files are too large.";
      return -2;
    }

  iret = book_off();
  if ( iret < 0 ) { return iret; }

  pf_book = file_open( str_book, "wb" );
  if ( pf_book == NULL ) { return -2; }

  precord_move = (record_move_t*)memory_alloc( sizeof(record_move_t) * (MAX_LEGAL_MOVES+1) );
  if ( precord_move == NULL ) { return -2; }
  
  for ( i = 0; i < num_tmpfile; i++ )
    {
      snprintf( str_filename, SIZE_FILENAME, "tmp%02d.bin", i );
      ppf[i] = file_open( str_filename, "rb" );
      if ( ppf[i] == NULL )
	{
	  memory_free( precord_move );
	  file_close( pf_book );
	  for ( j = 0; j < i; j++ ) { file_close( ppf[j] ); }
	  return -2;
	}
    }

  iret = merge_cell( precord_move, ppf, num_tmpfile );
  if ( iret < 0 )
    {
      memory_free( precord_move );
      file_close( pf_book );
      for ( i = 0; i < num_tmpfile; i++ ) { file_close( ppf[i] ); }
      return iret;
    }

  memory_free( precord_move );

  iret = book_on();
  if ( iret < 0 ) { return iret; }

#if 1
  iret = record_open( &record, "book_anti.csa", mode_read, NULL, NULL );
  if ( iret < 0 ) { return iret; }

  iret = read_anti_book( ptree, &record );
  if ( iret < 0 )
    {
      record_close( &record );
      return iret;
    }
#endif

  return record_close( &record );
}


static int CONV
read_anti_book( tree_t * restrict ptree, record_t * pr )
{
  uint64_t key;
  book_move_t abook_move[ BK_MAX_MOVE+1 ];
  size_t size;
  unsigned int move, position, bm, umoves;
  int iret, result, istatus, is_flip, i, moves;

  do {

    istatus = examine_game( ptree, pr, &result, &umoves );
    if ( istatus < 0 ) { return istatus; }
    if ( result == -2 )
      {
	str_error = "no result in book_anti.csa";
	return -2;
      }

    while ( pr->moves < umoves-1U )
      {
	istatus = in_CSA( ptree, pr, NULL, 0 );
	if ( istatus != record_misc )
	  {
	    str_error = "internal error at book.c";
	    return -2;
	  }
      }

    istatus = in_CSA( ptree, pr, &move, flag_nomake_move );
    if ( istatus < 0 ) { return istatus; }

    key = book_hash_func( ptree, &is_flip );
    
    moves = book_read( key, abook_move, &position );
    if ( moves < 0 ) { return moves; }

    bm = move2bm( move, root_turn, is_flip );
    for ( i = 0; i < moves; i++ )
      {
	if ( bm == abook_move[i].smove ) { break; }
      }

    if ( i == moves )
      {
	out_board( ptree, stdout, 0, 0 );
	printf( "%s is not found in the book\n\n", str_CSA_move(move) );
      }
    else {
      abook_move[i].freq = 0;

      iret = normalize_book_move( abook_move, moves );
      if ( iret < 0 ) { return iret; }

      for ( i = 0; i < moves; i++ )
	{
	  *(unsigned short *)( book_section + i*BK_SIZE_MOVE )
	    = abook_move[i].smove;
	  *(unsigned short *)( book_section + i*BK_SIZE_MOVE + 2 )
	    = abook_move[i].freq;
	}
      size = (size_t)( moves * BK_SIZE_MOVE );
      if ( fseek( pf_book, (long)(position+BK_SIZE_HEADER), SEEK_SET ) == EOF
	   || fwrite( book_section, sizeof(unsigned char),
		      size, pf_book ) != size )
	{
	  str_error = str_io_error;
	  return -2;
	}
      
      out_board( ptree, stdout, 0, 0 );
      printf( "%s is discarded\n\n", str_CSA_move(move) );
    }

    if ( istatus != record_eof && istatus != record_next )
      {
	istatus = record_wind( pr );
	if ( istatus < 0 ) { return istatus; }
      }
  } while ( istatus != record_eof );

  return 1;
}


static int CONV
make_cell_csa( tree_t * restrict ptree, record_t *pr, cell_t *pcell,
	       int num_tmpfile )
{
  struct {
    uint64_t hash_key;
    unsigned int hand, move;
  } rep_tbl[MaxPlyBook+1];
  uint64_t key;
  unsigned int nwhite_win, nblack_win, ndraw, ninvalid, nbnz_black, nbnz_white;
  unsigned int move, moves, uresult;
  int icell, result, is_flip, iret, istatus, ply, i, black_bnz, white_bnz;

  nwhite_win = nblack_win = ndraw = ninvalid = nbnz_white = nbnz_black = 0;
  icell = black_bnz = white_bnz = 0;
  istatus = record_next;

  while ( istatus != record_eof ) {

    istatus = examine_game( ptree, pr, &result, &moves );
    if ( istatus < 0 ) { return istatus; }

    if      ( result == -1 ) { nwhite_win++; }
    else if ( result ==  1 ) { nblack_win++; }
    else if ( result ==  0 ) { ndraw++; }
    else {
      ninvalid++;
      continue;
    }
    
    if ( moves > MaxPlyBook ) { moves = MaxPlyBook; }

    for ( ply = 0;; ply++ ) {
      istatus = in_CSA( ptree, pr, &move, flag_nomake_move );
      if ( ! ply )
	{
	  black_bnz = strcmp( pr->str_name1, "Bonanza" ) ? 0 : 1;
	  white_bnz = strcmp( pr->str_name2, "Bonanza" ) ? 0 : 1;
	  if ( ! strcmp( pr->str_name1, "Bonanza" ) )
	    {
	      black_bnz   = 1;
	      nbnz_black += 1;
	    }
	  else { black_bnz = 0; }
	  if ( ! strcmp( pr->str_name2, "Bonanza" ) )
	    {
	      white_bnz   = 1;
	      nbnz_white += 1;
	    }
	  else { white_bnz = 0; }
	}
      if ( istatus < 0 ) { return istatus; }
      if ( istatus == record_resign && ! moves ) { break; }
      if ( istatus != record_misc )
	{
	  str_error = "internal error at book.c";
	  return -2;
	}

      rep_tbl[ply].hash_key = HASH_KEY;
      rep_tbl[ply].hand     = HAND_B;
      rep_tbl[ply].move     = move;
      for ( i = ( ply & 1 ); i < ply; i += 2 )
	{
	  if ( rep_tbl[i].hash_key == HASH_KEY
	       && rep_tbl[i].hand == HAND_B
	       && rep_tbl[i].move == move ) { break; }
	}

      if ( i == ply ) {
	key     = book_hash_func( ptree, &is_flip );
	uresult = (unsigned int)( root_turn ? -1*result+1 : result+1 );
	if ( ( root_turn == black && black_bnz )
	     || ( root_turn == white && white_bnz ) ) { uresult |= 0x4U; }

	pcell[icell].key    = key;
	pcell[icell].result = (unsigned char)uresult;
	pcell[icell].smove  = (unsigned short)move2bm( move, root_turn,
						       is_flip );
	icell++;
	if ( icell == MaxNumCell ) {
	  iret = dump_cell( pcell, icell, num_tmpfile++ );
	  if ( iret < 0 ) { return iret; }
	  icell = 0;
	}
	if ( ! ( (icell-1) & 0x1ffff ) ) { Out( "." ); }
      }
      
      if ( pr->moves >= moves ) { break; }

      iret = make_move_root( ptree, move, 0 );
      if ( iret < 0 ) {	return iret; }
    }

    if ( istatus != record_eof && istatus != record_next )
      {
	istatus = record_wind( pr );
	if ( istatus < 0 ) { return istatus; }
      }
  }

  iret  = dump_cell( pcell, icell, num_tmpfile++ );
  if ( iret < 0 ) { return iret; }

  Out( "\n"
       "Total games:   %7u\n"
       "  Discarded:   %7u\n"
       "  Black wins:  %7u\n"
       "  White wins:  %7u\n"
       "  Drawn:       %7u\n"
       "  Black Bnz:   %7u\n"
       "  White Bnz:   %7u\n", nblack_win + nwhite_win + ndraw + ninvalid,
       ninvalid, nblack_win, nwhite_win, ndraw, nbnz_black, nbnz_white );

  return num_tmpfile;
}


static int CONV
merge_cell( record_move_t *precord_move, FILE **ppf, int num_tmpfile )
{
  double dscale;
  record_move_t swap;
  cell_t acell[101];
  uint64_t key;
  unsigned int book_moves, book_positions, move, size_data, size_section;
  unsigned int max_size_section, nwin, nwin_bnz, ngame, ngame_bnz, position;
  unsigned int u, norm;
  int i, j, iret, ibook_section, imin, nmove;
  unsigned short s;

  for ( i = 0; i < num_tmpfile; i++ )
    {
      iret = read_a_cell( acell + i, ppf[i] );
      if ( iret < 0 ) { return iret; }
    }
  
  imin             = find_min_cell( acell, num_tmpfile );
  position         = BK_SIZE_INDEX * NUM_SECTION;
  max_size_section = book_moves = book_positions = 0;
  for ( ibook_section = 0; ibook_section < NUM_SECTION; ibook_section++ ) {
    size_section = 0;
    for (;;) {
      key  = acell[imin].key;
      i    = (int)( (unsigned int)key & (unsigned int)(NUM_SECTION-1) );
      if ( i != ibook_section || key == UINT64_MAX ) { break; }
      
      nwin = nmove = nwin_bnz = ngame = ngame_bnz = precord_move[0].move = 0;
      do {
	move = (unsigned int)acell[imin].smove;
	for ( i = 0; precord_move[i].move && precord_move[i].move != move;
	      i++ );
	if ( ! precord_move[i].move )
	  {
	    precord_move[i].nwin     = precord_move[i].ngame     = 0;
	    precord_move[i].nwin_bnz = precord_move[i].ngame_bnz = 0;
	    precord_move[i].move     = move;
	    precord_move[i+1].move   = 0;
	    nmove++;
	  }

	if ( acell[imin].result & b0010 )
	  {
	    if ( acell[imin].result & b0100 )
	      {
		precord_move[i].nwin_bnz += 1;
		nwin_bnz                 += 1;
	      }
	    precord_move[i].nwin     += 1;
	    nwin                     += 1;
	  }

	if ( acell[imin].result & b0100 )
	  {
	    precord_move[i].ngame_bnz += 1;
	    ngame_bnz                 += 1;
	  }
	precord_move[i].ngame += 1;
	ngame                 += 1;

	iret = read_a_cell( acell + imin, ppf[imin] );
	if ( iret < 0 ) { return iret; }
	
	imin = find_min_cell( acell, num_tmpfile );
      } while ( key == acell[imin].key );

#if defined(BK_COM)
      while ( nmove > 1 && ngame_bnz >= 128 )
	{
	  double max_rate, rate;

	  max_rate = 0.0;
	  for ( i = 0; i < nmove; i++ )
	    {
	      rate = ( (double)precord_move[i].nwin_bnz
		       / (double)( precord_move[i].ngame_bnz + 7 ) );
	      if ( rate > max_rate ) { max_rate = rate; }
	    }
	  if ( max_rate < 0.1 ) { break; }

	  max_rate *= 0.85;
	  i = 0;
	  do {
	    rate = ( (double)precord_move[i].nwin_bnz
		     / (double)( precord_move[i].ngame_bnz + 7 ) );
	    
	    if ( rate > max_rate ) { i++; }
	    else {
	      precord_move[i] = precord_move[nmove-1];
	      nmove -= 1;
	    }
	  } while ( i < nmove );

	  break;
	}
#endif
      if ( ! nmove ) { continue; }
      
      i = 0;
      do {
	if ( move_selection( precord_move + i, ngame, nwin ) ) { i++; }
	else {
	  precord_move[i] = precord_move[nmove-1];
	  nmove -= 1;
	}
      } while ( i < nmove );

      if ( ! nmove ) { continue; }

      size_data = BK_SIZE_HEADER + BK_SIZE_MOVE * nmove;
      if ( size_section + size_data > MAX_SIZE_SECTION
	   || size_data > UCHAR_MAX )
	{
	  str_error = "book_section buffer overflow";
	  return -2;
	}
      if ( nmove > BK_MAX_MOVE )
	{
	  str_error = "BK_MAX_MOVE is too small";
	  return -2;
	}

      /* insertion sort by nwin */
      precord_move[nmove].nwin = 0;
      for ( i = nmove-2; i >= 0; i-- )
	{
	  u    = precord_move[i].nwin;
	  swap = precord_move[i];
	  for ( j = i+1; precord_move[j].nwin > u; j++ )
	    {
	      precord_move[j-1] = precord_move[j];
	    }
	  precord_move[j-1] = swap;
	}

      /* normalize nwin */
      for ( norm = 0, i = 0; i < nmove; i++ ) { norm += precord_move[i].nwin; }
      dscale = (double)USHRT_MAX / (double)norm;
      for ( norm = 0, i = 0; i < nmove; i++ )
	{
	  u = (unsigned int)( (double)precord_move[i].nwin * dscale );
	  if ( ! u )           { u = 1U; }
	  if ( u > USHRT_MAX ) { u = USHRT_MAX; }
	  
	  precord_move[i].nwin = u;
	  norm                += u;
	}
      if ( norm > precord_move[0].nwin + USHRT_MAX )
	{
	  str_error = "normalization error\n";
	  return -2;
	}
      precord_move[0].nwin += USHRT_MAX - norm;

      book_section[size_section+0] = (unsigned char)size_data;
      *(uint64_t *)(book_section+size_section+1) = key;

      for ( u = size_section+BK_SIZE_HEADER, i = 0; i < nmove;
	    u += BK_SIZE_MOVE, i++ )
	{
	  *(unsigned short *)(book_section+u)
	    = (unsigned short)precord_move[i].move;
	  *(unsigned short *)(book_section+u+2)
	    = (unsigned short)precord_move[i].nwin;
	}
      book_positions += 1;
      book_moves     += nmove;
      size_section   += size_data;
    }
    if ( fseek( pf_book, BK_SIZE_INDEX * ibook_section, SEEK_SET ) == EOF )
      {
	str_error = str_io_error;
	return -2;
      }
    if ( fwrite( &position, sizeof(unsigned int), 1, pf_book ) != 1 )
      {
	str_error = str_io_error;
	return -2;
      }
    s = (unsigned short)size_section;
    if ( fwrite( &s, sizeof(unsigned short), 1, pf_book ) != 1 )
      {
	str_error = str_io_error;
	return -2;
      }
    if ( fseek( pf_book, position, SEEK_SET ) == EOF )
      {
	str_error = str_io_error;
	return -2;
      }
    if ( fwrite( &book_section, sizeof(unsigned char), (size_t)size_section,
		 pf_book ) != (size_t)size_section )
      {
	str_error = str_io_error;
	return -2;
      }

    if ( size_section > max_size_section ) { max_size_section = size_section; }
    position += size_section;
  }
  
  Out( "Positions in the book:  %u\n", book_positions );
  Out( "Moves in the book:      %u\n", book_moves );
  Out( "Max. size of a section: %d\n", max_size_section );
  
  return 1;
}


static int CONV
move_selection( const record_move_t *p, int ngame, int nwin )
{
  double total_win_norm, win_norm, win, game, win_move, game_move;

#if defined(BK_TINY)
  if ( p->nwin < 15 ) { return 0; }
#elif defined(BK_SMALL)
  if ( p->nwin < 3 ) { return 0; }
#else
  if ( ! p->nwin || p->ngame < 2 ) { return 0; }
#endif

  win       = (double)nwin;
  game      = (double)ngame;
  win_move  = (double)p->nwin;
  game_move = (double)p->ngame;

  total_win_norm = win      * game_move;
  win_norm       = win_move * game;
  if ( win_norm < total_win_norm * 0.85 ) { return 0; }

  return 1;
}


static int CONV
find_min_cell( const cell_t *pcell, int num_tmpfile )
{
  int imin, i;

  imin = 0;
  for ( i = 1; i < num_tmpfile; i++ )
    {
      if ( compare( pcell+imin, pcell+i ) == 1 ) { imin = i; }
    }
  return imin;
}


static int CONV
read_a_cell( cell_t *pcell, FILE *pf )
{
  if ( fread( &pcell->key, sizeof(uint64_t), 1, pf ) != 1 )
    {
      if ( feof( pf ) )
	{
	  pcell->key = UINT64_MAX;
	  return 1;
	}
      str_error = str_io_error;
      return -2;
    }
  if ( fread( &pcell->smove, sizeof(unsigned short), 1, pf ) != 1 )
    {
      str_error = str_io_error;
      return -2;
    }
  if ( fread( &pcell->result, sizeof(unsigned char), 1, pf ) != 1 )
    {
      str_error = str_io_error;
      return -2;
    }

  return 1;
}


static int CONV
examine_game( tree_t * restrict ptree, record_t *pr, int *presult,
	      unsigned int *pmoves )
{
  rpos_t rpos;
  int iret, istatus, is_lost, is_win;
  unsigned int moves;

  *presult = -2;

  iret = record_getpos( pr, &rpos );
  if ( iret < 0 ) { return iret; }

  is_lost = is_win = 0;
  moves = 0;
  do {
    istatus = in_CSA( ptree, pr, NULL, flag_detect_hang );
    if ( istatus < 0 )
      {
	/* the game is end, however the record is invalid */
	if ( strstr( str_error, str_bad_record ) != NULL
	     && ( game_status & mask_game_end ) )
	  {
	    break;
	  }

	/* a hang-king and a double-pawn are counted as a lost game */
	if ( strstr( str_error, str_king_hang ) != NULL
	     || strstr( str_error, str_double_pawn )  != NULL
	     || strstr( str_error, str_mate_drppawn ) != NULL )
	  {
	    is_lost = 1;
	    break;
	  }

	return istatus;
      }
    /* previous move had an error, count as a won game */
    else if ( istatus == record_error )
      {
	is_win = 1;
	break;
      }
    else if ( istatus == record_misc ) { moves++; }
  } while ( istatus != record_next && istatus != record_eof );

  if ( istatus != record_next && istatus != record_eof )
    {
      istatus = record_wind( pr );
      if ( istatus < 0 ) { return istatus; }
    }

  if ( ! ( is_lost || is_win || ( game_status & mask_game_end ) ) )
    {
      return istatus;
    }

  if      ( is_win ) { *presult = root_turn ? -1 : 1; }
  else if ( is_lost || ( game_status & ( flag_mated | flag_resigned ) ) )
    {
      *presult = root_turn ? 1 : -1;
    }
  else { *presult = 0; }

  *pmoves = moves;

  iret = record_setpos( pr, &rpos );
  if ( iret < 0 ) { return iret; }

  return istatus;
}


static int CONV
dump_cell( cell_t *pcell, int ncell, int num_tmpfile )
{
  char str_filename[SIZE_FILENAME];
  FILE *pf;
  int i, iret;

  Out( " sort" );
  qsort( pcell, ncell, sizeof(cell_t), compare );

  Out( " dump", str_filename );
  snprintf( str_filename, SIZE_FILENAME, "tmp%02d.bin", num_tmpfile );
  pf = file_open( str_filename, "wb" );
  if ( pf == NULL ) { return -2; }

  for ( i = 0; i < ncell; i++ )
    {
      if ( fwrite( &pcell[i].key, sizeof(uint64_t), 1, pf ) != 1 )
	{
	  file_close( pf );
	  str_error = str_io_error;
	  return -2;
	}
      if ( fwrite( &pcell[i].smove, sizeof(unsigned short), 1, pf ) != 1 )
	{
	  file_close( pf );
	  str_error = str_io_error;
	  return -2;
	}
      if ( fwrite( &pcell[i].result, sizeof(unsigned char), 1, pf ) != 1 )
	{
	  file_close( pf );
	  str_error = str_io_error;
	  return -2;
	}
    }

  iret = file_close( pf );
  if ( iret < 0 ) { return iret; }

  Out( " done (%s)\n", str_filename );

  return 1;
}


static int compare( const void * p1, const void * p2 )
{
  const cell_t * pcell1 = (cell_t*)p1;
  const cell_t * pcell2 = (cell_t*)p2;
  unsigned int u1, u2;

  u1 = (unsigned int)pcell1->key & (unsigned int)(NUM_SECTION-1);
  u2 = (unsigned int)pcell2->key & (unsigned int)(NUM_SECTION-1);

  if ( u1 < u2 ) { return -1; }
  if ( u1 > u2 ) { return  1; }
  if ( pcell1->key < pcell2->key ) { return -1; }
  if ( pcell1->key > pcell2->key ) { return  1; }

  return 0;
}


static unsigned int CONV
move2bm( unsigned int move, int turn, int is_flip )
{
  ft_t ft;
  unsigned int bmove;
  int is_promote;

  ft.to      = I2To(move);
  ft.from    = I2From(move);
  is_promote = I2IsPromote(move);

  ft = flip_ft( ft, turn, is_flip );

  bmove = (unsigned int)( is_promote | From2Move(ft.from) | ft.to );

  return bmove;
}


#endif /* no MINIMUM */
