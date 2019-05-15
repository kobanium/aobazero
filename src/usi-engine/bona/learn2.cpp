// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <assert.h>
#include "shogi.h"

#if ! defined(MINIMUM)

#define GO_THROUGH_ALL_PARAMETERS_BY_FOO                                  \
  for ( i=0; i < nsquare*pos_n; i++ )           { Foo( pc_on_sq[0][i] ) } \
  for ( i=0; i < nsquare*nsquare*kkp_end; i++ ) { Foo( kkp[0][0][i] ) }


static void rmt( const double avalue[16], int pc );
static void rparam( short *pv, float dv );
static void fv_sym( void );
static int brand( void );
static int make_list( const tree_t * restrict ptree, int list0[52],
		      int list1[52], int anpiece[16], param_t * restrict pd,
		      float f );


void
ini_param( param_t *p )
{
  int i;

  p->pawn       = p->lance      = p->knight     = p->silver     = 0.0;
  p->gold       = p->bishop     = p->rook       = p->pro_pawn   = 0.0;
  p->pro_lance  = p->pro_knight = p->pro_silver = p->horse      = 0.0;
  p->dragon     = 0.0;

#define Foo(x) p->x = 0;
  GO_THROUGH_ALL_PARAMETERS_BY_FOO;
#undef Foo
}


void
add_param( param_t *p1, const param_t *p2 )
{
  int i;

  p1->pawn       += p2->pawn;
  p1->lance      += p2->lance;
  p1->knight     += p2->knight;
  p1->silver     += p2->silver;
  p1->gold       += p2->gold;
  p1->bishop     += p2->bishop;
  p1->rook       += p2->rook;
  p1->pro_pawn   += p2->pro_pawn;
  p1->pro_lance  += p2->pro_lance;
  p1->pro_knight += p2->pro_knight;
  p1->pro_silver += p2->pro_silver;
  p1->horse      += p2->horse;
  p1->dragon     += p2->dragon;

#define Foo(x) p1->x += p2->x;
  GO_THROUGH_ALL_PARAMETERS_BY_FOO;
#undef Foo
}


void
fill_param_zero( void )
{
  int i;

  p_value[15+pawn]       = 100;
  p_value[15+lance]      = 300;
  p_value[15+knight]     = 300;
  p_value[15+silver]     = 400;
  p_value[15+gold]       = 500;
  p_value[15+bishop]     = 600;
  p_value[15+rook]       = 700;
  p_value[15+pro_pawn]   = 400;
  p_value[15+pro_lance]  = 400;
  p_value[15+pro_knight] = 400;
  p_value[15+pro_silver] = 500;
  p_value[15+horse]      = 800;
  p_value[15+dragon]     = 1000;

#define Foo(x) x = 0;
  GO_THROUGH_ALL_PARAMETERS_BY_FOO;
#undef Foo

  set_derivative_param();
}


void
param_sym( param_t *p )
{
  int q, r, il, ir, ir0, jl, jr, k0l, k0r, k1l, k1r;

  for ( k0l = 0; k0l < nsquare; k0l++ ) {
    q = k0l / nfile;
    r = k0l % nfile;
    k0r = q*nfile + nfile-1-r;
    if ( k0l > k0r ) { continue; }

    for ( il = 0; il < fe_end; il++ ) {
      if ( il < fe_hand_end ) { ir0 = il; }
      else {
	q = ( il- fe_hand_end ) / nfile;
	r = ( il- fe_hand_end ) % nfile;
	ir0 = q*nfile + nfile-1-r + fe_hand_end;
      }

      for ( jl = 0; jl <= il; jl++ ) {
	if ( jl < fe_hand_end )
	  {
	    ir = ir0;
	    jr = jl;
	  }
	else {
	  q = ( jl - fe_hand_end ) / nfile;
	  r = ( jl - fe_hand_end ) % nfile;
	  jr = q*nfile + nfile-1-r + fe_hand_end;
	  if ( jr > ir0 )
	    {
	      ir = jr;
	      jr = ir0;
	    }
	  else { ir = ir0; }
	}
	if ( k0l == k0r && il*(il+1)/2+jl >= ir*(ir+1)/2+jr ) { continue; }

	p->PcPcOnSq(k0l,il,jl)
	  = p->PcPcOnSq(k0r,ir,jr)
	  = p->PcPcOnSq(k0l,il,jl) + p->PcPcOnSq(k0r,ir,jr);
      }
    }
  }

  for ( k0l = 0; k0l < nsquare; k0l++ ) {
    q = k0l / nfile;
    r = k0l % nfile;
    k0r = q*nfile + nfile-1-r;
    if ( k0l > k0r ) { continue; }

    for ( k1l = 0; k1l < nsquare; k1l++ ) {
      q = k1l / nfile;
      r = k1l % nfile;
      k1r = q*nfile + nfile-1-r;
      if ( k0l == k0r && k1l > k1r ) { continue; }

      for ( il = 0; il < kkp_end; il++ ) {
	if ( il < kkp_hand_end ) { ir = il; }
	else {
	  q  = ( il- kkp_hand_end ) / nfile;
	  r  = ( il- kkp_hand_end ) % nfile;
	  ir = q*nfile + nfile-1-r + kkp_hand_end;
	}
	if ( k0l == k0r && k1l == k1r && il >= ir ) { continue; }

	p->kkp[k0l][k1l][il]
	  = p->kkp[k0r][k1r][ir]
	  = p->kkp[k0l][k1l][il] + p->kkp[k0r][k1r][ir];
      }
    }
  }
}


static void fv_sym( void )
{
  int q, r, il, ir, ir0, jl, jr, k0l, k0r, k1l, k1r;

  for ( k0l = 0; k0l < nsquare; k0l++ ) {
    q = k0l / nfile;
    r = k0l % nfile;
    k0r = q*nfile + nfile-1-r;
    if ( k0l > k0r ) { continue; }

    for ( il = 0; il < fe_end; il++ ) {
      if ( il < fe_hand_end ) { ir0 = il; }
      else {
	q = ( il- fe_hand_end ) / nfile;
	r = ( il- fe_hand_end ) % nfile;
	ir0 = q*nfile + nfile-1-r + fe_hand_end;
      }

      for ( jl = 0; jl <= il; jl++ ) {
	if ( jl < fe_hand_end )
	  {
	    ir = ir0;
	    jr = jl;
	  }
	else {
	  q = ( jl - fe_hand_end ) / nfile;
	  r = ( jl - fe_hand_end ) % nfile;
	  jr = q*nfile + nfile-1-r + fe_hand_end;
	  if ( jr > ir0 )
	    {
	      ir = jr;
	      jr = ir0;
	    }
	  else { ir = ir0; }
	}
	if ( k0l == k0r && il*(il+1)/2+jl >= ir*(ir+1)/2+jr ) { continue; }

	PcPcOnSq(k0l,il,jl) = PcPcOnSq(k0r,ir,jr);
      }
    }
  }

  for ( k0l = 0; k0l < nsquare; k0l++ ) {
    q = k0l / nfile;
    r = k0l % nfile;
    k0r = q*nfile + nfile-1-r;
    if ( k0l > k0r ) { continue; }

    for ( k1l = 0; k1l < nsquare; k1l++ ) {
      q = k1l / nfile;
      r = k1l % nfile;
      k1r = q*nfile + nfile-1-r;
      if ( k0l == k0r && k1l > k1r ) { continue; }

      for ( il = 0; il < kkp_end; il++ ) {
	if ( il < kkp_hand_end ) { ir = il; }
	else {
	  q  = ( il- kkp_hand_end ) / nfile;
	  r  = ( il- kkp_hand_end ) % nfile;
	  ir = q*nfile + nfile-1-r + kkp_hand_end;
	}
	if ( k0l == k0r && k1l == k1r && il >= ir ) { continue; }

	kkp[k0l][k1l][il] = kkp[k0r][k1r][ir];
      }
    }
  }
}


double
calc_penalty( void )
{
  uint64_t u64sum;
  int i;

  u64sum = 0;

#define Foo(x) u64sum += (uint64_t)abs((int)x);
  GO_THROUGH_ALL_PARAMETERS_BY_FOO;
#undef Foo

  return (double)u64sum * FV_PENALTY;
}


void
renovate_param( const param_t *pd )
{
  double *pv[14], *p;
  double v[16];
  unsigned int u32rand, u;
  int i, j;

  v[pawn]       = pd->pawn;         v[lance]      = pd->lance;
  v[knight]     = pd->knight;       v[silver]     = pd->silver;
  v[gold]       = pd->gold;         v[bishop]     = pd->bishop;
  v[rook]       = pd->rook;         v[pro_pawn]   = pd->pro_pawn;
  v[pro_lance]  = pd->pro_lance;    v[pro_knight] = pd->pro_knight;
  v[pro_silver] = pd->pro_silver;   v[horse]      = pd->horse;
  v[dragon]     = pd->dragon;       v[king]       = FLT_MAX;

  pv[ 0] = v + pawn;         pv[ 1] = v + lance;
  pv[ 2] = v + knight;       pv[ 3] = v + silver;
  pv[ 4] = v + gold;         pv[ 5] = v + bishop;
  pv[ 6] = v + rook;         pv[ 7] = v + pro_pawn;
  pv[ 8] = v + pro_lance;    pv[ 9] = v + pro_knight;
  pv[10] = v + pro_silver;   pv[11] = v + horse;
  pv[12] = v + dragon;       pv[13] = v + king;

  /* insertion sort */
  for ( i = 13 - 2; i >= 0; i-- )
    {
      p = pv[i];
      for ( j = i+1; *pv[j] < *p; j++ ) { pv[j-1] = pv[j]; }
      pv[j-1] = p;
    }

  u32rand = rand32();
  u       = u32rand % 7U;
  u32rand = u32rand / 7U;
  p = pv[u+6];  pv[u+6] = pv[12];  pv[12] = p;
  for ( i = 5; i > 0; i-- )
    {
      u       = u32rand % (i+1);
      u32rand = u32rand / (i+1);
      p = pv[u];  pv[u] = pv[i];  pv[i] = p;

      u       = u32rand % (i+1);
      u32rand = u32rand / (i+1);
      p = pv[u+6];  pv[u+6] = pv[i+6];  pv[i+6] = p;
    }

  *pv[ 0] = *pv[ 1] = -2.0;
  *pv[ 2] = *pv[ 3] = *pv[ 4] = -1.0;
  *pv[ 5] = *pv[ 6] = *pv[ 7] =  0.0;
  *pv[ 8] = *pv[ 9] = *pv[10] =  1.0;
  *pv[11] = *pv[12] =  2.0;

  rmt( v, pawn );        rmt( v, lance );       rmt( v, knight );
  rmt( v, silver );      rmt( v, gold );        rmt( v, bishop );
  rmt( v, rook );        rmt( v, pro_pawn );    rmt( v, pro_lance );
  rmt( v, pro_knight );  rmt( v, pro_silver );  rmt( v, horse );
  rmt( v, dragon );

#define Foo(v) rparam( &v, pd->v );
  GO_THROUGH_ALL_PARAMETERS_BY_FOO;
#undef Foo

  fv_sym();
  set_derivative_param();
  ehash_clear();
}


int
out_param( void )
{
  size_t size;
  FILE *pf;
  int apc[17], apv[17];
  int iret, i, j, pc, pv;

  for ( i = 0; i < 17; i++ ) { apc[i] = i;  apv[i] = INT_MAX; }
  apv[pawn]       = p_value_ex[15+pawn];
  apv[lance]      = p_value_ex[15+lance];
  apv[knight]     = p_value_ex[15+knight];
  apv[silver]     = p_value_ex[15+silver];
  apv[gold]       = p_value_ex[15+gold];
  apv[bishop]     = p_value_ex[15+bishop];
  apv[rook]       = p_value_ex[15+rook];
  apv[pro_pawn]   = p_value_ex[15+pro_pawn];
  apv[pro_lance]  = p_value_ex[15+pro_lance];
  apv[pro_knight] = p_value_ex[15+pro_knight];
  apv[pro_silver] = p_value_ex[15+pro_silver];
  apv[horse]      = p_value_ex[15+horse];
  apv[dragon]     = p_value_ex[15+dragon];

  /* insertion sort */
  for ( i = dragon-1; i >= 0; i-- )
    {
      pv = apv[i];  pc = apc[i];
      for ( j = i+1; apv[j] < pv; j++ )
	{
	  apv[j-1] = apv[j];
	  apc[j-1] = apc[j];
	}
      apv[j-1] = pv;  apc[j-1] = pc;
    }
      
  pf = file_open( "param.h_", "w" );
  if ( pf == NULL ) { return -2; }
  
  for ( i = 0; i < 13; i++ )
    {
      fprintf( pf, "#define " );
      switch ( apc[i] )
	{
	case pawn:        fprintf( pf, "DPawn     " );  break;
	case lance:       fprintf( pf, "DLance    " );  break;
	case knight:      fprintf( pf, "DKnight   " );  break;
	case silver:      fprintf( pf, "DSilver   " );  break;
	case gold:        fprintf( pf, "DGold     " );  break;
	case bishop:      fprintf( pf, "DBishop   " );  break;
	case rook:        fprintf( pf, "DRook     " );  break;
	case pro_pawn:    fprintf( pf, "DProPawn  " );  break;
	case pro_lance:   fprintf( pf, "DProLance " );  break;
	case pro_knight:  fprintf( pf, "DProKnight" );  break;
	case pro_silver:  fprintf( pf, "DProSilver" );  break;
	case horse:       fprintf( pf, "DHorse    " );  break;
	case dragon:      fprintf( pf, "DDragon   " );  break;
	}
      fprintf( pf, "     %4d /* %4d */\n", p_value[15+apc[i]], apv[i] );
    }
  fprintf( pf, "#define DKing         15000\n\n" );

  iret = file_close( pf );
  if ( iret < 0 ) { return -2; }

  pf = file_open( "fv.bin", "wb" );
  if ( pf == NULL ) { return -2; }

  size = nsquare * pos_n;
  if ( fwrite( pc_on_sq, sizeof(short), size, pf ) != size )
    {
      str_error = str_io_error;
      return -2;
    }

  size = nsquare * nsquare * kkp_end;
  if ( fwrite( kkp, sizeof(short), size, pf ) != size )
    {
      str_error = str_io_error;
      return -2;
    }
  
  return file_close( pf );
}


void
inc_param( const tree_t * restrict ptree, param_t * restrict pd, double dinc )
{
  float f;
  int anpiece[16], list0[52], list1[52];
  int nlist, sq_bk, sq_wk, k0, k1, l0, l1, i, j;

  f     = (float)( dinc / (double)FV_SCALE );
  nlist = make_list( ptree, list0, list1, anpiece, pd, f );
  sq_bk = SQ_BKING;
  sq_wk = Inv( SQ_WKING );

  pd->pawn       += dinc * (double)anpiece[pawn];
  pd->lance      += dinc * (double)anpiece[lance];
  pd->knight     += dinc * (double)anpiece[knight];
  pd->silver     += dinc * (double)anpiece[silver];
  pd->gold       += dinc * (double)anpiece[gold];
  pd->bishop     += dinc * (double)anpiece[bishop];
  pd->rook       += dinc * (double)anpiece[rook];
  pd->pro_pawn   += dinc * (double)anpiece[pro_pawn];
  pd->pro_lance  += dinc * (double)anpiece[pro_lance];
  pd->pro_knight += dinc * (double)anpiece[pro_knight];
  pd->pro_silver += dinc * (double)anpiece[pro_silver];
  pd->horse      += dinc * (double)anpiece[horse];
  pd->dragon     += dinc * (double)anpiece[dragon];

  for ( i = 0; i < nlist; i++ )
    {
      k0 = list0[i];
      k1 = list1[i];
      for ( j = 0; j <= i; j++ )
	{
	  l0 = list0[j];
	  l1 = list1[j];
	  assert( k0 >= l0 && k1 >= l1 );
	  pd->PcPcOnSq( sq_bk, k0, l0 ) += f;
	  pd->PcPcOnSq( sq_wk, k1, l1 ) -= f;
	}
    }
}


static int
make_list( const tree_t * restrict ptree, int list0[52], int list1[52],
	   int anpiece[16], param_t * restrict pd, float f )
{
  bitboard_t bb;
  int list2[34];
  int nlist, sq, sq_bk0, sq_bk1, sq_wk0, sq_wk1, n2, i, itemp1, itemp2;

  itemp1          = (int)I2HandPawn(HAND_B);
  itemp2          = (int)I2HandPawn(HAND_W);
  anpiece[pawn]   = itemp1 - itemp2;

  itemp1          = (int)I2HandLance(HAND_B);
  itemp2          = (int)I2HandLance(HAND_W);
  anpiece[lance]  = itemp1 - itemp2;

  itemp1          = (int)I2HandKnight(HAND_B);
  itemp2          = (int)I2HandKnight(HAND_W);
  anpiece[knight] = itemp1 - itemp2;

  itemp1          = (int)I2HandSilver(HAND_B);
  itemp2          = (int)I2HandSilver(HAND_W);
  anpiece[silver] = itemp1 - itemp2;

  itemp1          = (int)I2HandGold(HAND_B);
  itemp2          = (int)I2HandGold(HAND_W);
  anpiece[gold]   = itemp1 - itemp2;

  itemp1          = (int)I2HandBishop(HAND_B);
  itemp2          = (int)I2HandBishop(HAND_W);
  anpiece[bishop] = itemp1 - itemp2;

  itemp1          = (int)I2HandRook(HAND_B);
  itemp2          = (int)I2HandRook(HAND_W);
  anpiece[rook]   = itemp1 - itemp2;

  anpiece[pro_pawn]   = anpiece[pro_lance]  = anpiece[pro_knight] = 0;
  anpiece[pro_silver] = anpiece[horse]      = anpiece[dragon]     = 0;
  nlist  = 14;
  sq_bk0 = SQ_BKING;
  sq_wk0 = SQ_WKING;
  sq_bk1 = Inv(SQ_WKING);
  sq_wk1 = Inv(SQ_BKING);

  list0[ 0] = f_hand_pawn   + I2HandPawn(HAND_B);
  list0[ 1] = e_hand_pawn   + I2HandPawn(HAND_W);
  list0[ 2] = f_hand_lance  + I2HandLance(HAND_B);
  list0[ 3] = e_hand_lance  + I2HandLance(HAND_W);
  list0[ 4] = f_hand_knight + I2HandKnight(HAND_B);
  list0[ 5] = e_hand_knight + I2HandKnight(HAND_W);
  list0[ 6] = f_hand_silver + I2HandSilver(HAND_B);
  list0[ 7] = e_hand_silver + I2HandSilver(HAND_W);
  list0[ 8] = f_hand_gold   + I2HandGold(HAND_B);
  list0[ 9] = e_hand_gold   + I2HandGold(HAND_W);
  list0[10] = f_hand_bishop + I2HandBishop(HAND_B);
  list0[11] = e_hand_bishop + I2HandBishop(HAND_W);
  list0[12] = f_hand_rook   + I2HandRook(HAND_B);
  list0[13] = e_hand_rook   + I2HandRook(HAND_W);

  list1[ 0] = f_hand_pawn   + I2HandPawn(HAND_W);
  list1[ 1] = e_hand_pawn   + I2HandPawn(HAND_B);
  list1[ 2] = f_hand_lance  + I2HandLance(HAND_W);
  list1[ 3] = e_hand_lance  + I2HandLance(HAND_B);
  list1[ 4] = f_hand_knight + I2HandKnight(HAND_W);
  list1[ 5] = e_hand_knight + I2HandKnight(HAND_B);
  list1[ 6] = f_hand_silver + I2HandSilver(HAND_W);
  list1[ 7] = e_hand_silver + I2HandSilver(HAND_B);
  list1[ 8] = f_hand_gold   + I2HandGold(HAND_W);
  list1[ 9] = e_hand_gold   + I2HandGold(HAND_B);
  list1[10] = f_hand_bishop + I2HandBishop(HAND_W);
  list1[11] = e_hand_bishop + I2HandBishop(HAND_B);
  list1[12] = f_hand_rook   + I2HandRook(HAND_W);
  list1[13] = e_hand_rook   + I2HandRook(HAND_B);

  pd->kkp[sq_bk0][sq_wk0][kkp_hand_pawn  +I2HandPawn(HAND_B)  ] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_lance +I2HandLance(HAND_B) ] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_knight+I2HandKnight(HAND_B)] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_silver+I2HandSilver(HAND_B)] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_gold  +I2HandGold(HAND_B)  ] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_bishop+I2HandBishop(HAND_B)] += f;
  pd->kkp[sq_bk0][sq_wk0][kkp_hand_rook  +I2HandRook(HAND_B)  ] += f;

  pd->kkp[sq_bk1][sq_wk1][kkp_hand_pawn  +I2HandPawn(HAND_W)  ] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_lance +I2HandLance(HAND_W) ] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_knight+I2HandKnight(HAND_W)] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_silver+I2HandSilver(HAND_W)] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_gold  +I2HandGold(HAND_W)  ] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_bishop+I2HandBishop(HAND_W)] -= f;
  pd->kkp[sq_bk1][sq_wk1][kkp_hand_rook  +I2HandRook(HAND_W)  ] -= f;

  n2 = 0;
  bb = BB_BPAWN;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_pawn+sq] += f;
    anpiece[pawn] += 1;
    list0[nlist] = f_pawn + sq;
    list2[n2]    = e_pawn + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WPAWN;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_pawn+Inv(sq)] -= f;
    anpiece[pawn] -= 1;
    list0[nlist] = e_pawn + sq;
    list2[n2]    = f_pawn + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }

  n2 = 0;
  bb = BB_BLANCE;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_lance+sq] += f;
    anpiece[lance] += 1;
    list0[nlist] = f_lance + sq;
    list2[n2]    = e_lance + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WLANCE;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_lance+Inv(sq)] -= f;
    anpiece[lance] -= 1;
    list0[nlist] = e_lance + sq;
    list2[n2]    = f_lance + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BKNIGHT;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_knight+sq] += f;
    anpiece[knight] += 1;
    list0[nlist] = f_knight + sq;
    list2[n2]    = e_knight + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WKNIGHT;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_knight+Inv(sq)] -= f;
    anpiece[knight] -= 1;
    list0[nlist] = e_knight + sq;
    list2[n2]    = f_knight + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BSILVER;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_silver+sq] += f;
    anpiece[silver] += 1;
    list0[nlist] = f_silver + sq;
    list2[n2]    = e_silver + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WSILVER;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_silver+Inv(sq)] -= f;
    anpiece[silver] -= 1;
    list0[nlist] = e_silver + sq;
    list2[n2]    = f_silver + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BTGOLD;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_gold+sq] += f;
    anpiece[BOARD[sq]] += 1;
    list0[nlist] = f_gold + sq;
    list2[n2]    = e_gold + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WTGOLD;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_gold+Inv(sq)] -= f;
    anpiece[-BOARD[sq]] -= 1;
    list0[nlist] = e_gold + sq;
    list2[n2]    = f_gold + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BBISHOP;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_bishop+sq] += f;
    anpiece[bishop] += 1;
    list0[nlist] = f_bishop + sq;
    list2[n2]    = e_bishop + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WBISHOP;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_bishop+Inv(sq)] -= f;
    anpiece[bishop] -= 1;
    list0[nlist] = e_bishop + sq;
    list2[n2]    = f_bishop + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BHORSE;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_horse+sq] += f;
    anpiece[horse] += 1;
    list0[nlist] = f_horse + sq;
    list2[n2]    = e_horse + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WHORSE;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_horse+Inv(sq)] -= f;
    anpiece[horse] -= 1;
    list0[nlist] = e_horse + sq;
    list2[n2]    = f_horse + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BROOK;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_rook+sq] += f;
    anpiece[rook] += 1;
    list0[nlist] = f_rook + sq;
    list2[n2]    = e_rook + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WROOK;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_rook+Inv(sq)] -= f;
    anpiece[rook] -= 1;
    list0[nlist] = e_rook + sq;
    list2[n2]    = f_rook + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }


  n2 = 0;
  bb = BB_BDRAGON;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk0][sq_wk0][kkp_dragon+sq] += f;
    anpiece[dragon] += 1;
    list0[nlist] = f_dragon + sq;
    list2[n2]    = e_dragon + Inv(sq);
    nlist += 1;
    n2    += 1;
  }

  bb = BB_WDRAGON;
  while ( BBTest(bb) ) {
    sq = FirstOne( bb );
    Xor( sq, bb );

    pd->kkp[sq_bk1][sq_wk1][kkp_dragon+Inv(sq)] -= f;
    anpiece[dragon] -= 1;
    list0[nlist] = e_dragon + sq;
    list2[n2]    = f_dragon + Inv(sq);
    nlist += 1;
    n2    += 1;
  }
  for ( i = 0; i < n2; i++ ) { list1[nlist-i-1] = list2[i]; }

  assert( nlist <= 52 );
  return nlist;
}


static void
rmt( const double avalue[16], int pc )
{
  int pc_v;

  pc_v  = (int)p_value[15+pc] + (int)avalue[pc];

  if ( 0 < pc_v && pc_v <= SHRT_MAX ) { p_value[15+pc] = (short)pc_v; }
  else {
    out_warning( "A material value is out of bounce. (%s=%d)\n",
		 astr_table_piece[pc], pc_v );
  }
}


static void
rparam( short *pv, float dv )
{
  int v, istep;

  istep  = brand();
  istep += brand();
  v      = *pv;

  if      ( v > 0 ) { dv -= (float)FV_PENALTY; }
  else if ( v < 0 ) { dv += (float)FV_PENALTY; }

  if      ( dv >= 0.0 && v <= SHRT_MAX - istep ) { v += istep; }
  else if ( dv <= 0.0 && v >= SHRT_MIN + istep ) { v -= istep; }
  else { out_warning( "A fvcoef parameter is out of bounce.\n" ); }

  *pv = (short)v;
}


static int
brand( void )
{
  static unsigned int urand = 0;
  static unsigned int uc    = 31;
  unsigned int uret;

  if ( uc == 31 )
    {
      urand = rand32();
      uc    = 0;
    }
  else { uc += 1; }
  uret    = urand & 1U;
  urand >>= 1;
  return uret;
}

#endif /* MINIMUM */
