// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(DFPN)

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "shogi.h"
#include "dfpn.h"

dfpn_hash_entry_t * restrict dfpn_hash_tbl = NULL;
unsigned int dfpn_hash_age;
unsigned int dfpn_hash_mask;

#if defined(DFPN_DBG)
int dbg_flag = 1;
static void CONV dbg_out_move_seq( const node_t * restrict nodes, int ply );
static void CONV dbg_min_delta_c( const node_t * restrict pnode, int ply );
static void CONV dbg_sum_phi_c( const node_t * restrict pnode, int ply );
static void CONV dbg_nodes( const node_t * restrict pnode, int ply );
#endif


static int CONV useless_delta_ticking( node_t * restrict pnode );
static unsigned int CONV min_delta_c( const node_t * restrict pnode );
static unsigned int CONV sum_phi_c( const node_t * restrict pnode );
static int CONV init_children( tree_t * restrict ptree,
			       dfpn_tree_t * restrict pdfpn_tree, int ply );
static int CONV mid( tree_t * restrict ptree,
		     dfpn_tree_t * restrict pdfpn_tree, int ply );
static void CONV select_child( const tree_t * restrict ptree,
			       node_t * restrict pnode_c,
			       unsigned int * restrict pphi_c,
			       unsigned int * restrict pdelta_c,
			       unsigned int * restrict pdelta_2 );
static void CONV num2str( char buf[16], unsigned int num );

#if defined(TLP)
static dfpn_tree_t dfpn_tree[ TLP_MAX_THREADS ];
#else
static dfpn_tree_t dfpn_tree;
#endif

int CONV
dfpn( tree_t * restrict ptree, int turn, int ply )
{
  dfpn_tree_t * restrict pdfpn_tree;
  node_t * restrict pnode;
  float fcpu_percent, fnps, fsat;
  unsigned int cpu0, cpu1, elapse0, elapse1, u;
  int iret, i;
  
  if ( get_cputime( &cpu0 )    < 0 ) { return -1; }
  if ( get_elapsed( &elapse0 ) < 0 ) { return -1; }

  u =  node_per_second / 16U;
  if      ( u > TIME_CHECK_MAX_NODE ) { u = TIME_CHECK_MAX_NODE; }
  else if ( u < TIME_CHECK_MIN_NODE ) { u = TIME_CHECK_MIN_NODE; }
  node_next_signal = u;
  node_last_check  = 0;
  root_abort       = 0;
  time_max_limit   = UINT_MAX;
  time_limit       = UINT_MAX;
  game_status     &= ~( flag_move_now | flag_suspend | flag_search_error );
  game_status     |= flag_thinking;

#if defined(TLP)
  pdfpn_tree = &dfpn_tree[ptree->tlp_id];
#else
  pdfpn_tree = &dfpn_tree;
#endif
  ptree->node_searched = 0;

  pdfpn_tree->root_ply     = ply;
  pdfpn_tree->turn_or      = turn;
  pdfpn_tree->sum_phi_max  = 0;
  pdfpn_tree->node_limit   = 50000000;

  pnode = pdfpn_tree->anode + ply;
  pnode->phi           = INF_1;
  pnode->delta         = INF_1;
  pnode->turn          = turn;
  pnode->children      = pdfpn_tree->child_tbl;
  pnode->new_expansion = 1;

  assert( 1 <= ply );
  assert( pdfpn_tree->node_limit < DFPN_NODES_MASK );
  iret = mid( ptree, pdfpn_tree, ply );
  if ( 0 <= iret
       && pnode->phi   != INF
       && pnode->delta != INF )
    {
      char buf1[16], buf2[16];

      num2str( buf1, pnode->phi );
      num2str( buf2, pnode->delta );
      Out( "Research [%s:%s]\n", buf1, buf2 );
      pnode->phi   = INF;
      pnode->delta = INF;
      iret = mid( ptree, pdfpn_tree, ply );
    }

  game_status &= ~flag_thinking;

  if ( game_status & flag_search_error ) { return -1; }

  if  ( iret < 0 )
    {
      DFPNOut( "UNSOLVED %d\n", iret );
      switch ( iret ) {
      case DFPN_ERRNO_MAXNODE:  Out( "RESULT: MAX NODE\n" );      break;
      case DFPN_ERRNO_MAXPLY:   Out( "RESULT: MAX PLY\n" );       break;
      case DFPN_ERRNO_SUMPHI:   Out( "RESULT: MAX SUM_PHI\n" );   break;
      case DFPN_ERRNO_DELTA2P:  Out( "RESULT: MAX DELTA2+1\n" );  break;
      default:
	assert( DFPN_ERRNO_SIGNAL == iret );
        Out( "RESULT: SIGNAL\n" );
	break;
      }
    }
  else if ( pnode->delta == INF )
    {
      const char *str;
      for ( i = 0; i < pnode->nmove && pnode->children[i].phi != INF; i++ );
      assert( i < pnode->nmove );
      str = str_CSA_move(pnode->children[i].move);
      Out( "RESULT: WIN %s\n", str );
      DFPNOut( "WIN %s\n", str );
    }
  else {
    Out( "RESULT: LOSE\n" );
    DFPNOut( "LOSE\n" );
  }

  fsat = dfpn_hash_sat();
  if ( 3.0 < fsat )
    {
      dfpn_hash_age += 1U;
      dfpn_hash_age &= DFPN_AGE_MASK;
    }

  if ( get_cputime( &cpu1 )    < 0 ) { return -1; }
  if ( get_elapsed( &elapse1 ) < 0 ) { return -1; }
  fcpu_percent     = 100.0F * (float)( cpu1 - cpu0 );
  fcpu_percent    /= (float)( elapse1 - elapse0 + 1U );
  fnps             = 1000.0F * (float)ptree->node_searched;
  fnps            /= (float)( elapse1 - elapse0 + 1U );
  node_per_second  = (unsigned int)( fnps + 0.5 );

  Out( "n=%" PRIu64 " sat=%3.1f%%", ptree->node_searched, fsat );
  Out( " age=%u sum_phi=%u", dfpn_hash_age, pdfpn_tree->sum_phi_max );
  Out( " time=%.2f cpu=%.1f%% nps=%.2fK\n",
       (float)( elapse1 - elapse0 + 1U ) / 1000.0F,
       fcpu_percent, fnps / 1e3F );
    
  Out( "\n" );

  return 1;
}


static void CONV
num2str( char buf[16], unsigned int num )
{
  if      ( num == INF )   { snprintf( buf, 16, "inf" ); }
  else if ( num == INF_1 ) { snprintf( buf, 16, "inf-1" ); }
  else                     { snprintf( buf, 16, "%u", num ); }
  buf[15] = '\0';
}


static int CONV
mid( tree_t * restrict ptree, dfpn_tree_t * restrict pdfpn_tree, int ply )
{
  node_t * restrict pnode = pdfpn_tree->anode + ply;
  uint64_t node_searched0;

#if defined(DFPN_DBG)
  if ( ptree->node_searched >= 1000000 ) { dbg_flag = 1; }
  DOut( ply, "MID START [%u,%u]",
	pnode->phi, pnode->delta );
  dbg_out_move_seq( pdfpn_tree->anode, ply );
#endif

  node_searched0 = ptree->node_searched;
  
  if ( pdfpn_tree->node_limit
       <= ++ptree->node_searched ) { return DFPN_ERRNO_MAXNODE; }
  
#if ! defined(MINIMUM)
  if ( ! ( game_status & flag_learning ) )
#endif
#if defined(TLP)
    if ( ! ptree->tlp_id )
#endif
      if ( node_next_signal < ++node_last_check && detect_signals( ptree ) )
	{
	  return DFPN_ERRNO_SIGNAL;
	}
  
  if ( PLY_MAX-4 < ply ) { return DFPN_ERRNO_MAXPLY; }
  
  if ( init_children( ptree, pdfpn_tree, ply ) )
    {
      pnode->phi   = 0;
      pnode->delta = INF;
      pnode->nodes = 1;
      dfpn_hash_store( ptree, pdfpn_tree, ply );
      DOut( ply, "MID END: WIN\n" );
      return 1;
    }
  else if ( ! pnode->nmove )
    {
      pnode->phi   = INF;
      pnode->delta = 0;
      pnode->nodes = 1;
      dfpn_hash_store( ptree, pdfpn_tree, ply );
      DOut( ply, "MID END: LOSE\n" );
      return 1;
    }

  DOut( ply, "NMOVE: %d", pnode->nmove );
  pnode->hash_key = HASH_KEY;
  pnode->hand_b   = HAND_B;

  for ( ;; ) {

    node_t * restrict pnode_c = pdfpn_tree->anode + ply + 1;
    unsigned int delta_c, phi_c, delta_2;
    unsigned int delta_2p1;
    int iret;

    pnode->min_delta = min_delta_c( pnode );
    pnode->sum_phi   = sum_phi_c( pnode );
    if ( pnode->sum_phi == UINT_MAX ) { return DFPN_ERRNO_SUMPHI; }

    if ( pdfpn_tree->sum_phi_max < pnode->sum_phi && pnode->sum_phi < INF_1 )
      {
	pdfpn_tree->sum_phi_max = pnode->sum_phi;
      }

#if defined(DFPN_DBG)
    dbg_nodes( pnode, ply );
    dbg_min_delta_c( pnode, ply );
    dbg_sum_phi_c( pnode, ply );
#endif
    
    if ( pnode->phi <= pnode->min_delta
	 || pnode->delta <= pnode->sum_phi ) { break; }


    DOut( ply, "PROGRESS: [%u/%u,%u/%u] %u\n",
	  pnode->min_delta, pnode->phi, pnode->sum_phi, pnode->delta, INF );
    
    select_child( ptree, pnode, &phi_c, &delta_c, &delta_2 );
    assert( pdfpn_tree->root_ply == ply
	    || ! pnode->children[pnode->icurr_c].is_loop );

    if ( useless_delta_ticking( pnode ) )
      {
	DOut( ply, "USELESS DELTA TICKING, DELTA_2=%u", pnode->phi );
	delta_2 = pnode->phi;
      }

    if      ( phi_c        == INF_1 ) { pnode_c->phi = INF; }
    else if ( pnode->delta >= INF_1 ) { pnode_c->phi = INF_1; }
    else {
      assert( phi_c != INF && pnode->sum_phi <= pnode->delta + phi_c );
      pnode_c->phi = pnode->delta + phi_c - pnode->sum_phi;
    }

    if ( delta_c == INF_1 ) { pnode_c->delta = INF; }
    else {
      if      ( INF_1 - 1 == delta_2 ) { return DFPN_ERRNO_DELTA2P; }
      else if ( INF_1     <= delta_2 ) { delta_2p1 = delta_2; }
      else                             { delta_2p1 = delta_2 + 1; }

      pnode_c->delta = delta_2p1 < pnode->phi ? delta_2p1 : pnode->phi;
    }

    pnode_c->turn          = Flip(pnode->turn);
    pnode_c->children      = pnode->children + pnode->nmove;

    if ( pnode->children[pnode->icurr_c].nodes == 0 )
      {
	pnode_c->new_expansion = 1;
      }
    else { pnode_c->new_expansion = 0; }

    MakeMove( pnode->turn, pnode->children[pnode->icurr_c].move, ply );

    iret = mid( ptree, pdfpn_tree, ply+1 );

    UnMakeMove( pnode->turn, pnode->children[pnode->icurr_c].move, ply );

    if ( SEARCH_ABORT ) { return iret; }
    if ( iret < 0 )     { return iret; }

    pnode->new_expansion                     += pnode_c->new_expansion;
    pnode->children[pnode->icurr_c].expanded  = pnode_c->new_expansion;

    dfpn_hash_probe( pdfpn_tree, &pnode->children[pnode->icurr_c], ply,
		     Flip(pnode->turn) );
  }

  pnode->phi   = pnode->min_delta;
  pnode->delta = pnode->sum_phi;
  pnode->nodes = ptree->node_searched - node_searched0;
  dfpn_hash_store( ptree, pdfpn_tree, ply );

  DOut( ply, "MID END [%u,%u]\n", pnode->min_delta, pnode->sum_phi );
  return 1;
}


static int CONV
useless_delta_ticking( node_t * restrict pnode )
{
  int i, n;

  n = pnode->nmove;
  for ( i = 0; i < n; i++ )
    {
      if ( pnode->phi <= pnode->children[i].delta )   { continue; }
      if ( pnode->children[i].is_weak == weak_chuai ) { continue; }
      if ( pnode->children[i].is_delta_loop )         { continue; }
      if ( pnode->children[i].expanded == 0 )         { continue; }
      if ( 2000U < pnode->children[i].delta )         { continue; }

      return 0;
    }

  return 1;
}


static int CONV
init_children( tree_t * restrict ptree, dfpn_tree_t * restrict pdfpn_tree,
	       int ply )
{
  node_t * restrict pnode = pdfpn_tree->anode + ply;
  const unsigned int *plast, *pmove;
  unsigned int amove[MAX_NMOVE];
  int ip, sq_king, n;
  
  n = 0;
  
  if ( pdfpn_tree->turn_or == pnode->turn ) {

    sq_king = pnode->turn ? SQ_BKING : SQ_WKING;
    plast   = GenCheck( pnode->turn, amove );
    
    for ( pmove = amove; pmove != plast; pmove++ )
      {
	int from = I2From(*pmove);
	int to   = I2To(*pmove);
	assert( is_move_valid( ptree, *pmove, pnode->turn ) );
	
	MakeMove( pnode->turn, *pmove, ply );

	if ( InCheck(pnode->turn) )
	  {
	    UnMakeMove( pnode->turn, *pmove, ply );
	    continue;
	  }

	if ( pnode->new_expansion
	     && ! ( pnode->turn ? b_have_evasion(ptree)
                                : w_have_evasion(ptree) ) )
	  {
	    pnode->children[0].move  = *pmove;
	    pnode->children[0].phi   = INF;
	    pnode->children[0].delta = 0;
	    pnode->icurr_c           = 0;
	    pnode->nmove             = 1;
	    pnode->children[0].min_hand_b
	      = pnode->turn ? dfpn_max_hand_b( HAND_B, HAND_W )
                            : dfpn_max_hand_w( HAND_B, HAND_W );
	    
	    UnMakeMove( pnode->turn, *pmove, ply );
	    return 1;
	  }


	if ( from < nsquare )
	  {
	    pnode->children[n].is_weak  = 0;
	    pnode->children[n].priority = 1U;
	  }
	/* drop to next square of the king */
	else if ( From2Drop(from) == knight
		  || BBContract( abb_king_attacks[sq_king], abb_mask[to] ) )
	  {
	    pnode->children[n].is_weak  = 0;
	    pnode->children[n].priority = 1U;
	  }
	/* check by piece drop from far way */
	else {
	  pnode->children[n].is_weak
	    = weak_drop + ( adirec[sq_king][to] << 1 ) + ( sq_king < to );
	  pnode->children[n].priority = 1U;
	}

	pnode->children[n].nodes         = 0;
	pnode->children[n].phi           = 1U;
	pnode->children[n].delta         = 1U;
	pnode->children[n].is_loop       = 0;
	pnode->children[n].is_phi_loop   = 0;
	pnode->children[n].is_delta_loop = 0;
	pnode->children[n].hash_key      = HASH_KEY;
	pnode->children[n].hand_b        = HAND_B;
	pnode->children[n].min_hand_b    = HAND_B;
	pnode->children[n].move          = *pmove;
	pnode->children[n].expanded      = UINT64_MAX;
	switch( dfpn_detect_rep( pdfpn_tree, HASH_KEY, HAND_B, ply-3, &ip ) )
	  {
	  case 1:
	    DOut( ply, "LOOP DELTA: %s", str_CSA_move(*pmove) );
	    pnode->children[n].is_loop       = 1;
	    pnode->children[n].is_delta_loop = 1;
	    pnode->children[n].delta = pdfpn_tree->anode[ip].delta;
	    assert( pnode->phi <= pdfpn_tree->anode[ip].delta );
	    break;

	  default:
	    dfpn_hash_probe( pdfpn_tree, &pnode->children[n], ply,
			     Flip(pnode->turn) );
	  }

	UnMakeMove( pnode->turn, *pmove, ply );
	n += 1;
      }

  } else {
    
    bitboard_t bb_intercept;
    unsigned int to;

    assert( InCheck(pnode->turn) );
    plast = GenEvasion( pnode->turn, amove );
    BBIni( bb_intercept );

    sq_king = pnode->turn ? SQ_WKING : SQ_BKING;
    
    for ( pmove  = amove; pmove != plast; pmove++ )
      {
	assert( is_move_valid( ptree, *pmove, pnode->turn ) );
	
	MakeMove( pnode->turn, *pmove, ply );

	pnode->children[n].is_weak = 0;
	to                         = I2To(*pmove);

	/* capture or king move */
	if ( I2PieceMove(*pmove) == king || UToCap(*pmove) )
	  {
	    if ( I2PieceMove(*pmove) == king && UToCap(*pmove) )
	      {
		pnode->children[n].priority = 2U;
	      }
	    else if ( UToCap(*pmove) )
	      {
		pnode->children[n].priority = 3U;
	      }
	    else { pnode->children[n].priority = 4U; }

	    /* non-intercepts may disproof this node easily. */
	    if ( pnode->new_expansion
		 && ! ( pnode->turn ? b_have_checks(ptree)
                                    : w_have_checks(ptree) ) )
	      {
		pnode->children[0].move  = *pmove;
		pnode->children[0].phi   = INF;
		pnode->children[0].delta = 0;
		pnode->icurr_c           = 0;
		pnode->nmove             = 1;
		pnode->children[0].min_hand_b
		  = pnode->turn ? dfpn_max_hand_b( HAND_B, HAND_W )
                                : dfpn_max_hand_w( HAND_B, HAND_W );
		UnMakeMove( pnode->turn, *pmove, ply );
		return 1;
	      }
	  }

	/* interseptions by move */
	else if ( I2From(*pmove) < nsquare )
	  {
	    pnode->children[n].priority = 4U;
	    BBOr( bb_intercept, bb_intercept, abb_mask[to] );
	  }
	/* interseptions by drop */
	else if ( BBContract( bb_intercept, abb_mask[to] )
		  || BBContract( abb_king_attacks[sq_king],
				 abb_mask[to] ) )
	  {
	    pnode->children[n].priority = 5U;
	    pnode->children[n].is_weak  = weak_drop + to;
	  }
	/* 'chuai' interseptions */
	else {
	  pnode->children[n].priority = 6U;
	  pnode->children[n].is_weak  = weak_chuai;
	}

	pnode->children[n].is_loop       = 0;
	pnode->children[n].is_phi_loop   = 0;
	pnode->children[n].is_delta_loop = 0;
	pnode->children[n].hash_key      = HASH_KEY;
	pnode->children[n].hand_b        = HAND_B;
	pnode->children[n].min_hand_b    = HAND_B;
	pnode->children[n].nodes         = 0;
	pnode->children[n].phi           = 1U;
	pnode->children[n].delta         = 1U;
	pnode->children[n].move          = *pmove;
	pnode->children[n].expanded      = UINT64_MAX;
	switch( dfpn_detect_rep( pdfpn_tree, HASH_KEY, HAND_B, ply-3, &ip ) )
	  {
	  case 1:
	    DOut( ply, "LOOP PHI: %s", str_CSA_move(*pmove) );
	    pnode->children[n].is_loop     = 1;
	    pnode->children[n].is_phi_loop = 1;
	    pnode->children[n].phi = pdfpn_tree->anode[ip].phi;
	    assert( pnode->delta <= pdfpn_tree->anode[ip].phi );
	    break;

	  default:
	    dfpn_hash_probe( pdfpn_tree, &pnode->children[n], ply,
			     Flip(pnode->turn) );
	  }

	if ( pnode->children[n].nodes == 0
	     && ! pnode->children[n].is_loop
	     && ! InCheck(Flip(pnode->turn)) ) {
	  unsigned int move = IsMateIn1Ply(Flip(pnode->turn));
	  if ( move ) {
	    
	    unsigned int from       = I2From(move);
	    unsigned int min_hand_b = pnode->turn ? 0 : HAND_B + HAND_W;
	    if ( nsquare <= from ) {
	      unsigned int pc = From2Drop(from);
	      if ( pnode->turn ) { min_hand_b += hand_tbl[pc]; }
	      else               { min_hand_b -= hand_tbl[pc]; }
	    }
	    pnode->children[n].min_hand_b = min_hand_b;
	    pnode->children[n].nodes      = 1;
	    pnode->children[n].phi        = 0;
	    pnode->children[n].delta      = INF;
	  }
	}

	UnMakeMove( pnode->turn, *pmove, ply );
	n += 1;
      }
  }
  
  pnode->nmove = n;

  return 0;
}


/* Select the most promising child */
static void CONV
select_child( const tree_t * restrict ptree,
	      node_t * restrict pnode,
	      unsigned int * restrict pphi_c,
	      unsigned int * restrict pdelta_c,
	      unsigned int * restrict pdelta_2 )
{
  int n = pnode->nmove;
  int i, sq_king, dist, drank, dfile, to, dist_c;
  uint64_t nodes, nodes_c;
  unsigned int phi, delta, priori, priori_c;

  *pdelta_c = INF;
  *pdelta_2 = INF;
  *pphi_c   = INF;
  priori_c  = UINT_MAX;
  nodes_c   = UINT64_MAX;

  for ( i = 0; i < n; i++ )
    {
      if ( pnode->children[i].is_weak == weak_chuai) { continue; }

      phi    = pnode->children[i].phi;
      delta  = pnode->children[i].delta;
      nodes  = pnode->children[i].nodes;
      priori = pnode->children[i].priority;
      assert( phi <= INF && delta <= INF );

      /* Store the smallest and second smallest delta in delta_c and delta_2 */
      if ( delta < *pdelta_c
	   || ( delta == *pdelta_c && nodes < nodes_c )
	   || ( delta == *pdelta_c && nodes == nodes_c && phi < *pphi_c )
	   || ( delta == *pdelta_c && nodes == nodes_c && phi == *pphi_c
		&& priori < priori_c ) )
	{
	  pnode->icurr_c = i;

	  *pdelta_2 = *pdelta_c;
	  *pphi_c   = phi;
	  *pdelta_c = delta;
	  priori_c  = priori;
	  nodes_c   = nodes;
	}
      else if ( delta < *pdelta_2 ) { *pdelta_2 = delta; }
      
      assert( phi != INF );
    }


  if ( *pdelta_c < INF_1 ) { return; }


  /* expand chuai moves, or current node loses */
  dist_c  = INT_MAX;
  sq_king = pnode->turn ? SQ_WKING : SQ_BKING;
  for ( i = 0; i < n; i++ )
    {
      if ( pnode->children[i].is_weak != weak_chuai ) { continue; }

      to       = I2To(pnode->children[i].move);
      drank    = abs( (int)airank[to] - (int)airank[sq_king] );
      dfile    = abs( (int)aifile[to] - (int)aifile[sq_king] );
      dist     = drank < dfile ? dfile : drank;
      phi      = pnode->children[i].phi;
      delta    = pnode->children[i].delta;
      assert( phi <= INF && delta <= INF );

      if ( pnode->phi <= delta ) { continue; }

      if ( dist_c == INT_MAX || dist < dist_c )
	{
	  pnode->icurr_c = i;
	  *pphi_c        = phi;
	  *pdelta_c      = delta;
	  dist_c         = dist;
	}
      
      assert( phi != INF );
    }
  
  *pdelta_2 = INF;
  pnode->children[pnode->icurr_c].is_weak = 0;
}


static unsigned int CONV
min_delta_c( const node_t * restrict pnode )
{
  int n            = pnode->nmove;
  unsigned int min = UINT_MAX;
  int i;

  for ( i = 0; i < n; i++ )
    {
      if ( pnode->children[i].is_weak == weak_chuai ) { continue; }
      if ( pnode->children[i].delta < min ) { min = pnode->children[i].delta; }
    }
  if ( min < INF_1 ) { return min; }
  
  for ( i = 0; i < n; i++ )
    {
      if ( pnode->children[i].is_weak != weak_chuai ) { continue; }
      if ( pnode->children[i].delta < min ) { min = pnode->children[i].delta; }
    }

  assert( min <= INF ); 
  return min;
}


static unsigned int CONV
sum_phi_c( const node_t * restrict pnode )
{
  int n            = pnode->nmove;
  unsigned int sum = 0;
  unsigned int value, type, value_chuai;
  int i, j, have_inf_1, ntype;
  struct { unsigned int value, type; } aphi[ MAX_NMOVE+1 ];

  ntype       = 0;
  have_inf_1  = 0;
  value_chuai = 0;
  sum         = 0;
  for ( i = 0; i < n; i++ )
    {
      type  = pnode->children[i].is_weak;
      value = pnode->children[i].phi;

      if ( value == INF ) { return INF; }

      if ( value == INF_1 ) { have_inf_1 = 1; }

      if ( have_inf_1 ) { continue; }
      if ( value == 0 ) { continue; }

      if ( type == weak_chuai )
	{
	  if ( value_chuai < value ) { value_chuai = value; }
	  continue;
	}
      
      if ( type == 0 || value != 1 )
	{
	  sum += value;
	  continue;
	}

      /* find type in aphi[j].type */
      for ( j = 0, aphi[ntype].type = type; aphi[j].type != type; j++ );

      
      if ( j == ntype )  /* not found */
	{
	  aphi[j].value  = value;
	  ntype         += 1;
	}
      else if ( aphi[j].value < value ) { aphi[j].value = value; }
    }


  if ( have_inf_1 ) { return INF_1; }

  for ( i = ntype-1; i >= 0; i-- ) { sum += aphi[i].value; }

  if      ( INF_1 <= sum ) { sum = UINT_MAX; }
  else if ( sum == 0 )     { sum = value_chuai; }

  return sum;
}


#  if defined(DFPN_DBG)
static void CONV dbg_nodes( const node_t * restrict pnode, int ply )
{
  char buf[65536];
  int n = pnode->nmove;
  int i;

  buf[0] = '\0';
  for ( i = 0; i < n; i++ )
    {
      snprintf( buf, 65536, "%s %" PRIu64 "%c ", buf, pnode->children[i].nodes,
		pnode->children[i].expanded != 0 ? 'o' : 'x' );
    }
  DOut( ply, "NODES:%s", buf );
}


static void CONV dbg_min_delta_c( const node_t * restrict pnode, int ply )
{
  unsigned int value;
  char buf[65536];
  int n = pnode->nmove;
  int i;

  buf[0] = '\0';
  for ( i = 0; i < n; i++ )
    {
      value = pnode->children[i].delta;
      if      ( value == INF )   { snprintf( buf, 65536, "%s inf", buf ); }
      else if ( value == INF_1 ) { snprintf( buf, 65536, "%s inf-1", buf ); }
      else {
	snprintf( buf, 65536, "%s %u%s", buf, value,
		  pnode->children[i].is_delta_loop ? "l" : "" );
      }
    }
  DOut( ply, "DELTA_C=%u:%s", pnode->min_delta, buf );
}


static void CONV dbg_sum_phi_c( const node_t * restrict pnode, int ply )
{
  int n = pnode->nmove;
  unsigned int value, type, value_chuai;
  int i, j, have_inf_1, ntype, iinf_1;
  struct {
    unsigned int value, type, move;
    int is_loop;
  } aphi[ MAX_NMOVE+1 ];

  ntype       = 0;
  have_inf_1  = 0;
  value_chuai = 0;
  iinf_1      = 0;
  for ( i = 0; i < n; i++ )
    {
      type  = pnode->children[i].is_weak;
      value = pnode->children[i].phi;

      if ( value == INF )
	{
	  DOut( ply, "PHI_C=inf: %s",
		 str_CSA_move(pnode->children[i].move) );
	  return;
	}

      if ( value == INF_1 ) { have_inf_1 = 1;  iinf_1 = i; }

      if ( have_inf_1 ) { continue; }

      if ( type == weak_chuai )
	{
	  if ( value_chuai < value ) { value_chuai = value; }
	  continue;
	}
      if ( type == 0 || value != 1 ) { type  = UINT_MAX - i; }

      /* find type in aphi[j].type */
      for ( j = 0, aphi[ntype].type = type; type != aphi[j].type; j++ );

      
      if ( j == ntype )  /* not found */
	{
	  aphi[j].value   = value;
	  aphi[j].move    = pnode->children[i].move;
	  aphi[j].is_loop = pnode->children[i].is_phi_loop;
	  ntype          += 1;
	}
      else if ( aphi[j].value < value )
	{
	  aphi[j].value    = value;
	  aphi[j].move     = pnode->children[i].move;
	  aphi[j].is_loop &= pnode->children[i].is_phi_loop;
	}
    }

  if ( have_inf_1 )
    {
      DOut( ply, "PHI_C=inf-1: %s",
	    str_CSA_move(pnode->children[iinf_1].move) );
    }
  else {
    char buf[65536];

    buf[0] = '\0';
    for ( i = 0; i < ntype; i++ )
      {
	snprintf ( buf, 65536, "%s %s(%u%s)", buf,
		   str_CSA_move(aphi[i].move), aphi[i].value,
		   aphi[i].is_loop ? "l" : "" );
      }
    
    if ( value_chuai )
      {
	snprintf( buf, 65536, "%s CHUAI(%u)", buf, value_chuai );
      }
    DOut( ply, "PHI_C=%u:%s", pnode->sum_phi, buf );
  }
}


static void CONV
dbg_out_move_seq( const node_t * restrict nodes, int ply )
{
  char buf[65536];
  int i;

  buf[0] = '\0';
  for ( i = 1; i < ply; i++ )
    {
      unsigned int move = nodes[i].children[nodes[i].icurr_c].move;
      snprintf( buf, 65536, "%s %s", buf, str_CSA_move(move) );
    }
  DOut( ply, "HIST:%s", buf );
}

#  endif

#endif /* DFPN */
