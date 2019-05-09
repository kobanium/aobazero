// 2019 Team AobaZero
// This source code is in the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "shogi.h"

FILE *pf_book;
uint64_t ehash_tbl[ EHASH_MASK + 1 ];
trans_table_t *ptrans_table_orig;
SHARE trans_table_t *ptrans_table;
record_t record_problems;
record_t record_game;
rand_work_t rand_work;
root_move_t root_move_list[ MAX_LEGAL_MOVES ];
pv_t last_pv;
pv_t alast_pv_save[NUM_UNMAKE];
slide_tbl_t aslide[ nsquare ];
bitboard_t abb_b_knight_attacks[ nsquare ];
bitboard_t abb_b_silver_attacks[ nsquare ];
bitboard_t abb_b_gold_attacks[ nsquare ];
bitboard_t abb_w_knight_attacks[ nsquare ];
bitboard_t abb_w_silver_attacks[ nsquare ];
bitboard_t abb_w_gold_attacks[ nsquare ];
bitboard_t abb_king_attacks[ nsquare ];
bitboard_t abb_bishop_attacks_rl45[ nsquare ][ 128 ];
bitboard_t abb_bishop_attacks_rr45[ nsquare ][ 128 ];
bitboard_t abb_rank_attacks[ nsquare ][ 128 ];
bitboard_t abb_file_attacks[ nsquare ][ 128 ];
bitboard_t abb_obstacle[ nsquare ][ nsquare ];
bitboard_t abb_mask[ nsquare ];
bitboard_t abb_mask_rl90[ nsquare ];
bitboard_t abb_mask_rl45[ nsquare ];
bitboard_t abb_mask_rr45[ nsquare ];
bitboard_t abb_plus_rays[ nsquare ];
bitboard_t abb_minus_rays[ nsquare ];
uint64_t b_pawn_rand[ nsquare ];
uint64_t b_lance_rand[ nsquare ];
uint64_t b_knight_rand[ nsquare ];
uint64_t b_silver_rand[ nsquare ];
uint64_t b_gold_rand[ nsquare ];
uint64_t b_bishop_rand[ nsquare ];
uint64_t b_rook_rand[ nsquare ];
uint64_t b_king_rand[ nsquare ];
uint64_t b_pro_pawn_rand[ nsquare ];
uint64_t b_pro_lance_rand[ nsquare ];
uint64_t b_pro_knight_rand[ nsquare ];
uint64_t b_pro_silver_rand[ nsquare ];
uint64_t b_horse_rand[ nsquare ];
uint64_t b_dragon_rand[ nsquare ];
uint64_t b_hand_pawn_rand[ npawn_max ];
uint64_t b_hand_lance_rand[ nlance_max ];
uint64_t b_hand_knight_rand[ nknight_max ];
uint64_t b_hand_silver_rand[ nsilver_max ];
uint64_t b_hand_gold_rand[ ngold_max ];
uint64_t b_hand_bishop_rand[ nbishop_max ];
uint64_t b_hand_rook_rand[ nrook_max ];
uint64_t w_pawn_rand[ nsquare ];
uint64_t w_lance_rand[ nsquare ];
uint64_t w_knight_rand[ nsquare ];
uint64_t w_silver_rand[ nsquare ];
uint64_t w_gold_rand[ nsquare ];
uint64_t w_bishop_rand[ nsquare ];
uint64_t w_rook_rand[ nsquare ];
uint64_t w_king_rand[ nsquare ];
uint64_t w_pro_pawn_rand[ nsquare ];
uint64_t w_pro_lance_rand[ nsquare ];
uint64_t w_pro_knight_rand[ nsquare ];
uint64_t w_pro_silver_rand[ nsquare ];
uint64_t w_horse_rand[ nsquare ];
uint64_t w_dragon_rand[ nsquare ];
uint64_t w_hand_pawn_rand[ npawn_max ];
uint64_t w_hand_lance_rand[ nlance_max ];
uint64_t w_hand_knight_rand[ nknight_max ];
uint64_t w_hand_silver_rand[ nsilver_max ];
uint64_t w_hand_gold_rand[ ngold_max ];
uint64_t w_hand_bishop_rand[ nbishop_max ];
uint64_t w_hand_rook_rand[ nrook_max ];
uint64_t node_limit;
SHARE unsigned int game_status;
unsigned int amove_save[NUM_UNMAKE];
unsigned int move_evasion_pchk;
unsigned int node_per_second;
unsigned int node_next_signal;
unsigned int node_last_check;
unsigned int hash_mask;
unsigned int sec_elapsed;
unsigned int sec_b_total;
unsigned int sec_w_total;
unsigned int sec_limit;
unsigned int sec_limit_up;
unsigned int sec_limit_depth;
unsigned int time_last_result;
unsigned int time_last_check;
unsigned int time_start;
unsigned int time_turn_start;
unsigned int time_limit;
unsigned int time_max_limit;
unsigned int time_last_search;
unsigned int time_response;
unsigned int ponder_move;
int easy_abs;
int easy_min;
int easy_max;
int easy_value;
SHARE int fmg_misc;
SHARE int fmg_cap;
SHARE int fmg_drop;
SHARE int fmg_mt;
SHARE int fmg_misc_king;
SHARE int fmg_cap_king;
unsigned int ponder_move_list[ MAX_LEGAL_MOVES ];
int ponder_nmove;
SHARE int root_abort;
int root_nmove;
int root_alpha;
int root_beta;
int root_value;
int root_turn;
int root_index;
int root_nfail_high;
int root_nfail_low;
int trans_table_age;
int log2_ntrans_table;
int last_root_value;
int iteration_depth;
int depth_limit;
int record_num;
int npawn_box;
int nlance_box;
int nknight_box;
int nsilver_box;
int ngold_box;
int nbishop_box;
int nrook_box;
int resign_threshold;
int amaterial_save[NUM_UNMAKE];
int alast_root_value_save[NUM_UNMAKE];
int p_value_ex[31];
int p_value_pm[15];
int p_value[31];
short pc_on_sq[nsquare][pos_n];
short kkp[nsquare][nsquare][kkp_end];
unsigned char ansuc_check_save[NUM_UNMAKE];
unsigned char book_section[ MAX_SIZE_SECTION+1 ];
unsigned char adirec[ nsquare ][ nsquare ];
unsigned char is_same[ 16 ][ 16 ];
char str_cmdline[ SIZE_CMDLINE ];
char str_message[ SIZE_MESSAGE ];
char str_buffer_cmdline[ SIZE_CMDBUFFER ];
const char *str_error;

#if defined(MPV)
int root_mpv;
int mpv_num;
int mpv_width;
pv_t mpv_pv[ MPV_MAX_PV*2 + 1 ];
#endif

#if defined(DFPN)
unsigned int dfpn_hash_log2;
sckt_t dfpn_sckt;
#endif

#  if ! defined(_WIN32) && ( defined(DFPN_CLIENT) || defined(TLP) )
pthread_attr_t pthread_attr;
#  endif

#if defined(TLP) || defined(DFPN_CLIENT)
lock_t io_lock;
#endif

#if defined(DFPN_CLIENT)
volatile sckt_t dfpn_client_sckt;
volatile int dfpn_client_flag_read;
volatile unsigned int dfpn_client_move_unlocked;
volatile int dfpn_client_rresult_unlocked;
volatile int dfpn_client_rresult;
volatile int dfpn_client_num_cresult;
volatile char dfpn_client_signature[ DFPN_CLIENT_SIZE_SIGNATURE ];
volatile dfpn_client_cresult_t dfpn_client_cresult[ MAX_LEGAL_MOVES ];
volatile char dfpn_client_str_move[7];
unsigned int dfpn_client_best_move;
lock_t dfpn_client_lock;
char dfpn_client_str_addr[256];
int dfpn_client_port;
int dfpn_client_cresult_index;
#endif

#if defined(TLP)
lock_t tlp_lock;
tree_t tlp_atree_work[ TLP_NUM_WORK ];
tree_t * volatile tlp_ptrees[ TLP_MAX_THREADS ];
volatile int tlp_abort;
volatile int tlp_idle;
volatile int tlp_num;
int tlp_max;
int tlp_nsplit;
int tlp_nabort;
int tlp_nslot;
#else
tree_t tree;
#endif

#if ! defined(_WIN32)
clock_t clk_tck;
#endif

#if ! defined(NO_LOGGING)
FILE *pf_log;
const char *str_dir_logs = "log";
#endif

#if defined(CSA_LAN)||defined(MNJ_LAN)|| defined(DFPN_CLIENT)|| defined(DFPN)
unsigned int time_last_send;
#endif

#if defined(CSA_LAN)
int client_turn;
int client_ngame;
int client_max_game;
long client_port;
char client_str_addr[256];
char client_str_id[256];
char client_str_pwd[256];
sckt_t sckt_csa;
#endif

#if defined(MNJ_LAN) || defined(USI)
unsigned int moves_ignore[MAX_LEGAL_MOVES];
#endif

#if defined(MNJ_LAN)
sckt_t sckt_mnj;
int mnj_posi_id;
int mnj_depth_stable;
unsigned int mnj_moves_ignore[MAX_LEGAL_MOVES];
#endif

#if defined(USI)
enum usi_mode usi_mode;
unsigned int usi_time_out_last;
unsigned int usi_byoyomi;
#endif

check_table_t b_chk_tbl[nsquare];
check_table_t w_chk_tbl[nsquare];

#if defined(_MSC_VER)
#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
#else
unsigned char aifirst_one[512];
unsigned char ailast_one[512];
#endif

#if defined(NDEBUG)
#  if ! defined(CSASHOGI)
const char *str_myname = ( BNZ_NAME " " BNZ_VER );
#  else
const char *str_myname = ( BNZ_NAME " " BNZ_VER );
#  endif
#else
const char *str_myname = ( BNZ_NAME " " BNZ_VER " Debug Build ("
			   __TIME__ " " __DATE__ ")" );
#endif

#if defined(DBG_EASY)
unsigned int easy_move;
#endif

#if defined(INANIWA_SHIFT)
int inaniwa_flag;
#endif

const char *str_resign        = "%TORYO";
const char *str_repetition    = "%SENNICHITE";
const char *str_jishogi       = "%JISHOGI";
const char *str_record_error  = "%ERROR";
const char *str_delimiters    = " \t,";
const char *str_fmt_line      = "Line %u: %s";
const char *str_on            = "on";
const char *str_off           = "off";
const char *str_book          = "book.bin";
const char *str_fv            = "fv.bin";
const char *str_book_error    = "invalid opening book";
const char *str_io_error      = "I/O error";
const char *str_perpet_check  = "perpetual check";
const char *str_bad_cmdline   = "invalid command line";
const char *str_busy_think    = "I'm busy in thinking now";
const char *str_bad_record    = "invalid record of game";
const char *str_bad_board     = "invalid board representation";
const char *str_illegal_move  = "illegal move";
const char *str_double_pawn   = "double pawn";
const char *str_mate_drppawn  = "mated by a droped pawn";
const char *str_unexpect_eof  = "unexpected end of file";
const char *str_king_hang     = "The king is hang.";
const char *str_game_ended    = "move after a game was concluded";
const char *str_fopen_error   = "Can't open a file";
const char *str_ovrflw_line   = "Too many characters in a line.";
const char *str_no_legal_move = "No legal moves to search";
const char *str_warning       = "WARNING: ";
#if defined(CSA_LAN)
const char *str_server_err   = "received invalid message from the server";
#endif

const char *astr_table_piece[16]  = { "* ", "FU", "KY", "KE", "GI", "KI",
				      "KA", "HI", "OU", "TO", "NY", "NK",
				      "NG", "##", "UM", "RY" };

const char ach_turn[2] = { '+', '-' };

const int ashell_h[ SHELL_H_LEN ] = { 1, 3, 7, 15, 31, 63, 127 };

const int aikpp[31] = { e_dragon, e_horse,  0,        e_gold,
			e_gold,   e_gold,   e_gold,   0,
			e_rook,   e_bishop, e_gold,   e_silver,
			e_knight, e_lance,  e_pawn,   0,
			f_pawn,   f_lance,  f_knight,
			f_silver, f_gold,   f_bishop, f_rook,
			0,        f_gold,   f_gold,   f_gold,
			f_gold,   0,        f_horse,  f_dragon };

const int aikkp[16] = { 0,          kkp_pawn, kkp_lance,  kkp_knight,
			kkp_silver, kkp_gold, kkp_bishop, kkp_rook,
			0,          kkp_gold, kkp_gold,   kkp_gold,
			kkp_gold,   kkp_gold, kkp_horse,  kkp_dragon };

const int aikkp_hand[8] = { 0,               kkp_hand_pawn,   kkp_hand_lance,
			    kkp_hand_knight, kkp_hand_silver, kkp_hand_gold,
			    kkp_hand_bishop, kkp_hand_rook };

const unsigned char aifile[ nsquare ]= {
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9,
  file1, file2, file3, file4, file5, file6, file7, file8, file9 };

const unsigned char airank[ nsquare ]= {
  rank1, rank1, rank1, rank1, rank1, rank1, rank1, rank1, rank1,
  rank2, rank2, rank2, rank2, rank2, rank2, rank2, rank2, rank2,
  rank3, rank3, rank3, rank3, rank3, rank3, rank3, rank3, rank3,
  rank4, rank4, rank4, rank4, rank4, rank4, rank4, rank4, rank4,
  rank5, rank5, rank5, rank5, rank5, rank5, rank5, rank5, rank5,
  rank6, rank6, rank6, rank6, rank6, rank6, rank6, rank6, rank6,
  rank7, rank7, rank7, rank7, rank7, rank7, rank7, rank7, rank7,
  rank8, rank8, rank8, rank8, rank8, rank8, rank8, rank8, rank8,
  rank9, rank9, rank9, rank9, rank9, rank9, rank9, rank9, rank9 };

const min_posi_t min_posi_no_handicap = {
  0,0,0,
  { -lance, -knight, -silver, -gold, -king, -gold, -silver, -knight, -lance,
    empty, -rook, empty, empty, empty, empty, empty, -bishop, empty,
    -pawn, -pawn, -pawn, -pawn, -pawn, -pawn, -pawn, -pawn, -pawn,
    empty, empty, empty, empty, empty, empty, empty, empty, empty,
    empty, empty, empty, empty, empty, empty, empty, empty, empty,
    empty, empty, empty, empty, empty, empty, empty, empty, empty,
    pawn, pawn, pawn, pawn, pawn, pawn, pawn, pawn, pawn,
    empty, bishop, empty, empty, empty, empty, empty, rook, empty,
    lance, knight, silver, gold, king, gold, silver, knight, lance } };
