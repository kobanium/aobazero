// 2019 Team AobaZero
// This source code is in the public domain.
/* yss.h  class����� */

#define SMP	// ����õ��������ˤ��������

#ifdef SMP
#define SMP_CRITICALSECTION	// lock()��critical section��Ȥ���硣1CPU�Ǥ�����Ǹ������硣HT�Ǥ�Ҥ�äȤ��ơ�������
#define SMP_HT				// HyperThreading �Ǥ�spin loop���pause̿��򤤤�롣Win64�Ǥϳ�����
#endif

#if defined(_MSC_VER)
#pragma warning (error:4706)		//  VC++�� ifʸ���������Error�ˡ�
#pragma warning (disable:4201)		//  VC++�� ̵̾��¤�Τηٹ����ɽ��
#pragma warning (disable:4514)		//  VC++�� ���Ȥ���ʤ�inline�ؿ��ٹ��OFF
#include <windows.h>
#include <process.h>
#include <io.h>
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#if defined(SMP)
#  define CPUS 16	// CPU�θĿ���Ʊ���˻׹ͤ�ư������åɿ���GUI����åɤϽ�����
#else
#  define CPUS 1
#endif


#ifdef YSSFISH
const int MAX_MOVES      = 593;
#endif
const int MAX_PLY        = 100;
const int MAX_PLY_PLUS_2 = MAX_PLY + 2;

enum Color {
	WHITE, BLACK, NO_COLOR	// WHITE �����(man)�֡�BLACK�ϸ��(com)��
};
enum Move {
};
enum Square256 {
};

inline Color operator~(Color c) {
  return Color(c ^ 1);
}



#include "yss_var.h"
#include "yss_pack.h"
#include "yss_dcnn.h"

#include <stdio.h>
#include <limits.h>




// ���饹�Ǿ����פ�������Ƥߤ�ȡ�������
class shogi {
public:
	int init_ban[BAN_SIZE];
	int mo_m[8];
	int mo_c[8];
	int ban[BAN_SIZE];           /** koma bangou ni yoru ban data **/
	int kiki_m[BAN_SIZE];        /** kiki no kazu man & com **/
	int kiki_c[BAN_SIZE];
	int kb_m[BAN_SIZE][KB_MAX];      /** ���������ơ�1�ս�ؤκ����18���ե�˻Ȥ���**/
	int kb_c[BAN_SIZE][KB_MAX];
	int kn[41][2];          /** ���ֹ� [][0]...����  [][1]...����        **/
	int  kn_stoc[8][32];    /** �����Ƥ�����ֹ档�������Ǥ���18������ **/
	int *kn_stn[8];         /**                 �ؤΥݥ���             **/
	int stoc[32];
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
	char nifu_table_com[16][10];
	char nifu_table_man[16][10];
#endif


	int saizen[D_MAX][D_MAX][4];	/* �����α�����򵭲� ���翼��64 */
	int *tejun[D_MAX];				/* ���ޤǤμ�礬���ä��ݥ��� */

	int kyokumen;				// õ�����̿�
	int read_max;               /* õ���³� */


	int fukasa_add;	// õ���³������ξ�������ʬ
	int fukasa;
	int tesuu;		// ���ߤμ�����ʺ�����ˤ���ѡ�
	int all_tesuu;	//
	int kifu[KIFU_MAX][5];

	// yss_main.cpp ɾ���ؿ�������

	int tume_hyouka;

	unsigned int hash_code1;
	unsigned int hash_code2;
	unsigned int hash_motigoma;		// ���(com)�λ����������򼨤�


	int ret_fukasa[D_MAX];		// ɾ���ͤη��ꤷ��������Ͽ


	// yss_dcnn.cpp
	int path[D_MAX];
	int          move_hit_kif[KIFU_MAX];
	unsigned int move_hit_hashcode[KIFU_MAX][3];

	// yss_ki1.cpp ������񤯸���Ū�ؿ���
	void fu_wm(int z);
	void fu_wc(int z);
	void fu_dm(int z);
	void fu_dc(int z);

	void yari_wm(int z);
	void yari_wc(int z);
	void yari_dm(int z);
	void yari_dc(int z);

	void kei_wm(int z);
	void kei_wc(int z);
	void kei_dm(int z);
	void kei_dc(int z);

	void gin_wm(int z);
	void gin_wc(int z);
	void gin_dm(int z);
	void gin_dc(int z);

	void kin_wm(int z);
	void kin_wc(int z);
	void kin_dm(int z);
	void kin_dc(int z);

	void ou_wm(int z);
	void ou_wc(int z);
	void ou_dm(int z);
	void ou_dc(int z);

	void ka_wm(int z);
	void ka_wc(int z);
	void ka_dm(int z);
	void ka_dc(int z);

	void hi_wm(int z);
	void hi_wc(int z);
	void hi_dm(int z);
	void hi_dc(int z);

	void um_wm(int z);
	void um_wc(int z);
	void um_dm(int z);
	void um_dc(int z);

	void um4_wm(int z);
	void um4_wc(int z);
	void um4_dm(int z);
	void um4_dc(int z);

	void ryu_wm(int z);
	void ryu_wc(int z);
	void ryu_dm(int z);
	void ryu_dc(int z);
	void ryu4_wm(int z);
	void ryu4_wc(int z);
	void ryu4_dm(int z);
	void ryu4_dc(int z);

	void hir_wm(int z);
	void hir_dm(int z);
	void hir_wc(int z);
	void hir_dc(int z);
	void hil_wm(int z);
	void hil_dm(int z);
	void hil_wc(int z);
	void hil_dc(int z);
	void hid_wm(int z);
	void hid_dm(int z);
	void hid_wc(int z);
	void hid_dc(int z);

	void kaur_wm(int z);
	void kaur_dm(int z);
	void kaur_wc(int z);
	void kaur_dc(int z);
	void kaul_wm(int z);
	void kaul_dm(int z);
	void kaul_wc(int z);
	void kaul_dc(int z);

	void kadr_wm(int z);
	void kadr_dm(int z);
	void kadr_wc(int z);
	void kadr_dc(int z);
	void kadl_wm(int z);
	void kadl_dm(int z);
	void kadl_wc(int z);
	void kadl_dc(int z);

	void ekw(int z);	// Error������
	void ekd(int z);	// Error������

	void allkaku(void);	// �����������
	void allkesu(void);

	// yss_ki2.cpp
	void init(void);	// kn[]���������ƺ����ꡣhash_code �� tume_hyouka �������
	void koma_koukan_table_init(void);	// ��򴹤򤷤���������򡢴��ܤζ�β��ͤ�����ľ����
	void init_without_koma_koukan_table_init();

	void move(int az,int bz,int nf); //  az... ��ư���� yx, bz... ���� yx, nf... ���ե饰
	void rmove(int az,int bz,int nf,int tk);
	void hit(int az,int uk);
	void rhit(int az);

	void move_hash(int az,int bz,int nf);			// �����ǡ����Ϲ���������hash�ͤ�������������
	void rmove_hash(int az,int bz,int nf,int tk);
	void hit_hash(int az,int uk);
	void rhit_hash(int az);
	
	inline void move_hit(int bz,int az,int tk,int nf) {		// ���η����̾��
//		PRT("%02x%02x%02x%02x, phash=%d\n",bz,az,tk,nf, phash(0, bz, az, tk) );
#ifdef YSSFISH_NOHASH
#ifdef _DEBUG
		if ( bz==0 ) { PRT("pass?\n"); debug(); }
#endif
		if ( bz==0xff ) hit(az,tk); 
		else move(az,bz,nf);
#else
		if ( bz==0 ) HashPass();
		else if ( bz==0xff ) hit(az,tk); 
		else move(az,bz,nf);
#endif
	}
	inline void remove_hit(int bz,int az,int tk,int nf) {
#ifdef YSSFISH_NOHASH
		if ( bz==0xff ) rhit(az);
		else rmove(az,bz,nf,tk);
#else
		if ( bz==0 ) HashPass();
		else if ( bz==0xff ) rhit(az);
		else rmove(az,bz,nf,tk);
#endif
	}

	inline void move_hit(int *p) {
		if ( *(p+0)==0 ) HashPass();
		else if ( *(p+0)==0xff ) hit(*(p+1),*(p+2)); 
		else move(*(p+1),*(p+0),*(p+3));
	}
	inline void remove_hit(int *p) {
		if ( *(p+0)==0 ) HashPass();
		else if ( *(p+0)==0xff ) rhit(*(p+1)); 
		else rmove(*(p+1),*(p+0),*(p+3),*(p+2));
	}

	inline void move_hit_hash(int bz,int az,int tk,int nf) {	// ���η���hash�ͤΤߡ�
		if ( bz==0 ) HashPass();
		else if ( bz==0xff ) hit_hash(az,tk); 
		else move_hash(az,bz,nf);
	}
	inline void remove_hit_hash(int bz,int az,int tk,int nf) {
		if ( bz==0 ) HashPass();
		else if ( bz==0xff ) rhit_hash(az);
		else rmove_hash(az,bz,nf,tk);
	}
	inline void move_hit_hash(int *p) {
		if ( *(p+0)==0 ) HashPass();
		else if ( *(p+0)==0xff ) hit_hash(*(p+1),*(p+2)); 
		else move_hash(*(p+1),*(p+0),*(p+3));
	}
	inline void remove_hit_hash(int *p) {
		if ( *(p+0)==0 ) HashPass();
		else if ( *(p+0)==0xff ) rhit_hash(*(p+1)); 
		else rmove_hash(*(p+1),*(p+0),*(p+3),*(p+2));
	}
	
	inline void do_moveYSS(Move m) {
		int bz = get_bz(m);
		int az = get_az(m);
		int tk = get_tk(m);
		int nf = get_nf(m);
		move_hit(bz,az,tk,nf);
	}
	inline void undo_moveYSS(Move m) {
		int bz = get_bz(m);
		int az = get_az(m);
		int tk = get_tk(m);
		int nf = get_nf(m);
		remove_hit(bz,az,tk,nf);
	}
	


	// yss.cpp
	void initialize_yss(void);	// YSS �ν��������
	void init_file_load_yss_engine();	// �����������

	void back_move(void);	// ����˽��äƶ��̤�ʤᡢ�᤹��---> �׹ͥ롼��������������Ǥ�Ƥ�Ǥ��롣
	void forth_move(void);
	void jump_move(int new_tesuu);	// ���ꤷ�����������

	void clear_all_data();
	void copy_org_ban(void);	// init_ban[] �� org_ban[]�򥳥ԡ�
	void PRT_hyoukati(void);	// ���ߤ�õ����̤򤤤���ɽ��

	void hyouji(void);
	void print_kakinoki_banmen(int (*func_prt)(const char *, ... ), char *sName0,char *sName1);
	void print_kb(void);
	void print_kn(void);
	void print_base(void);
	void print_kiki(void);
	void print_init_ban(void);
	void print_total_time(void);	// ��פλ׹ͻ��֤򻻽�
	void debug(void);		// �ǥХå���
	int kifu_set_move(int bz,int az,int tk,int nf, int t);
	int kifu_set_move(TE Te, int t);	// ���ʤ�ƴ���򥻥å�
	void kifu_set_move_sennitite(int bz,int az,int tk,int nf, int t);
	void kifu_set_move_sennitite(int te, int t);

	void save_banmen();				// ���̤��������
	int getMoveFromCsaStr(int *bz, int *az, int *tk, int *nf, char *str);
	int LoadCSA();
	void LoadKaki();
	int ReadOneMoveKi2(char *top_p, int *p_next_n);	// ����2�����δ����1���ɤ߹��ࡣ
	void LoadShotest(void);
	void LoadOutBan(void);
	void LoadTextBan();
	void SaveCSATume();
	void SaveCSA(char *sTagInfo, char *sKifDate);
	void SaveKaki(char *sTagInfo, char *sKifDate);
	void SetSennititeKif(void);	// ����������������Ͽ����

	// yss_base.cpp
	inline int nifu_c(register int z);
	inline int nifu_m(register int z);

	// yss_pvs.cpp
	int sennitite_check(int value, int *k);	// ����������å�
	int internal_sennitite_check(int *p_value);		// õ�������Ǥ��������Ĵ�٤�


	// yss_gra.cpp
	void change(int bz,int az,int tk,int nf,char retp[]);
	void change(int *p,char retp[]);
	void change_small(int bz,int az,int tk,int nf,char retp[]);
	void change_sg(int bz,int az,int tk,int nf,int depth,char retp[]);	// ��76�⡢��ɽ�����Ѵ����롣
	void change_log_pv(char *);
	void print_tejun(void);	// ���ߤޤǤ�õ������ɽ������ؿ��������Ƥ����ΤǤ����ǤޤȤ���
	void print_path();
	void print_te(int bz, int az,int tk,int nf);	// ���ɽ���������
	void print_te(HASH_RET *phr);					// �ϥå���μ��ɽ����
	void print_te(int *top_ptr);					// sasite[]�ʤɤμ��ɽ����
	void print_te(int pack);
	void print_te_no_space(int pack);
	void print_sasite(int *p, int nokori);			// sasite[] ����μ������ɽ������
	void print_te_width(int pack,int depth, int width);
	
	// yss_fusg.cpp
	void check_kn(void);	// ���̤������������å�

	// yss_ugok.cpp
	void hanten(void);				// ���̤�ȿž�������ǡ����������ؤ�
	void hanten_with_hash(void);	// ��������ȿž�����̤�ȿž�����ƥϥå������ľ����
	void hanten_sayuu(void);		// ������ȿž�����롣�ճ���������ʬ���ä����򤤤��⡣
	void clear_kb_kiki_kn();
	void clear_mo();
	void clear_init_ban();
	void hirate_ban_init(int n);/*** ���̤������֤��᤹�ʶ�����դ��� ***/
	void ban_saikousei(void);	// ���ߤλ��������̤ξ���(ban_init)�򸵤����̾��֤�ƹ������롣
	int is_hirate_ban();	// ʿ������̤�Ƚ�ꤹ��
	void hanten_with_hash_kifu();

	// yss_fmax.cpp
	int able_move( int bz, int az, int tk, int nf );
#ifdef _DEBUG
	int check_move_sub( int bz, int az, int tk, int nf, int hash_flag );	
	int check_move( int bz, int az, int tk, int nf );		// �ؤ��������å��ʼ��֤��θ���ʤ���
	int check_move_hash( int bz, int az, int tk, int nf );	// �����Υǡ���������å����ʤ��ǻؤ��������å�
#endif


	// yss_hash.cpp
	void hash_init(void);
	void hash_calc( int upsidedown, int complement );	// �ϥå����ͤη׻��Τߡ�����ȿž�ȡ��ӥå�ȿž
	void set_sennitite_info();

	inline void HashPass() { hash_code1 = ~hash_code1;	hash_code2 = ~hash_code2; }	// �ѥ�����(�ϥå����ͤΤ�)
	inline void HashXor(int k,int z) {
		unsigned int *p = hash_ban[k][z];
		hash_code1 ^= *(p+0);
		hash_code2 ^= *(p+1);
	}

	// yss_bwh1.cpp
	void trans_4_to_2_KDB(int b0,int b1, int num, int *bz,int *az, int *tk, int *nf);	// ����������2�Х��Ȥ�4�Х��Ȥ��Ѵ���

	// yss_dcnn.cpp
	void make_policy_leveldb();
	void set_dcnn_channels(Color sideToMove, const int ply, float *p_data, int stock_num, int net_type);
	void setYssChannels(Color sideToMove, int moves, float *p_data, int net_kind, int input_num);
	int get_cnn_next_move();
	HASH_SHOGI* HashShogiReadLock();
	uint64 get_hashcode64() { return (((uint64)hash_code1 << 32) | hash_code2) ^ hash_motigoma; }
	double uct_tree(Color sideToMove);
	void create_node(Color sideToMove, HASH_SHOGI *phg);
	Color flip_color(Color sideToMove) { return ~sideToMove; }
	int uct_search_start(Color sideToMove);
	int think_kifuset();
	void update_zero_kif_db();
	void copy_restore_dccn_init_board(int fCopy);
	void prepare_kif_db(int fPW, int mini_batch, float *data, float *label_policy, float *label_value, float label_policy_visit[][MOVE_C_Y_X_ID_MAX]);
	void init_prepare_kif_db();
	float get_network_policy_value(Color sideToMove, int ply, HASH_SHOGI *phg);
	char *prt_pv_from_hash(int ply);
	void add_one_kif_to_db();
	void load_exist_all_kif();
	int wait_and_get_new_kif(int next_weight_n);
	int add_a_little_from_archive();
	int make_www_samples();



	// fish��Ϣ
	bool is_pseudo_legalYSS(Move m, Color sideToMove);
	bool is_pl_move_is_legalYSS(Move m, Color sideToMove);
	// ���μ꤬����ˤʤ뤫���Ť��ʡ�����Ŭ���Ǥ��������	
	inline int is_move_gives_check(Move m, Color sideToMove) {
		const Color them = ~sideToMove;
		const int teki_ou_z = kn[1+them][1];
		return is_move_attack_sq256(m, sideToMove, Square256(teki_ou_z));
	}
	int  is_move_attack_sq256(Move m, Color sideToMove, Square256 attack_z);
	int  is_drop_atari_himo(Move m);
	bool is_pin_safe_move_man(const int az);
	bool is_pin_safe_move_com(const int az);


	int IsSenteTurn();
	int GetSennititeMax();

};


#if defined(AISHOGI)	//[
#define MAX_BLOCKS 16	// �ݻ���ǽ�����̹�¤�Το�
#else
#define MAX_BLOCKS 64	// �ݻ���ǽ�����̹�¤�Το�
#endif

#ifdef SMP
	extern shogi          local[MAX_BLOCKS+1];	// ���줬õ���ǻȤ����饹���Ρ�
	extern lock_t         lock_smp, lock_root, lock_io; 
	extern int            lock_io_init_done;

	// �ؿ�
	void InitLockYSS();
#else
	extern shogi          local[1];				// õ���ǻȤ����饹���Ρ�
#endif

extern shogi *PS;	// ��ʸ����PS�ϥ����Х�ǻȤ����̾� local[0] �򼨤��Ƥ���

// ����饤��ؿ�������ʥ���饤��ϥإå��ե�������������ɬ�פ������
// 2�������å����⤬�ǤƤ��1���֤���
inline int shogi::nifu_c(register int z)
{
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
	return (nifu_table_com[1][z&0x0f]);
#else
#endif
}

inline int shogi::nifu_m(register int z)
{
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
	return (nifu_table_man[1][z&0x0f]);
#else
#endif
}

