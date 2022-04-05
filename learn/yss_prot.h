// 2019 Team AobaZero
// This source code is in the public domain.
// yss_prot.h
// 関数の中で、外部で呼ばれるもののみを書いている・・・つもり '95/11/19
#ifndef INCLUDE_YSS_PROT_H_GUARD	//[
#define INCLUDE_YSS_PROT_H_GUARD

/* YSS_BASE */
void debug();
void str_euc2sjis(char *p);
void str_sjis2euc(char *p);

/* YSS_HASH.C */
int rand_yss();			// 乗数合同法により乱数を算出。
void init_rnd521(unsigned long seed);
unsigned long rand_m521();
void hash_ban_init();
void hash_all_clear();
void hash_alloc_big();
int get_localtime(int *year, int *month, int *day, int *week, int *hour, int *minute, int *second);
int print_time();

void hash_table_all_free();


/* YSS_GRA.H */
void print_teban(int fukasa_teban);	// 手番によって先後マークを表示するだけ
void print_4tab(int dep);			// 深さによって4文字空白を書くだけ
char *get_str_zahyo(int z);



/* YSS_FMAX.C */
void make_move_is_check_table();
void make_move_is_tobi_check_table();


/* YSS_HISI.C */
void hanten_sasite( int *bz, int *az, int *tk, int *nf );
void hanten_sasite_sayuu( int *bz, int *az, int *tk, int *nf );



/* YSS_WIN.C Win95依存関数群 */
int PRT(const char *fmt, ...);	// 可変のリストを持つprintf代用関数。printf() の書式文字列は const
void WaitKeyInput(void);	// デバッグ用のメッセージボックス
void PassWindowsSystem();	// Windowsへ一時的に制御を渡す。User からの指示による思考停止。
void denou_kif_add(int bz,int az,int tk,int nf,int bz_k, int moves, int t);
void denou_kif_name_add(char *p0,char *p1);


/* YSS_PVS.C */
double get_spend_time(int ct1);				// 経過時間を返す
double get_diff_sec(int diff_ct);

// yss_dcnn.cpp
void make_unique_from_to();
int get_unique_from_te(int bz,int az, int tk, int nf);
int get_te_from_unique(int u);
void flip_horizontal_channels(int stock_num);
void make_move_id_c_y_x();
void get_c_y_x_from_move(int *pc, int *py, int *px, int pack_move);
int get_move_id_c_y_x(int pack);
int get_move_from_c_y_x_id(int id);

 
// yss.cpp
void PRT_sub(char *fmt, ...);
int PRT_CHAR_ALL(char *s);
void debug_set(const char *file, int line);
void debug_print(const char *fmt, ... );
#define DEBUG_PRT (debug_set(__FILE__,__LINE__), debug_print)	

void SleepMsec(int mill_sec);
int IsNan(double dx);
void PRT_OFF();
void PRT_ON();
void PRT_FILE(int flag);
void PassWindowsFlagOff();				// fPassWindowsフラグを0にするだけ。
int GetSystemMemoryMB();
int open_one_file(char *filename);
int change_dir(const char *sDir);
void return_dir();
int get_next_file(char *filename, const char *ext);
int get_next_file_from_dirs(char *filename, const char *ext, const char *dir_list[]);
int get_learning_dir_number();
int strlen_int(const char *);	// 文字列の長さをint型を返すだけ。Win64での警告対策。
const char *get_kdbDirSL();
const char *getCPUInfo();
const char *getYssCPU();
int KIF_OUT(const char *fmt, ...);	// KIFのバッファに書き出す
void InitGameInfo();				// 後手から指してるか、対局者名、などを初期化
void ClearKifuComment();			// 棋譜のコメント、ログをクリアする。

void check_input();
int get_clock();	// msec 単位の経過時間を返す。clock()はlinuxのマルチスレッドではおかしくなる
int getpid_YSS();
int read_YSS(int handle, void *buf, unsigned int count);
int fileno_YSS( void *stream );
void write_down_file_prt_and_die();
void print_usi_best_update_fish(const char *str);
void OffFishTaikyokukan();
void OnFishTaikyokukan();
int count_same_position();


void pack_from4_to_2_KDB(int *pb0, int *pb1, int bz, int az, int tk, int nf);

#endif	//] INCLUDE_YSS_PROT_H_GUARD
