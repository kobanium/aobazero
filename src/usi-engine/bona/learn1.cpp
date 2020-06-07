// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#if defined(_WIN32)
#  include <process.h>
#else
#  include <sched.h>
#endif
#include "shogi.h"

#if ! defined(MINIMUM)

#  define SEARCH_DEPTH      2
#  define NUM_RESULT        8
#  define MAX_RECORD_LENGTH 1024
#  define DOT_INTERVAL      10
#  define MAX_BOOK_PLY      64
#  define NumBookCluster    256
#  define NumBookEntry      0x1000
#  define SIZE_PV_BUFFER    0x100000
#  define MOVE_VOID         0x80000000U
#  define Move2S(move)      (unsigned short)((move) & 0x7fffU)


typedef struct {
  struct { uint64_t a,b; } cluster[NumBookCluster];
} book_entry_t;

typedef struct {

  /* input */
  tree_t *ptree;
  book_entry_t *pbook_entry;
  FILE *pf_tmp;
  record_t *precord;
  unsigned int max_games, id;
  int nworker;

  /* output */
  uint64_t result[ NUM_RESULT ];
  uint64_t num_moves_counted, num_moves, result_norm, num_nodes;
  double target, target_out_window;
  unsigned int illegal_moves, max_pos_buf;
  int info;

  /* work area */
  unsigned int record_moves[ MAX_RECORD_LENGTH ];
  unsigned int amove_legal[ MAX_LEGAL_MOVES ];
  unsigned int record_length, pos_buf;
  unsigned short buf[ SIZE_PV_BUFFER ];
  int root_turn;

} parse1_data_t;

typedef struct {

  /* input */
  tree_t *ptree;
  FILE *pf_tmp;
  unsigned int id;
  int nworker;

  /* output */
  param_t param;
  uint64_t num_moves_counted;
  double target;
  int info;

  /* work area */
  unsigned short buf[ SIZE_PV_BUFFER ];
  unsigned int pv[ PLY_MAX + 2 ];

} parse2_data_t;

#  if defined(_WIN32)
static unsigned int __stdcall parse1_worker( void *arg );
static unsigned int __stdcall parse2_worker( void *arg );
#  else
static void *parse1_worker( void *arg );
static void *parse2_worker( void *arg );
#endif
static void ini_book( book_entry_t *pbook_entry );
static void make_pv( parse1_data_t *pdata, unsigned int record_move );
static int read_game( parse1_data_t *pdata );
static int read_buf( unsigned short *buf, FILE *pf );
static int calc_deriv( parse2_data_t *pdata, int pos_buf, int turn );
static int learn_parse1( tree_t * restrict ptree, book_entry_t *pbook_entry,
			 FILE *pf_tmp, record_t *precord,
			 unsigned int max_games, double *ptarget_out_window,
			 double *pobj_norm, int tlp1 );
static int learn_parse2( tree_t * restrict ptree, FILE *pf_tmp,
			 int nsteps, double target_out_window,
			 double obj_norm, int tlp2 );
static int book_probe_learn( const tree_t * restrict ptree,
			     book_entry_t *pbook_entry, int ply,
			     unsigned int move );
static int rep_check_learn( tree_t * restrict ptree, int ply );
static unsigned int s2move( const tree_t * restrict ptree, unsigned int move,
			    int tt );
static double func( double x );
static double dfunc( double x );

int
learn( tree_t * restrict ptree, int is_ini, int nsteps, unsigned int max_games,
       int max_iterations, int nworker1, int nworker2 )
{
  record_t record;
  book_entry_t *pbook_entry;
  FILE *pf_tmp;
  double target_out_window, obj_norm;
  int iret, niterations;

  pbook_entry = (book_entry_t*)memory_alloc( sizeof(book_entry_t) * NumBookEntry );
  if ( pbook_entry == NULL ) { return -2; }

  if ( is_ini )
    {
      fill_param_zero();
      fmg_misc      = fmg_cap      = fmg_drop = fmg_mt = 0;
      fmg_misc_king = fmg_cap_king = 0;
    }
  else {
    fmg_misc      = FMG_MISC;
    fmg_cap       = FMG_CAP;
    fmg_drop      = FMG_DROP;
    fmg_mt        = FMG_MT;
    fmg_misc_king = FMG_MISC_KING;
    fmg_cap_king  = FMG_CAP_KING;
  }

  game_status     |= flag_learning;
  root_alpha       = - ( score_max_eval + 1 );
  root_beta        = + ( score_max_eval + 1 );
  root_abort       = 0;
  iret             = 1;

  for ( niterations = 1; niterations <= max_iterations; niterations++ )
    {
      Out( "\n  Iteration %03d\n", niterations );

      iret = record_open( &record, "records.csa", mode_read, NULL, NULL );
      if ( iret < 0 ) { break; }
  
      pf_tmp = file_open( "tmp.bin", "wb" );
      if ( pf_tmp == NULL )
	{
	  record_close( &record );
	  iret = -2;
	  break;
	}

      iret = learn_parse1( ptree, pbook_entry, pf_tmp, &record, max_games,
			   &target_out_window, &obj_norm, nworker1 );

      if ( iret < 0 )
	{
	  record_close( &record );
	  file_close( pf_tmp );
	  break;
	}

      iret = file_close( pf_tmp );
      if ( iret < 0 )
	{
	  record_close( &record );
	  break;
	}

      pf_tmp = file_open( "tmp.bin", "rb" );
      if ( pf_tmp == NULL )
	{
	  record_close( &record );
	  iret = -2;
	  break;
	}

      iret = learn_parse2( ptree, pf_tmp, nsteps, target_out_window, obj_norm,
			   nworker2 );
      if ( iret < 0 )
	{
	  file_close( pf_tmp );
	  record_close( &record );
	  break;
	}

      iret = file_close( pf_tmp );
      if ( iret < 0 )
	{
	  record_close( &record );
	  break;
	}

      iret = record_close( &record );
      if ( iret < 0 ) { break; }

      if ( ! ( game_status & flag_learning ) )
	{
	  out_warning( "flag_learning is not set." );
	}
    }
  
  memory_free( pbook_entry );
  game_status &= ~flag_learning;
  
  return iret;
}


static int
learn_parse1( tree_t * restrict ptree, book_entry_t *pbook_entry, FILE *pf_tmp,
	      record_t *precord, unsigned int max_games,
	      double *ptarget_out_window, double *pobj_norm, int nworker )
{
  parse1_data_t *pdata[ TLP_MAX_THREADS ];
  int i, id;

  ini_book( pbook_entry );

  for ( id = 0; id < nworker; id++ )
    {
      pdata[id] = (parse1_data_t*)memory_alloc( sizeof(parse1_data_t) );
      if ( pdata[id] == NULL ) { return -1; }

      pdata[id]->ptree       = NULL;
      pdata[id]->pbook_entry = pbook_entry;
      pdata[id]->pf_tmp      = pf_tmp;
      pdata[id]->precord     = precord;
      pdata[id]->max_games   = max_games;
      pdata[id]->id          = id;
      pdata[id]->nworker     = nworker;
    }

#if defined(TLP)
  tlp_num = nworker;
  for ( id = 1; id < nworker; id++ )
    {
#  if defined(_WIN32)
      pdata[id]->ptree = tlp_atree_work + id;
      if ( ! _beginthreadex( 0, 0, parse1_worker, pdata[id], 0, 0 ) )
	{
	  str_error = "_beginthreadex() failed.";
	  return -1;
	}
#  else
      pthread_t pt;

      pdata[id]->ptree = tlp_atree_work + id;
      if ( pthread_create( &pt, &pthread_attr, parse1_worker, pdata[id] ) )
	{
	  str_error = "pthread_create() failed.";
	  return -1;
	}
#  endif
    }
#endif /* TLP */

  pdata[0]->ptree = ptree;
  parse1_worker( pdata[0] );

#if defined(TLP)
  while ( tlp_num ) { tlp_yield(); }
#endif
  
  for ( id = 0; id < nworker; id++ )
    {
      if ( pdata[id]->info < 0 ) { return -1; }
    }

  for ( id = 1; id < nworker; id++ )
    {
      for ( i = 0; i < NUM_RESULT; i++ )
	{
	  pdata[0]->result[i] += pdata[id]->result[i];
	}
      pdata[0]->num_moves_counted += pdata[id]->num_moves_counted;
      pdata[0]->num_moves         += pdata[id]->num_moves;
      pdata[0]->num_nodes         += pdata[id]->num_nodes;
      pdata[0]->result_norm       += pdata[id]->result_norm;
      pdata[0]->target            += pdata[id]->target;
      pdata[0]->target_out_window += pdata[id]->target_out_window;
      pdata[0]->illegal_moves     += pdata[id]->illegal_moves;
      if ( pdata[0]->max_pos_buf < pdata[id]->max_pos_buf )
	{
	  pdata[0]->max_pos_buf = pdata[id]->max_pos_buf;
	}
    }
  if ( pdata[0]->result_norm == 0 ) { pdata[0]->result_norm = 1; }
  if ( pdata[0]->num_moves   == 0 ) { pdata[0]->num_moves   = 1; }
  *ptarget_out_window = pdata[0]->target_out_window;
  *pobj_norm          = (double)pdata[0]->num_moves;

  {
    double dtemp;
    int misc, drop, cap, mt, misc_king, cap_king;

    Out( " done\n" );
    Out( "   Number of Games : %u\n",       precord->games );
    Out( "   Total Moves     : %" PRIu64 "\n",pdata[0]->num_moves );
    Out( "   Moves Counted   : %" PRIu64 "\n",pdata[0]->num_moves_counted);
    Out( "   Illegal Moves   : %u\n",       pdata[0]->illegal_moves );
    Out( "   Nodes Searched  : %" PRIu64 "\n",pdata[0]->num_nodes );
    Out( "   Max pos_buf     : %x\n",       pdata[0]->max_pos_buf );
    Out( "   Prediction (%)  :" );
    for ( i = 0, dtemp = 0.0; i < NUM_RESULT; i++ )
      {
	dtemp += (double)pdata[0]->result[i] * 100.0;
	Out( " %4.2f", dtemp / (double)pdata[0]->result_norm );
      }
    Out( "\n" );
  
    pdata[0]->target /= *pobj_norm;
    dtemp             = *ptarget_out_window / *pobj_norm;
    Out( "   Target          : %f (%f)\n", pdata[0]->target, dtemp );

    misc      = fmg_misc      / 2;
    drop      = fmg_drop      / 2;
    cap       = fmg_cap       / 2;
    mt        = fmg_mt        / 2;
    misc_king = fmg_misc_king / 2;
    cap_king  = fmg_cap_king  / 2;
    Out( "   Futility        : misc=%d drop=%d cap=%d mt=%d misc(k)=%d "
	 "cap(k)=%d\n", misc, drop, cap, mt, misc_king, cap_king );
  }

  for ( id = 0; id < nworker; id++ ) { memory_free( pdata[id] ); }
  
  return 1;
}


#  if defined(_MSC_VER)
static unsigned int __stdcall parse1_worker( void *arg )
#  else
static void *parse1_worker( void *arg )
#endif
{
  parse1_data_t *pdata;
  tree_t *ptree;
  unsigned int record_move;
  int i, imove, iret;

  iret                 = 0;
  pdata                = (parse1_data_t *)arg;
  ptree                = pdata->ptree;

  for ( i = 0; i < NUM_RESULT; i++ ) { pdata->result[i] = 0; }
  pdata->num_moves_counted = 0;
  pdata->num_moves         = 0;
  pdata->num_nodes         = 0;
  pdata->max_pos_buf       = 0;
  pdata->result_norm       = 0;
  pdata->record_length     = 0;
  pdata->illegal_moves     = 0;
  pdata->info              = 0;
  pdata->target            = 0.0;
  pdata->target_out_window = 0.0;

  for ( ;; ) {
    /* make pv */
    pdata->pos_buf = 2U;
    for ( imove = 0; imove < (int)pdata->record_length; imove++ )
      {
	record_move                    = pdata->record_moves[imove];

	pdata->buf[ pdata->pos_buf++ ] = Move2S(record_move);

	if ( record_move & MOVE_VOID ) { record_move &= ~MOVE_VOID; }
	else                           { make_pv( pdata, record_move ); }

	pdata->buf[ pdata->pos_buf++ ] = 0;

	MakeMove( pdata->root_turn, record_move, 1 );
	pdata->root_turn     = Flip( pdata->root_turn );
	ptree->move_last[1]  = ptree->move_last[0];
	ptree->nsuc_check[0] = 0;
	ptree->nsuc_check[1]
	  = (unsigned char)( InCheck( pdata->root_turn ) ? 1U : 0 );
      }

#if defined(TLP)
    lock( &tlp_lock );
#endif

    /* save pv */
    if ( pdata->record_length )
      {
	if ( pdata->pos_buf > pdata->max_pos_buf )
	  {
	    pdata->max_pos_buf = pdata->pos_buf;
	  }
	pdata->buf[0] = (unsigned short)( pdata->pos_buf / 0x10000U );
	pdata->buf[1] = (unsigned short)( pdata->pos_buf % 0x10000U );

	if ( fwrite( pdata->buf, sizeof(unsigned short), pdata->pos_buf,
		     pdata->pf_tmp ) != pdata->pos_buf )
	  {
	    str_error = str_io_error;
	    iret      = -2;
	  }
      }

    /* read next game */
    while ( iret >= 0 ) {
      iret = read_game( pdata );
      if ( iret == 1 )            { break; } /* end of record */
      if ( pdata->record_length ) { break; } /* read a record */
    }
    
#if defined(TLP)
    unlock( &tlp_lock );
#endif

    if ( iret < 0 )  { break; }
    if ( iret == 1 ) { break; }
  }

#if defined(TLP)
  lock( &tlp_lock );
  tlp_num -= 1;
  unlock( &tlp_lock );
#endif

  pdata->info = iret;
  return 0;
}


static void
make_pv( parse1_data_t *pdata, unsigned int record_move )
{
  double func_value;
  tree_t *ptree;
  unsigned int *pmove;
  unsigned int move, pos_buf;
  int i, imove, record_value, nth, nc, nmove_legal;
  int value, alpha, beta, depth, ply, tt, new_depth;

  record_value          = INT_MIN;
  nc                    = 0;
  nth                   = 0;
  depth                 = PLY_INC * SEARCH_DEPTH + PLY_INC / 2;
  tt                    = Flip(pdata->root_turn);
  pos_buf               = pdata->pos_buf;
  ptree                 = pdata->ptree;
  ptree->node_searched  = 0;
  ptree->save_eval[0]   = INT_MAX;
  ptree->save_eval[1]   = INT_MAX;
#if defined(TLP)
  ptree->tlp_abort      = 0;
#endif
  for ( ply = 0; ply < PLY_MAX; ply++ )
    {
      ptree->amove_killer[ply].no1 = ptree->amove_killer[ply].no2 = 0U;
      ptree->killers[ply].no1      = ptree->killers[ply].no2      = 0U;
    }
  for ( i = 0; i < (int)HIST_SIZE; i++ )
    {
      ptree->hist_good[i]  /= 256U;
      ptree->hist_tried[i] /= 256U;
    }
  evaluate( ptree, 1, pdata->root_turn );

  pmove = GenCaptures     ( pdata->root_turn, pdata->amove_legal );
  pmove = GenNoCaptures   ( pdata->root_turn, pmove );
  pmove = GenDrop         ( pdata->root_turn, pmove );
  pmove = GenCapNoProEx2  ( pdata->root_turn, pmove );
  pmove = GenNoCapNoProEx2( pdata->root_turn, pmove );
  nmove_legal = (int)( pmove - pdata->amove_legal );
  
  for ( i = 0; pdata->amove_legal[i] != record_move; i++ );
  move                  = pdata->amove_legal[0];
  pdata->amove_legal[0] = pdata->amove_legal[i];
  pdata->amove_legal[i] = move;

  for ( imove = 0; imove < nmove_legal; imove++ ) {

    move = pdata->amove_legal[imove];
    ptree->current_move[1] = move;
    if ( imove )
      {
	alpha = record_value - FV_WINDOW;
	beta  = record_value + FV_WINDOW;
	if ( alpha < root_alpha ) { alpha = root_alpha; }
	if ( beta  > root_beta )  { beta  = root_beta; }
      }
    else {
      alpha = root_alpha;
      beta  = root_beta;
    }

    MakeMove( pdata->root_turn, move, 1 );
    if ( InCheck(pdata->root_turn) )
      {
	UnMakeMove( pdata->root_turn, move, 1 );
	continue;
      }

    if ( InCheck(tt) )
      {
	new_depth = depth + PLY_INC;
	ptree->nsuc_check[2] = (unsigned char)( ptree->nsuc_check[0] + 1U );
      }
    else {
      new_depth            = depth;
      ptree->nsuc_check[2] = 0;
    }

    ptree->current_move[1] = move;
    ptree->pv[1].type = no_rep;
      
    value = -search( ptree, -beta, -alpha, tt, new_depth, 2,
		     node_do_mate | node_do_null | node_do_futile
		     | node_do_recap | node_do_recursion | node_do_hashcut );

    UnMakeMove( pdata->root_turn, move, 1 );

    if ( abs(value) > score_mate1ply )
      {
	out_warning( "value is larger than mate1ply!" );
      }
    
    if ( imove )
      {
	func_value        = func( value - record_value );
	pdata->target    += func_value;
	pdata->num_moves += 1U;
	if ( alpha < value && value < beta )
	  {
	    nc += 1;
	  }
	else { pdata->target_out_window += func_value; }
	if ( value >= record_value ) { nth += 1; }
      }
    else if ( alpha < value && value < beta )
      {
	nth          += 1;
	record_value  = value;
      }
    else { break; } /* record move failed high or low. */

    if ( alpha < value && value < beta )
      {
	pdata->buf[ pos_buf++ ] = Move2S(move);
	for ( ply = 2; ply <= ptree->pv[1].length; ply++ )
	  {
	    pdata->buf[ pos_buf++ ] = Move2S(ptree->pv[1].a[ply]);
	  }
	pdata->buf[ pos_buf - 1 ] |= 0x8000U;
      }
  }

  if ( nth - 1 >= 0 )
    {
      pdata->result_norm += 1;
      if ( nth-1 < NUM_RESULT ) { pdata->result[nth-1] += 1; }
    }

  if ( nc )
    {
      pdata->pos_buf            = pos_buf;
      pdata->num_moves_counted += nc;
    }
  pdata->num_nodes += ptree->node_searched;
}


static int
read_game( parse1_data_t *pdata )
{
  tree_t *ptree;
  tree_t tree;
  unsigned int record_move;
  int istatus, iret, imove;

  ptree   = & tree;
  istatus = 0;
  if ( pdata->precord->games == pdata->max_games ) { return 1; }

  if ( pdata->precord->games == 0 ) { Out( "  Parse 1 " ); }

  if ( ! ( (pdata->precord->games+1) % DOT_INTERVAL ) )
    {
      if ( ! ( (pdata->precord->games+1) % ( DOT_INTERVAL * 10 ) ) )
	{
	  Out( "o" );
	  if ( ! ( (pdata->precord->games+1) % ( DOT_INTERVAL * 50 ) ) )
	    {
	      Out( "%7d\n          ", pdata->precord->games+1 );
	    }
	}
      else { Out( "." ); }
    }

  for ( imove = 0; imove < MAX_RECORD_LENGTH; imove++ )
    {
      istatus = in_CSA( ptree, pdata->precord, &record_move,
			flag_nomake_move | flag_detect_hang | flag_nofmargin );
      if ( istatus < 0 )
	{
	  pdata->illegal_moves += 1;
	  break;
	}
      if ( istatus >= record_eof ) { break; }

      if ( ! imove
	   && ( root_turn != min_posi_no_handicap.turn_to_move
		|| HAND_B != min_posi_no_handicap.hand_black
		|| HAND_W != min_posi_no_handicap.hand_white
		|| memcmp( BOARD, min_posi_no_handicap.asquare, nsquare ) ) )
	{
	  break;
	}
      
      if ( ! imove )
	{
	  *(pdata->ptree)  = *ptree;
	  pdata->root_turn = root_turn;
	}

      if ( rep_check_learn( ptree, 1 ) == four_fold_rep
	   || ( pdata->precord->moves < MAX_BOOK_PLY
		&& book_probe_learn( ptree, pdata->pbook_entry,
				     pdata->precord->moves, record_move ) ) )
	{
	  pdata->record_moves[ imove ] = record_move | MOVE_VOID;
	}
      else { pdata->record_moves[ imove ] = record_move; }

      iret = make_move_root( ptree, record_move, 0 );
      if ( iret < 0 ) { return iret; }
    }
  
  if ( istatus != record_next && istatus != record_eof )
    {
      istatus = record_wind( pdata->precord );
      if ( istatus < 0 ) { return istatus; }
    }
  
  if ( ! imove && istatus == record_eof ) { return 1; }
  pdata->record_length = imove;

  return 0;
}


static int
learn_parse2( tree_t * restrict ptree, FILE *pf_tmp, int nsteps,
	      double target_out_window, double obj_norm, int nworker )
{
  parse2_data_t *pdata[ TLP_MAX_THREADS ];
  int istep, id;

  for ( id = 0; id < nworker; id++ )
    {
      pdata[id] = (parse2_data_t*)memory_alloc( sizeof(parse2_data_t) );
      if ( pdata[id] == NULL ) { return -1; }
  
      pdata[id]->ptree   = NULL;
      pdata[id]->pf_tmp  = pf_tmp;
      pdata[id]->id      = id;
      pdata[id]->nworker = nworker;
    }

  Out( "  Parse 2\n" );

  istep = 0;
  for ( ;; ) {

#if defined(TLP)
    tlp_num = nworker;
    for ( id = 1; id < nworker; id++ )
      {
#  if defined(_WIN32)
	pdata[id]->ptree = tlp_atree_work + id;
	if ( ! _beginthreadex( 0, 0, parse2_worker, pdata[id], 0, 0 ) )
	  {
	    str_error = "_beginthreadex() failed.";
	    return -1;
	  }
#  else
	pthread_t pt;
	
	pdata[id]->ptree = tlp_atree_work + id;
	if ( pthread_create( &pt, &pthread_attr, parse2_worker, pdata[id] ) )
	  {
	    str_error = "pthread_create() failed.";
	    return -1;
	  }
#  endif
      }
#endif /* TLP */
    
    pdata[0]->ptree = ptree;
    parse2_worker( pdata[0] );

#if defined(TLP)
    while ( tlp_num ) { tlp_yield(); }
#endif
  
    for ( id = 0; id < nworker; id++ )
      {
	if ( pdata[id]->info < 0 ) { return -1; }
      }

    for ( id = 1; id < nworker; id++ )
      {
	add_param( &pdata[0]->param, &pdata[id]->param );
	pdata[0]->num_moves_counted += pdata[id]->num_moves_counted;
	pdata[0]->target            += pdata[id]->target;
      }

    if ( ! istep )
      {
	double target, penalty, objective_function;

	penalty = calc_penalty() / obj_norm;
	target  = ( pdata[0]->target + target_out_window ) / obj_norm;
	objective_function = target + penalty;
	Out( "   Moves Counted   : %d\n", pdata[0]->num_moves_counted );
	Out( "   Objective Func. : %.8f %.8f %.8f\n",
		  objective_function, target, penalty );
	Out( "   Steps " );
      }

    param_sym( &pdata[0]->param );

    renovate_param( &pdata[0]->param );
    istep += 1;
    if ( istep < nsteps ) { Out( "." ); }
    else {
      Out( ". done\n\n" );
      break;
    }
    rewind( pf_tmp );
  }
  for ( id = 0; id < nworker; id++ ) { memory_free( pdata[id] ); }

  return out_param();
}


#  if defined(_MSC_VER)
static unsigned int __stdcall parse2_worker( void *arg )
#  else
static void *parse2_worker( void *arg )
#endif
{
  parse2_data_t *pdata;
  tree_t *ptree;
  unsigned int record_move;
  int iret, imove, nbuf, pos_buf, turn;

  iret  = 0;
  pdata = (parse2_data_t *)arg;
  ptree = pdata->ptree;

  pdata->num_moves_counted = 0;
  pdata->info              = 0;
  pdata->target            = 0.0;
  ini_param( &pdata->param );

  for ( ;; ) {
#if defined(TLP)
    lock( &tlp_lock );
#endif
    iret = read_buf( pdata->buf, pdata->pf_tmp );
#if defined(TLP)
    unlock( &tlp_lock );
#endif

    if ( iret <= 0 )  { break; } /* 0: eof, -2: error */
    nbuf = iret;

    iret = ini_game( ptree, &min_posi_no_handicap, flag_nofmargin,
		     NULL, NULL );
    if ( iret < 0 ) { break; }

    turn    = black;
    pos_buf = 2;
    for ( imove = 0; pos_buf < nbuf; imove++ )
      {
	record_move = s2move( ptree, pdata->buf[pos_buf++], turn );

	if ( pdata->buf[pos_buf] )
	  {
	    pos_buf = calc_deriv( pdata, pos_buf, turn );
	  }
	pos_buf += 1;

	MakeMove( turn, record_move, 1 );
	turn                 = Flip( turn );
	ptree->move_last[1]  = ptree->move_last[0];
	ptree->nsuc_check[0] = 0;
	ptree->nsuc_check[1] = (unsigned char)( InCheck( turn ) ? 1U : 0 );
      }
  }

#if defined(TLP)
  lock( &tlp_lock );
  tlp_num -= 1;
  unlock( &tlp_lock );
#endif

  pdata->info = iret;
  return 0;
}


static int
read_buf( unsigned short *buf, FILE *pf )
{
  size_t size;

  size = fread( buf, sizeof(unsigned short), 2, pf );
  if ( ! size && feof( pf ) ) { return 0; }
  if ( size != 2 )
    {
      str_error = str_io_error;
      return -2;
    }

  size = (size_t)buf[1] + (size_t)buf[0] * 0x10000 - 2;
  if ( fread( buf+2, sizeof(unsigned short), size, pf ) != size )
    {
      str_error = str_io_error;
      return -2;
    }

  return (int)size;
}


static int
calc_deriv( parse2_data_t *pdata, int pos0, int turn0 )
{
  double target, dT, sum_dT;
  tree_t * restrict ptree;
  const unsigned short *buf;
  unsigned int nc;
  int ply, turn, pv_length, pos, record_value, value;

  ptree  = pdata->ptree;
  buf    = pdata->buf;
  nc     = 0;
  turn   = turn0;
  pos    = pos0;
  target = 0.0;
  sum_dT = 0.0;

  ply  = 1;
  for ( ;; ) {
    pdata->pv[ply] = s2move( ptree, buf[ pos+ply-1 ] & 0x7fffU, turn );
    MakeMove( turn, pdata->pv[ply], ply );
    turn = Flip( turn );
    if ( buf[ pos+ply-1 ] & 0x8000U ) { break; }
    ply += 1;
  }
  pv_length = ply;

  record_value = evaluate( ptree, ply+1, turn );
  if ( turn != turn0 ) { record_value = -record_value; }

  for ( ;; ) {
    turn  = Flip( turn );
    UnMakeMove( turn, pdata->pv[ply], ply );
    if ( ply == 1 ) { break; }
    ply -= 1;
  }
  pos += pv_length;

  while ( buf[ pos ] ) {
    ply  = 1;
    for ( ;; ) {
      pdata->pv[ply] = s2move( ptree, buf[ pos+ply-1 ] & 0x7fffU, turn );
      MakeMove( turn, pdata->pv[ply], ply );
      turn = Flip( turn );
      if ( buf[ pos+ply-1 ] & 0x8000U ) { break; }
      ply += 1;
    }
    pv_length = ply;

    value = evaluate( ptree, ply+1, turn );
    if ( turn != turn0 ) { value = -value; }
    target += func( value - record_value );
	  
    dT = dfunc( value - record_value );
    if ( turn0 ) { dT = -dT; }
    sum_dT += dT;
    inc_param( ptree, &pdata->param, -dT );

    for ( ;; ) {
      turn  = Flip( turn );
      UnMakeMove( turn, pdata->pv[ply], ply );
      if ( ply == 1 ) { break; }
      ply -= 1;
    }

    pos += pv_length;
    nc  += 1;
  }

  ply  = 1;
  for ( ;; ) {
    pdata->pv[ply] = s2move( ptree, buf[ pos0+ply-1 ] & 0x7fffU, turn );
    MakeMove( turn, pdata->pv[ply], ply );
    turn = Flip( turn );
    if ( buf[ pos0+ply-1 ] & 0x8000U ) { break; }
    ply += 1;
  }

  inc_param( ptree, &pdata->param, sum_dT );

  for ( ;; ) {
    turn  = Flip( turn );
    UnMakeMove( turn, pdata->pv[ply], ply );
    if ( ply == 1 ) { break; }
    ply -= 1;
  }

  pdata->num_moves_counted += nc;
  pdata->target            += target;

  return pos;
}


static double
func( double x )
{
  const double delta = (double)FV_WINDOW / 7.0;
  double d;
  
  if      ( x < -FV_WINDOW ) { x = -FV_WINDOW; }
  else if ( x >  FV_WINDOW ) { x =  FV_WINDOW; }
  d = 1.0 / ( 1.0 + exp(-x/delta) );

  return d;
}


static double
dfunc( double x )
{
  const double delta = (double)FV_WINDOW / 7.0;
  double dd, dn, dtemp, dret;

  if      ( x <= -FV_WINDOW ) { dret = 0.0; }
  else if ( x >=  FV_WINDOW ) { dret = 0.0; }
  else {
    dn    = exp( - x / delta );
    dtemp = dn + 1.0;
    dd    = delta * dtemp * dtemp;
    dret  = dn / dd;
  }
  
  return dret;
}


static unsigned int
s2move( const tree_t * restrict ptree, unsigned int move, int tt )
{
  int from, to;

  from = I2From(move);
  if ( from < nsquare )
    {
      to = I2To(move);
      move |= tt ? (Piece2Move(-BOARD[from])|Cap2Move( BOARD[to]))
	         : (Piece2Move( BOARD[from])|Cap2Move(-BOARD[to]));
    }
  return move;
}


static void
ini_book( book_entry_t *pbook_entry )
{
  int i, j;
  
  for ( i = 0; i < NumBookEntry; i++ )
    for ( j = 0; j < NumBookCluster; j++ ) {
      pbook_entry[i].cluster[j].a = (uint64_t)0;
      pbook_entry[i].cluster[j].b = (uint64_t)0x1ffU << 41;
    }
}


/*
a: key     64   0

b: ply      9  41
   move    19  22
   turn     1  21
   hand    21   0
*/
static int
book_probe_learn( const tree_t * restrict ptree,
		  book_entry_t *pbook_entry,
		  int ply, unsigned int move )
{
  book_entry_t *p;
  int i, j;

  move &= 0x7ffffU;
  p = pbook_entry
    + ((unsigned int)HASH_KEY & (unsigned int)(NumBookEntry-1));

  for ( i = 0; i < NumBookCluster; i++ )
    if ( p->cluster[i].a == HASH_KEY
	 && ((unsigned int)p->cluster[i].b & 0x1fffffU) == HAND_B
	 && ( (((unsigned int)p->cluster[i].b>>21) & 0x1U)
	      == (unsigned int)root_turn )
	 && ((unsigned int)(p->cluster[i].b>>22) & 0x7ffffU) == move ) {
      return 1;
    }
  
  for ( i = 0; i < NumBookCluster; i++ )
    if ( ( (unsigned int)( p->cluster[i].b >> 41 ) & 0x1ffU )
	 > (unsigned int)ply ) { break; }

  if ( i < NumBookCluster ) {
    for ( j = NumBookCluster-1; j > i; j-- ) {
      p->cluster[j].a = p->cluster[j-1].a;
      p->cluster[j].b = p->cluster[j-1].b;
    }

    p->cluster[i].a = HASH_KEY;
    p->cluster[i].b
      = ( (uint64_t)move<<22 ) | ( (uint64_t)ply << 41 )
      | (uint64_t)( (root_turn<<21) | HAND_B );
  }

  return 0;
}


static int
rep_check_learn( tree_t * restrict ptree, int ply )
{
  int n, i, imin;

  n    = ptree->nrep + ply - 1;
  imin = n - REP_MAX_PLY;
  if ( imin < 0 ) { imin = 0; }

  for ( i = n-2; i >= imin; i -= 2 )
    if ( ptree->rep_board_list[i] == HASH_KEY
	 && ptree->rep_hand_list[i] == HAND_B ) { return four_fold_rep; }

  return no_rep;
}

#endif /* no MINIMUM */
