// 2019 Team AobaZero
// This source code is in the public domain.
/* yss.h  classの定義 */



#define F2187



#define SMP	// 並列探索する場合には定義する

#ifdef SMP
#define SMP_CRITICALSECTION	// lock()にcritical sectionを使う場合。1CPUでの並列で効果絶大。HTでもひょっとして・・・？
#define SMP_HT				// HyperThreading ではspin loop中にpause命令をいれる。Win64では外す。
#endif

#if defined(_MSC_VER)
#pragma warning (error:4706)		//  VC++用 if文中の代入をErrorに。
#pragma warning (disable:4201)		//  VC++用 無名構造体の警告を非表示
#pragma warning (disable:4514)		//  VC++用 参照されないinline関数警告をOFF
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
#  define CPUS 16	// CPUの個数。同時に思考で動くスレッド数（GUIスレッドは除く）
#else
#  define CPUS 1
#endif


#ifdef YSSFISH
const int MAX_MOVES      = 593;
#endif
const int MAX_PLY        = 100;
const int MAX_PLY_PLUS_2 = MAX_PLY + 2;

enum Color {
	WHITE, BLACK, NO_COLOR	// WHITE は先手(man)番、BLACKは後手(com)番
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




// クラスで将棋盤を定義してみると・・・？
class shogi {
public:
	int init_ban[BAN_SIZE];
	int mo_m[8];
	int mo_c[8];
	int ban[BAN_SIZE];           /** koma bangou ni yoru ban data **/
	int kiki_m[BAN_SIZE];        /** kiki no kazu man & com **/
	int kiki_c[BAN_SIZE];
	int kb_m[BAN_SIZE][KB_MAX];      /** 利きの内容。1箇所への最大は18。フルに使う。**/
	int kb_c[BAN_SIZE][KB_MAX];
	int kn[41][2];          /** 駒番号 [][0]...種類  [][1]...位置        **/
	int  kn_stoc[8][32];    /** 空いている駒番号。歩の枚数である18が最大 **/
	int *kn_stn[8];         /**                 へのポインタ             **/
	int stoc[32];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
	char nifu_table_com[16][10];
	char nifu_table_man[16][10];
#endif


	int saizen[D_MAX][D_MAX][4];	/* 最善の応手手順を記憶 最大深さ64 */
	int *tejun[D_MAX];				/* 今までの手順が入ったポインタ */

	int kyokumen;				// 探索局面数
	int read_max;               /* 探索限界 */


	int fukasa_add;	// 探索限界深さの小数点部分
	int fukasa;
	int tesuu;		// 現在の手数。（再生中にも使用）
	int all_tesuu;	//
	int kifu[KIFU_MAX][5];

	// yss_main.cpp 評価関数で利用

	int tume_hyouka;

	unsigned int hash_code1;
	unsigned int hash_code2;
	unsigned int hash_motigoma;		// 後手(com)の持ち駒の枚数を示す


	int ret_fukasa[D_MAX];		// 評価値の決定した深さを記録


	// yss_dcnn.cpp
	int path[D_MAX];
	int          move_hit_kif[KIFU_MAX];
	unsigned int move_hit_hashcode[KIFU_MAX][3];

	// yss_ki1.cpp 利きを書く原始的関数群
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

	void ekw(int z);	// Error処理用
	void ekd(int z);	// Error処理用

	void allkaku(void);	// 全部利きを書く
	void allkesu(void);
	void kiki_write(int z);
	void kiki_delete(int z);

	// yss_ki2.cpp
	void init(void);	// kn[]を初期化して再設定。hash_code と tume_hyouka を再設定
	void koma_koukan_table_init(void);	// 駒交換をした場合の配列を、基本の駒の価値から作り直す。
	void init_without_koma_koukan_table_init();

	void move(int az,int bz,int nf); //  az... 移動する yx, bz... 元の yx, nf... 成フラグ
	void rmove(int az,int bz,int nf,int tk);
	void hit(int az,int uk);
	void rhit(int az);

	void move_hash(int az,int bz,int nf);			// 利きデータは更新せずにhash値だけ更新する場合
	void rmove_hash(int az,int bz,int nf,int tk);
	void hit_hash(int az,int uk);
	void rhit_hash(int az);
	
	inline void move_hit(int bz,int az,int tk,int nf) {		// 一体型（通常）
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

	inline void move_hit_hash(int bz,int az,int tk,int nf) {	// 一体型（hash値のみ）
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
	void initialize_yss(void);	// YSS の初期化処理
	void init_file_load_yss_engine();	// 総合初期化処理

	void back_move(void);	// 棋譜に従って局面を進め、戻す。---> 思考ルーチンの定跡生成でも呼んでいる。
	void forth_move(void);
	void jump_move(int new_tesuu);	// 指定した手数へ飛ぶ

	void clear_all_data();
	void copy_org_ban(void);	// init_ban[] に org_ban[]をコピー
	void PRT_hyoukati(void);	// 現在の探索結果をいろいろ表示

	void hyouji(void);
	void print_kakinoki_banmen(int (*func_prt)(const char *, ... ), char *sName0,char *sName1);
	void print_kb(void);
	void print_kn(void);
	void print_base(void);
	void print_kiki(void);
	void print_init_ban(void);
	void print_total_time(void);	// 合計の思考時間を算出
	void debug(void);		// デバッグ用
	int kifu_set_move(int bz,int az,int tk,int nf, int t);
	int kifu_set_move(TE Te, int t);	// 手を進めて棋譜をセット
	void kifu_set_move_sennitite(int bz,int az,int tk,int nf, int t);
	void kifu_set_move_sennitite(int te, int t);

	void save_banmen();				// 盤面だけを出力
	int getMoveFromCsaStr(int *bz, int *az, int *tk, int *nf, char *str);
	int LoadCSA();
	void LoadKaki();
	int ReadOneMoveKi2(char *top_p, int *p_next_n);	// 柿木2形式の棋譜を1手読み込む。
	void LoadShotest(void);
	void LoadOutBan(void);
	void LoadTextBan();
	void SaveCSATume();
	void SaveCSA(char *sTagInfo, char *sKifDate);
	void SaveKaki(char *sTagInfo, char *sKifDate);
	void SetSennititeKif(void);	// 千日手も棋譜情報に登録する

	// yss_base.cpp
	inline int nifu_c(register int z);
	inline int nifu_m(register int z);

	// yss_pvs.cpp
	int sennitite_check(int value, int *k);	// 千日手チェック
	int internal_sennitite_check(int *p_value);		// 探索内部での千日手を調べる


	// yss_gra.cpp
	void change(int bz,int az,int tk,int nf,char retp[]);
	void change(int *p,char retp[]);
	void change_small(int bz,int az,int tk,int nf,char retp[]);
	void change_sg(int bz,int az,int tk,int nf,int depth,char retp[]);	// ▲76歩、の表記に変換する。
	void change_log_pv(char *);
	void print_tejun(void);	// 現在までの探索手順を表示する関数（増えてきたのでここでまとめる）
	void print_path();
	void print_te(int bz, int az,int tk,int nf);	// 手を表示するだけ
	void print_te(HASH_RET *phr);					// ハッシュの手を表示。
	void print_te(int *top_ptr);					// sasite[]などの手を表示。
	void print_te(int pack);
	void print_te_no_space(int pack);
	void print_sasite(int *p, int nokori);			// sasite[] 配列の手を全部表示する
	void print_te_width(int pack,int depth, int width);
	
	// yss_fusg.cpp
	void check_kn(void);	// 盤面が正当かチェック

	// yss_ugok.cpp
	void hanten(void);				// 盤面の反転。利きデータの入れ替え
	void hanten_with_hash(void);	// 完全盤面反転。局面を反転させてハッシュも作り直す。
	void hanten_sayuu(void);		// 左右を反転させる。意外と盲点が分かって面白いかも。
	void clear_kb_kiki_kn();
	void clear_mo();
	void clear_init_ban();
	void hirate_ban_init(int n);/*** 盤面を初期状態に戻す（駒落ち付き） ***/
	int get_handicap_from_board();
	void ban_saikousei(void);	// 現在の持ち駒、盤面の状態(ban_init)を元に盤面状態を再構成する。
	int is_hirate_ban();	// 平手の盤面か判定する
	void hanten_with_hash_kifu();

	// yss_fmax.cpp
	int able_move( int bz, int az, int tk, int nf );
#ifdef _DEBUG
	int check_move_sub( int bz, int az, int tk, int nf, int hash_flag );	
	int check_move( int bz, int az, int tk, int nf );		// 指し手をチェック（手番を考慮しない）
	int check_move_hash( int bz, int az, int tk, int nf );	// 利きのデータをチェックしないで指し手をチェック
#endif


	// yss_hash.cpp
	void hash_init(void);
	void hash_calc( int upsidedown, int complement );	// ハッシュ値の計算のみ。盤面反転と、ビット反転
	void set_sennitite_info();

	inline void HashPass() { hash_code1 = ~hash_code1;	hash_code2 = ~hash_code2; }	// パスする(ハッシュ値のみ)
	inline void HashXor(int k,int z) {
		unsigned int *p = hash_ban[k][z];
		hash_code1 ^= *(p+0);
		hash_code2 ^= *(p+1);
	}

	// yss_bwh1.cpp
	void trans_4_to_2_KDB(int b0,int b1, bool bGoteTurn, int *bz,int *az, int *tk, int *nf);	// 棋泉形式の2バイトを4バイトに変換。

	// yss_dcnn.cpp
	void make_policy_leveldb();
	void set_dcnn_channels(Color sideToMove, const int ply, float *p_data, int stock_num, int nHandicap);
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
	void copy_restore_dccn_init_board(int handicap, bool fCopy);
#ifdef F2187
	void prepare_kif_db(int fPW, int mini_batch, float *data, float *label_policy, float *label_value, float label_policy_visit[][2187]);
#else
	void prepare_kif_db(int fPW, int mini_batch, float *data, float *label_policy, float *label_value, float label_policy_visit[][MOVE_C_Y_X_ID_MAX]);
#endif
	void init_prepare_kif_db();
	float get_network_policy_value(Color sideToMove, int ply, HASH_SHOGI *phg);
	char *prt_pv_from_hash(int ply);
	void add_one_kif_to_db();
	void load_exist_all_kif();
	int wait_and_get_new_kif(int next_weight_n);
	int add_a_little_from_archive();
	int make_www_samples();



	// fish関連
	bool is_pseudo_legalYSS(Move m, Color sideToMove);
	bool is_pl_move_is_legalYSS(Move m, Color sideToMove);
	// この手が王手になるか？重いな・・・適当でお茶を濁す	
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
#define MAX_BLOCKS 16	// 保持可能な盤面構造体の数
#else
#define MAX_BLOCKS 64	// 保持可能な盤面構造体の数
#endif

#ifdef SMP
	extern shogi          local[MAX_BLOCKS+1];	// これが探索で使うクラス本体。
	extern lock_t         lock_smp, lock_root, lock_io; 
	extern int            lock_io_init_done;

	// 関数
	void InitLockYSS();
#else
	extern shogi          local[1];				// 探索で使うクラス本体。
#endif

extern shogi *PS;	// 大文字のPSはグローバルで使う。通常 local[0] を示している

// インライン関数の定義（インラインはヘッダファイルで定義する必要がある）
// 2歩をチェック（歩が打てれば1を返す）
inline int shogi::nifu_c(register int z)
{
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
	return (nifu_table_com[1][z&0x0f]);
#else
#endif
}

inline int shogi::nifu_m(register int z)
{
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
	return (nifu_table_man[1][z&0x0f]);
#else
#endif
}

