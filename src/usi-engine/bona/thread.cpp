// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#if defined(_WIN32)
#  include <process.h>
#else
#  include <arpa/inet.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include <sched.h>
#endif
#include "shogi.h"


#if defined(TLP) || defined(DFPN_CLIENT)
int CONV
lock_init( lock_t *plock )
{
#  if defined(_MSC_VER)
  *plock = 0;
#  elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
  *plock = 0;
#  else
  if ( pthread_mutex_init( plock, 0 ) )
    {
      str_error = "pthread_mutex_init() failed.";
      return -1;
    }
#  endif
  return 1;
}


int CONV
lock_free( lock_t *plock )
{
#  if defined(_MSC_VER)
  *plock = 0;
#  elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
  *plock = 0;
#  else
  if ( pthread_mutex_destroy( plock ) )
    {
      str_error = "pthread_mutex_destroy() failed.";
      return -1;
    }
#  endif
  return 1;
}


void CONV
unlock( lock_t *plock )
{
#  if defined(_MSC_VER)
  *plock = 0;
#  elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
  *plock = 0;
#  else
  pthread_mutex_unlock( plock );
#  endif
}


void CONV
lock( lock_t *plock )
{
#  if defined(_MSC_VER)
#if defined(YSS_ZERO)
  while ( _InterlockedExchange( plock, 1 ) )
#else
  while ( _InterlockedExchange( (void *)plock, 1 ) )
#endif
    {
      while ( *plock ) { tlp_yield();}
    }
#  elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
  int itemp;

  for ( ;; )
    {
      asm ( "1:   movl     $1,  %1 \n\t"
	    "     xchgl   (%0), %1 \n\t"
	    : "=g" (plock), "=r" (itemp) : "0" (plock) );
      if ( ! itemp ) { return; }
      while ( *plock ) { tlp_yield();}
    }
#  else
  pthread_mutex_lock( plock );
#  endif
}


void
tlp_yield( void )
{
#if defined(_WIN32)
  Sleep( 0 );
#else
  sched_yield();
#endif
}
#endif /* TLP || DFPN_CLIENT */


#if defined(DFPN_CLIENT)

static int CONV proce_line( char *line_buf );
#  if defined(_WIN32)
static unsigned int __stdcall dfpn_client_receiver( void *arg );
#  else
static void *dfpn_client_receiver( void *arg );
#  endif

void CONV
dfpn_client_start( const tree_t * restrict ptree )
{
  if ( root_turn != min_posi_no_handicap.turn_to_move
       || HAND_B != min_posi_no_handicap.hand_black
       || HAND_W != min_posi_no_handicap.hand_white
       || memcmp( BOARD, min_posi_no_handicap.asquare, nsquare ) )
    {
      sckt_shutdown( dfpn_client_sckt );
      dfpn_client_sckt = SCKT_NULL;
      return;
    }

  if ( dfpn_client_sckt        != SCKT_NULL ) { return; }
  if ( dfpn_client_str_addr[0] == '\0' )      { return; }

  dfpn_client_sckt = sckt_connect( dfpn_client_str_addr, dfpn_client_port );
  if ( dfpn_client_sckt == SCKT_NULL )
    {
      out_warning( "Connection to DFPN server failed." );
      return;
    }
  else {
    Out( "New connection to DFPN server: %s %d\n",
	 dfpn_client_str_addr, dfpn_client_port );
  }

#  if defined(_WIN32)
  if ( ! _beginthreadex( 0, 0, &dfpn_client_receiver, NULL, 0, 0 ) )
    {
      sckt_shutdown( dfpn_client_sckt );
      dfpn_client_sckt = SCKT_NULL;
      out_warning( "_beginthreadex() failed." );
    }
#  else
  {
    pthread_t pt;
    if ( pthread_create( &pt, &pthread_attr, &dfpn_client_receiver, NULL ) )
      {
	sckt_shutdown( dfpn_client_sckt );
	dfpn_client_sckt = SCKT_NULL;
	out_warning( "_beginthreadex() failed." );
      }
  }
#  endif

  dfpn_client_out( "Client: anonymous\n" );
}


#  if defined(_MSC_VER)
#    pragma warning(disable:4100)
#  elif defined(__ICC)
#    pragma warning(disable:869)
#  endif

#  if defined(_MSC_VER)
static unsigned int __stdcall dfpn_client_receiver( void *arg )
#  else
static void *dfpn_client_receiver( void *arg )
#endif
{
#define SIZE_RECV_BUF ( 1024 * 16 )
#define SIZE_LINE_BUF 1024
  char recv_buf[ SIZE_RECV_BUF ];
  char line_buf[ SIZE_LINE_BUF ];
  char *str_end, *str_line_end;
  size_t size;
  int len_recv_buf;
  struct timeval tv;
  fd_set readfds;
  int iret;

  recv_buf[0] = '\0';

  /* recv loop */
  for ( ;; ) {

    /* select loop */
    for ( ;; ) {
      tv.tv_sec  = SEC_KEEP_ALIVE;
      tv.tv_usec = 0;
      FD_ZERO( &readfds );
#  if defined(_MSC_VER)
#    pragma warning(disable:4127)
#  endif
      FD_SET( dfpn_client_sckt, &readfds );
#  if defined(_MSC_VER)
#    pragma warning(default:4127)
#  endif
      
      iret = select( (int)dfpn_client_sckt+1, &readfds, NULL, NULL, &tv );
      if ( iret == SOCKET_ERROR )
	{
	  out_warning( "%s", str_error );
	  goto dfpn_client_receiver_shutdown;
	}

      /* message arrived */
      if ( iret ) { break; }

      /* timeout and keepalive */
      lock( &dfpn_client_lock);
      iret = dfpn_client_out( "ping\n" );
      unlock( &dfpn_client_lock);
      if ( iret < 0 ) { return 0; }
    }

    /* read messages */
    len_recv_buf = (int)strlen( recv_buf );
    str_end      = recv_buf + len_recv_buf;

    iret = recv( dfpn_client_sckt, str_end, SIZE_RECV_BUF-1-len_recv_buf, 0 );
    if ( iret == SOCKET_ERROR )
      {
	str_error = str_WSAError( "recv() failed:" );
	out_warning( "%s", str_error );
	goto dfpn_client_receiver_shutdown;
      }
    if ( ! iret ) { goto dfpn_client_receiver_shutdown; }
    recv_buf[ len_recv_buf + iret ] = '\0';

    /* take each line */
    lock( &dfpn_client_lock );
    for ( ;; ) {

      str_line_end = strchr( recv_buf, '\n' );
      if ( str_line_end == NULL )
	{
	  if ( iret + len_recv_buf + 1 >= SIZE_RECV_BUF )
	    {
	      unlock( &dfpn_client_lock );
	      out_warning( "%s", str_ovrflw_line );
	      goto dfpn_client_receiver_shutdown;
	    }
	  break;
	}

      size = str_line_end - recv_buf;
      if ( size + 1 >= SIZE_LINE_BUF )
	{
	  unlock( &dfpn_client_lock );
	  out_warning( "%s", str_ovrflw_line );
	  goto dfpn_client_receiver_shutdown;
	}
      
      memcpy( line_buf, recv_buf, size );
      memmove( recv_buf, str_line_end+1, strlen(str_line_end+1) + 1 );

      line_buf[size] = '\0';

      if ( proce_line( line_buf ) < 0 )
	{
	  unlock( &dfpn_client_lock );
	  out_warning( "invalid messages from DFPN server" );
	  goto dfpn_client_receiver_shutdown;
	}
    }
    unlock( &dfpn_client_lock );
  }

 dfpn_client_receiver_shutdown:
  sckt_shutdown( dfpn_client_sckt );
  dfpn_client_sckt = SCKT_NULL;
  out_warning( "A connection to DFPN server is down." );

  return 0;
}
#  if defined(_MSC_VER)
#    pragma warning(default:4100)
#  elif defined(__ICC)
#    pragma warning(default:869)
#endif


static int CONV proce_line( char *line_buf )
{
  const char *token;
  volatile dfpn_client_cresult_t *pcresult;
  char *last;

  token = strtok_r( line_buf, str_delimiters, &last );
  if ( token == NULL ) { return -1; }

  /* check signature */
  if ( strcmp( token, (const char *)dfpn_client_signature ) ) { return 1; }

  token = strtok_r( NULL, str_delimiters, &last );
  if ( token == NULL ) { return -1; }

  /* root node */
  if ( ! strcmp( token, "WIN" ) )
    {
      token = strtok_r( NULL, str_delimiters, &last );
      if ( token == NULL || ! is_move( token ) ) { return -1; }
      dfpn_client_rresult   = dfpn_client_win;
      dfpn_client_flag_read = 1;
      memcpy( (char *)dfpn_client_str_move, token, 7 );
      return 1;
    }

  if ( ! strcmp( token, "LOSE" ) )
    {
      dfpn_client_rresult   = dfpn_client_lose;
      dfpn_client_flag_read = 1;
      return 1;
    }

  if ( ! strcmp( token, "UNSOLVED" ) )
    {
      dfpn_client_rresult   = dfpn_client_misc;
      dfpn_client_flag_read = 1;
      return 1;
    }

  /* child node */
  if ( ! is_move( token ) ) { return -1; }
  if ( MAX_LEGAL_MOVES <= dfpn_client_num_cresult ) { return -1; }
  pcresult = &dfpn_client_cresult[dfpn_client_num_cresult];
  memcpy( (char *)pcresult->str_move, token, 7 );

  token = strtok_r( NULL, str_delimiters, &last );
  if ( token == NULL ) { return -1; }
  
  if ( ! strcmp( token, "WIN" ) )
    {
      pcresult->result         = dfpn_client_win;
      dfpn_client_flag_read    = 1;
      dfpn_client_num_cresult += 1;
      return 1;
    }

  if ( ! strcmp( token, "LOSE" ) )
    {
      pcresult->result         = dfpn_client_lose;
      dfpn_client_flag_read    = 1;
      dfpn_client_num_cresult += 1;
      return 1;
    }

  if ( ! strcmp( token, "UNSOLVED" ) )
    {
      pcresult->result         = dfpn_client_misc;
      dfpn_client_flag_read    = 1;
      dfpn_client_num_cresult += 1;
      return 1;
    }

  return -1;
}

#endif /* DFPN_CLINET */


#if defined(TLP)

#  if defined(_WIN32)
static unsigned int __stdcall start_address( void *arg );
#  else
static void *start_address( void *arg );
#  endif

static tree_t *find_child( void );
//static void init_state( const tree_t * restrict parent, tree_t * restrict child );
static void copy_state( tree_t * restrict parent,
			const tree_t * restrict child, int value );
static void wait_work( int tid, tree_t *parent );

int
tlp_start( void )
{
  int work[ TLP_MAX_THREADS ];
  int num;

  if ( tlp_num ) { return 1; }

  for ( num = 1; num < tlp_max; num++ )
    {
      work[num] = num;
      
#  if defined(_WIN32)
      if ( ! _beginthreadex( 0, 0, start_address, work+num, 0, 0 ) )
	{
	  str_error = "_beginthreadex() failed.";
	  return -2;
	}
#  else
      {
	pthread_t pt;
	if ( pthread_create( &pt, &pthread_attr, start_address, work+num ) )
	  {
	    str_error = "pthread_create() failed.";
	    return -2;
	  }
      }
#  endif
    }
  while ( tlp_num +1 < tlp_max ) { tlp_yield(); }

  return 1;
}


void
tlp_end( void )
{
  tlp_abort = 1;
  while ( tlp_num ) { tlp_yield(); }
  tlp_abort = 0;
}


int
tlp_split( tree_t * restrict ptree )
{
  tree_t *child;
  int num, nchild, i;

  lock( &tlp_lock );

  if ( ! tlp_idle || ptree->tlp_abort )
    {
      unlock( &tlp_lock );
      return 0;
    }

  tlp_ptrees[ ptree->tlp_id ] = NULL;
  ptree->tlp_nsibling         = 0;
  nchild                      = 0;
  for ( num = 0; num < tlp_max; num++ )
    {
      if ( tlp_ptrees[num] ) { ptree->tlp_ptrees_sibling[num] = 0; }
      else {
	child = find_child();
	if ( ! child ) { continue; }

	nchild += 1;

	for ( i=0; i<tlp_max; i++ ) { child->tlp_ptrees_sibling[i] = NULL; }
	child->tlp_ptree_parent        = ptree;
	child->tlp_id                  = (unsigned char)num;
	child->tlp_used                = 1;
	child->tlp_abort               = 0;
	ptree->tlp_ptrees_sibling[num] = child;
	ptree->tlp_nsibling           += 1;
	init_state( ptree, child );

	tlp_ptrees[num] = child;
      }
    }

  if ( ! nchild )
    {
      tlp_ptrees[ ptree->tlp_id ] = ptree;
      unlock( &tlp_lock );
      return 0;
    }
  
  tlp_nsplit += 1;
  tlp_idle   += 1;

  unlock( &tlp_lock );
  
  wait_work( ptree->tlp_id, ptree );

  return 1;
}


void
tlp_set_abort( tree_t * restrict ptree )
{
  int num;

  ptree->tlp_abort = 1;
  for ( num = 0; num < tlp_max; num++ )
    if ( ptree->tlp_ptrees_sibling[num] )
      {
	tlp_set_abort( ptree->tlp_ptrees_sibling[num] );
      }
}


#if defined(MNJ_LAN) || defined(USI)
uint64_t
tlp_count_node( tree_t * restrict ptree )
{
  uint64_t uret = ptree->node_searched;
  int num;

  for ( num = 0; num < tlp_max; num++ )
    if ( ptree->tlp_ptrees_sibling[num] )
      {
	uret += tlp_count_node( ptree->tlp_ptrees_sibling[num] );
      }

  return uret;
}
#endif


int
tlp_is_descendant( const tree_t * restrict ptree, int slot_ancestor )
{
  int slot = (int)ptree->tlp_slot;

  for ( ;; ) {
    if ( slot == slot_ancestor ) { return 1; }
    else if ( ! slot )           { return 0; }
    else { slot = tlp_atree_work[slot].tlp_ptree_parent->tlp_slot; }
  }
}


#  if defined(_MSC_VER)
static unsigned int __stdcall start_address( void *arg )
#  else
static void *start_address( void *arg )
#endif
{
  int tid = *(int *)arg;

  tlp_ptrees[tid] = NULL;

  lock( &tlp_lock );
  Out( "Hi from thread no.%d\n", tid );
  tlp_num  += 1;
  tlp_idle += 1;
  unlock( &tlp_lock );

  wait_work( tid, NULL );

  lock( &tlp_lock );
  Out( "Bye from thread no.%d\n", tid );
  tlp_num  -= 1;
  tlp_idle -= 1;
  unlock( &tlp_lock );

  return 0;
}


static void
wait_work( int tid, tree_t *parent )
{
  tree_t *slot;
  int value;

  for ( ;; ) {

    for ( ;; ) {
      if ( tlp_ptrees[tid] )                  { break; }
      if ( parent && ! parent->tlp_nsibling ) { break; }
      if ( tlp_abort )                        { return; }

      tlp_yield();
    }

    lock( &tlp_lock );
    if ( ! tlp_ptrees[tid] ) { tlp_ptrees[tid] = parent; }
    tlp_idle -= 1;
    unlock( &tlp_lock );

    slot = tlp_ptrees[tid];
    if ( slot == parent ) { return; }

    value = tlp_search( slot,
			slot->tlp_ptree_parent->tlp_best,
			slot->tlp_ptree_parent->tlp_beta,
			slot->tlp_ptree_parent->tlp_turn,
			slot->tlp_ptree_parent->tlp_depth,
			slot->tlp_ptree_parent->tlp_ply,
			slot->tlp_ptree_parent->tlp_state_node );
    
    lock( &tlp_lock );
    copy_state( slot->tlp_ptree_parent, slot, value );
    slot->tlp_ptree_parent->tlp_nsibling            -= 1;
    slot->tlp_ptree_parent->tlp_ptrees_sibling[tid]  = NULL;
    slot->tlp_used   = 0;
    tlp_ptrees[tid]  = NULL;
    tlp_idle        += 1;
    unlock( &tlp_lock);
  }
}


static tree_t *
find_child( void )
{
  int i;

  for ( i = 1; i < TLP_NUM_WORK && tlp_atree_work[i].tlp_used; i++ );
  if ( i == TLP_NUM_WORK ) { return NULL; }
  if ( i > tlp_nslot ) { tlp_nslot = i; }

  return tlp_atree_work + i;
}


void
init_state( const tree_t * restrict parent, tree_t * restrict child )
{
  int i, ply;

  child->posi                  = parent->posi;
  child->node_searched         = 0;
  child->null_pruning_done     = 0;
  child->null_pruning_tried    = 0;
  child->check_extension_done  = 0;
  child->recap_extension_done  = 0;
  child->onerp_extension_done  = 0;
  child->neval_called          = 0;
  child->nquies_called         = 0;
  child->nfour_fold_rep        = 0;
  child->nperpetual_check      = 0;
  child->nsuperior_rep         = 0;
  child->nrep_tried            = 0;
  child->ntrans_always_hit     = 0;
  child->ntrans_prefer_hit     = 0;
  child->ntrans_probe          = 0;
  child->ntrans_exact          = 0;
  child->ntrans_lower          = 0;
  child->ntrans_upper          = 0;
  child->ntrans_superior_hit   = 0;
  child->ntrans_inferior_hit   = 0;
  child->fail_high             = 0;
  child->fail_high_first       = 0;
#if defined(YSS_ZERO)
  ply = 1;
#else
  ply = parent->tlp_ply;
#endif

  child->anext_move[ply].value_cap1 = parent->anext_move[ply].value_cap1;
  child->anext_move[ply].value_cap2 = parent->anext_move[ply].value_cap2;
  child->anext_move[ply].move_cap1  = parent->anext_move[ply].move_cap1;
  child->anext_move[ply].move_cap2  = parent->anext_move[ply].move_cap2;

#if defined(YSS_ZERO)
  child->move_last[0]          = child->amove;
#else
  child->move_last[ply]        = child->amove;
  child->save_eval[ply]        = parent->save_eval[ply];
  child->current_move[ply-1]   = parent->current_move[ply-1];
  child->nsuc_check[ply-1]     = parent->nsuc_check[ply-1];
  child->nsuc_check[ply]       = parent->nsuc_check[ply];
#endif
  child->nrep                  = parent->nrep;

  memcpy( child->hist_good,  parent->hist_good,  sizeof(parent->hist_good) );
  memcpy( child->hist_tried, parent->hist_tried, sizeof(parent->hist_tried) );

  for ( i = 0; i < child->nrep + ply - 1; i++ )
    {
      child->rep_board_list[i] = parent->rep_board_list[i];
      child->rep_hand_list[i]  = parent->rep_hand_list[i];
    }
  for ( i = ply; i < PLY_MAX; i++ )
    {
      child->amove_killer[i] = parent->amove_killer[i];
      child->killers[i]      = parent->killers[i];
    }

#if defined(YSS_ZERO)
  // need to copy record_plus_ply_min_posi[0].
  int loop = child->nrep + ply - 0;
  if ( loop <= 0 ) { printf( "thread copy err.\n" ); debug(); }
  for ( i = 0; i < loop; i++ )
    {
      child->record_plus_ply_min_posi[i] = parent->record_plus_ply_min_posi[i];
      child->history_in_check[i]         = parent->history_in_check[i];
      child->keep_sequence_hash[i]       = parent->keep_sequence_hash[i];
    }
  child->sequence_hash = parent->sequence_hash;
#endif

}


static void
copy_state( tree_t * restrict parent, const tree_t * restrict child,
	    int value )
{
  int i, ply;

  parent->check_extension_done += child->check_extension_done;
  parent->recap_extension_done += child->recap_extension_done;

  if ( ! child->node_searched ) { return; }

  parent->node_searched        += child->node_searched;
  parent->null_pruning_done    += child->null_pruning_done;
  parent->null_pruning_tried   += child->null_pruning_tried;
  parent->onerp_extension_done += child->onerp_extension_done;
  parent->neval_called         += child->neval_called;
  parent->nquies_called        += child->nquies_called;
  parent->nrep_tried           += child->nrep_tried;
  parent->nfour_fold_rep       += child->nfour_fold_rep;
  parent->nperpetual_check     += child->nperpetual_check;
  parent->nsuperior_rep        += child->nsuperior_rep;
  parent->ntrans_always_hit    += child->ntrans_always_hit;
  parent->ntrans_prefer_hit    += child->ntrans_prefer_hit;
  parent->ntrans_probe         += child->ntrans_probe;
  parent->ntrans_exact         += child->ntrans_exact;
  parent->ntrans_lower         += child->ntrans_lower;
  parent->ntrans_upper         += child->ntrans_upper;
  parent->ntrans_superior_hit  += child->ntrans_superior_hit;
  parent->ntrans_inferior_hit  += child->ntrans_inferior_hit;
  parent->fail_high_first      += child->fail_high_first;
  parent->fail_high            += child->fail_high;

  if ( child->tlp_abort || value <= parent->tlp_best ) { return; }

  ply              = parent->tlp_ply;
  parent->tlp_best = (short)value;
  parent->pv[ply]  = child->pv[ply];

  lock( &parent->tlp_lock );
  parent->current_move[ply] = child->current_move[ply];
  memcpy( parent->hist_tried, child->hist_tried, sizeof(child->hist_tried) );
  memcpy( parent->hist_good,  child->hist_good,  sizeof(child->hist_good) );
  for ( i = ply; i < PLY_MAX; i++ ) 
    {
      parent->amove_killer[i] = child->amove_killer[i];
      parent->killers[i]      = child->killers[i];
    }
  unlock( &parent->tlp_lock );
}


#endif /* TLP */
