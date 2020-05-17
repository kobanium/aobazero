// 2019 Team AobaZero
// This source code is in the public domain.
// yss_dcnn.h
#ifndef INCLUDE_YSS_DCNN_H_GUARD	//[
#define INCLUDE_YSS_DCNN_H_GUARD

#include "lock.h"

const int B_SIZE = 9;
const int DCNN_CHANNELS = 362;
const int LABEL_CHANNELS = 139;


const int SHOGI_MOVES_MAX = 593;
const float ILLEGAL_MOVE = -1000;

typedef struct child {
	int   move;			// position
	int   games;		// number of selected
	float value;		// win rate (win=+1, loss=0)
	float bias;			// policy
} CHILD;

#define CHILD_VEC

typedef struct hash_shogi {
	lock_yss_t entry_lock;		// lock for SMP
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
  WHITE, BLACK, NO_COLOR	// WHITE is Sente(man) turn, BLACK is Gote(com) turn
};

extern int fAddNoise;
extern int fVisitCount;
extern int fUSIMoveCount;
extern int fPrtNetworkRawPath;
extern int nNNetServiceNumber;

extern std::string default_weights;
#ifdef USE_OPENCL
extern std::vector<int> default_gpus;
#endif

extern int usi_go_count;
extern int usi_bestmove_count;

void debug();
void debug_set(const char *file, int line);
void debug_print(const char *fmt, ... );
#define DEBUG_PRT (debug_set(__FILE__,__LINE__), debug_print)	
void PRT(const char *fmt, ...);
int get_clock();
double get_spend_time(int ct1);
void create_node(tree_t * restrict ptree, int sideToMove, int ply, HASH_SHOGI *phg);
double uct_tree(tree_t * restrict ptree, int sideToMove, int ply);
int uct_search_start(tree_t * restrict ptree, int sideToMove, int ply, char *buf_move_count);
void print_all_min_posi(tree_t * restrict ptree, int ply);
//int check_enter_input();
int check_stop_input();
int is_ignore_stop();
void send_latest_bestmove();
void set_latest_bestmove(char *str);
int is_send_usi_info(int nodes);
void send_usi_info(tree_t * restrict ptree, int sideToMove, int ply, int nodes, int nps);
void usi_newgame();
int is_declare_win(tree_t * restrict ptree, int sideToMove);
int is_declare_win_root(tree_t * restrict ptree, int sideToMove);

// yss_net.cpp
void init_network();
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

#endif	//]] INCLUDE__GUARD
