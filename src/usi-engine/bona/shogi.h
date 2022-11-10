// 2019 Team AobaZero
// This source code is in the public domain.
#ifndef SHOGI_H
#define SHOGI_H

#include <stdio.h>
#include "bitop.h"
#include "param.h"
#include <string>

#if defined(_WIN32)

#  include <Winsock2.h>
#  define CONV              __fastcall
#  define SCKT_NULL         INVALID_SOCKET
typedef SOCKET sckt_t;

#else

#  include <pthread.h>
#  include <sys/times.h>
#  define CONV
#  define SCKT_NULL         -1
#  define SOCKET_ERROR      -1
typedef int sckt_t;

#endif

/* Microsoft C/C++ on x86 and x86-64 */
#if defined(_MSC_VER)

#  define _CRT_DISABLE_PERFCRIT_LOCKS
#ifndef UINT64_MAX
#  define UINT64_MAX    ULLONG_MAX
#endif
#  define PRIu64        "I64u"
#  define PRIx64        "I64x"
#ifndef UINT64_C
#  define UINT64_C(u)  ( u )
#endif

#  define restrict      __restrict
#  define strtok_r      strtok_s
#  define read          _read
#  define strncpy( dst, src, len ) strncpy_s( dst, len, src, _TRUNCATE )
#  define snprintf( buf, size, fmt, ... )   \
          _snprintf_s( buf, size, _TRUNCATE, fmt, __VA_ARGS__ )
#  define vsnprintf( buf, size, fmt, list ) \
          _vsnprintf_s( buf, size, _TRUNCATE, fmt, list )
typedef unsigned __int64 uint64_t;
typedef volatile long lock_t;

/* GNU C and Intel C/C++ on x86 and x86-64 */
#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )

#  include <inttypes.h>
#  define restrict __restrict
typedef volatile int lock_t;

/* other targets. */
#else

#  include <inttypes.h>
typedef struct { unsigned int p[3]; } bitboard_t;
typedef pthread_mutex_t lock_t;
extern unsigned char aifirst_one[512];
extern unsigned char ailast_one[512];

#endif

#define BK_TINY
/*
  #define BK_SMALL
  #define BK_ULTRA_NARROW
  #define BK_COM
  #define NO_STDOUT
  #define DBG_EASY
*/

//#if defined(CSASHOGI)
#if defined(CSASHOGI) || (defined(_WIN32) && defined(YSS_ZERO))
#  define NO_STDOUT
#  if ! defined(WIN32_PIPE)
#    define WIN32_PIPE
#  endif
#endif

#if defined(TLP)
#  define SHARE volatile
#  define SEARCH_ABORT ( root_abort || ptree->tlp_abort )
#else
#  define SHARE
#  define SEARCH_ABORT root_abort
#endif

#define NUM_UNMAKE              2
#define QUIES_PLY_LIMIT         7
#define SHELL_H_LEN             7
#define MAX_ANSWER              8
#define PLY_INC                 8
#define PLY_MAX                 128
#define RAND_N                  624
#define TC_NMOVE                35U
#define SEC_MARGIN              15U
#define SEC_KEEP_ALIVE          180U
#define TIME_RESPONSE           200U
#define RESIGN_THRESHOLD       ( ( MT_CAP_DRAGON * 5 ) /  8 )
//#define BNZ_VER                 "0.1"
//#define BNZ_VER                 "1"	// 20190311
//#define BNZ_VER                 "2"	// 20190322
//#define BNZ_VER                 "3"	// 20190323
//#define BNZ_VER                 "4"	// 20190323
//#define BNZ_VER                 "5"	// 20190324
//#define BNZ_VER                 "6"	// 20190419
//#define BNZ_VER                 "7"	// 20190420
//#define BNZ_VER                 "8"	// 20190430
//#define BNZ_VER                 "9"	// 20190527
//#define BNZ_VER                 "10"	// 20190708
//#define BNZ_VER                 "11"	// 20190709
//#define BNZ_VER                 "12"	// 20201013
//#define BNZ_VER                 "13"	// 20201023 resign 10%
//#define BNZ_VER                 "14"	// 20201108 declare win bug fix. fAutoResign
//#define BNZ_VER                 "15"	// 20201207 sente 1 mate bug fix
//#define BNZ_VER                 "16"	// 20210930 kldgain, visit Limit, name
//#define BNZ_VER                 "17"	// 20220108 kldgain
//#define BNZ_VER                 "18"	// 20220110 kldgain 0.0000013, -p 3200 (autousiで指定)
//#define BNZ_VER                 "27"	// 20220224 Convert from AobaKomaochi v26. Swish, mate3 kyo promote bug fix. balanced_opening, Network V3
//#define BNZ_VER                 "28"	// 20220225 balanced_opening ply<=2 always use value
//#define BNZ_VER                 "29"	// 20220406 policy softmax temperature 1.0 -> 1.8
//#define BNZ_VER                 "30"	// 20220406 kldgain 0.000006, balanced_opening within +150 ELO
//#define BNZ_VER                 "31"	// 20220407 cancel balanced_opening. resign ok under 30 moves in autousi.
//#define BNZ_VER                 "32"	// 20220418 initial winrate is adjusted(aka, first play urgency, fpu), +20 ELO. dfpn for all node visits >= 10, +40 ELO.
//#define BNZ_VER                 "33"	// 20220429 perpetual check is illegal with 3 times(bug fixed).
//#define BNZ_VER                 "34"	// 20220429 dfpn time limit stop.
//#define BNZ_VER                 "35"	// 20220430 perpetual check bug fixed(again).
//#define BNZ_VER                 "36"	// 20220626 pawn ,rook, bishop are always promoted. discovered attack moves have 30% of best policy. safe LCB, kldgain 0.000005.
//#define BNZ_VER                 "37"	// 20220626 kldgain 000000075. ave playouts is 1568/move.
#define BNZ_VER                 "38"	// 20221110 test get_best_move_alphabeta_usi().
#define BNZ_NAME                "AobaZero"

//#define BNZ_VER                 "16"	// 20210528 komaochi, mate3
//#define BNZ_VER                 "17"	// 20210607 bug fix. does not tend to move 1 ply mate.
//#define BNZ_VER                 "18"	// 20210608 OpenCL selfcheck off temporarily
//#define BNZ_VER                 "19"	// 20210622 Windows binary
//#define BNZ_VER                 "20"	// 20210628 softmax temperature > 1.0 is adjusted, even if moves <= 30.
//#define BNZ_VER                 "21"	// 20210919 LCB, kld_gain, reset_root_visit
//#define BNZ_VER                 "22"	// 20210920 fDiffRootVisit
//#define BNZ_VER                 "23"	// 20210920 no change. autousi uses kldgain.
//#define BNZ_VER                 "24"	// 20210922 sum_games bug fix.
//#define BNZ_VER                 "25"	// 20211102 mtemp 1.3 (autousi)
//#define BNZ_VER                 "26"	// 20211204 final with weight w1250. -name
//#define BNZ_NAME                "AobaKomaochi"


#define REP_MAX_PLY             32
#if defined(YSS_ZERO)
#define REP_HIST_LEN            (1024+PLY_MAX)
#else
#define REP_HIST_LEN            256
#endif

#define EHASH_MASK              0x3fffffU      /* occupies 32MB */
#define MATE3_MASK              0x07ffffU      /* occupies 4MB */

#define HIST_SIZE               0x4000U
#define HIST_INVALID            0xffffU
#define HIST_MAX                0x8000U

#define REJEC_MASK              0x0ffffU
#define REJEC_MIN_DEPTH        ( ( PLY_INC  * 5 ) )

#define EXT_RECAP1             ( ( PLY_INC  * 1 ) /  4 )
#define EXT_RECAP2             ( ( PLY_INC  * 2 ) /  4 )
#define EXT_ONEREP             ( ( PLY_INC  * 2 ) /  4 )
#define EXT_CHECK              ( ( PLY_INC  * 4 ) /  4 )

#define EFUTIL_MG1             ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define EFUTIL_MG2             ( ( MT_CAP_DRAGON * 2 ) /  8 )

#define FMG_MG                 ( ( MT_CAP_DRAGON * 2 ) / 16 )
#define FMG_MG_KING            ( ( MT_CAP_DRAGON * 3 ) / 16 )
#define FMG_MG_MT              ( ( MT_CAP_DRAGON * 8 ) / 16 )
#define FMG_MISC               ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define FMG_CAP                ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define FMG_DROP               ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define FMG_MT                 ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define FMG_MISC_KING          ( ( MT_CAP_DRAGON * 2 ) /  8 )
#define FMG_CAP_KING           ( ( MT_CAP_DRAGON * 2 ) /  8 )

#define FV_WINDOW               256
#define FV_SCALE                32
#define FV_PENALTY             ( 0.2 / (double)FV_SCALE )

#define MPV_MAX_PV              16

#define TLP_MAX_THREADS         12
#define TLP_NUM_WORK           ( TLP_MAX_THREADS * 8 )

#define TIME_CHECK_MIN_NODE     10000U
#define TIME_CHECK_MAX_NODE     100000U

#define SIZE_FILENAME           256
#define SIZE_PLAYERNAME         256
#define SIZE_MESSAGE            512
#define SIZE_CSALINE            512

#if defined(USI)
#  define SIZE_CMDLINE          ( 1024 * 16 )
#  define SIZE_CMDBUFFER        ( 1024 * 16 )
#else
#  define SIZE_CMDLINE          512
#  define SIZE_CMDBUFFER        512
#endif

#define IsMove(move)           ( (move) & 0xffffffU )
#define MOVE_NA                 0x00000000U
#define MOVE_PASS               0x01000000U
#define MOVE_PONDER_FAILED      0xfe000000U
#define MOVE_RESIGN             0xff000000U
#define MOVE_CHK_SET            0x80000000U
#define MOVE_CHK_CLEAR          0x40000000U

#define MAX_LEGAL_MOVES         700
#define MAX_LEGAL_EVASION       256
#define MOVE_LIST_LEN           16384

#define MAX_SIZE_SECTION        0xffff
#define NUM_SECTION             0x4000

#define MATERIAL            (ptree->posi.material)
#define HAND_B              (ptree->posi.hand_black)
#define HAND_W              (ptree->posi.hand_white)

#define BB_BOCCUPY          (ptree->posi.b_occupied)
#define BB_BTGOLD           (ptree->posi.b_tgold)
#define BB_B_HDK            (ptree->posi.b_hdk)
#define BB_B_BH             (ptree->posi.b_bh)
#define BB_B_RD             (ptree->posi.b_rd)
#define BB_BPAWN_ATK        (ptree->posi.b_pawn_attacks)
#define BB_BPAWN            (ptree->posi.b_pawn)
#define BB_BLANCE           (ptree->posi.b_lance)
#define BB_BKNIGHT          (ptree->posi.b_knight)
#define BB_BSILVER          (ptree->posi.b_silver)
#define BB_BGOLD            (ptree->posi.b_gold)
#define BB_BBISHOP          (ptree->posi.b_bishop)
#define BB_BROOK            (ptree->posi.b_rook)
#define BB_BKING            (abb_mask[SQ_BKING])
#define BB_BPRO_PAWN        (ptree->posi.b_pro_pawn)
#define BB_BPRO_LANCE       (ptree->posi.b_pro_lance)
#define BB_BPRO_KNIGHT      (ptree->posi.b_pro_knight)
#define BB_BPRO_SILVER      (ptree->posi.b_pro_silver)
#define BB_BHORSE           (ptree->posi.b_horse)
#define BB_BDRAGON          (ptree->posi.b_dragon)

#define BB_WOCCUPY          (ptree->posi.w_occupied)
#define BB_WTGOLD           (ptree->posi.w_tgold)
#define BB_W_HDK            (ptree->posi.w_hdk)
#define BB_W_BH             (ptree->posi.w_bh)
#define BB_W_RD             (ptree->posi.w_rd)
#define BB_WPAWN_ATK        (ptree->posi.w_pawn_attacks)
#define BB_WPAWN            (ptree->posi.w_pawn)
#define BB_WLANCE           (ptree->posi.w_lance)
#define BB_WKNIGHT          (ptree->posi.w_knight)
#define BB_WSILVER          (ptree->posi.w_silver)
#define BB_WGOLD            (ptree->posi.w_gold)
#define BB_WBISHOP          (ptree->posi.w_bishop)
#define BB_WROOK            (ptree->posi.w_rook)
#define BB_WKING            (abb_mask[SQ_WKING])
#define BB_WPRO_PAWN        (ptree->posi.w_pro_pawn)
#define BB_WPRO_LANCE       (ptree->posi.w_pro_lance)
#define BB_WPRO_KNIGHT      (ptree->posi.w_pro_knight)
#define BB_WPRO_SILVER      (ptree->posi.w_pro_silver)
#define BB_WHORSE           (ptree->posi.w_horse)
#define BB_WDRAGON          (ptree->posi.w_dragon)

#define OCCUPIED_FILE       (ptree->posi.occupied_rl90)
#define OCCUPIED_DIAG1      (ptree->posi.occupied_rr45)
#define OCCUPIED_DIAG2      (ptree->posi.occupied_rl45)
#define BOARD               (ptree->posi.asquare)

#define SQ_BKING            (ptree->posi.isquare_b_king)
#define SQ_WKING            (ptree->posi.isquare_w_king)

#define HASH_KEY            (ptree->posi.hash_key)
#define HASH_VALUE          (ptree->sort_value[0])
#define MOVE_CURR           (ptree->current_move[ply])
#define MOVE_LAST           (ptree->current_move[ply-1])

#define NullDepth(d) ( (d) <  PLY_INC*26/4 ? (d)-PLY_INC*12/4 :              \
                     ( (d) <= PLY_INC*30/4 ? PLY_INC*14/4                    \
                                           : (d)-PLY_INC*16/4) )

#define RecursionThreshold  ( PLY_INC * 3 )

#define RecursionDepth(d) ( (d) < PLY_INC*18/4 ? PLY_INC*6/4                 \
                                               : (d)-PLY_INC*12/4 )

#define LimitExtension(e,ply) if ( (e) && (ply) > 2 * iteration_depth ) {     \
                                if ( (ply) < 4 * iteration_depth ) {          \
                                  e *= 4 * iteration_depth - (ply);           \
                                  e /= 2 * iteration_depth;                   \
                                } else { e = 0; } }

#define Flip(turn)          ((turn)^1)
#define Inv(sq)             (nsquare-1-sq)
#define PcOnSq(k,i)         pc_on_sq[k][(i)*((i)+3)/2]
#define PcPcOnSq(k,i,j)     pc_on_sq[k][(i)*((i)+1)/2+(j)]

/*
  xxxxxxxx xxxxxxxx xxx11111  pawn
  xxxxxxxx xxxxxxxx 111xxxxx  lance
  xxxxxxxx xxxxx111 xxxxxxxx  knight
  xxxxxxxx xx111xxx xxxxxxxx  silver
  xxxxxxx1 11xxxxxx xxxxxxxx  gold
  xxxxx11x xxxxxxxx xxxxxxxx  bishop
  xxx11xxx xxxxxxxx xxxxxxxx  rook
 */
#define I2HandPawn(hand)       (((hand) >>  0) & 0x1f)
#define I2HandLance(hand)      (((hand) >>  5) & 0x07)
#define I2HandKnight(hand)     (((hand) >>  8) & 0x07)
#define I2HandSilver(hand)     (((hand) >> 11) & 0x07)
#define I2HandGold(hand)       (((hand) >> 14) & 0x07)
#define I2HandBishop(hand)     (((hand) >> 17) & 0x03)
#define I2HandRook(hand)        ((hand) >> 19)
#define IsHandPawn(hand)       ((hand) & 0x000001f)
#define IsHandLance(hand)      ((hand) & 0x00000e0)
#define IsHandKnight(hand)     ((hand) & 0x0000700)
#define IsHandSilver(hand)     ((hand) & 0x0003800)
#define IsHandGold(hand)       ((hand) & 0x001c000)
#define IsHandBishop(hand)     ((hand) & 0x0060000)
#define IsHandRook(hand)       ((hand) & 0x0180000)
#define IsHandSGBR(hand)       ((hand) & 0x01ff800)
/*
  xxxxxxxx xxxxxxxx x1111111  destination
  xxxxxxxx xx111111 1xxxxxxx  starting square or drop piece+nsquare-1
  xxxxxxxx x1xxxxxx xxxxxxxx  flag for promotion
  xxxxx111 1xxxxxxx xxxxxxxx  piece to move
  x1111xxx xxxxxxxx xxxxxxxx  captured piece
 */
#define To2Move(to)             ((unsigned int)(to)   <<  0)
#define From2Move(from)         ((unsigned int)(from) <<  7)
#define Drop2Move(piece)        ((nsquare-1+(piece))  <<  7)
#define Drop2From(piece)         (nsquare-1+(piece))
#define FLAG_PROMO               (1U                  << 14)
#define Piece2Move(piece)       ((piece)              << 15)
#define Cap2Move(piece)         ((piece)              << 19)
#define I2To(move)              (((move) >>  0) & 0x007fU)
#define I2From(move)            (((move) >>  7) & 0x007fU)
#define I2FromTo(move)          (((move) >>  0) & 0x3fffU)
#define I2IsPromote(move)       ((move) & FLAG_PROMO)
#define I2PieceMove(move)       (((move) >> 15) & 0x000fU)
#define UToFromToPromo(u)       ( (u) & 0x7ffffU )
#define UToCap(u)               (((u)    >> 19) & 0x000fU)
#define From2Drop(from)         ((from)-nsquare+1)


#define AttackFile(i)  (abb_file_attacks[i]                               \
                         [((ptree->posi.occupied_rl90.p[aslide[i].irl90]) \
                            >> aslide[i].srl90) & 0x7f])

#define AttackRank(i)  (abb_rank_attacks[i]                               \
                         [((ptree->posi.b_occupied.p[aslide[i].ir0]       \
                            |ptree->posi.w_occupied.p[aslide[i].ir0])     \
                             >> aslide[i].sr0) & 0x7f ])

#define AttackDiag1(i)                                         \
          (abb_bishop_attacks_rr45[i]                        \
            [((ptree->posi.occupied_rr45.p[aslide[i].irr45]) \
               >> aslide[i].srr45) & 0x7f])

#define AttackDiag2(i)                                         \
          (abb_bishop_attacks_rl45[i]                        \
            [((ptree->posi.occupied_rl45.p[aslide[i].irl45]) \
               >> aslide[i].srl45) & 0x7f])

#define BishopAttack0(i) ( AttackDiag1(i).p[0] | AttackDiag2(i).p[0] )
#define BishopAttack1(i) ( AttackDiag1(i).p[1] | AttackDiag2(i).p[1] )
#define BishopAttack2(i) ( AttackDiag1(i).p[2] | AttackDiag2(i).p[2] )
#define AttackBLance(bb,i) BBAnd( bb, abb_minus_rays[i], AttackFile(i) )
#define AttackWLance(bb,i) BBAnd( bb, abb_plus_rays[i],  AttackFile(i) )
#define AttackBishop(bb,i) BBOr( bb, AttackDiag1(i), AttackDiag2(i) )
#define AttackRook(bb,i)   BBOr( bb, AttackFile(i), AttackRank(i) )
#define AttackHorse(bb,i)  AttackBishop(bb,i); BBOr(bb,bb,abb_king_attacks[i])
#define AttackDragon(bb,i) AttackRook(bb,i);   BBOr(bb,bb,abb_king_attacks[i])

#define InCheck(turn)                                        \
         ( (turn) ? is_white_attacked( ptree, SQ_WKING )     \
                  : is_black_attacked( ptree, SQ_BKING ) )

#define MakeMove(turn,move,ply)                                \
                ( (turn) ? make_move_w( ptree, move, ply ) \
                         : make_move_b( ptree, move, ply ) )

#define UnMakeMove(turn,move,ply)                                \
                ( (turn) ? unmake_move_w( ptree, move, ply ) \
                         : unmake_move_b( ptree, move, ply ) )

#define IsMoveCheck( ptree, turn, move )                        \
                ( (turn) ? is_move_check_w( ptree, move )   \
                         : is_move_check_b( ptree, move ) )

#define GenCaptures(turn,pmove) ( (turn) ? w_gen_captures( ptree, pmove )   \
                                         : b_gen_captures( ptree, pmove ) )

#define GenNoCaptures(turn,pmove)                                             \
                               ( (turn) ? w_gen_nocaptures( ptree, pmove )  \
                                        : b_gen_nocaptures( ptree, pmove ) )

#define GenDrop(turn,pmove)     ( (turn) ? w_gen_drop( ptree, pmove )       \
                                         : b_gen_drop( ptree, pmove ) )

#define GenCapNoProEx2(turn,pmove)                                 \
                ( (turn) ? w_gen_cap_nopro_ex2( ptree, pmove )   \
                         : b_gen_cap_nopro_ex2( ptree, pmove ) )

#define GenNoCapNoProEx2(turn,pmove)                                \
                ( (turn) ? w_gen_nocap_nopro_ex2( ptree, pmove )  \
                         : b_gen_nocap_nopro_ex2( ptree, pmove ) )

#define GenEvasion(turn,pmove)                                  \
                ( (turn) ? w_gen_evasion( ptree, pmove )      \
                         : b_gen_evasion( ptree, pmove ) )

#define GenCheck(turn,pmove)                                  \
                ( (turn) ? w_gen_checks( ptree, pmove )      \
                         : b_gen_checks( ptree, pmove ) )

#define IsMateIn1Ply(turn)                                    \
                ( (turn) ? is_w_mate_in_1ply( ptree )         \
                         : is_b_mate_in_1ply( ptree ) )

#define IsDiscoverBK(from,to)                                  \
          idirec = (int)adirec[SQ_BKING][from],               \
          ( idirec && ( idirec!=(int)adirec[SQ_BKING][to] )   \
            && is_pinned_on_black_king( ptree, from, idirec ) )

#define IsDiscoverWK(from,to)                                  \
          idirec = (int)adirec[SQ_WKING][from],               \
          ( idirec && ( idirec!=(int)adirec[SQ_WKING][to] )   \
            && is_pinned_on_white_king( ptree, from, idirec ) )
#define IsMateWPawnDrop(ptree,to) ( BOARD[(to)+9] == king                 \
                                     && is_mate_w_pawn_drop( ptree, to ) )

#define IsMateBPawnDrop(ptree,to) ( BOARD[(to)-9] == -king                \
                                     && is_mate_b_pawn_drop( ptree, to ) )

enum { b0000, b0001, b0010, b0011, b0100, b0101, b0110, b0111,
       b1000, b1001, b1010, b1011, b1100, b1101, b1110, b1111 };

enum { A9 = 0, B9, C9, D9, E9, F9, G9, H9, I9,
           A8, B8, C8, D8, E8, F8, G8, H8, I8,
           A7, B7, C7, D7, E7, F7, G7, H7, I7,
           A6, B6, C6, D6, E6, F6, G6, H6, I6,
           A5, B5, C5, D5, E5, F5, G5, H5, I5,
           A4, B4, C4, D4, E4, F4, G4, H4, I4,
           A3, B3, C3, D3, E3, F3, G3, H3, I3,
           A2, B2, C2, D2, E2, F2, G2, H2, I2,
           A1, B1, C1, D1, E1, F1, G1, H1, I1 };

enum { promote = 8, empty = 0,
       pawn, lance, knight, silver, gold, bishop, rook, king, pro_pawn,
       pro_lance, pro_knight, pro_silver, piece_null, horse, dragon };

enum { npawn_max = 18,  nlance_max  = 4,  nknight_max = 4,  nsilver_max = 4,
       ngold_max = 4,   nbishop_max = 2,  nrook_max   = 2,  nking_max   = 2 };

enum { rank1 = 0, rank2, rank3, rank4, rank5, rank6, rank7, rank8, rank9 };
enum { file1 = 0, file2, file3, file4, file5, file6, file7, file8, file9 };

enum { nhand = 7, nfile = 9,  nrank = 9,  nsquare = 81 };

enum { mask_file1 = (( 1U << 18 | 1U << 9 | 1U ) << 8) };

enum { flag_diag1 = b0001, flag_plus = b0010 };

enum { score_draw     =     1,
       score_max_eval = 30000,
       score_matelong = 30002,
       score_mate1ply = 32598,
       score_inferior = 32599,
       score_bound    = 32600,
       score_foul     = 32600 };

enum { phase_hash      = b0001,
       phase_killer1   = b0001 << 1,
       phase_killer2   = b0010 << 1,
       phase_killer    = b0011 << 1,
       phase_cap1      = b0001 << 3,
       phase_cap_misc  = b0010 << 3,
       phase_cap       = b0011 << 3,
       phase_history1  = b0001 << 5,
       phase_history2  = b0010 << 5,
       phase_history   = b0011 << 5,
       phase_misc      = b0100 << 5 };

enum { next_move_hash = 0,  next_move_capture,   next_move_history2,
       next_move_misc };

/* next_evasion_hash should be the same as next_move_hash */
enum { next_evasion_hash = 0, next_evasion_genall, next_evasion_misc };


enum { next_quies_gencap, next_quies_captures, next_quies_misc };

enum { no_rep = 0, four_fold_rep, perpetual_check, perpetual_check2,
       black_superi_rep, white_superi_rep, hash_hit, prev_solution, book_hit,
       pv_fail_high, mate_search };

enum { record_misc, record_eof, record_next, record_resign, record_drawn,
       record_error };

enum { black = 0, white = 1 };	// black(sente), white(gote).

enum { direc_misc           = b0000,
       direc_file           = b0010, /* | */
       direc_rank           = b0011, /* - */
       direc_diag1          = b0100, /* / */
       direc_diag2          = b0101, /* \ */
       flag_cross           = b0010,
       flag_diag            = b0100 };

enum { value_null           = b0000,
       value_upper          = b0001,
       value_lower          = b0010,
       value_exact          = b0011,
       flag_value_up_exact  = b0001,
       flag_value_low_exact = b0010,
       node_do_null         = b0100,
       node_do_recap        = b1000,
       node_do_mate         = b0001 << 4,
       node_mate_threat     = b0010 << 4, /* <- don't change it */ 
       node_do_futile       = b0100 << 4,
       node_do_recursion    = b1000 << 4,
       node_do_hashcut      = b0001 << 8,
       state_node_end };
/* note: maximum bits are 8.  tlp_state_node uses type unsigned char. */

enum { flag_from_ponder     = b0001 };

enum { flag_time            = b0001,
       flag_history         = b0010,
       flag_rep             = b0100,
       flag_detect_hang     = b1000,
       flag_nomake_move     = b0010 << 4,
       flag_nofmargin       = b0100 << 4 };

/* flags represent status of root move */
enum { flag_searched        = b0001,
       flag_first           = b0010 };


enum { flag_mated           = b0001,
       flag_resigned        = b0010,
       flag_drawn           = b0100,
       flag_suspend         = b1000,
       mask_game_end        = b1111,
       flag_quit            = b0001 << 4,
       flag_puzzling        = b0010 << 4,
       flag_pondering       = b0100 << 4,
       flag_thinking        = b1000 << 4,
       flag_problem         = b0001 << 8,
       flag_move_now        = b0010 << 8,
       flag_quit_ponder     = b0100 << 8,
       flag_nostdout        = b1000 << 8,
       flag_search_error    = b0001 << 12,
       flag_nonewlog        = b0010 << 12,
       flag_reverse         = b0100 << 12,
       flag_narrow_book     = b1000 << 12,
       flag_time_extendable = b0001 << 16,
       flag_learning        = b0010 << 16,
       flag_nobeep          = b0100 << 16,
       flag_nostress        = b1000 << 16,
       flag_nopeek          = b0001 << 20,
       flag_noponder        = b0010 << 20,
       flag_noprompt        = b0100 << 20,
       flag_sendpv          = b1000 << 20,
       flag_skip_root_move  = b0001 << 24 };


enum { flag_hand_pawn       = 1 <<  0,
       flag_hand_lance      = 1 <<  5,
       flag_hand_knight     = 1 <<  8,
       flag_hand_silver     = 1 << 11,
       flag_hand_gold       = 1 << 14,
       flag_hand_bishop     = 1 << 17,
       flag_hand_rook       = 1 << 19 };

enum { f_hand_pawn   =    0,
       e_hand_pawn   =   19,
       f_hand_lance  =   38,
       e_hand_lance  =   43,
       f_hand_knight =   48,
       e_hand_knight =   53,
       f_hand_silver =   58,
       e_hand_silver =   63,
       f_hand_gold   =   68,
       e_hand_gold   =   73,
       f_hand_bishop =   78,
       e_hand_bishop =   81,
       f_hand_rook   =   84,
       e_hand_rook   =   87,
       fe_hand_end   =   90,
       f_pawn        =   81,
       e_pawn        =  162,
       f_lance       =  225,
       e_lance       =  306,
       f_knight      =  360,
       e_knight      =  441,
       f_silver      =  504,
       e_silver      =  585,
       f_gold        =  666,
       e_gold        =  747,
       f_bishop      =  828,
       e_bishop      =  909,
       f_horse       =  990,
       e_horse       = 1071,
       f_rook        = 1152,
       e_rook        = 1233,
       f_dragon      = 1314,
       e_dragon      = 1395,
       fe_end        = 1476,

       kkp_hand_pawn   =   0,
       kkp_hand_lance  =  19,
       kkp_hand_knight =  24,
       kkp_hand_silver =  29,
       kkp_hand_gold   =  34,
       kkp_hand_bishop =  39,
       kkp_hand_rook   =  42,
       kkp_hand_end    =  45,
       kkp_pawn        =  36,
       kkp_lance       = 108,
       kkp_knight      = 171,
       kkp_silver      = 252,
       kkp_gold        = 333,
       kkp_bishop      = 414,
       kkp_horse       = 495,
       kkp_rook        = 576,
       kkp_dragon      = 657,
       kkp_end         = 738 };

enum { pos_n = fe_end * ( fe_end + 1 ) / 2 };

typedef struct { bitboard_t gold, silver, knight, lance; } check_table_t;

#if ! defined(MINIMUM)
typedef struct { fpos_t fpos;  unsigned int games, moves, lines; } rpos_t;
typedef struct {
  double pawn, lance, knight, silver, gold, bishop, rook;
  double pro_pawn, pro_lance, pro_knight, pro_silver, horse, dragon;
  float pc_on_sq[nsquare][fe_end*(fe_end+1)/2];
  float kkp[nsquare][nsquare][kkp_end];
} param_t;
#endif

typedef enum { mode_write, mode_read_write, mode_read } record_mode_t;

typedef struct { uint64_t word1, word2; }                        trans_entry_t;
typedef struct { trans_entry_t prefer, always[2]; }              trans_table_t;
typedef struct { int count;  unsigned int cnst[2], vec[RAND_N]; }rand_work_t;

typedef struct {
  int no1_value, no2_value;
  unsigned int no1, no2;
} move_killer_t;

typedef struct { unsigned int no1, no2; } killer_t;

typedef struct {
  union { char str_move[ MAX_ANSWER ][ 8 ]; } info;
  char str_name1[ SIZE_PLAYERNAME ];
  char str_name2[ SIZE_PLAYERNAME ];
  FILE *pf;
  unsigned int games, moves, lines;
} record_t;

typedef struct {
  unsigned int a[PLY_MAX];
  unsigned char type;
  unsigned char length;
  unsigned char depth;
} pv_t;

typedef struct {
  unsigned char ir0,   sr0;
  unsigned char irl90, srl90;
  unsigned char irl45, srl45;
  unsigned char irr45, srr45;
} slide_tbl_t;


typedef struct {
  uint64_t hash_key;
  bitboard_t b_occupied,     w_occupied;
  bitboard_t occupied_rl90,  occupied_rl45, occupied_rr45;
  bitboard_t b_hdk,          w_hdk;
  bitboard_t b_tgold,        w_tgold;
  bitboard_t b_bh,           w_bh;
  bitboard_t b_rd,           w_rd;
  bitboard_t b_pawn_attacks, w_pawn_attacks;
  bitboard_t b_lance,        w_lance;
  bitboard_t b_knight,       w_knight;
  bitboard_t b_silver,       w_silver;
  bitboard_t b_bishop,       w_bishop;
  bitboard_t b_rook,         w_rook;
  bitboard_t b_horse,        w_horse;
  bitboard_t b_dragon,       w_dragon;
  bitboard_t b_pawn,         w_pawn;
  bitboard_t b_gold,         w_gold;
  bitboard_t b_pro_pawn,     w_pro_pawn;
  bitboard_t b_pro_lance,    w_pro_lance;
  bitboard_t b_pro_knight,   w_pro_knight;
  bitboard_t b_pro_silver,   w_pro_silver;
  unsigned int hand_black, hand_white;
  int material;
  signed char asquare[nsquare];
  unsigned char isquare_b_king, isquare_w_king;
} posi_t;


typedef struct {
  unsigned int hand_black, hand_white;
  char turn_to_move;
  signed char asquare[nsquare];
} min_posi_t;

typedef struct {
  uint64_t nodes;
  unsigned int move, status;
#if defined(DFPN_CLIENT)
  volatile int dfpn_cresult;
#endif
} root_move_t;

typedef struct {
  unsigned int *move_last;
  unsigned int move_cap1;
  unsigned int move_cap2;
  int phase_done, next_phase, remaining, value_cap1, value_cap2;
} next_move_t;

/* data: 31  1bit flag_learned */
/*       30  1bit is_flip      */
/*       15 16bit value        */
typedef struct {
  uint64_t key_book;
  unsigned int key_responsible, key_probed, key_played;
  unsigned int hand_responsible, hand_probed, hand_played;
  unsigned int move_played, move_responsible, move_probed, data;
} history_book_learn_t;

typedef struct tree tree_t;
struct tree {
  posi_t posi;
  uint64_t rep_board_list[ REP_HIST_LEN ];
#if defined(YSS_ZERO)
  // 棋譜と探索木を含めた局面図
  min_posi_t record_plus_ply_min_posi[REP_HIST_LEN];
  int history_in_check[REP_HIST_LEN];	// 王手がかかっているか
  uint64_t sequence_hash;
  uint64_t keep_sequence_hash[REP_HIST_LEN];
  int reached_ply;
  int max_reached_ply;
  int sum_reached_ply;
#endif
  uint64_t node_searched;
  unsigned int *move_last[ PLY_MAX ];
  next_move_t anext_move[ PLY_MAX ];
  pv_t pv[ PLY_MAX ];
  move_killer_t amove_killer[ PLY_MAX ];
  unsigned int null_pruning_done;
  unsigned int null_pruning_tried;
  unsigned int check_extension_done;
  unsigned int recap_extension_done;
  unsigned int onerp_extension_done;
  unsigned int neval_called;
  unsigned int nquies_called;
  unsigned int nfour_fold_rep;
  unsigned int nperpetual_check;
  unsigned int nsuperior_rep;
  unsigned int nrep_tried;
  unsigned int ntrans_always_hit;
  unsigned int ntrans_prefer_hit;
  unsigned int ntrans_probe;
  unsigned int ntrans_exact;
  unsigned int ntrans_lower;
  unsigned int ntrans_upper;
  unsigned int ntrans_superior_hit;
  unsigned int ntrans_inferior_hit;
  unsigned int fail_high;
  unsigned int fail_high_first;
  unsigned int rep_hand_list[ REP_HIST_LEN ];
  unsigned int amove_hash[ PLY_MAX ];
  unsigned int amove[ MOVE_LIST_LEN ];
  unsigned int current_move[ PLY_MAX ];
  killer_t killers[ PLY_MAX ];
  unsigned int hist_nmove[ PLY_MAX ];
  unsigned int hist_move[ PLY_MAX ][ MAX_LEGAL_MOVES ];
  int sort_value[ MAX_LEGAL_MOVES ];
  unsigned short hist_tried[ HIST_SIZE ];
  unsigned short hist_good[ HIST_SIZE ];
  short save_material[ PLY_MAX ];
  int save_eval[ PLY_MAX+1 ];
  unsigned char nsuc_check[ PLY_MAX+1 ];
  int nrep;
#if defined(TLP)
  struct tree *tlp_ptrees_sibling[ TLP_MAX_THREADS ];
  struct tree *tlp_ptree_parent;
  lock_t tlp_lock;
  volatile int tlp_abort;
  volatile int tlp_used;
  unsigned short tlp_slot;
  short tlp_beta;
  short tlp_best;
  volatile unsigned char tlp_nsibling;
  unsigned char tlp_depth;
  unsigned char tlp_state_node;
  unsigned char tlp_id;
  char tlp_turn;
  char tlp_ply;
#endif
};

#if defined(YSS_ZERO)
void copy_min_posi(tree_t * restrict ptree, int sideToMove, int ply);
#endif

extern SHARE unsigned int game_status;

extern int npawn_box;
extern int nlance_box;
extern int nknight_box;
extern int nsilver_box;
extern int ngold_box;
extern int nbishop_box;
extern int nrook_box;

extern unsigned int ponder_move_list[ MAX_LEGAL_MOVES ];
extern unsigned int ponder_move;
extern int ponder_nmove;

extern root_move_t root_move_list[ MAX_LEGAL_MOVES ];
extern SHARE int root_abort;
extern int root_nmove;
extern int root_index;
extern int root_value;
extern int root_alpha;
extern int root_beta;
extern int root_turn;
extern int root_nfail_high;
extern int root_nfail_low;
extern int resign_threshold;

extern uint64_t node_limit;
extern unsigned int node_per_second;
extern unsigned int node_next_signal;
extern unsigned int node_last_check;

extern unsigned int hash_mask;
extern int trans_table_age;

extern pv_t last_pv;
extern pv_t alast_pv_save[NUM_UNMAKE];
extern int alast_root_value_save[NUM_UNMAKE];
extern int last_root_value;
extern int amaterial_save[NUM_UNMAKE];
extern unsigned int amove_save[NUM_UNMAKE];
extern unsigned char ansuc_check_save[NUM_UNMAKE];

extern SHARE trans_table_t *ptrans_table;
extern trans_table_t *ptrans_table_orig;
extern int log2_ntrans_table;

extern int depth_limit;

extern unsigned int time_last_result;
extern unsigned int time_last_eff_search;
extern unsigned int time_last_search;
extern unsigned int time_last_check;
extern unsigned int time_turn_start;
extern unsigned int time_start;
extern unsigned int time_max_limit;
extern unsigned int time_limit;
extern unsigned int time_response;
extern unsigned int sec_limit;
extern unsigned int sec_limit_up;
extern unsigned int sec_limit_depth;
extern unsigned int sec_elapsed;
extern unsigned int sec_b_total;
extern unsigned int sec_w_total;

extern record_t record_problems;
extern record_t record_game;
extern FILE *pf_book;
extern int record_num;

extern int p_value_ex[31];
extern int p_value_pm[15];
extern int p_value[31];
extern short pc_on_sq[nsquare][fe_end*(fe_end+1)/2];
extern short kkp[nsquare][nsquare][kkp_end];

extern uint64_t ehash_tbl[ EHASH_MASK + 1 ];
extern rand_work_t rand_work;
extern slide_tbl_t aslide[ nsquare ];
extern bitboard_t abb_b_knight_attacks[ nsquare ];
extern bitboard_t abb_b_silver_attacks[ nsquare ];
extern bitboard_t abb_b_gold_attacks[ nsquare ];
extern bitboard_t abb_w_knight_attacks[ nsquare ];
extern bitboard_t abb_w_silver_attacks[ nsquare ];
extern bitboard_t abb_w_gold_attacks[ nsquare ];
extern bitboard_t abb_king_attacks[ nsquare ];
extern bitboard_t abb_obstacle[ nsquare ][ nsquare ];
extern bitboard_t abb_bishop_attacks_rl45[ nsquare ][ 128 ];
extern bitboard_t abb_bishop_attacks_rr45[ nsquare ][ 128 ];
extern bitboard_t abb_rank_attacks[ nsquare ][ 128 ];
extern bitboard_t abb_file_attacks[ nsquare ][ 128 ];
extern bitboard_t abb_mask[ nsquare ];
extern bitboard_t abb_mask_rl90[ nsquare ];
extern bitboard_t abb_mask_rl45[ nsquare ];
extern bitboard_t abb_mask_rr45[ nsquare ];
extern bitboard_t abb_plus_rays[ nsquare ];
extern bitboard_t abb_minus_rays[ nsquare ];
extern uint64_t b_pawn_rand[ nsquare ];
extern uint64_t b_lance_rand[ nsquare ];
extern uint64_t b_knight_rand[ nsquare ];
extern uint64_t b_silver_rand[ nsquare ];
extern uint64_t b_gold_rand[ nsquare ];
extern uint64_t b_bishop_rand[ nsquare ];
extern uint64_t b_rook_rand[ nsquare ];
extern uint64_t b_king_rand[ nsquare ];
extern uint64_t b_pro_pawn_rand[ nsquare ];
extern uint64_t b_pro_lance_rand[ nsquare ];
extern uint64_t b_pro_knight_rand[ nsquare ];
extern uint64_t b_pro_silver_rand[ nsquare ];
extern uint64_t b_horse_rand[ nsquare ];
extern uint64_t b_dragon_rand[ nsquare ];
extern uint64_t b_hand_pawn_rand[ npawn_max ];
extern uint64_t b_hand_lance_rand[ nlance_max ];
extern uint64_t b_hand_knight_rand[ nknight_max ];
extern uint64_t b_hand_silver_rand[ nsilver_max ];
extern uint64_t b_hand_gold_rand[ ngold_max ];
extern uint64_t b_hand_bishop_rand[ nbishop_max ];
extern uint64_t b_hand_rook_rand[ nrook_max ];
extern uint64_t w_pawn_rand[ nsquare ];
extern uint64_t w_lance_rand[ nsquare ];
extern uint64_t w_knight_rand[ nsquare ];
extern uint64_t w_silver_rand[ nsquare ];
extern uint64_t w_gold_rand[ nsquare ];
extern uint64_t w_bishop_rand[ nsquare ];
extern uint64_t w_rook_rand[ nsquare ];
extern uint64_t w_king_rand[ nsquare ];
extern uint64_t w_pro_pawn_rand[ nsquare ];
extern uint64_t w_pro_lance_rand[ nsquare ];
extern uint64_t w_pro_knight_rand[ nsquare ];
extern uint64_t w_pro_silver_rand[ nsquare ];
extern uint64_t w_horse_rand[ nsquare ];
extern uint64_t w_dragon_rand[ nsquare ];
extern uint64_t w_hand_pawn_rand[ npawn_max ];
extern uint64_t w_hand_lance_rand[ nlance_max ];
extern uint64_t w_hand_knight_rand[ nknight_max ];
extern uint64_t w_hand_silver_rand[ nsilver_max ];
extern uint64_t w_hand_gold_rand[ ngold_max ];
extern uint64_t w_hand_bishop_rand[ nbishop_max ];
extern uint64_t w_hand_rook_rand[ nrook_max ];
extern unsigned int move_evasion_pchk;
extern int easy_abs;
extern int easy_min;
extern int easy_max;
extern int easy_value;
extern SHARE int fmg_misc;
extern SHARE int fmg_cap;
extern SHARE int fmg_drop;
extern SHARE int fmg_mt;
extern SHARE int fmg_misc_king;
extern SHARE int fmg_cap_king;
extern int iteration_depth;
extern unsigned char book_section[ MAX_SIZE_SECTION+1 ];
extern unsigned char adirec[nsquare][nsquare];
extern unsigned char is_same[16][16];
extern char str_message[ SIZE_MESSAGE ];
extern char str_cmdline[ SIZE_CMDLINE ];
extern char str_buffer_cmdline[ SIZE_CMDBUFFER ];
extern const char *str_error;

extern const char *astr_table_piece[ 16 ];
extern const char *str_resign;
extern const char *str_repetition;
extern const char *str_jishogi;
extern const char *str_record_error;
extern const char *str_unexpect_eof;
extern const char *str_ovrflw_line;
extern const char *str_warning;
extern const char *str_on;
extern const char *str_off;
extern const char *str_book;
extern const char *str_hash;
extern const char *str_fv;
extern const char *str_book_error;
extern const char *str_perpet_check;
extern const char *str_bad_cmdline;
extern const char *str_busy_think;
extern const char *str_bad_record;
extern const char *str_bad_board;
extern const char *str_delimiters;
extern const char *str_fmt_line;
extern const char *str_illegal_move;
extern const char *str_double_pawn;
extern const char *str_mate_drppawn;
extern const char *str_fopen_error;
extern const char *str_game_ended;
extern const char *str_io_error;
extern const char *str_spaces;
extern const char *str_no_legal_move;
extern const char *str_king_hang;
#if defined(CSA_LAN)
extern const char *str_server_err;
#endif
extern const char *str_myname;
extern const char *str_version;
extern char engine_name[];
extern const min_posi_t min_posi_no_handicap;
extern const int ashell_h[ SHELL_H_LEN ];
extern const int aikkp[16];
extern const int aikpp[31];
extern const int aikkp_hand[8];
extern const char ach_turn[2];
extern const unsigned char aifile[ nsquare ];
extern const unsigned char airank[ nsquare ];

#if defined(DFPN_CLIENT)
#  define DFPN_CLIENT_SIZE_SIGNATURE 64
enum { dfpn_client_na, dfpn_client_win, dfpn_client_lose, dfpn_client_misc };
typedef struct { char str_move[7], result; } dfpn_client_cresult_t;
extern volatile int dfpn_client_flag_read;
extern volatile sckt_t dfpn_client_sckt;
extern volatile unsigned int dfpn_client_move_unlocked;
extern volatile int dfpn_client_rresult_unlocked;
extern volatile int dfpn_client_num_cresult;
extern volatile char dfpn_client_signature[ DFPN_CLIENT_SIZE_SIGNATURE ];
extern volatile dfpn_client_cresult_t dfpn_client_cresult[ MAX_LEGAL_MOVES ];
extern volatile char dfpn_client_str_move[7];
extern volatile int dfpn_client_rresult;
extern unsigned int dfpn_client_best_move;
extern lock_t dfpn_client_lock;
extern char dfpn_client_str_addr[256];
extern int dfpn_client_port;
extern int dfpn_client_cresult_index;
void CONV dfpn_client_start( const tree_t * restrict ptree );
void CONV dfpn_client_check_results( void );
int CONV dfpn_client_out( const char *fmt, ... );
#endif

void CONV pv_close( tree_t * restrict ptree, int ply, int type );
void CONV pv_copy( tree_t * restrict ptree, int ply );
void set_derivative_param( void );
void CONV set_search_limit_time( int turn );
void CONV ehash_clear( void );
void CONV hash_store_pv( const tree_t * restrict ptree, unsigned int move,
			 int turn );
void CONV check_futile_score_quies( const tree_t * restrict ptree,
				    unsigned int move, int old_val,
				    int new_val, int turn );
void out_warning( const char *format, ... );
void out_error( const char *format, ... );
void show_prompt( void );
void CONV make_move_w( tree_t * restrict ptree, unsigned int move, int ply );
void CONV make_move_b( tree_t * restrict ptree, unsigned int move, int ply );
void CONV unmake_move_b( tree_t * restrict ptree, unsigned int move, int ply );
void CONV unmake_move_w( tree_t * restrict ptree, unsigned int move, int ply );
void ini_rand( unsigned int s );
void out_CSA( tree_t * restrict ptree, record_t *pr, unsigned int move );
void CONV out_pv( tree_t * restrict ptree, int value, int turn,
		  unsigned int time );
void CONV hash_store( const tree_t * restrict ptree, int ply, int depth,
		      int turn, int value_type, int value, unsigned int move,
		      unsigned int state_node );
void * CONV memory_alloc( size_t nbytes );
void CONV adjust_time( unsigned int elapsed_new, int turn );
int CONV load_fv( void );
int CONV unmake_move_root( tree_t * restrict ptree );
int CONV popu_count012( unsigned int u0, unsigned int u1, unsigned int u2 );
int CONV first_one012( unsigned int u0, unsigned int u1, unsigned int u2 );
int CONV last_one210( unsigned int u2, unsigned int u1, unsigned int u0 );
int CONV first_one01( unsigned int u0, unsigned int u1 );
int CONV first_one12( unsigned int u1, unsigned int u2 );
int CONV last_one01( unsigned int u0, unsigned int u1 );
int CONV last_one12( unsigned int u1, unsigned int u2 );
int CONV first_one1( unsigned int u1 );
int CONV first_one2( unsigned int u2 );
int CONV last_one0( unsigned int u0 );
int CONV last_one1( unsigned int u1 );
int CONV memory_free( void *p );
int CONV reset_time( unsigned int b_remain, unsigned int w_remain );
int CONV gen_legal_moves( tree_t * restrict ptree, unsigned int *p0,
			  int flag );
#if defined(YSS_ZERO)
int CONV gen_legal_moves_aobazero( tree_t * restrict ptree, unsigned int *p0, int flag, int turn );
#endif

int CONV detect_signals( tree_t * restrict ptree );
int ini( tree_t * restrict ptree );
int fin( void );
int ponder( tree_t * restrict ptree );
int CONV book_on( void );
int CONV book_off( void );
int CONV solve_problems( tree_t * restrict ptree, unsigned int nposition );
int CONV solve_mate_problems( tree_t * restrict ptree,
			      unsigned int nposition );
int read_board_rep1( const char *str_line, min_posi_t *pmin_posi );
int CONV com_turn_start( tree_t * restrict ptree, int flag );
int read_record( tree_t * restrict ptree, const char *str_file,
		 unsigned int moves, int flag );
int out_board( const tree_t * restrict ptree, FILE *pf, unsigned int move,
	       int flag );
int make_root_move_list( tree_t * restrict ptree );
int record_wind( record_t *pr );
int CONV book_probe( tree_t * restrict ptree );
int CONV detect_repetition( tree_t * restrict ptree, int ply, int turn,
			    int nth );
int CONV is_move( const char *str );
int CONV is_mate( tree_t * restrict ptree, int ply );
int CONV is_mate_w_pawn_drop( tree_t * restrict ptree, int sq_drop );
int CONV is_mate_b_pawn_drop( tree_t * restrict ptree, int sq_drop );
int CONV clear_trans_table( void );
int CONV eval_max_score( const tree_t * restrict ptree, unsigned int move,
			 int value, int turn, int diff );
int CONV estimate_score_diff( const tree_t * restrict ptree, unsigned int move,
			      int turn );
int CONV eval_material( const tree_t * restrict ptree );
int CONV ini_trans_table( void );
int CONV is_hand_eq_supe( unsigned int u, unsigned int uref );
int CONV is_move_valid( tree_t * restrict ptree, unsigned int move, int turn );
int CONV iterate( tree_t * restrict ptree );
int CONV gen_next_move( tree_t * restrict ptree, int ply, int turn );
int CONV gen_next_evasion( tree_t * restrict ptree, int ply, int turn );
int CONV ini_game( tree_t * restrict ptree, const min_posi_t *pmin_posi,
		   int flag, const char *str_name1, const char *str_name2 );
int open_history( const char *str_name1, const char *str_name2 );
int next_cmdline( int is_wait );
int CONV procedure( tree_t * restrict ptree );
int CONV get_cputime( unsigned int *ptime );
int CONV get_elapsed( unsigned int *ptime );
int interpret_CSA_move( tree_t * restrict ptree, unsigned int *pmove,
			const char *str );
int in_CSA( tree_t * restrict ptree, record_t *pr, unsigned int *pmove,
	    int do_history );
int in_CSA_record( FILE * restrict pf, tree_t * restrict ptree );
int CONV update_time( int turn );
int CONV exam_tree( const tree_t * restrict ptree );
int rep_check_root( tree_t * restrict ptree );
int CONV make_move_root( tree_t * restrict ptree, unsigned int move,
			 int flag );
int CONV search_quies( tree_t * restrict ptree, int alpha, int beta, int turn,
		       int ply, int qui_ply );
int CONV search_quies_aoba( tree_t * restrict ptree, int alpha, int beta, int turn,
		       int ply, int qui_ply );
int CONV search( tree_t * restrict ptree, int alpha, int beta, int turn,
		 int depth, int ply, unsigned int state_node );
int CONV searchr( tree_t * restrict ptree, int alpha, int beta, int turn,
	     int depth );
int CONV evaluate( tree_t * restrict ptree, int ply, int turn );
int CONV swap( const tree_t * restrict ptree, unsigned int move, int alpha,
	       int beta, int turn );
int file_close( FILE *pf );
int record_open( record_t *pr, const char *str_file,
		 record_mode_t record_mode, const char *str_name1,
		 const char *str_name2 );
int record_close( record_t *pr );
unsigned int CONV phash( unsigned int move, int turn );
unsigned int CONV is_mate_in3ply( tree_t * restrict ptree, int turn, int ply );
unsigned int CONV is_b_mate_in_1ply( tree_t * restrict ptree );
unsigned int CONV is_w_mate_in_1ply( tree_t * restrict ptree );
unsigned int CONV hash_probe( tree_t * restrict ptree, int ply, int depth,
			      int turn, int alpha, int beta,
			      unsigned int *pstate_node );
unsigned int rand32( void );
unsigned int CONV is_black_attacked( const tree_t * restrict ptree, int sq );
unsigned int CONV is_white_attacked( const tree_t * restrict ptree, int sq );
unsigned int CONV is_pinned_on_black_king( const tree_t * restrict ptree,
				     int isquare, int idirec );
unsigned int CONV is_pinned_on_white_king( const tree_t * restrict ptree,
				     int isquare, int idirec );
unsigned int * CONV b_gen_captures( const tree_t * restrict ptree,
				    unsigned int * restrict pmove );
unsigned int * CONV b_gen_nocaptures( const tree_t * restrict ptree,
				      unsigned int * restrict pmove );
unsigned int * CONV b_gen_drop( tree_t * restrict ptree,
			  unsigned int * restrict pmove );
unsigned int * CONV b_gen_evasion( tree_t *restrict ptree,
				   unsigned int * restrict pmove );
unsigned int * CONV b_gen_checks( tree_t * restrict __ptree__,
				  unsigned int * restrict pmove );
unsigned int * CONV b_gen_cap_nopro_ex2( const tree_t * restrict ptree,
					 unsigned int * restrict pmove );
unsigned int * CONV b_gen_nocap_nopro_ex2( const tree_t * restrict ptree,
				     unsigned int * restrict pmove );
unsigned int * CONV w_gen_captures( const tree_t * restrict ptree,
				    unsigned int * restrict pmove );
unsigned int * CONV w_gen_nocaptures( const tree_t * restrict ptree,
				      unsigned int * restrict pmove );
unsigned int * CONV w_gen_drop( tree_t * restrict ptree,
				unsigned int * restrict pmove );
unsigned int * CONV w_gen_evasion( tree_t * restrict ptree,
				   unsigned int * restrict pmove );
unsigned int * CONV w_gen_checks( tree_t * restrict __ptree__,
				  unsigned int * restrict pmove );
unsigned int * CONV w_gen_cap_nopro_ex2( const tree_t * restrict ptree,
					 unsigned int * restrict pmove );
unsigned int * CONV w_gen_nocap_nopro_ex2( const tree_t * restrict ptree,
					   unsigned int * restrict pmove );
int CONV b_have_checks( tree_t * restrict ptree );
int CONV w_have_checks( tree_t * restrict ptree );
int CONV b_have_evasion( tree_t * restrict ptree );
int CONV w_have_evasion( tree_t * restrict ptree );
int CONV is_move_check_b( const tree_t * restrict ptree, unsigned int move );
int CONV is_move_check_w( const tree_t * restrict ptree, unsigned int move );
uint64_t CONV hash_func( const tree_t * restrict ptree );
uint64_t rand64( void );
FILE *file_open( const char *str_file, const char *str_mode );
bitboard_t CONV attacks_to_piece( const tree_t * restrict ptree, int sq );
bitboard_t CONV b_attacks_to_piece( const tree_t * restrict ptree, int sq );
bitboard_t CONV w_attacks_to_piece( const tree_t * restrict ptree, int sq );
const char * CONV str_time( unsigned int time );
const char * CONV str_time_symple( unsigned int time );
const char *str_CSA_move( unsigned int move );
#if defined(YSS_ZERO)
std::string string_CSA_move( unsigned int move );
#endif

#if defined(MPV)
int root_mpv;
int mpv_num;
int mpv_width;
pv_t mpv_pv[ MPV_MAX_PV*2 + 1 ];
#endif

#  if ! defined(_WIN32) && ( defined(DFPN_CLIENT) || defined(TLP) )
extern pthread_attr_t pthread_attr;
#  endif

#if defined(DFPN_CLIENT) || defined(TLP)
void CONV lock( lock_t *plock );
void CONV unlock( lock_t *plock );
int CONV lock_init( lock_t *plock );
int CONV lock_free( lock_t *plock );
void tlp_yield( void );
extern lock_t io_lock;
#endif

#if defined(TLP)
#  define SignKey(word2, word1) word2 ^= ( word1 )
#  define TlpEnd()              tlp_end();
#  if defined(MNJ_LAN) || defined(USI)
uint64_t tlp_count_node( tree_t * restrict ptree );
#  endif
void tlp_set_abort( tree_t * restrict ptree );
void tlp_end( void );
int CONV tlp_search( tree_t * restrict ptree, int alpha, int beta, int turn,
		int depth, int ply, unsigned int state_node );
int tlp_split( tree_t * restrict ptree );
int tlp_start( void );
int tlp_is_descendant( const tree_t * restrict ptree, int slot_ancestor );
extern lock_t tlp_lock;
extern volatile int tlp_abort;
extern volatile int tlp_idle;
extern volatile int tlp_num;
extern int tlp_max;
extern int tlp_nsplit;
extern int tlp_nabort;
extern int tlp_nslot;
extern tree_t tlp_atree_work[ TLP_NUM_WORK ];
extern tree_t * volatile tlp_ptrees[ TLP_MAX_THREADS ];
#else /* no TLP */
#  define SignKey(word2, word1)
#  define TlpEnd()
extern tree_t tree;
#endif

#if ! defined(_WIN32)
extern clock_t clk_tck;
#endif

#if ! defined(NDEBUG)
int exam_bb( const tree_t *ptree );
#endif

#if ! ( defined(NO_STDOUT) && defined(NO_LOGGING) )
#  define Out( ... ) out( __VA_ARGS__ )
void out( const char *format, ... );
#else
#  define Out( ... )
#endif

#if ! defined(NO_LOGGING)
extern FILE *pf_log;
extern const char *str_dir_logs;
#endif

#if defined(NO_STDOUT) || defined(WIN32_PIPE)
#  define OutBeep()
#  define StdoutStress(x,y) 1
#  define StdoutNormal()    1
#else
#  define OutBeep()         out_beep()
#  define StdoutStress(x,y) stdout_stress(x,y)
#  define StdoutNormal()    stdout_normal()
void out_beep( void );
int stdout_stress( int is_promote, int ifrom );
int stdout_normal( void );
#endif

#if defined(CSA_LAN) || defined(MNJ_LAN) || defined(DFPN)
void CONV shutdown_all( void );
#  define ShutdownAll() shutdown_all();
#else
#  define ShutdownAll()
#endif

#if defined(CSA_LAN)||defined(MNJ_LAN)||defined(DFPN_CLIENT)||defined(DFPN)
int client_next_game( tree_t * restrict ptree, const char *str_addr,
		      int iport );
sckt_t CONV sckt_connect( const char *str_addr, int iport );
int CONV sckt_recv_all( sckt_t sd );
int CONV sckt_shutdown( sckt_t sd );
int CONV sckt_check( sckt_t sd );
int CONV sckt_in( sckt_t sd, char *str, int n );
int CONV sckt_out( sckt_t sd, const char *fmt, ... );
extern unsigned int time_last_send;
#endif

#if defined(DFPN)
#  define DFPNOut( ... ) if ( dfpn_sckt != SCKT_NULL ) \
                           sckt_out( dfpn_sckt, __VA_ARGS__ )
const int DFPN_NODE_LIMIT = 50000000;
int CONV dfpn( tree_t * restrict ptree, int turn, int ply, unsigned int *pret_move, int dfpn_node_limit = DFPN_NODE_LIMIT );
int CONV dfpn_ini_hash( void );
extern unsigned int dfpn_hash_log2;
extern sckt_t dfpn_sckt;
#else
#  define DFPNOut( ... )
#endif

#if defined(CSA_LAN)
extern int client_turn;
extern int client_ngame;
extern int client_max_game;
extern long client_port;
extern char client_str_addr[256];
extern char client_str_id[256];
extern char client_str_pwd[256];
extern sckt_t sckt_csa;
#endif

#if defined(MNJ_LAN) || defined(USI)
extern unsigned int moves_ignore[MAX_LEGAL_MOVES];
#endif

#if defined(MNJ_LAN)
#  define MnjOut( ... ) if ( sckt_mnj != SCKT_NULL ) \
                          sckt_out( sckt_mnj, __VA_ARGS__ )
extern sckt_t sckt_mnj;
extern int mnj_posi_id;
extern int mnj_depth_stable;
void CONV mnj_check_results( void );
int CONV mnj_reset_tbl( int sd, unsigned int seed );
int analyze( tree_t * restrict ptree );
#else
#  define MnjOut( ... )
#endif

#if defined(USI)
#  define USIOut( ... ) if ( usi_mode != usi_off ) usi_out( __VA_ARGS__ )
enum usi_mode { usi_off, usi_on };
extern enum usi_mode usi_mode;
extern unsigned int usi_time_out_last;
extern unsigned int usi_byoyomi;
extern int fUSIMoveCount;

void CONV usi_out( const char *format, ... );
int CONV usi_book( tree_t * restrict ptree );
int CONV usi_root_list( tree_t * restrict ptree );
int CONV usi2csa( const tree_t * restrict ptree, const char *str_usi,
		  char *str_csa );
int CONV csa2usi( const tree_t * restrict ptree, const char *str_csa,
		  char *str_usi );
int analyze( tree_t * restrict ptree );
int CONV usi_posi( tree_t * restrict ptree, char **lasts );
#else
#  define USIOut( ... )
#endif

#if defined(CSA_LAN) || defined(MNJ_LAN) || defined(DFPN_CLIENT)||defined(DFPN)
const char *str_WSAError( const char *str );
#endif

#if defined(CSASHOGI)
#  define OutCsaShogi( ... ) out_csashogi( __VA_ARGS__ )
void out_csashogi( const char *format, ... );
#else
#  define OutCsaShogi( ... )
#endif



extern check_table_t b_chk_tbl[nsquare];
extern check_table_t w_chk_tbl[nsquare];

#if defined(DBG_EASY)
extern unsigned int easy_move;
#endif

#if defined(INANIWA_SHIFT)
extern int inaniwa_flag;
#endif

#if defined(MINIMUM)

#  define MT_CAP_PAWN       ( DPawn      + DPawn )
#  define MT_CAP_LANCE      ( DLance     + DLance )
#  define MT_CAP_KNIGHT     ( DKnight    + DKnight )
#  define MT_CAP_SILVER     ( DSilver    + DSilver )
#  define MT_CAP_GOLD       ( DGold      + DGold )
#  define MT_CAP_BISHOP     ( DBishop    + DBishop )
#  define MT_CAP_ROOK       ( DRook      + DRook )
#  define MT_CAP_PRO_PAWN   ( DProPawn   + DPawn )
#  define MT_CAP_PRO_LANCE  ( DProLance  + DLance )
#  define MT_CAP_PRO_KNIGHT ( DProKnight + DKnight )
#  define MT_CAP_PRO_SILVER ( DProSilver + DSilver )
#  define MT_CAP_HORSE      ( DHorse     + DBishop )
#  define MT_CAP_DRAGON     ( DDragon    + DRook )
#  define MT_CAP_KING       ( DKing      + DKing )
#  define MT_PRO_PAWN       ( DProPawn   - DPawn )
#  define MT_PRO_LANCE      ( DProLance  - DLance )
#  define MT_PRO_KNIGHT     ( DProKnight - DKnight )
#  define MT_PRO_SILVER     ( DProSilver - DSilver )
#  define MT_PRO_BISHOP     ( DHorse     - DBishop )
#  define MT_PRO_ROOK       ( DDragon    - DRook )

#else

#  define MT_CAP_PAWN       ( p_value_ex[ 15 + pawn ] )
#  define MT_CAP_LANCE      ( p_value_ex[ 15 + lance ] )
#  define MT_CAP_KNIGHT     ( p_value_ex[ 15 + knight ] )
#  define MT_CAP_SILVER     ( p_value_ex[ 15 + silver ] )
#  define MT_CAP_GOLD       ( p_value_ex[ 15 + gold ] )
#  define MT_CAP_BISHOP     ( p_value_ex[ 15 + bishop ] )
#  define MT_CAP_ROOK       ( p_value_ex[ 15 + rook ] )
#  define MT_CAP_PRO_PAWN   ( p_value_ex[ 15 + pro_pawn ] )
#  define MT_CAP_PRO_LANCE  ( p_value_ex[ 15 + pro_lance ] )
#  define MT_CAP_PRO_KNIGHT ( p_value_ex[ 15 + pro_knight ] )
#  define MT_CAP_PRO_SILVER ( p_value_ex[ 15 + pro_silver ] )
#  define MT_CAP_HORSE      ( p_value_ex[ 15 + horse ] )
#  define MT_CAP_DRAGON     ( p_value_ex[ 15 + dragon ] )
#  define MT_CAP_KING       ( DKing + DKing )
#  define MT_PRO_PAWN       ( p_value_pm[ 7 + pawn ] )
#  define MT_PRO_LANCE      ( p_value_pm[ 7 + lance ] )
#  define MT_PRO_KNIGHT     ( p_value_pm[ 7 + knight ] )
#  define MT_PRO_SILVER     ( p_value_pm[ 7 + silver ] )
#  define MT_PRO_BISHOP     ( p_value_pm[ 7 + bishop ] )
#  define MT_PRO_ROOK       ( p_value_pm[ 7 + rook ] )

void fill_param_zero( void );
void ini_param( param_t *p );
void add_param( param_t *p1, const param_t *p2 );
void inc_param( const tree_t * restrict ptree, param_t * restrict pd,
		double dinc );
void param_sym( param_t *p );
void renovate_param( const param_t *pd );
int learn( tree_t * restrict ptree, int is_ini, int nsteps,
	   unsigned int max_games, int max_iterations,
	   int nworker1, int nworker2 );
int record_setpos( record_t *pr, const rpos_t *prpos );
int record_getpos( record_t *pr, rpos_t *prpos );
int record_rewind( record_t *pr );
int CONV book_create( tree_t * restrict ptree );
int out_param( void );
double calc_penalty( void );

#endif /* no MINIMUM */

#if ( REP_HIST_LEN - PLY_MAX ) < 1
#  error "REP_HIST_LEN - PLY_MAX is too small."
#endif

#if defined(CSA_LAN) && '\n' != 0x0a
#  error "'\n' is not the ASCII code of LF (0x0a)."
#endif

#if defined(YSS_ZERO)
int YssZero_com_turn_start( tree_t * restrict ptree );
int getCmdLineParam(int argc, char *argv[]);
const char *get_cmd_line_ptr();
void init_seqence_hash();
const int SEQUENCE_HASH_SIZE = 512;	// 2^n.   別手順できた同一局面を区別するため
uint64_t get_sequence_hash_from_to(int moves, int from, int to, int promote);
uint64_t get_sequence_hash_drop(int moves, int to, int piece);
void debug();
void PRT(const char *fmt, ...);
void print_board(const tree_t * restrict ptree);
void init_yss_zero();
void set_default_param();
void init_state( const tree_t * restrict parent, tree_t * restrict child );
extern int sfen_current_move_number;
extern int nHandicap;
extern float average_winrate;
int is_stop_search();
int is_limit_sec_or_stop_input();

#endif

#endif /* SHOGI_H */
