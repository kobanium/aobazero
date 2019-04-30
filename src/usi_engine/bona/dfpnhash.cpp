// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(DFPN)
#include <limits.h>
#include <assert.h>
#include "shogi.h"
#include "dfpn.h"

#if defined(DEBUG)
static int CONV dbg_is_hand_valid( unsigned int hand );
#endif

static unsigned int CONV calc_hand_win( const node_t *pnode,
					unsigned int hand_b,
					unsigned int hand_w );
static unsigned int CONV calc_hand_lose( const node_t * restrict pnode,
					 unsigned int hand_b,
					 unsigned int hand_w );
static unsigned int CONV hand_or( unsigned int a, unsigned int b );
static unsigned int CONV hand_and( unsigned int a, unsigned int b );
static int CONV dfpn_hand_supe( unsigned int u, unsigned int uref );

const unsigned int hand_tbl[16] = {
  0,                flag_hand_pawn, flag_hand_lance,  flag_hand_knight,
  flag_hand_silver, flag_hand_gold, flag_hand_bishop, flag_hand_rook,
  0,                flag_hand_pawn, flag_hand_lance,  flag_hand_knight,
  flag_hand_silver, 0,              flag_hand_bishop, flag_hand_rook };

static const unsigned int hand_mask[16] = {
  0,         0x00001fU, 0x0000e0U, 0x000700U,
  0x003800U, 0x01c000U, 0x060000U, 0x180000U,
  0,         0x00001fU, 0x0000e0U, 0x000700U,
  0x003800U, 0,         0x060000U, 0x180000U };


int CONV dfpn_ini_hash( void )
{
  size_t size;
  unsigned int u, n2;

  if ( dfpn_hash_tbl != NULL ) { memory_free( dfpn_hash_tbl ); }
  n2 = 1U << dfpn_hash_log2;
  size = sizeof( dfpn_hash_entry_t ) * ( n2 + 1 + DFPN_NUM_REHASH );

  dfpn_hash_tbl = memory_alloc( size );
  if ( dfpn_hash_tbl == NULL ) { return -1; }

  dfpn_hash_mask = n2 -1;
  dfpn_hash_age  = 0;
  for ( u = 0; u < n2 + 1 + DFPN_NUM_REHASH; u++ )
    {
      dfpn_hash_tbl[u].word3 = 0;
    }

  Out( "DFPN Table Entries = %uk (%uMB)\n",
       ( dfpn_hash_mask + 1U ) / 1024U, size / ( 1024U * 1024U ) );

  return 1;
}


float CONV dfpn_hash_sat( void )
{
  uint64_t nodes;
  unsigned int u, n, used, age;

  if ( dfpn_hash_mask >= 0x10000U ) { n = 0x10000U; }
  else                              { n = dfpn_hash_mask + 1U; }
    
  used = 0;
  for ( u = 0; u < n; u++ )
    {
      nodes = dfpn_hash_tbl[u].word3 & DFPN_NODES_MASK;
      age   = (unsigned int)( dfpn_hash_tbl[u].word2 >> 46 ) & DFPN_AGE_MASK;
      if ( nodes != 0 && age == dfpn_hash_age ) { used += 1; }
    }

  return (float)( used * 100U ) / (float)n;
}


#  if defined(_MSC_VER)
#    pragma warning(disable:4100)
#  elif defined(__ICC)
#    pragma warning(disable:869)
#  endif
void CONV dfpn_hash_probe( const dfpn_tree_t * restrict pdfpn_tree,
			   child_t * restrict pchild, int ply, int turn )
{
  uint64_t hash_key_curr, word1, word2, word3, hash_key, nodes;
  unsigned int index, hand_b, phi, delta, u, offset, hand_b_curr;
  int relation, is_phi_loop, is_delta_loop;

#define REL_SUPE 0
#define REL_INFE 1

  if ( pdfpn_tree->turn_or == turn ) { hash_key_curr = pchild->hash_key; }
  else                               { hash_key_curr = ~pchild->hash_key; }
  hand_b_curr = pchild->hand_b;

  index = (unsigned int)hash_key_curr & dfpn_hash_mask;
  for ( u = index; u < index + DFPN_NUM_REHASH; u++ ) {
  
    word1 = dfpn_hash_tbl[u].word1;
    word2 = dfpn_hash_tbl[u].word2;
    word3 = dfpn_hash_tbl[u].word3;
    DFPNSignKey( word1, word2, word3 );

    hash_key      = word1 & ~UINT64_C(0x1fffff);
    hand_b        = (unsigned int)word1 & 0x1fffffU;
    delta         = (unsigned int)word2 & INF;
    phi           = (unsigned int)( word2 >> 23 ) & INF;
    offset        = (unsigned int)( word2 >> 54 ) & 0xffU;
    is_phi_loop   = (int)( word2 >> 63 );
    is_delta_loop = (int)( word2 >> 62 ) & 1;
    nodes         = word3 & DFPN_NODES_MASK;
    assert( offset <= u );

    if ( nodes == 0 )         { break; }
    if ( index + offset < u ) { break; }
    if ( index + offset > u ) { continue; }
    if ( hash_key != ( hash_key_curr & ~UINT64_C(0x1fffff) ) ) { continue; }

    if ( hand_b_curr == hand_b )
      {
	pchild->phi           = phi;
	pchild->delta         = delta;
	pchild->nodes         = nodes;
	pchild->min_hand_b    = hand_b;
	pchild->is_phi_loop   = is_phi_loop;
	pchild->is_delta_loop = is_delta_loop;
	DOut( ply, "HASH EXACT at %u+%u, %x, %x: [%u,%u] %u %x %c %c",
	      index, offset, (unsigned int)hash_key_curr, hand_b_curr, phi,
	      delta, (unsigned int)nodes, pchild->min_hand_b,
	      is_phi_loop ? 'y' : 'n', is_delta_loop ? 'y' : 'n' );
	return;
      }
    
    if      ( dfpn_hand_supe( hand_b_curr, hand_b ) ) { relation = REL_SUPE; }
    else if ( dfpn_hand_supe( hand_b, hand_b_curr ) ) { relation = REL_INFE; }
    else continue;
    
    if ( turn ) { relation = relation ^ 1; }

    if ( relation == REL_SUPE && phi == 0 && delta == INF )
      {
	pchild->phi           = phi;
	pchild->delta         = delta;
	pchild->nodes         = nodes;
	pchild->min_hand_b    = hand_b;
	pchild->is_phi_loop   = is_phi_loop;
	pchild->is_delta_loop = is_delta_loop;
	DOut( ply, "HASH SUPE at %u+%u, %x, %x: [%u,%u] %u %x n n", index,
	      offset, (unsigned int)hash_key_curr, hand_b_curr, phi, delta,
	      (unsigned int)nodes, pchild->min_hand_b );
	return;
      }

    if ( relation == REL_INFE && phi == INF && delta == 0 )
      {
	pchild->phi           = phi;
	pchild->delta         = delta;
	pchild->nodes         = nodes;
	pchild->min_hand_b    = hand_b;
	pchild->is_phi_loop   = is_phi_loop;
	pchild->is_delta_loop = is_delta_loop;
	DOut( ply, "HASH INFE at %u+%u, %x, %x: [%u,%u] %u %x n n", index,
	      offset, (unsigned int)hash_key_curr, hand_b_curr, phi, delta,
	       (unsigned int)nodes, pchild->min_hand_b );
	return;
      }
  }
}
#  if defined(_MSC_VER)
#    pragma warning(default:4100)
#  elif defined(__ICC)
#    pragma warning(default:869)
#  endif


void CONV dfpn_hash_store( const tree_t * restrict ptree,
			   dfpn_tree_t * restrict pdfpn_tree, int ply )
{
  node_t * restrict pnode = pdfpn_tree->anode + ply;
  uint64_t word1, word2, word3, min_nodes, nodes, hash_key;
  uint64_t tmp1, tmp2, tmp3;
  unsigned int index, min_u, u, u1, hand_b, min_hand_b, offset, age;
  int turn_win, i, n, is_phi_loop, is_delta_loop;

  assert( pnode->phi != INF || pnode->delta != INF );

  if ( pdfpn_tree->turn_or == pnode->turn ) { hash_key = HASH_KEY; }
  else                                      { hash_key = ~HASH_KEY; }

  word1 = ( hash_key & ~UINT64_C(0x1fffff) );
  index = (unsigned int)hash_key & dfpn_hash_mask;

  if ( pnode->phi == 0 && pnode->delta == INF )
    {
      min_hand_b    = calc_hand_win( pnode, HAND_B, HAND_W );
      turn_win      = pnode->turn;
      is_phi_loop   = 0;
      is_delta_loop = 0;
      word1        |= (uint64_t)min_hand_b;
    }
  else if ( pnode->phi == INF && pnode->delta == 0 )
    {
      min_hand_b    = calc_hand_lose( pnode, HAND_B, HAND_W );
      turn_win      = Flip(pnode->turn);
      is_phi_loop   = 0;
      is_delta_loop = 0;
      word1        |= (uint64_t)min_hand_b;
    }      
  else {
    min_hand_b    = HAND_B;
    word1        |= (uint64_t)min_hand_b;
    is_phi_loop   = 1;
    is_delta_loop = 0;
    n = pnode->nmove;
    for ( i = 0; i < n; i++ )
      {
	if ( pnode->phi == pnode->children[i].delta )
	  {
	    is_phi_loop &= pnode->children[i].is_delta_loop;
	  }
	is_delta_loop |= pnode->children[i].is_phi_loop;
      }

    goto skip;
  }

  for ( u = index; u < index + DFPN_NUM_REHASH; u++ ) {

    tmp1 = dfpn_hash_tbl[u].word1;
    tmp2 = dfpn_hash_tbl[u].word2;
    tmp3 = dfpn_hash_tbl[u].word3;
    DFPNSignKey( tmp1, tmp2, tmp3 );

    nodes  = tmp3 & DFPN_NODES_MASK;
    offset = ( tmp2 >> 54 ) & 0xffU;

    if ( nodes == 0 )          { continue; }
    if ( u != index + offset ) { continue; }
    if ( word1 == tmp1 )       { continue; }
    if ( ( word1 & ~UINT64_C(0x1fffff) ) != ( tmp1 & ~UINT64_C(0x1fffff) ) )
      {
	continue;
      }
    
    hand_b = (unsigned int)tmp1 & 0x1fffffU;
    assert( min_hand_b != hand_b );

    if ( ! turn_win && ! dfpn_hand_supe( hand_b, min_hand_b ) ) { continue; }
    if (   turn_win && ! dfpn_hand_supe( min_hand_b, hand_b ) ) { continue; }

    /* sort by index */
    for ( u1 = u+1;; u1++ ) {

      tmp1 = dfpn_hash_tbl[u1].word1;
      tmp2 = dfpn_hash_tbl[u1].word2;
      tmp3 = dfpn_hash_tbl[u1].word3;
      DFPNSignKey( tmp1, tmp2, tmp3 );
      
      nodes = tmp3 & DFPN_NODES_MASK;
      if ( nodes == 0 ) { break; }
      
      assert( u1 < DFPN_NUM_REHASH + 1 + dfpn_hash_mask );
      offset = (unsigned int)( tmp2 >> 54 ) & 0xffU;
      if ( offset == 0 ) { break; }
      
      tmp2 &= ~( (uint64_t)0xffU << 54 );
      tmp2 |= (uint64_t)( offset - 1U ) << 54;
      
      DFPNSignKey( tmp1, tmp2, tmp3 );
      dfpn_hash_tbl[u1-1].word1 = tmp1;      
      dfpn_hash_tbl[u1-1].word2 = tmp2;
      dfpn_hash_tbl[u1-1].word3 = tmp3;
    }

    dfpn_hash_tbl[u1-1].word3 = 0;
  }

 skip:

  min_nodes = UINT64_MAX;
  min_u     = UINT_MAX;
  for ( u = index; u < index + DFPN_NUM_REHASH * 3U; u++ )
    {
      if ( u == dfpn_hash_mask + 2 + DFPN_NUM_REHASH ) { break; }

      tmp1 = dfpn_hash_tbl[u].word1;
      tmp2 = dfpn_hash_tbl[u].word2;
      tmp3 = dfpn_hash_tbl[u].word3;
      DFPNSignKey( tmp1, tmp2, tmp3 );

      offset = ( tmp2 >> 54 ) & 0xffU;
      age    = (unsigned int)( tmp2 >> 46 ) & DFPN_AGE_MASK;
      nodes  = tmp3 & DFPN_NODES_MASK;

      assert( offset <= u );

      if ( nodes == 0
	   || age != dfpn_hash_age
	   || ( index == u - offset && word1 == tmp1 ) )
	{
	  min_u = u;
	  break;
	}

      if ( index <= u - offset && offset == DFPN_NUM_REHASH - 1 ) { break; }

      if ( nodes < min_nodes )
	{
	  min_nodes = nodes;
	  min_u = u;
	}
    }

  if ( min_u == UINT_MAX )
    {
      out_warning( "HASH ERROR in $s line %d", __FILE__, __LINE__ );
      min_u = index;
    }

  /* sort by index */
  tmp1 = dfpn_hash_tbl[min_u].word1;
  tmp2 = dfpn_hash_tbl[min_u].word2;
  tmp3 = dfpn_hash_tbl[min_u].word3;
  DFPNSignKey( tmp1, tmp2, tmp3 );
  offset    = (unsigned int)( tmp2 >> 54 ) & 0xffU;
  min_nodes = tmp3 & DFPN_NODES_MASK;
  age       = (unsigned int)(tmp2 >> 46) & DFPN_AGE_MASK;

  if ( min_nodes == 0
       || age != dfpn_hash_age
       || index <= min_u - offset ) {

    for ( ; index < min_u; min_u -= 1 ) {

      assert( dfpn_hash_tbl[min_u-1].word2 != 0 );
      tmp1 = dfpn_hash_tbl[min_u-1].word1;
      tmp2 = dfpn_hash_tbl[min_u-1].word2;
      tmp3 = dfpn_hash_tbl[min_u-1].word3;
      DFPNSignKey( tmp1, tmp2, tmp3 );

      offset = (unsigned int)( tmp2 >> 54 ) & 0xffU;
      assert( offset <= min_u - 1 );
      if ( min_u - 1 - offset <= index ) { break; }
      
      assert( offset + 1U < DFPN_NUM_REHASH );

      dfpn_hash_tbl[min_u-1].word3 = 0;

      tmp2 &= ~( (uint64_t)0xffU << 54 );
      tmp2 |= (uint64_t)( offset + 1U ) << 54;

      DFPNSignKey( tmp1, tmp2, tmp3 );
      dfpn_hash_tbl[min_u].word1 = tmp1;
      dfpn_hash_tbl[min_u].word2 = tmp2;
      dfpn_hash_tbl[min_u].word3 = tmp3;
    }
    
  } else {

    for ( ; min_u < index + DFPN_NUM_REHASH - 1; min_u += 1 ) {
      
      assert( dfpn_hash_tbl[min_u+1].word2 != 0 );
      tmp1 = dfpn_hash_tbl[min_u+1].word1;
      tmp2 = dfpn_hash_tbl[min_u+1].word2;
      tmp3 = dfpn_hash_tbl[min_u+1].word3;
      DFPNSignKey( tmp1, tmp2, tmp3 );

      offset = (unsigned int)( tmp2 >> 54 ) & 0xffU;
      assert( offset <= min_u + 1 );
      if ( index <= min_u + 1 - offset ) { break; }
      
      assert( offset != 0 );

      dfpn_hash_tbl[min_u+1].word3 = 0;

      tmp2 &= ~( (uint64_t)0xffU << 54  );
      tmp2 |= (uint64_t)( offset - 1U ) << 54;

      DFPNSignKey( tmp1, tmp2, tmp3 );
      dfpn_hash_tbl[min_u].word1 = tmp1;
      dfpn_hash_tbl[min_u].word2 = tmp2;
      dfpn_hash_tbl[min_u].word3 = tmp3;
    }
  }
  
  assert( min_nodes != UINT_MAX );
  assert( pnode->phi <= INF && pnode->delta <= INF );
  assert( index <= min_u && min_u - index < DFPN_NUM_REHASH );

  offset                = min_u - index;
  pnode->is_phi_loop    = is_phi_loop;
  pnode->is_delta_loop  = is_delta_loop;
  pnode->min_hand_b     = min_hand_b;

  word2  = (uint64_t)is_phi_loop   << 63;
  word2 |= (uint64_t)is_delta_loop << 62;
  word2 |= (uint64_t)offset        << 54;
  word2 |= (uint64_t)dfpn_hash_age << 46;
  word2 |= (uint64_t)pnode->phi    << 23;
  word2 |= (uint64_t)pnode->delta  <<  0;
  word3  = pnode->nodes;

  DFPNSignKey( word1, word2, word3 );
  dfpn_hash_tbl[min_u].word1 = word1;
  dfpn_hash_tbl[min_u].word2 = word2;
  dfpn_hash_tbl[min_u].word3 = word3;

  DOut( ply, "STORE[%u+%u] %x, %x: [%d,%d] %c %c",
	index, offset, (unsigned int)hash_key, min_hand_b, pnode->phi,
	pnode->delta, is_phi_loop ? 'y': 'n', is_delta_loop ? 'y' : 'n' );
}


unsigned int CONV
dfpn_max_hand_b( unsigned int hand_b, unsigned hand_w )
{
  unsigned hand_tot = hand_b + hand_w;
  unsigned hand     = 0;
  int i;

  for ( i = pawn; i <= rook; i++ )
    if ( hand_b & hand_mask[i] ) { hand += hand_tot & hand_mask[i]; }

  return hand;
}


unsigned int CONV
dfpn_max_hand_w( unsigned int hand_b, unsigned hand_w )
{
  unsigned hand_tot = hand_b + hand_w;
  unsigned hand     = hand_b + hand_w;
  int i;

  for ( i = pawn; i <= rook; i++ )
    if ( hand_w & hand_mask[i] ) { hand -= hand_tot & hand_mask[i]; }

  return hand;
}


int CONV
dfpn_detect_rep( const dfpn_tree_t * restrict pdfpn_tree, uint64_t hash_key,
		 unsigned int hand_b, int ply, int * restrict ip )
{
  const node_t * restrict pnode;

  for ( *ip = ply; *ip >= pdfpn_tree->root_ply; *ip -=2 )
    {
      pnode = pdfpn_tree->anode + *ip;
      if ( pnode->hash_key == hash_key
	   && pnode->hand_b == hand_b ) { return 1; }
    }

  return 0;
}


static int CONV
dfpn_hand_supe( unsigned int u, unsigned int uref )
{
  if ( IsHandPawn(u) >= IsHandPawn(uref)
       && IsHandLance(u)  >= IsHandLance(uref)
       && IsHandKnight(u) >= IsHandKnight(uref)
       && IsHandSilver(u) >= IsHandSilver(uref)
       && IsHandGold(u)   >= IsHandGold(uref)
       && IsHandBishop(u) >= IsHandBishop(uref)
       && IsHandRook(u)   >= IsHandRook(uref) ) { return 1; }
  
  return 0;
}


static unsigned int CONV
calc_hand_lose( const node_t * restrict pnode,
		unsigned int hand_b, unsigned int hand_w )
{
  int i, n;
  unsigned int hand_tot = hand_b + hand_w;
  unsigned int hand;

  n = pnode->nmove;

  if ( pnode->turn ) {

    hand = dfpn_max_hand_w( hand_b, hand_w );

    for ( i = 0; i < n; i++ ) {
      hand = hand_or( hand, pnode->children[i].min_hand_b );
    }
    hand = hand_and( hand_tot, hand );

    assert( dbg_is_hand_valid( hand ) && dfpn_hand_supe( hand_b, hand ) );

  } else {

    hand = dfpn_max_hand_b( hand_b, hand_w );

    for ( i = 0; i < n; i++ ) {
      
      unsigned int h0 = pnode->children[i].min_hand_b;
      int from        = (int)I2From(pnode->children[i].move);
      int pc_cap      = (int)UToCap(pnode->children[i].move);

      if ( from >= nsquare )             { h0 += hand_tbl[From2Drop(from)]; }
      else if ( h0 & hand_mask[pc_cap] ) { h0 -= hand_tbl[pc_cap]; }

      hand = hand_and( hand, h0 );
    }

    assert( dbg_is_hand_valid( hand ) && dfpn_hand_supe( hand, hand_b ) );
  }

  return hand;
}


static unsigned int CONV
calc_hand_win( const node_t *pnode, unsigned int hand_b, unsigned int hand_w )
{
  int i, n;
  unsigned int hand, h0;

  n = pnode->nmove;

  if ( pnode->turn ) {

    hand = hand_b + hand_w;

    for ( i = 0; i < n; i++ ) {
      
      if ( pnode->children[i].phi   != INF ) { continue; }
      if ( pnode->children[i].delta != 0 )   { continue; }

      h0 = hand_and( hand_b + hand_w, pnode->children[i].min_hand_b );
      if ( dfpn_hand_supe( hand, h0 ) ) { hand = h0; }
    }

    assert( dbg_is_hand_valid(hand) && dfpn_hand_supe(hand, hand_b) );

  } else {

    hand = 0;

    for ( i = 0; i < n; i++ ) {
      
      int from, pc_cap;
      
      if ( pnode->children[i].phi   != INF ) { continue; }
      if ( pnode->children[i].delta != 0 )   { continue; }

      h0     = pnode->children[i].min_hand_b;
      from   = (int)I2From(pnode->children[i].move);
      pc_cap = (int)UToCap(pnode->children[i].move);

      if ( from >= nsquare )             { h0 += hand_tbl[From2Drop(from)]; }
      else if ( h0 & hand_mask[pc_cap] ) { h0 -= hand_tbl[pc_cap]; }

      h0 = hand_and( hand_b + hand_w, h0 );

      if ( dfpn_hand_supe( h0, hand ) )	{ hand = h0; }
    }

    assert( dbg_is_hand_valid( hand ) && dfpn_hand_supe( hand_b, hand ) );
  }

  return hand;
}


static unsigned int CONV
hand_or( unsigned int a, unsigned int b )
{
  unsigned int c, u1, u2;
  int i;

  c = 0;
  for ( i = pawn; i <= rook; i++ )
    {
      u1 = hand_mask[i] & a;
      u2 = hand_mask[i] & b;
      c += u2 < u1 ? u1 : u2;
    }

  return c;
}


static unsigned int CONV
hand_and( unsigned int a, unsigned int b )
{
  unsigned int c, u1, u2;
  int i;

  c = 0;
  for ( i = pawn; i <= rook; i++ )
    {
      u1 = hand_mask[i] & a;
      u2 = hand_mask[i] & b;
      c += u1 < u2 ? u1 : u2;
    }

  return c;
}

#if defined(DEBUG)
static int CONV
dbg_is_hand_valid( unsigned int hand )
{
  return ( I2HandPawn(hand) <= 18U
	   && I2HandLance(hand)  <= 4U
	   && I2HandKnight(hand) <= 4U
	   && I2HandSilver(hand) <= 4U
	   && I2HandGold(hand)   <= 4U
	   && I2HandBishop(hand) <= 2U
	   && I2HandRook(hand)   <= 2U );
}
#endif

#endif /* DFPN */
