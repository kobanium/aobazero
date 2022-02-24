// 2019 Team AobaZero
// This source code is in the public domain.
// yss_dcnn.h
#ifndef INCLUDE_YSS_DCNN_H_GUARD	//[
#define INCLUDE_YSS_DCNN_H_GUARD

#include "lock.h"
#include <atomic>
#include "param.hpp"

const int B_SIZE = 9;
const int DCNN_CHANNELS = 362;
const int LABEL_CHANNELS = 139;	// 11259êÍópÇÃíl


const int SHOGI_MOVES_MAX = 593;
const float ILLEGAL_MOVE = -1000;

#define USE_LCB

typedef struct child {
	int   move;			//
	int   games;		// number of selected
	float value;		// win rate (win=+1, loss=0)
	float bias;			// policy
	int   exact_value;	// WIN or LOSS or DRAW
#ifdef USE_LCB
	float squared_eval_diff;	// Variable used for calculating variance of evaluations. for LCB
	int acc_virtual_loss;		// accumulate virtual loss. for LCB
#endif
//	std::atomic<bool> exact_value;
} CHILD;

#define CHILD_VEC

typedef struct hash_shogi {
	lock_yss_t entry_lock;		// lock
//	std::atomic<bool> lock;//{false};	// can't resize(N). atomic is non-moveable.

	uint64 hashcode64;			// sequence hash
	uint64 hash64pos;			// position hash, we check both hash key.
	int deleted;	//
	int games_sum;	// sum of children selected
	int col;		// color 1 or 2
	int age;		//
	float net_value;		// winrate from value network

	int child_num;
#ifdef CHILD_VEC
	std::vector <CHILD> child;
#else
	CHILD child[SHOGI_MOVES_MAX];
#endif
} HASH_SHOGI;

enum {
  EX_NONE, EX_WIN, EX_LOSS, EX_DRAW		// exact value.
};

extern int fAddNoise;
extern int fVisitCount;
extern int fUSIMoveCount;
extern int fPrtNetworkRawPath;
extern int nNNetServiceNumber;
extern int nDrawMove;
extern int nUseHalf;
extern int nUseWmma;
extern std::string sDirTune;
extern int nHandicapRate[HANDICAP_TYPE];

extern std::string default_weights;
extern std::vector<int> default_gpus;

extern int usi_go_count;
extern int usi_bestmove_count;

void debug_set(const char *file, int line);
void debug_print(const char *fmt, ... );
#define DEBUG_PRT (debug_set(__FILE__,__LINE__), debug_print)	
void PRT(const char *fmt, ...);
int get_clock();
double get_spend_time(int ct1);
void create_node(tree_t * restrict ptree, int sideToMove, int ply, HASH_SHOGI *phg, bool fOpeningHash = false);
double uct_tree(tree_t * restrict ptree, int sideToMove, int ply, int *pExactValue);
int uct_search_start(tree_t * restrict ptree, int sideToMove, int ply, char *buf_move_count);
void print_all_min_posi(tree_t * restrict ptree, int ply);
//int check_enter_input();
int check_stop_input();
int is_ignore_stop();
void send_latest_bestmove();
void set_latest_bestmove(char *str);
int is_send_usi_info(int nodes);
void send_usi_info(tree_t * restrict ptree, int sideToMove, int ply, int nodes, int nps);
void usi_newgame(tree_t * restrict ptree);
int is_declare_win(tree_t * restrict ptree, int sideToMove);
int is_declare_win_root(tree_t * restrict ptree, int sideToMove);
int get_thread_id(tree_t * restrict ptree);
bool is_selfplay();
double get_sigmoid_temperature_from_rate(int rate);
double get_sel_rand_prob_from_rate(int rate);
bool isKLDGainSmall(tree_t * restrict ptree, int sideToMove);
void init_KLDGain_prev_dist_visits_total(int games_sum);
void clear_opening_hash();
void make_balanced_opening(tree_t * restrict ptree, int sideToMove, int ply);

// yss_net.cpp
void init_network();
void replace_network(const char *token);
void stop_thread_submit();
void set_dcnn_channels(tree_t * restrict ptree, int sideToMove, int ply, float *p_data);
void prt_dcnn_data(float (*data)[B_SIZE][B_SIZE],int c,int turn_n);
void prt_dcnn_data_table(float (*data)[B_SIZE][B_SIZE]);
void make_move_id_c_y_x();
int get_move_from_c_y_x_id(int id);
int get_id_from_move(int pack_move);
void flip_dccn_move( int *bz, int *az, int *tk, int *nf );
int get_yss_z_from_bona_z(int to);
inline int pack_te(int bz,int az,int tk,int nf) {
	return (bz << 24) | (az << 16) | (tk << 8) | nf;
}
inline void unpack_te(int *bz,int *az,int *tk,int *nf, int te) {
	*bz = (te >> 24) & 0xff;
	*az = (te >> 16) & 0xff;
	*tk = (te >>  8) & 0xff;
	*nf = (te      ) & 0xff;
}
int get_yss_packmove_from_bona_move(int move);
float get_network_policy_value(tree_t * restrict ptree, int sideToMove, int ply, HASH_SHOGI *phg);
void add_dirichlet_noise(float epsilon, float alpha, HASH_SHOGI *phg);
int get_dlshogi_policy_id(int bz, int az, int tk, int nf);
int count_square_attack(tree_t * restrict ptree, int sideToMove, int square );
void kiki_count_indirect(tree_t * restrict ptree, int kiki_count[][81], int kiki_bit[][2][81], bool fKikiBit);
void update_HandicapRate(const char *token);
void update_AverageWinrate(const char *token);

#endif	//]] INCLUDE__GUARD
