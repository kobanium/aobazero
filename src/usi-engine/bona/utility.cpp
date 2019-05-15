// 2019 Team AobaZero
// This source code is in the public domain.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "shogi.h"


int CONV
ini_game( tree_t * restrict ptree, const min_posi_t *pmin_posi, int flag,
	  const char *str_name1, const char *str_name2 )
{
  bitboard_t bb;
  int piece;
  int sq, iret;

#if defined(INANIWA_SHIFT)
  if ( inaniwa_flag )
    {
      inaniwa_flag = 0;
      ehash_clear();
      iret = ini_trans_table();
      if ( iret < 0 ) { return iret; }
    }
#endif

  if ( flag & flag_history )
    {
      iret = open_history( str_name1, str_name2 );
      if ( iret < 0 ) { return iret; }
    }

  if ( ! ( flag & flag_nofmargin ) )
    {
      fmg_misc      = FMG_MISC;
      fmg_cap       = FMG_CAP;
      fmg_drop      = FMG_DROP;
      fmg_mt        = FMG_MT;
      fmg_misc_king = FMG_MISC_KING;
      fmg_cap_king  = FMG_CAP_KING;
    }

  memcpy( ptree->posi.asquare, pmin_posi->asquare, nsquare );
  ptree->move_last[0]  = ptree->amove;
  ptree->nsuc_check[0] = 0;
  ptree->nsuc_check[1] = 0;
  ptree->nrep          = 0;
  root_turn            = pmin_posi->turn_to_move;
  HAND_B               = pmin_posi->hand_black;
  HAND_W               = pmin_posi->hand_white;
  MATERIAL             = 0;

  BBIni( BB_BOCCUPY );
  BBIni( BB_BPAWN );
  BBIni( BB_BLANCE );
  BBIni( BB_BKNIGHT );
  BBIni( BB_BSILVER );
  BBIni( BB_BGOLD );
  BBIni( BB_BBISHOP );
  BBIni( BB_BROOK );
  BBIni( BB_BPRO_PAWN );
  BBIni( BB_BPRO_LANCE );
  BBIni( BB_BPRO_KNIGHT );
  BBIni( BB_BPRO_SILVER );
  BBIni( BB_BHORSE );
  BBIni( BB_BDRAGON );
  BBIni( BB_BTGOLD );
  BBIni( BB_WOCCUPY );
  BBIni( BB_WPAWN );
  BBIni( BB_WLANCE );
  BBIni( BB_WKNIGHT );
  BBIni( BB_WSILVER );
  BBIni( BB_WGOLD );
  BBIni( BB_WBISHOP );
  BBIni( BB_WROOK );
  BBIni( BB_WPRO_PAWN );
  BBIni( BB_WPRO_LANCE );
  BBIni( BB_WPRO_KNIGHT );
  BBIni( BB_WPRO_SILVER );
  BBIni( BB_WHORSE );
  BBIni( BB_WDRAGON );
  BBIni( BB_WTGOLD );
  BBIni( OCCUPIED_FILE );
  BBIni( OCCUPIED_DIAG1 );
  BBIni( OCCUPIED_DIAG2 );

  for ( sq = 0; sq < nsquare; sq++ ) {
    piece = BOARD[sq];
    if ( piece > 0 ) {
      Xor( sq, BB_BOCCUPY );
      XorFile( sq, OCCUPIED_FILE );
      XorDiag1( sq, OCCUPIED_DIAG1 );
      XorDiag2( sq, OCCUPIED_DIAG2 );
      switch ( piece )
	{
	case pawn:        Xor( sq, BB_BPAWN );        break;
	case lance:       Xor( sq, BB_BLANCE );       break;
	case knight:      Xor( sq, BB_BKNIGHT );      break;
	case silver:      Xor( sq, BB_BSILVER );      break;
	case rook:        Xor( sq, BB_BROOK );        break;
	case bishop:      Xor( sq, BB_BBISHOP );      break;
	case king:	  SQ_BKING = (char)sq;        break;
	case dragon:      Xor( sq, BB_BDRAGON );      break;
	case horse:       Xor( sq, BB_BHORSE );       break;
	case gold:	  Xor( sq, BB_BGOLD );        break;
	case pro_pawn:	  Xor( sq, BB_BPRO_PAWN );    break;
	case pro_lance:	  Xor( sq, BB_BPRO_LANCE );   break;
	case pro_knight:  Xor( sq, BB_BPRO_KNIGHT );  break;
	case pro_silver:  Xor( sq, BB_BPRO_SILVER );  break;
	}
    }
    else if ( piece < 0 ) {
      Xor( sq, BB_WOCCUPY );
      XorFile( sq, OCCUPIED_FILE );
      XorDiag1( sq, OCCUPIED_DIAG1 );
      XorDiag2( sq, OCCUPIED_DIAG2 );
      switch ( - piece )
	{
	case pawn:        Xor( sq, BB_WPAWN );        break;
	case lance:       Xor( sq, BB_WLANCE );       break;
	case knight:      Xor( sq, BB_WKNIGHT );      break;
	case silver:      Xor( sq, BB_WSILVER );      break;
	case rook:        Xor( sq, BB_WROOK );        break;
	case bishop:      Xor( sq, BB_WBISHOP );      break;
	case king:	  SQ_WKING = (char)sq;        break;
	case dragon:      Xor( sq, BB_WDRAGON );      break;
	case horse:       Xor( sq, BB_WHORSE );       break;
	case gold:        Xor( sq, BB_WGOLD );        break;
	case pro_pawn:    Xor( sq, BB_WPRO_PAWN );    break;
	case pro_lance:   Xor( sq, BB_WPRO_LANCE );   break;
	case pro_knight:  Xor( sq, BB_WPRO_KNIGHT );  break;
	case pro_silver:  Xor( sq, BB_WPRO_SILVER );  break;
	}
    }
  }

  BBOr( BB_BTGOLD, BB_BPRO_PAWN,   BB_BGOLD );
  BBOr( BB_BTGOLD, BB_BPRO_LANCE,  BB_BTGOLD );
  BBOr( BB_BTGOLD, BB_BPRO_KNIGHT, BB_BTGOLD );
  BBOr( BB_BTGOLD, BB_BPRO_SILVER, BB_BTGOLD );
  BBOr( BB_B_HDK,  BB_BHORSE,      BB_BDRAGON );
  BBOr( BB_B_HDK,  BB_BKING,       BB_B_HDK );
  BBOr( BB_B_BH,   BB_BBISHOP,     BB_BHORSE );
  BBOr( BB_B_RD,   BB_BROOK,       BB_BDRAGON );

  BBOr( BB_WTGOLD, BB_WPRO_PAWN,   BB_WGOLD );
  BBOr( BB_WTGOLD, BB_WPRO_LANCE,  BB_WTGOLD );
  BBOr( BB_WTGOLD, BB_WPRO_KNIGHT, BB_WTGOLD );
  BBOr( BB_WTGOLD, BB_WPRO_SILVER, BB_WTGOLD );
  BBOr( BB_W_HDK,  BB_WHORSE,      BB_WDRAGON );
  BBOr( BB_W_HDK,  BB_WKING,       BB_W_HDK );
  BBOr( BB_W_BH,   BB_WBISHOP,     BB_WHORSE );
  BBOr( BB_W_RD,   BB_WROOK,       BB_WDRAGON );

  BB_BPAWN_ATK.p[0]  = ( BB_BPAWN.p[0] <<  9 ) & 0x7ffffffU;
  BB_BPAWN_ATK.p[0] |= ( BB_BPAWN.p[1] >> 18 ) & 0x00001ffU;
  BB_BPAWN_ATK.p[1]  = ( BB_BPAWN.p[1] <<  9 ) & 0x7ffffffU;
  BB_BPAWN_ATK.p[1] |= ( BB_BPAWN.p[2] >> 18 ) & 0x00001ffU;
  BB_BPAWN_ATK.p[2]  = ( BB_BPAWN.p[2] <<  9 ) & 0x7ffffffU;

  BB_WPAWN_ATK.p[2]  = ( BB_WPAWN.p[2] >>  9 );
  BB_WPAWN_ATK.p[2] |= ( BB_WPAWN.p[1] << 18 ) & 0x7fc0000U;
  BB_WPAWN_ATK.p[1]  = ( BB_WPAWN.p[1] >>  9 );
  BB_WPAWN_ATK.p[1] |= ( BB_WPAWN.p[0] << 18 ) & 0x7fc0000U;
  BB_WPAWN_ATK.p[0]  = ( BB_WPAWN.p[0] >>  9 );

  MATERIAL = eval_material( ptree );
  HASH_KEY = hash_func( ptree );

  memset( ptree->hist_good,       0, sizeof(ptree->hist_good) );
  memset( ptree->hist_tried,      0, sizeof(ptree->hist_tried) );

  game_status &= ( flag_reverse | flag_narrow_book
		   | flag_time_extendable | flag_learning
		   | flag_nobeep | flag_nostress | flag_nopeek
		   | flag_noponder | flag_noprompt | flag_sendpv
		   | flag_nostdout | flag_nonewlog );

  sec_b_total     = 0;
  sec_w_total     = 0;
  sec_elapsed     = 0;
  last_root_value = 0;
  last_pv.depth   = 0;
  last_pv.length  = 0;
  last_pv.a[0]    = 0;
  last_pv.a[1]    = 0;

  if ( InCheck( root_turn ) )
    {
      ptree->nsuc_check[1] = 1U;
      if ( is_mate( ptree, 1 ) ) { game_status |= flag_mated; }
    }

  BBOr( bb, BB_BPAWN, BB_WPAWN );
  BBOr( bb, bb, BB_BPRO_PAWN );
  BBOr( bb, bb, BB_WPRO_PAWN );
  npawn_box  = npawn_max;
  npawn_box -= PopuCount( bb );
  npawn_box -= (int)I2HandPawn(HAND_B);
  npawn_box -= (int)I2HandPawn(HAND_W);

  BBOr( bb, BB_BLANCE, BB_WLANCE );
  BBOr( bb, bb, BB_BPRO_LANCE );
  BBOr( bb, bb, BB_WPRO_LANCE );
  nlance_box  = nlance_max;
  nlance_box -= PopuCount( bb );
  nlance_box -= (int)I2HandLance(HAND_B);
  nlance_box -= (int)I2HandLance(HAND_W);
  
  BBOr( bb, BB_BKNIGHT, BB_WKNIGHT );
  BBOr( bb, bb, BB_BPRO_KNIGHT );
  BBOr( bb, bb, BB_WPRO_KNIGHT );
  nknight_box  = nknight_max;
  nknight_box -= PopuCount( bb );
  nknight_box -= (int)I2HandKnight(HAND_B);
  nknight_box -= (int)I2HandKnight(HAND_W);

  BBOr( bb, BB_BSILVER, BB_WSILVER );
  BBOr( bb, bb, BB_BPRO_SILVER );
  BBOr( bb, bb, BB_WPRO_SILVER );
  nsilver_box  = nsilver_max;
  nsilver_box -= PopuCount( bb );
  nsilver_box -= (int)I2HandSilver(HAND_B);
  nsilver_box -= (int)I2HandSilver(HAND_W);

  BBOr( bb, BB_BGOLD, BB_WGOLD );
  ngold_box  = ngold_max;
  ngold_box -= PopuCount( bb );
  ngold_box -= (int)I2HandGold(HAND_B);
  ngold_box -= (int)I2HandGold(HAND_W);

  BBOr( bb, BB_BBISHOP, BB_WBISHOP );
  BBOr( bb, bb, BB_BHORSE );
  BBOr( bb, bb, BB_WHORSE );
  nbishop_box  = nbishop_max;
  nbishop_box -= PopuCount( bb );
  nbishop_box -= (int)I2HandBishop(HAND_B);
  nbishop_box -= (int)I2HandBishop(HAND_W);

  BBOr( bb, BB_BROOK, BB_WROOK );
  BBOr( bb, bb, BB_BDRAGON );
  BBOr( bb, bb, BB_WDRAGON );
  nrook_box  = nrook_max;
  nrook_box -= PopuCount( bb );
  nrook_box -= (int)I2HandRook(HAND_B);
  nrook_box -= (int)I2HandRook(HAND_W);

#if defined(YSS_ZERO)
  copy_min_posi(ptree, 0, 0);
  ptree->sequence_hash = 0;
#endif

  iret = exam_tree( ptree );
  if ( iret < 0 )
    {
      ini_game( ptree, &min_posi_no_handicap, 0, NULL, NULL );
      return iret;
    }

  /* connect to Tsumeshogi server */
#if defined(DFPN_CLIENT)
  lock( &dfpn_client_lock );
  dfpn_client_start( ptree );
  snprintf( (char *)dfpn_client_signature, DFPN_CLIENT_SIZE_SIGNATURE,
	    "%" PRIx64 "_%x_%x_%x", HASH_KEY, HAND_B, HAND_W, root_turn );
  dfpn_client_signature[DFPN_CLIENT_SIZE_SIGNATURE-1] = '\0';
  dfpn_client_rresult       = dfpn_client_na;
  dfpn_client_num_cresult   = 0;
  dfpn_client_flag_read     = 0;
  dfpn_client_out( "new %s\n", dfpn_client_signature );
  unlock( &dfpn_client_lock );
#endif

  return 1;
}


int CONV gen_legal_moves( tree_t * restrict ptree, unsigned int *p0, int flag )
{
  unsigned int *p1;
  int i, j, n;

  p1 = GenCaptures( root_turn, p0 );
  p1 = GenNoCaptures( root_turn, p1 );
  if ( flag )
    {
      p1 = GenCapNoProEx2( root_turn, p1 );
      p1 = GenNoCapNoProEx2( root_turn, p1 );
    }
  p1 = GenDrop( root_turn, p1 );
  n  = (int)( p1 - p0 );

  for ( i = 0; i < n; i++ )
    {
      MakeMove( root_turn, p0[i], 1 );
      if ( InCheck( root_turn ) )
	{
	  UnMakeMove( root_turn, p0[i], 1 );
	  p0[i] = 0;
	  continue;
	}
      if ( InCheck(Flip(root_turn)) )
	{
	  ptree->nsuc_check[2] = (unsigned char)( ptree->nsuc_check[0] + 1U );
	  if ( ptree->nsuc_check[2] >= 6U
	       && ( detect_repetition( ptree, 2, Flip(root_turn), 3 )
		    == perpetual_check ) )
	    {
	      UnMakeMove( root_turn, p0[i], 1 );
	      p0[i] = 0;
	      continue;
	    }
	}
      UnMakeMove( root_turn, p0[i], 1 );
    }

  for ( i = 0; i < n; )
    {
      if ( ! p0[i] )
	{
	  for ( j = i+1; j < n; j++ ) { p0[j-1] = p0[j]; }
	  n -= 1;
	}
      else { i++; }
    }

  return n;
}

#if defined(YSS_ZERO)
/*
useless
int CONV gen_legal_moves_aobazero( tree_t * restrict ptree, unsigned int *p0, int flag, int turn, int ply )
{
  unsigned int *p1;
  int i, j, n;

  p1 = GenCaptures( turn, p0 );
  p1 = GenNoCaptures( turn, p1 );
  if ( flag )
    {
      p1 = GenCapNoProEx2( turn, p1 );
      p1 = GenNoCapNoProEx2( turn, p1 );
    }
  p1 = GenDrop( turn, p1 );
  n  = (int)( p1 - p0 );

  for ( i = 0; i < n; i++ )
    {
      MakeMove( turn, p0[i], 1 );	// not 1, must be ply.
      if ( InCheck( turn ) )
	{
	  UnMakeMove( turn, p0[i], 1 );
	  p0[i] = 0;
	  continue;
	}
      UnMakeMove( turn, p0[i], 1 );
    }

  for ( i = 0; i < n; )
    {
      if ( ! p0[i] )
	{
	  for ( j = i+1; j < n; j++ ) { p0[j-1] = p0[j]; }
	  n -= 1;
	}
      else { i++; }
    }

  return n;
}
*/
#endif


/*
  - detection of perpetual check is omitted.
  - weak moves are omitted.
*/
int CONV
is_mate( tree_t * restrict ptree, int ply )
{
  int iret = 0;

  assert( InCheck(root_turn) );

  ptree->move_last[ply] = GenEvasion( root_turn, ptree->move_last[ply-1] );
  if ( ptree->move_last[ply] == ptree->move_last[ply-1] ) { iret = 1; }

  return iret;
}


int CONV
is_hand_eq_supe( unsigned int u, unsigned int uref )
{
#if 1
/* aggressive superior correspondences are applied, that is:
 *   pawn  <= lance, silver, gold, rook
 *   lance <= rook.
 */
  int nsupe;

  if ( IsHandKnight(u) < IsHandKnight(uref)
       || IsHandSilver(u) < IsHandSilver(uref)
       || IsHandGold(u)   < IsHandGold(uref)
       || IsHandBishop(u) < IsHandBishop(uref)
       || IsHandRook(u)   < IsHandRook(uref) ) { return 0; }

  nsupe  = (int)I2HandRook(u)  - (int)I2HandRook(uref);
  nsupe += (int)I2HandLance(u) - (int)I2HandLance(uref);
  if ( nsupe < 0 ) { return 0; }

  nsupe += (int)I2HandSilver(u) - (int)I2HandSilver(uref);
  nsupe += (int)I2HandGold(u)   - (int)I2HandGold(uref);
  nsupe += (int)I2HandPawn(u)   - (int)I2HandPawn(uref);
  if ( nsupe < 0 ) { return 0; }

  return 1;
#else
  if ( IsHandPawn(u) >= IsHandPawn(uref)
       && IsHandLance(u)  >= IsHandLance(uref)
       && IsHandKnight(u) >= IsHandKnight(uref)
       && IsHandSilver(u) >= IsHandSilver(uref)
       && IsHandGold(u)   >= IsHandGold(uref)
       && IsHandBishop(u) >= IsHandBishop(uref)
       && IsHandRook(u)   >= IsHandRook(uref) ) { return 1; }
  
  return 0;
#endif
}


/* weak moves are omitted. */
int CONV
detect_repetition( tree_t * restrict ptree, int ply, int turn, int nth )
{
  const unsigned int *p;
  unsigned int hand1, hand2;
  int n, i, imin, counter, irep, ncheck;

  ncheck = (int)ptree->nsuc_check[ply];
  n      = ptree->nrep + ply - 1;

  /*if ( ncheck >= 6 )*/
  if ( ncheck >= nth * 2 )
    {
      /* imin = n - ncheck*2; */
      imin = n - ncheck*2 + 1;
      if ( imin < 0 ) { imin = 0; }

      ptree->move_last[ply] = GenEvasion( turn, ptree->move_last[ply-1] );
      for ( p = ptree->move_last[ply-1]; p < ptree->move_last[ply]; p++ )
	{
	  MakeMove( turn, *p, ply );

	  /* for ( i = n-1, counter = 0; i >= imin; i -= 2 ) */
	  for ( i = n-3, counter = 0; i >= imin; i -= 2 )
	    {
	      if ( ptree->rep_board_list[i] == HASH_KEY
		   && ptree->rep_hand_list[i] == HAND_B
		   && ++counter == nth )
		   /* && ncheck*2 - 1 >= n - i )*/
		{
		  UnMakeMove( turn, *p, ply );
		  move_evasion_pchk = *p;
		  return perpetual_check;
		}
	    }
	  UnMakeMove( turn, *p, ply );
	}
    }

  irep = no_rep;
  for ( i = n-4, counter = 0; i >= 0; i-- )
    {
      if ( ptree->rep_board_list[i] == HASH_KEY )
	{
	  hand1 = HAND_B;
	  hand2 = ptree->rep_hand_list[i];

	  if ( (n-i) & 1 )
	    {
	      if ( irep == no_rep )
		{
		  if ( turn )
		    {
		      if ( is_hand_eq_supe( hand2, hand1 ) )
			{
			  irep = white_superi_rep;
			}
		    }
		  else if ( is_hand_eq_supe( hand1, hand2 ) )
		    {
		      irep = black_superi_rep;
		    }
		}
	    }
	  else if ( hand1 == hand2 )
	    {
	      if ( ++counter == nth )
		{
		  if ( (ncheck-1)*2 >= n - i ) { return perpetual_check; }
		  else                         { return four_fold_rep; }
		}
	    }
	  else if ( irep == no_rep )
	    {
	      if ( is_hand_eq_supe( hand1, hand2 ) )
		{
		  irep = black_superi_rep;
		}
	      else if ( is_hand_eq_supe( hand2, hand1 ) )
		{
		  irep = white_superi_rep;
		}
	    }
	}
    }

  return irep;
}


int CONV
com_turn_start( tree_t * restrict ptree, int flag )
{
  const char *str_move;
  unsigned int move, sec_total;
  int iret, is_resign, value;

  if ( ! ( flag & flag_from_ponder ) )
    {
      assert( ! ( game_status & mask_game_end ) );
      
      time_start = time_turn_start;
      
      game_status |=  flag_thinking;
      iret         = iterate( ptree );
      game_status &= ~flag_thinking;
      if ( iret < 0 ) { return iret; }
    }
  if ( game_status & flag_suspend ) { return 1; }

  move     = last_pv.a[1];
  value    = root_turn ? -last_root_value : last_root_value;

  if ( value < -resign_threshold && last_pv.type != pv_fail_high )
    {
      is_resign = 1;
    }
  else { is_resign = 0; }

#if defined(DBG_EASY)
  if ( easy_move && easy_move != move )
    {
      out_warning( "EASY MOVE DITECTION FAILED." );
    }
#endif

  /* send urgent outputs */
  if ( is_resign )
    {
#if defined(CSA_LAN)
      if ( sckt_csa != SCKT_NULL )
	{
	  iret = sckt_out( sckt_csa, "%%TORYO\n" );
	  if ( iret < 0 ) { return iret; }
	}
#endif
      OutCsaShogi( "resign\n" );
    }
  else {
#if defined(USI)
    if ( usi_mode != usi_off )
      {
	char buf[6];
	csa2usi( ptree, str_CSA_move(move), buf );
	USIOut( "bestmove %s\n", buf );
      }
#endif

    OutCsaShogi( "move%s\n", str_CSA_move( move ) );

#if defined(CSA_LAN)
    if ( sckt_csa != SCKT_NULL ) {
      
      if ( game_status & flag_sendpv ) {
	int i, turn, byte;
	char buf[256];
	
	byte = snprintf( buf, 256, "%c%s,\'* %d",
			 ach_turn[root_turn], str_CSA_move( move ),
			 last_root_value );
	
	turn = root_turn;
	for( i = 2; i <= last_pv.length && i < 5; i++ )
	  {
	    turn = Flip(turn);
	    byte += snprintf( buf+byte, 256-byte, " %c%s",
			      ach_turn[turn], str_CSA_move(last_pv.a[i]) );
	  }
	
	iret = sckt_out( sckt_csa, "%s\n", buf );
	if ( iret < 0 ) { return iret; }
	
      } else {
	
	iret = sckt_out( sckt_csa, "%c%s\n", ach_turn[root_turn],
			 str_CSA_move( move ) );
	if ( iret < 0 ) { return iret; }
      }
    }
#endif
  }
  OutBeep();
  
  /* show search result and make a move */
  if ( is_resign )
    {
      show_prompt();
      game_status |= flag_resigned;
      update_time( root_turn );
      out_CSA( ptree, &record_game, MOVE_RESIGN );
      sec_total = root_turn ? sec_w_total : sec_b_total;
      str_move  = "resign";
    }
  else {
    show_prompt();
    iret = make_move_root( ptree, move,
			   ( flag_rep | flag_time | flag_history ) );
    if ( iret < 0 ) { return iret; }
    sec_total = root_turn ? sec_b_total : sec_w_total;
    str_move  = str_CSA_move( move );
  }

  OutCsaShogi( "info tt %03u:%02u\n", sec_total / 60U, sec_total % 60U );
  Out( "%s '(%d%s) %03u:%02u/%03u:%02u  elapsed: b%u, w%u\n",
       str_move, value,
       ( last_pv.type == pv_fail_high ) ? "!" : "",
       sec_elapsed / 60U, sec_elapsed % 60U,
       sec_total   / 60U, sec_total   % 60U,
       sec_b_total, sec_w_total );
 
  if ( ! is_resign )
    {
#if ! defined(NO_STDOUT)
      iret = out_board( ptree, stdout, move, 0 );
      if ( iret < 0 ) { return iret; }
#endif
    }

  return 1;
}

#if defined(MNJ_LAN)
int CONV mnj_reset_tbl( int sd, unsigned int seed )
{
  double average, deviation, d;
  unsigned int u;
  int i, j;

  if ( sd <= 0 ) { return load_fv(); }

  if ( load_fv()           < 0 ) { return -1; }
  if ( clear_trans_table() < 0 ) { return -1; }
  ehash_clear();


  ini_rand( seed );
  average   = 0.0;
  deviation = 0.0;

  for( i = 0; i < nsquare * pos_n; i++ )
    {
      d = -6.0;

      for ( j = 0; j < 12; j++ ) { d += (double)rand32() / (double)UINT_MAX; }
      d             *= (double)sd;
      average       += d;
      deviation     += d * d;
      pc_on_sq[0][i] = (short)( (int)pc_on_sq[0][i] + (int)d );
    }

  for( i = 0; i < nsquare * nsquare * kkp_end; i++ )
    {
      d = -6.0;

      for ( j = 0; j < 12; j++ ) { d += (double)rand32() / (double)UINT_MAX; }
      d           *= (double)sd;
      average     += d;
      deviation   += d * d;
      kkp[0][0][i] = (short)( (int)kkp[0][0][i] + (int)d );
    }

  average   /= (double)( nsquare * pos_n + nsquare * nsquare * kkp_end );
  deviation /= (double)( nsquare * pos_n + nsquare * nsquare * kkp_end );
  deviation  = sqrt( deviation );

  if ( get_elapsed( &u ) < 0 ) { return -1; }
  ini_rand( u );

  Out( "\nThe normal distribution N(0,sd^2) is generated.\n" );
  Out( "  actual average:            % .7f\n", average );
  Out( "  actual standard deviation: % .7f\n", deviation );
  Out( "rand seed = %x\n", u );

  return 1;
}
#endif

void * CONV memory_alloc( size_t nbytes )
{
#if defined(_WIN32)
  void *p = VirtualAlloc( NULL, nbytes, MEM_COMMIT, PAGE_READWRITE );
  if ( p == NULL ) { str_error = "VirturlAlloc() faild"; }
#else
  void *p = malloc( nbytes );
  if ( p == NULL ) { str_error = "malloc() faild"; }
#endif
  return p;
}


int CONV memory_free( void *p )
{
#if defined(_WIN32)
  if ( VirtualFree( p, 0, MEM_RELEASE ) ) { return 1; }
  str_error = "VirtualFree() faild";
  return -2;
#else
  free( p );
  return 1;
#endif
}
