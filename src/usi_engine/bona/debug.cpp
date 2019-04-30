// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "shogi.h"

#if !defined(NDEBUG)

#  define DOut(str)                                                          \
  out_error( "invalid %s: node= %" PRIu64 "\n", str, ptree->node_searched ); \
  return 0;

#  define CheckNum( piece )                                      \
  if ( n ## piece ## _max - n ## piece ## _box != n ## piece ) { \
    DOut( "number of " # piece );                                \
  }

#  define CheckBoard( PIECE, piece )                           \
  bb = BB_B ## PIECE;                                          \
  while( BBTest( bb ) ) {                                      \
    sq = FirstOne( bb );                                       \
    Xor( sq, bb );                                             \
    if ( BOARD[sq] != piece ) { DOut( "BB_B" # PIECE  ); }     \
  }                                                            \
  bb = BB_W ## PIECE;                                          \
  while( BBTest( bb ) ) {                                      \
    sq = FirstOne( bb );                                       \
    Xor( sq, bb );                                             \
    if ( BOARD[sq] != -piece ) { DOut( "BB_W" # PIECE  ); }  \
  }

int
exam_bb( const tree_t *ptree )
{
  bitboard_t bb;
  uint64_t hk;
  int npawn, nlance, nknight, nsilver, ngold, nbishop, nrook, npiece;
  int sq, mate;

  /* leading zero-bits */
  if ( root_turn               & ~0x0000001U ) { DOut( "root_turn" ); }
  if ( HAND_B                  & ~0x01fffffU ) { DOut( "HAND_B" ); }
  if ( HAND_W                  & ~0x01fffffU ) { DOut( "HAND_W" ); }
  if ( BBToU(OCCUPIED_FILE)    & ~0x7ffffffU ) { DOut( "OCCUPIED_FILE" ); }
  if ( BBToU(OCCUPIED_DIAG2)   & ~0x7ffffffU ) { DOut( "OCCUPIED_DIAG2" ); }
  if ( BBToU(OCCUPIED_DIAG1)   & ~0x7ffffffU ) { DOut( "OCCUPIED_DIAG1" ); }

  if ( BBToU(BB_BOCCUPY)       & ~0x7ffffffU ) { DOut( "BB_BOCCUPY" ); }
  if ( BBToU(BB_BPAWN_ATK)     & ~0x7ffffffU ) { DOut( "BB_BPAWN_ATK" ); }
  if ( BBToU(BB_BTGOLD)        & ~0x7ffffffU ) { DOut( "BB_BTGOLD" ); }
  if ( BBToU(BB_B_HDK)         & ~0x7ffffffU ) { DOut( "BB_B_HDK" ); }
  if ( BBToU(BB_B_BH)          & ~0x7ffffffU ) { DOut( "BB_B_BH" ); }
  if ( BBToU(BB_B_RD)          & ~0x7ffffffU ) { DOut( "BB_B_RD" ); }
  if ( BBToU(BB_BPAWN)         & ~0x7ffffffU ) { DOut( "BB_BPAWN" ); }
  if ( BBToU(BB_BLANCE)        & ~0x7ffffffU ) { DOut( "BB_BLANCE" ); }
  if ( BBToU(BB_BKNIGHT)       & ~0x7ffffffU ) { DOut( "BB_BKNIGHT" ); }
  if ( BBToU(BB_BSILVER)       & ~0x7ffffffU ) { DOut( "BB_BSILVER" ); }
  if ( BBToU(BB_BGOLD)         & ~0x7ffffffU ) { DOut( "BB_BGOLD" ); }
  if ( BBToU(BB_BBISHOP)       & ~0x7ffffffU ) { DOut( "BB_BBISHOP" ); }
  if ( BBToU(BB_BROOK)         & ~0x7ffffffU ) { DOut( "BB_BROOK" ); }
  if ( BBToU(BB_BPRO_PAWN)     & ~0x7ffffffU ) { DOut( "BB_BPRO_PAWN" ); }
  if ( BBToU(BB_BPRO_LANCE)    & ~0x7ffffffU ) { DOut( "BB_BPRO_LANCE" ); }
  if ( BBToU(BB_BPRO_KNIGHT)   & ~0x7ffffffU ) { DOut( "BB_BPRO_KNIGHT" ); }
  if ( BBToU(BB_BPRO_SILVER)   & ~0x7ffffffU ) { DOut( "BB_BPRO_SILVER" ); }
  if ( BBToU(BB_BHORSE)        & ~0x7ffffffU ) { DOut( "BB_BHORSE" ); }
  if ( BBToU(BB_BDRAGON)       & ~0x7ffffffU ) { DOut( "BB_BDRAGON" ); }

  if ( BBToU(BB_WOCCUPY)       & ~0x7ffffffU ) { DOut( "BB_WOCCUPY" ); }
  if ( BBToU(BB_WPAWN_ATK)     & ~0x7ffffffU ) { DOut( "BB_WPAWN_ATK" ); }
  if ( BBToU(BB_WTGOLD)        & ~0x7ffffffU ) { DOut( "BB_WTGOLD" ); }
  if ( BBToU(BB_W_HDK)         & ~0x7ffffffU ) { DOut( "BB_W_HDK" ); }
  if ( BBToU(BB_W_BH)          & ~0x7ffffffU ) { DOut( "BB_W_BH" ); }
  if ( BBToU(BB_W_RD)          & ~0x7ffffffU ) { DOut( "BB_W_RD" ); }
  if ( BBToU(BB_WPAWN)         & ~0x7ffffffU ) { DOut( "BB_WPAWN" ); }
  if ( BBToU(BB_WLANCE)        & ~0x7ffffffU ) { DOut( "BB_WLANCE" ); }
  if ( BBToU(BB_WKNIGHT)       & ~0x7ffffffU ) { DOut( "BB_WKNIGHT" ); }
  if ( BBToU(BB_WSILVER)       & ~0x7ffffffU ) { DOut( "BB_WSILVER" ); }
  if ( BBToU(BB_WGOLD)         & ~0x7ffffffU ) { DOut( "BB_WGOLD" ); }
  if ( BBToU(BB_WBISHOP)       & ~0x7ffffffU ) { DOut( "BB_WBISHOP" ); }
  if ( BBToU(BB_WROOK)         & ~0x7ffffffU ) { DOut( "BB_WROOK" ); }
  if ( BBToU(BB_WPRO_PAWN)     & ~0x7ffffffU ) { DOut( "BB_WPRO_PAWN" ); }
  if ( BBToU(BB_WPRO_LANCE)    & ~0x7ffffffU ) { DOut( "BB_WPRO_LANCE" ); }
  if ( BBToU(BB_WPRO_KNIGHT)   & ~0x7ffffffU ) { DOut( "BB_WPRO_KNIGHT" ); }
  if ( BBToU(BB_WPRO_SILVER)   & ~0x7ffffffU ) { DOut( "BB_WPRO_SILVER" ); }
  if ( BBToU(BB_WHORSE)        & ~0x7ffffffU ) { DOut( "BB_WHORSE" ); }
  if ( BBToU(BB_WDRAGON)       & ~0x7ffffffU ) { DOut( "BB_WDRAGON" ); }

  if ( BB_BPAWN.p[0]           &  0x7fc0000U ) { DOut( "pawn at rank1" ); }
  if ( BB_BKNIGHT.p[0]         &  0x7fffe00U ) { DOut( "knight at rank1-2" ); }

  if ( BB_WPAWN.p[2]           &  0x00001ffU ) { DOut( "pawn at rank9" ); }
  if ( BB_WKNIGHT.p[2]         &  0x003ffffU ) { DOut( "knight at rank8-9" ); }


  /* number of pieces */
  BBOr( bb, BB_BPAWN, BB_WPAWN );
  BBOr( bb, BB_BPRO_PAWN, bb );
  BBOr( bb, BB_WPRO_PAWN, bb );
  npawn = I2HandPawn(HAND_B) + I2HandPawn(HAND_W) + PopuCount(bb);
  CheckNum( pawn );
    
  BBOr( bb, BB_BLANCE, BB_WLANCE );
  BBOr( bb, BB_BPRO_LANCE, bb );
  BBOr( bb, BB_WPRO_LANCE, bb );
  nlance = I2HandLance(HAND_B) + I2HandLance(HAND_W) + PopuCount(bb);
  CheckNum( lance );

  BBOr( bb, BB_BKNIGHT, BB_WKNIGHT );
  BBOr( bb, BB_BPRO_KNIGHT, bb );
  BBOr( bb, BB_WPRO_KNIGHT, bb );
  nknight = I2HandKnight(HAND_B) + I2HandKnight(HAND_W) + PopuCount(bb);
  CheckNum( knight );

  BBOr( bb, BB_BSILVER, BB_WSILVER );
  BBOr( bb, BB_BPRO_SILVER, bb );
  BBOr( bb, BB_WPRO_SILVER, bb );
  nsilver = I2HandSilver(HAND_B) + I2HandSilver(HAND_W) + PopuCount(bb);
  CheckNum( silver );

  BBOr( bb, BB_BGOLD, BB_WGOLD );
  ngold = I2HandGold(HAND_B) + I2HandGold(HAND_W) + PopuCount(bb);
  CheckNum( gold );

  BBOr( bb, BB_BBISHOP, BB_WBISHOP );
  BBOr( bb, bb, BB_BHORSE );
  BBOr( bb, bb, BB_WHORSE );
  nbishop = I2HandBishop(HAND_B) + I2HandBishop(HAND_W) + PopuCount(bb);
  CheckNum( bishop );

  BBOr( bb, BB_BROOK, BB_WROOK );
  BBOr( bb, bb, BB_BDRAGON );
  BBOr( bb, bb, BB_WDRAGON );
  nrook = I2HandRook(HAND_B) + I2HandRook(HAND_W) + PopuCount(bb);
  CheckNum( rook );


  /* consistency of redundant bitboards */
  BBOr( bb, BB_BGOLD, BB_BPRO_PAWN );
  BBOr( bb, bb, BB_BPRO_LANCE );
  BBOr( bb, bb, BB_BPRO_KNIGHT );
  BBOr( bb, bb, BB_BPRO_SILVER );
  if ( BBCmp( bb, BB_BTGOLD ) ) { DOut( "BB_BTGOLD" ); }

  BBOr( bb, BB_BBISHOP, BB_BHORSE );
  if ( BBCmp( bb, BB_B_BH ) ) { DOut( "BB_B_BH" ); }

  BBOr( bb, BB_BROOK, BB_BDRAGON );
  if ( BBCmp( bb, BB_B_RD ) ) { DOut( "BB_B_RD" ); }

  BBOr( bb, BB_BHORSE, BB_BDRAGON );
  BBOr( bb, BB_BKING, bb );
  if ( BBCmp( bb, BB_B_HDK ) ) { DOut( "BB_B_HDK" ); }

  bb.p[0]  = ( BB_BPAWN.p[0] <<  9 ) & 0x7ffffffU;
  bb.p[0] |= ( BB_BPAWN.p[1] >> 18 ) & 0x00001ffU;
  bb.p[1]  = ( BB_BPAWN.p[1] <<  9 ) & 0x7ffffffU;
  bb.p[1] |= ( BB_BPAWN.p[2] >> 18 ) & 0x00001ffU;
  bb.p[2]  = ( BB_BPAWN.p[2] <<  9 ) & 0x7ffffffU;
  if ( BBCmp( bb, BB_BPAWN_ATK ) ) { DOut( "BB_BPAWN_ATK" ); }

  BBOr( bb, BB_BPAWN, BB_BLANCE );
  BBOr( bb, bb, BB_BKNIGHT );
  BBOr( bb, bb, BB_BSILVER );
  BBOr( bb, bb, BB_BTGOLD );
  BBOr( bb, bb, BB_BBISHOP );
  BBOr( bb, bb, BB_BROOK );
  BBOr( bb, bb, BB_B_HDK );
  if ( BBCmp( bb, BB_BOCCUPY ) ) { DOut( "BB_BOCCUPY" ); }
      
  BBOr( bb, BB_WPRO_PAWN, BB_WGOLD );
  BBOr( bb, BB_WPRO_LANCE, bb );
  BBOr( bb, BB_WPRO_KNIGHT, bb );
  BBOr( bb, BB_WPRO_SILVER, bb );
  if ( BBCmp( bb, BB_WTGOLD ) ) { DOut( "BB_WTGOLD" ); }

  BBOr( bb, BB_WBISHOP, BB_WHORSE );
  if ( BBCmp( bb, BB_W_BH ) ) { DOut( "BB_W_BH" ); }

  BBOr( bb, BB_WROOK, BB_WDRAGON );
  if ( BBCmp( bb, BB_W_RD ) ) { DOut( "BB_W_RD" ); }

  BBOr( bb, BB_WHORSE, BB_WDRAGON );
  BBOr( bb, BB_WKING, bb );
  if ( BBCmp( bb, BB_W_HDK ) ) { DOut( "BB_W_HDK" ); }

  bb.p[2]  = ( BB_WPAWN.p[2] >>  9 );
  bb.p[2] |= ( BB_WPAWN.p[1] << 18 ) & 0x7fc0000U;
  bb.p[1]  = ( BB_WPAWN.p[1] >>  9 );
  bb.p[1] |= ( BB_WPAWN.p[0] << 18 ) & 0x7fc0000U;
  bb.p[0]  = ( BB_WPAWN.p[0] >>  9 );
  if ( BBCmp( bb, BB_WPAWN_ATK ) ) { DOut( "BB_WPAWN_ATK" ); }

  BBOr( bb, BB_WPAWN, BB_WLANCE );
  BBOr( bb, BB_WKNIGHT, bb );
  BBOr( bb, BB_WSILVER, bb );
  BBOr( bb, BB_WTGOLD, bb );
  BBOr( bb, BB_WBISHOP, bb );
  BBOr( bb, BB_WROOK, bb );
  BBOr( bb, BB_W_HDK, bb );
  if ( BBCmp( bb, BB_WOCCUPY ) ) { DOut( "BB_WOCCUPY" ); }
      
  /* consistency of board-array */
  CheckBoard( PAWN,        pawn );
  CheckBoard( LANCE,       lance );
  CheckBoard( KNIGHT,      knight );
  CheckBoard( SILVER,      silver );
  CheckBoard( GOLD,        gold );
  CheckBoard( BISHOP,      bishop );
  CheckBoard( ROOK,        rook );
  CheckBoard( KING,        king );
  CheckBoard( PRO_PAWN,    pro_pawn );
  CheckBoard( PRO_LANCE,   pro_lance );
  CheckBoard( PRO_KNIGHT,  pro_knight );
  CheckBoard( PRO_SILVER,  pro_silver );
  CheckBoard( HORSE,       horse );
  CheckBoard( DRAGON,      dragon );

  for ( sq = npiece = 0; sq < nsquare; sq++ )
    {
      if ( BOARD[sq] ) { npiece++; }
    }
  if ( npiece != PopuCount( OCCUPIED_FILE ) )  { DOut( "OCCUPIED_FILE" ); }
  if ( npiece != PopuCount( OCCUPIED_DIAG2 ) ) { DOut( "OCCUPIED_DIAG2" ); }
  if ( npiece != PopuCount( OCCUPIED_DIAG1 ) ) { DOut( "OCCUPIED_DIAG1" ); }


  /* Material and Hash signature */
  mate = eval_material( ptree );
  if ( mate != MATERIAL ) { DOut( "value of material" ); }

  hk = hash_func( ptree );
  if ( hk != HASH_KEY  ) { DOut( "hash signature" ); }

  if ( BOARD[SQ_BKING] !=  king ) { DOut( "SQ_BKING" ); }
  if ( BOARD[SQ_WKING] != -king ) { DOut( "SQ_WKING" ); }

  return 1;
}

#  undef DOut
#  undef CheckNum
#  undef CheckBoard

#endif  /* no NDEBUG */
