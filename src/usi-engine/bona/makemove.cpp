// 2019 Team AobaZero
// This source code is in the public domain.
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "shogi.h"

#define DropB( PIECE, piece )  Xor( to, BB_B ## PIECE );                    \
                               HASH_KEY    ^= ( b_ ## piece ## _rand )[to]; \
                               HAND_B      -= flag_hand_ ## piece;          \
                               BOARD[to]  = piece

#define DropW( PIECE, piece )  Xor( to, BB_W ## PIECE );                    \
                               HASH_KEY    ^= ( w_ ## piece ## _rand )[to]; \
                               HAND_W      -= flag_hand_ ## piece;          \
                               BOARD[to]  = - piece

#define CapB( PIECE, piece, pro_piece )                   \
          Xor( to, BB_B ## PIECE );                       \
          HASH_KEY  ^= ( b_ ## pro_piece ## _rand )[to];  \
          HAND_W    += flag_hand_ ## piece;               \
          MATERIAL  -= MT_CAP_ ## PIECE

#define CapW( PIECE, piece, pro_piece )                   \
          Xor( to, BB_W ## PIECE );                       \
          HASH_KEY  ^= ( w_ ## pro_piece ## _rand )[to];  \
          HAND_B    += flag_hand_ ## piece;               \
          MATERIAL  += MT_CAP_ ## PIECE

#define NocapProB( PIECE, PRO_PIECE, piece, pro_piece )       \
          Xor( from, BB_B ## PIECE );                         \
          Xor( to,   BB_B ## PRO_PIECE );                     \
          HASH_KEY    ^= ( b_ ## pro_piece ## _rand )[to]     \
                       ^ ( b_ ## piece     ## _rand )[from];  \
          MATERIAL    += MT_PRO_ ## PIECE;                    \
          BOARD[to] = pro_piece

#define NocapProW( PIECE, PRO_PIECE, piece, pro_piece )       \
          Xor( from, BB_W ## PIECE );                         \
          Xor( to,   BB_W ## PRO_PIECE );                     \
          HASH_KEY    ^= ( w_ ## pro_piece ## _rand )[to]     \
                       ^ ( w_ ## piece     ## _rand )[from];  \
          MATERIAL    -= MT_PRO_ ## PIECE;                    \
          BOARD[to]  = - pro_piece
 
#define NocapNoproB( PIECE, piece )                          \
          SetClear( BB_B ## PIECE );                         \
          HASH_KEY    ^= ( b_ ## piece ## _rand )[to]        \
                       ^ ( b_ ## piece ## _rand )[from];     \
          BOARD[to] = piece

#define NocapNoproW( PIECE, piece )                          \
          SetClear( BB_W ## PIECE );                         \
          HASH_KEY    ^= ( w_ ## piece ## _rand )[to]        \
                       ^ ( w_ ## piece ## _rand )[from];     \
          BOARD[to] = - piece


void CONV
make_move_b( tree_t * restrict ptree, unsigned int move, int ply )
{
  const int from = (int)I2From(move);
  const int to   = (int)I2To(move);
  const int nrep = ptree->nrep + ply - 1;

  assert( UToCap(move) != king );
  assert( move );
  assert( is_move_valid( ptree, move, black ) );

  ptree->rep_board_list[nrep]    = HASH_KEY;
  ptree->rep_hand_list[nrep]     = HAND_B;
  ptree->save_material[ply]      = (short)MATERIAL;
  ptree->save_eval[ply+1]        = INT_MAX;

#if defined(YSS_ZERO)
  ptree->history_in_check[nrep]   = InCheck(black);
  ptree->keep_sequence_hash[nrep] = ptree->sequence_hash;
  if ( from >= nsquare ) {
    ptree->sequence_hash ^= get_sequence_hash_drop(nrep & (SEQUENCE_HASH_SIZE-1), to, From2Drop(from)-1);
//    PRT("b:nrep=%3d(%3d):        to=%2d,drop=%d\n",nrep,nrep & (SEQUENCE_HASH_SIZE-1),to,From2Drop(from)-1);
  } else {
    ptree->sequence_hash ^= get_sequence_hash_from_to(nrep & (SEQUENCE_HASH_SIZE-1), from, to,(I2IsPromote(move)!=0));
//    PRT("b:nrep=%3d(%3d):from=%2d,to=%2d,prom=%d,%016" PRIx64 "",nrep,nrep & (SEQUENCE_HASH_SIZE-1),from,to,(I2IsPromote(move)!=0),sequence_hash_from_to[nrep & (SEQUENCE_HASH_SIZE-1)][from][to][(I2IsPromote(move)!=0)]);
  }
  if ( ptree->keep_sequence_hash[nrep] == ptree->sequence_hash ) { PRT("sequence_hash err!\n"); debug(); }
#endif


  if ( from >= nsquare )
    {
      switch ( From2Drop(from) )
	{
	case pawn:   Xor( to-nfile, BB_BPAWN_ATK );
                     DropB( PAWN,   pawn   );  break;
	case lance:  DropB( LANCE,  lance  );  break;
	case knight: DropB( KNIGHT, knight );  break;
	case silver: DropB( SILVER, silver );  break;
	case gold:   DropB( GOLD,   gold   );
                     Xor( to, BB_BTGOLD );     break;
	case bishop: DropB( BISHOP, bishop );
                     Xor( to, BB_B_BH );       break;
	default:     assert( From2Drop(from) == rook );
                     DropB( ROOK,  rook );
		     Xor( to, BB_B_RD );       break;
	}
      Xor( to, BB_BOCCUPY );
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
    }
  else {
    const int ipiece_move = (int)I2PieceMove(move);
    const int ipiece_cap  = (int)UToCap(move);
    const int is_promote  = (int)I2IsPromote(move);
    bitboard_t bb_set_clear;

    BBOr( bb_set_clear, abb_mask[from], abb_mask[to] );
    SetClear( BB_BOCCUPY );
    BOARD[from] = empty;

    if ( is_promote ) switch( ipiece_move )
      {
      case pawn:   Xor( to, BB_BPAWN_ATK );
                   Xor( to, BB_BTGOLD );
                   NocapProB( PAWN,   PRO_PAWN,   pawn,   pro_pawn );   break;
      case lance:  Xor( to, BB_BTGOLD );
                   NocapProB( LANCE,  PRO_LANCE,  lance,  pro_lance );  break;
      case knight: Xor( to, BB_BTGOLD );
                   NocapProB( KNIGHT, PRO_KNIGHT, knight, pro_knight ); break;
      case silver: Xor( to, BB_BTGOLD );
                   NocapProB( SILVER, PRO_SILVER, silver, pro_silver ); break;
      case bishop: Xor( to, BB_B_HDK );
		   SetClear( BB_B_BH );
                   NocapProB( BISHOP, HORSE,      bishop, horse );      break;
      default:     assert( ipiece_move == rook );
                   Xor( to, BB_B_HDK );
		   SetClear( BB_B_RD );
                   NocapProB( ROOK,   DRAGON,     rook,   dragon );     break;
      }
    else switch ( ipiece_move )
      {
      case pawn:       Xor( to-nfile, BB_BPAWN_ATK );
                       Xor( to,       BB_BPAWN_ATK );
                       NocapNoproB( PAWN,   pawn);       break;
      case lance:      NocapNoproB( LANCE,  lance);      break;
      case knight:     NocapNoproB( KNIGHT, knight);     break;
      case silver:     NocapNoproB( SILVER, silver);     break;
      case gold:       NocapNoproB( GOLD,   gold);
                       SetClear( BB_BTGOLD );             break;
      case bishop:     SetClear( BB_B_BH );
                       NocapNoproB( BISHOP, bishop);     break;
      case rook:       NocapNoproB( ROOK,   rook);
                       SetClear( BB_B_RD );                break;
      case king:       HASH_KEY ^= b_king_rand[to] ^ b_king_rand[from];
                       SetClear( BB_B_HDK );
                       BOARD[to] = king;
                       SQ_BKING  = (char)to;           break;
      case pro_pawn:   NocapNoproB( PRO_PAWN, pro_pawn );
                       SetClear( BB_BTGOLD );             break;
      case pro_lance:  NocapNoproB( PRO_LANCE, pro_lance );
                       SetClear( BB_BTGOLD );             break;
      case pro_knight: NocapNoproB( PRO_KNIGHT, pro_knight );
                       SetClear( BB_BTGOLD );             break;
      case pro_silver: NocapNoproB( PRO_SILVER, pro_silver );
                       SetClear( BB_BTGOLD );             break;
      case horse:      NocapNoproB( HORSE, horse );
                       SetClear( BB_B_HDK );
                       SetClear( BB_B_BH );                break;
      default:         assert( ipiece_move == dragon );
                       NocapNoproB( DRAGON, dragon );
                       SetClear( BB_B_HDK );
                       SetClear( BB_B_RD );                break;
      }
    
    if ( ipiece_cap )
      {
	switch( ipiece_cap )
	  {
	  case pawn:       CapW( PAWN, pawn, pawn );
                           Xor( to+nfile, BB_WPAWN_ATK );               break;
	  case lance:      CapW( LANCE,  lance, lance );       break;
	  case knight:     CapW( KNIGHT, knight, knight );      break;
	  case silver:     CapW( SILVER, silver, silver );      break;
	  case gold:       CapW( GOLD,   gold,   gold );
                           Xor( to, BB_WTGOLD );                       break;
	  case bishop:     CapW( BISHOP, bishop, bishop );
                           Xor( to, BB_W_BH );                          break;
	  case rook:       CapW( ROOK, rook, rook);
                           Xor( to, BB_W_RD );                          break;
	  case pro_pawn:   CapW( PRO_PAWN, pawn, pro_pawn );
                           Xor( to, BB_WTGOLD );                       break;
	  case pro_lance:  CapW( PRO_LANCE, lance, pro_lance );
                           Xor( to, BB_WTGOLD );                       break;
	  case pro_knight: CapW( PRO_KNIGHT, knight, pro_knight );
                           Xor( to, BB_WTGOLD );                       break;
	  case pro_silver: CapW( PRO_SILVER, silver, pro_silver );
                           Xor( to, BB_WTGOLD );                       break;
	  case horse:      CapW( HORSE, bishop, horse );
                           Xor( to, BB_W_HDK );
			   Xor( to, BB_W_BH );                          break;
	  default:         assert( ipiece_cap == dragon );
                           CapW( DRAGON, rook, dragon );
                           Xor( to, BB_W_HDK );
			   Xor( to, BB_W_RD );                         break;
	  }
	Xor( to, BB_WOCCUPY );
	XorFile( from, OCCUPIED_FILE );
	XorDiag2( from, OCCUPIED_DIAG2 );
	XorDiag1( from, OCCUPIED_DIAG1 );
      }
    else {
      SetClearFile( from, to, OCCUPIED_FILE );
      SetClearDiag1( from, to, OCCUPIED_DIAG1 );
      SetClearDiag2( from, to, OCCUPIED_DIAG2 );
    }
  }

  assert( exam_bb( ptree ) );
}


void CONV
make_move_w( tree_t * restrict ptree, unsigned int move, int ply )
{
  const int from = (int)I2From(move);
  const int to   = (int)I2To(move);
  const int nrep  = ptree->nrep + ply - 1;

  assert( UToCap(move) != king );
  assert( move );
  assert( is_move_valid( ptree, move, white ) );

  ptree->rep_board_list[nrep]    = HASH_KEY;
  ptree->rep_hand_list[nrep]     = HAND_B;
  ptree->save_material[ply]      = (short)MATERIAL;
  ptree->save_eval[ply+1]        = INT_MAX;

#if defined(YSS_ZERO)
  ptree->history_in_check[nrep]   = InCheck(white);
  ptree->keep_sequence_hash[nrep] = ptree->sequence_hash;
  if ( from >= nsquare ) {
    ptree->sequence_hash ^= get_sequence_hash_drop(nrep & (SEQUENCE_HASH_SIZE-1), to, From2Drop(from)-1);
//    PRT("w:nrep=%3d(%3d):        to=%2d,drop=%d\n",nrep,nrep & (SEQUENCE_HASH_SIZE-1),to,From2Drop(from)-1);
  } else {
    ptree->sequence_hash ^= get_sequence_hash_from_to(nrep & (SEQUENCE_HASH_SIZE-1), from, to, (I2IsPromote(move)!=0));
//    PRT("w:nrep=%3d(%3d):from=%2d,to=%2d,prom=%d,%016" PRIx64 "",nrep,nrep & (SEQUENCE_HASH_SIZE-1),from,to,(I2IsPromote(move)!=0),sequence_hash_from_to[nrep & (SEQUENCE_HASH_SIZE-1)][from][to][(I2IsPromote(move)!=0)]);
  }
  if ( ptree->keep_sequence_hash[nrep] == ptree->sequence_hash ) { PRT("sequence_hash err!\n"); debug(); }
#endif

  if ( from >= nsquare )
    {
      switch( From2Drop(from) )
	{
	case pawn:   Xor( to+nfile, BB_WPAWN_ATK );
                     DropW( PAWN,   pawn );    break;
	case lance:  DropW( LANCE,  lance );   break;
	case knight: DropW( KNIGHT, knight );  break;
	case silver: DropW( SILVER, silver );  break;
	case gold:   DropW( GOLD,   gold );
                     Xor( to, BB_WTGOLD );     break;
	case bishop: DropW( BISHOP, bishop );
                     Xor( to, BB_W_BH );       break;
	default:     DropW( ROOK,   rook );
                     Xor( to, BB_W_RD );       break;
	}
      Xor( to, BB_WOCCUPY );
      XorFile( to, OCCUPIED_FILE );
      XorDiag2( to, OCCUPIED_DIAG2 );
      XorDiag1( to, OCCUPIED_DIAG1 );
    }
  else {
    const int ipiece_move = (int)I2PieceMove(move);
    const int ipiece_cap  = (int)UToCap(move);
    const int is_promote  = (int)I2IsPromote(move);
    bitboard_t bb_set_clear;

    BBOr( bb_set_clear, abb_mask[from], abb_mask[to] );
    SetClear( BB_WOCCUPY );
    BOARD[from] = empty;

    if ( is_promote) switch( ipiece_move )
      {
      case pawn:   NocapProW( PAWN, PRO_PAWN, pawn, pro_pawn );
                   Xor( to, BB_WPAWN_ATK );
                   Xor( to, BB_WTGOLD );                           break;
      case lance:  NocapProW( LANCE, PRO_LANCE, lance, pro_lance );
                   Xor( to, BB_WTGOLD );                           break;
      case knight: NocapProW( KNIGHT, PRO_KNIGHT, knight, pro_knight );
                   Xor( to, BB_WTGOLD );                           break;
      case silver: NocapProW( SILVER, PRO_SILVER, silver, pro_silver );
                   Xor( to, BB_WTGOLD );                           break;
      case bishop: NocapProW( BISHOP, HORSE, bishop, horse );
                   Xor( to, BB_W_HDK );
		   SetClear( BB_W_BH );                              break;
      default:     NocapProW( ROOK, DRAGON, rook, dragon);
                   Xor( to, BB_W_HDK );
		   SetClear( BB_W_RD );                              break;
      }
    else switch ( ipiece_move )
      {
      case pawn:       NocapNoproW( PAWN, pawn );
                       Xor( to+nfile, BB_WPAWN_ATK );
                       Xor( to,       BB_WPAWN_ATK );     break;
      case lance:      NocapNoproW( LANCE,     lance);      break;
      case knight:     NocapNoproW( KNIGHT,    knight);     break;
      case silver:     NocapNoproW( SILVER,    silver);     break;
      case gold:       NocapNoproW( GOLD,      gold);
                       SetClear( BB_WTGOLD );             break;
      case bishop:     NocapNoproW( BISHOP,    bishop);
                       SetClear( BB_W_BH );                break;
      case rook:       NocapNoproW( ROOK,      rook);
                       SetClear( BB_W_RD );                break;
      case king:       HASH_KEY    ^= w_king_rand[to] ^ w_king_rand[from];
                       BOARD[to]  = - king;
                       SQ_WKING   = (char)to;
                       SetClear( BB_W_HDK );               break;
      case pro_pawn:   NocapNoproW( PRO_PAWN,   pro_pawn);
                       SetClear( BB_WTGOLD );             break;
      case pro_lance:  NocapNoproW( PRO_LANCE,  pro_lance);
                       SetClear( BB_WTGOLD );             break;
      case pro_knight: NocapNoproW( PRO_KNIGHT, pro_knight);
                       SetClear( BB_WTGOLD );             break;
      case pro_silver: NocapNoproW( PRO_SILVER, pro_silver);
                       SetClear( BB_WTGOLD );             break;
      case horse:      NocapNoproW( HORSE, horse );
                       SetClear( BB_W_HDK );
                       SetClear( BB_W_BH );                break;
      default:         NocapNoproW( DRAGON, dragon );
                       SetClear( BB_W_HDK );
                       SetClear( BB_W_RD );                break;
      }

    if ( ipiece_cap )
      {
	switch( ipiece_cap )
	  {
	  case pawn:       CapB( PAWN, pawn, pawn );
                           Xor( to-nfile, BB_BPAWN_ATK );           break;
	  case lance:      CapB( LANCE,  lance,  lance );           break;
	  case knight:     CapB( KNIGHT, knight, knight );          break;
	  case silver:     CapB( SILVER, silver, silver );          break;
	  case gold:       CapB( GOLD,   gold,   gold );
                           Xor( to, BB_BTGOLD );                   break;
	  case bishop:     CapB( BISHOP, bishop, bishop );
                           Xor( to, BB_B_BH );                      break;
	  case rook:       CapB( ROOK, rook, rook );
                           Xor( to, BB_B_RD );                      break;
	  case pro_pawn:   CapB( PRO_PAWN, pawn, pro_pawn );
                           Xor( to, BB_BTGOLD );                   break;
	  case pro_lance:  CapB( PRO_LANCE, lance, pro_lance );
                           Xor( to, BB_BTGOLD );                   break;
	  case pro_knight: CapB( PRO_KNIGHT, knight, pro_knight );
                           Xor( to, BB_BTGOLD );                   break;
	  case pro_silver: CapB( PRO_SILVER, silver, pro_silver );
                           Xor( to, BB_BTGOLD );                   break;
	  case horse:      CapB( HORSE, bishop, horse );
                           Xor( to, BB_B_HDK );
                           Xor( to, BB_B_BH );                      break;
	  default:         CapB( DRAGON, rook, dragon );
                           Xor( to, BB_B_HDK );
                           Xor( to, BB_B_RD );                      break;
	  }
	Xor( to, BB_BOCCUPY );
	XorFile( from, OCCUPIED_FILE );
	XorDiag1( from, OCCUPIED_DIAG1 );
	XorDiag2( from, OCCUPIED_DIAG2 );
      }
    else {
      SetClearFile( from, to, OCCUPIED_FILE );
      SetClearDiag1( from, to, OCCUPIED_DIAG1 );
      SetClearDiag2( from, to, OCCUPIED_DIAG2 );
    }
  }

  assert( exam_bb( ptree ) );
}

#undef DropB
#undef DropW
#undef CapB
#undef CapW
#undef NocapProB
#undef NocapProW
#undef NocapNoproB
#undef NocapNoproW

/*
 * flag_detect_hang
 * flag_rep
 * flag_time
 * flag_nomake_move
 * flag_history
 */
int CONV
make_move_root( tree_t * restrict ptree, unsigned int move, int flag )
{
  int check, drawn, iret, i, n;

  MakeMove( root_turn, move, 1 );

#if defined(YSS_ZERO)
  copy_min_posi(ptree, Flip(root_turn), 1);
#endif

  /* detect hang-king */
  if ( ( flag & flag_detect_hang ) && InCheck(root_turn) )
    {
      str_error = str_king_hang;
      UnMakeMove( root_turn, move, 1 );
      return -2;
    }

  drawn = 0;
  check = InCheck( Flip(root_turn) );
  ptree->move_last[1]  = ptree->move_last[0];
  if ( check )
    {
      ptree->nsuc_check[2] = (unsigned char)( ptree->nsuc_check[0] + 1U );
    }
  else { ptree->nsuc_check[2] = 0; }

  /* detect repetitions */
  if ( flag & flag_rep )
    {
      switch ( detect_repetition( ptree, 2, Flip(root_turn), 3 ) )
	{
	case perpetual_check:
	  str_error = str_perpet_check;
	  UnMakeMove( root_turn, move, 1 );
	  return -2;
      
	case four_fold_rep:
	  drawn = 1;
	  break;
	}
    }

  /* return, since all of rule-checks were done */
  if ( flag & flag_nomake_move )
    {
      UnMakeMove( root_turn, move, 1 );
      return drawn ? 2 : 1;
    }

  if ( drawn ) { game_status |= flag_drawn; }

  /* update time */
  if ( flag & flag_time )
    {
      iret = update_time( root_turn );
      if ( iret < 0 ) { return -1; }
    }

  root_turn = Flip( root_turn );

  /* detect checkmate */
  if ( check && is_mate( ptree, 1 ) ) { game_status |= flag_mated; }

  /* save history */
  if ( flag & flag_history ) { out_CSA( ptree, &record_game, move ); }

  /* renew repetition table */
  n = ptree->nrep;
  if ( n >= REP_HIST_LEN - PLY_MAX -1 )
    {
      for ( i = 0; i < n; i++ )
	{
	  ptree->rep_board_list[i] = ptree->rep_board_list[i+1];
	  ptree->rep_hand_list[i]  = ptree->rep_hand_list[i+1];
#if defined(YSS_ZERO)
	  ptree->history_in_check[i] = ptree->history_in_check[i+1];
#endif
	}
    }
  else { ptree->nrep++; }

  for ( i = 1; i < NUM_UNMAKE; i += 1 )
    {
      amove_save[i-1]            = amove_save[i];
      amaterial_save[i-1]        = amaterial_save[i];
      ansuc_check_save[i-1]      = ansuc_check_save[i];
      alast_root_value_save[i-1] = alast_root_value_save[i];
      alast_pv_save[i-1]         = alast_pv_save[i];
    }
  amove_save      [NUM_UNMAKE-1] = move;
  amaterial_save  [NUM_UNMAKE-1] = ptree->save_material[1];
  ansuc_check_save[NUM_UNMAKE-1] = ptree->nsuc_check[0];
  ptree->nsuc_check[0]           = ptree->nsuc_check[1];
  ptree->nsuc_check[1]           = ptree->nsuc_check[2];

  /* update pv */
  alast_root_value_save[NUM_UNMAKE-1] = last_root_value;
  alast_pv_save[NUM_UNMAKE-1]         = last_pv;

  if ( last_pv.a[1] == move && last_pv.length >= 2 )
    {
      if ( last_pv.depth )
	{
#if PLY_INC == EXT_CHECK
	  if ( ! check )
#endif
	    last_pv.depth--;
	}
      last_pv.length--;
      memmove( &(last_pv.a[1]), &(last_pv.a[2]),
	       last_pv.length * sizeof( unsigned int ) );
    }
  else {
    last_pv.a[0]    = 0;
    last_pv.a[1]    = 0;
    last_pv.depth   = 0;
    last_pv.length  = 0;
    last_root_value = 0;
  }

#if defined(DFPN_CLIENT)
  lock( &dfpn_client_lock );
  snprintf( (char *)dfpn_client_signature, DFPN_CLIENT_SIZE_SIGNATURE,
	    "%" PRIx64 "_%x_%x_%x", HASH_KEY, HAND_B, HAND_W, root_turn );
  dfpn_client_signature[DFPN_CLIENT_SIZE_SIGNATURE-1] = '\0';
  dfpn_client_rresult       = dfpn_client_na;
  dfpn_client_num_cresult   = 0;
  dfpn_client_flag_read     = 0;
  dfpn_client_out( "%s %s\n", str_CSA_move(move), dfpn_client_signature );
  unlock( &dfpn_client_lock );
#endif

  return 1;
}


int CONV unmake_move_root( tree_t * restrict ptree )
{
  unsigned int move;
  int i;

  if ( ptree->nrep == 0 || amove_save[NUM_UNMAKE-1] == MOVE_NA )
    {
      str_error = "no more undo infomation at root";
      return -1;
    }

  ptree->nsuc_check[1]    = ptree->nsuc_check[0];
  ptree->nsuc_check[0]    = ansuc_check_save[NUM_UNMAKE-1];
  ptree->save_material[1] = (short)amaterial_save[NUM_UNMAKE-1];
  move                    = amove_save[NUM_UNMAKE-1];
  last_root_value         = alast_root_value_save[NUM_UNMAKE-1];
  last_pv                 = alast_pv_save[NUM_UNMAKE-1];

  ptree->nrep -= 1;
  game_status &= ~( flag_drawn | flag_mated );
  root_turn    = Flip(root_turn);

  for ( i = NUM_UNMAKE-1; i > 0; i -= 1 )
    {
      amove_save[i]            = amove_save[i-1];
      amaterial_save[i]        = amaterial_save[i-1];
      ansuc_check_save[i]      = ansuc_check_save[i-1];
      alast_root_value_save[i] = alast_root_value_save[i-1];
      alast_pv_save[i]         = alast_pv_save[i-1];
    }
  amove_save[0]            = MOVE_NA;
  amaterial_save[0]        = 0;
  ansuc_check_save[0]      = 0;
  alast_root_value_save[0] = 0;
  alast_pv_save[0].a[0]    = 0;
  alast_pv_save[0].a[1]    = 0;
  alast_pv_save[0].depth   = 0;
  alast_pv_save[0].length  = 0;

  UnMakeMove( root_turn, move, 1 );

#if defined(DFPN_CLIENT)
  lock( &dfpn_client_lock );
  snprintf( (char *)dfpn_client_signature, DFPN_CLIENT_SIZE_SIGNATURE,
	    "%" PRIx64 "_%x_%x_%x", HASH_KEY, HAND_B, HAND_W, root_turn );
  dfpn_client_signature[DFPN_CLIENT_SIZE_SIGNATURE-1] = '\0';
  dfpn_client_rresult       = dfpn_client_na;
  dfpn_client_flag_read     = 0;
  dfpn_client_num_cresult   = 0;
  dfpn_client_out( "unmake\n" );
  unlock( &dfpn_client_lock );
#endif

  return 1;
}
