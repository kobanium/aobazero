// 2019 Team AobaZero
// This source code is in the public domain.
// Yamashita Shogi System    "yss_ext.h"
#ifndef INCLUDE_YSS_EXT_H_GUARD	//[
#define INCLUDE_YSS_EXT_H_GUARD


#define LEARNING_EVAL		// 学習評価関数を使う(基本)
#define AOBA_ZERO


#define DRAW_VALUE (+0)		// 千日手の点数。選手権の場合、状況によって変わる。通常は0点。引き分けでも勝ちなら(+15000)

#define NIFU_TABLE			// 2歩データを常にテーブルに保持する場合。0.6%(終盤)〜1.2%(序盤)の高速化。12.4秒---> 12.2秒、47.2秒--->47.0秒。、

/************************ initialize *************************/

#ifdef _DEBUG
const int DEBUG_FLAG = 1;
#else
const int DEBUG_FLAG = 0;
#endif

// AI将棋ではこのファイルを別の場所から見てるので変更したときは完全リビルドを！(最低でもyss2005をリビルド)
#if defined(AISHOGI)	//[
#define D_MAX 64			// 最大探索深さ  AI将棋の詰将棋で寿(611手)が解けるように。SMPだとDLLがサイズオーバー。むむ。
#else
//#define D_MAX 1024			
#define D_MAX 64			// 最大探索深さ  64手で固定 ...2048だとsaizen[]が64MBも食う！  
#endif	//]
#define SST_MAX 256			// 最大生成手数 128手で固定 ... 64手だと背尾詰の合駒であふれる。
#define TOP_SST_MAX 600		// rootでの最大生成手数。理論上の最大は593手。
#define SST_REAL_MAX 593	// 将棋の最大分岐数。
#define RML_MAX 20			// 最大反復深さ。20回まで反復深化を行う。
#if defined(AISHOGI)
#define KB_MAX	32			// kb_c[][32] を18に変更してサイズの縮小を試みる。2.0%も速く！なった（脊尾詰）。実探索では0.4%遅くなった。
#else
#define KB_MAX	16
#endif
#define BAN_SIZE 256		// [256] でなく、最小の 11*16=160+16= [176] にしてみると？---> 256が一番速い

#define TADASUTE_OK_DEPTH 1		// (fukasa<=TADASUTE_OK_DEPTH)    ならばただで捨てる歩も生成
#define TADASUTE_FU_OK_DEPTH 3	// (fukasa<=TADASUTE_FU_OK_DEPTH) 地平線対策をしている深さ。3手目、4手目の歩垂らしで

#define KAKO_BEST_SAME_MAX 5	// 同一の反復深化で最大何手まで最善手が入れ替わったか、を保持するか。

extern int org_ban[BAN_SIZE];	// 初期配置のみに使用
extern int org_mo_m[8];
extern int org_mo_c[8];
 
extern const char *koma[32];
extern const char *koma_kanji[32];
extern const char *num_kanji[];
extern const char *num_zenkaku[];

#define KIFU_MAX 2048	// 棋譜の手数は最大で2048手（1529手詰,ミクロコスモスを考慮）

#define PRT_LEN_MAX 512 // 一度に表示可能な最大文字数
#define TMP_BUF_LEN 256	// 一時的な文字列の長さ。
extern char kifu_comment[KIFU_MAX][TMP_BUF_LEN];	// 今のところ千日手のみを記録	
extern char kifu_log[KIFU_MAX][2][TMP_BUF_LEN];		// 1手ごとのログを記録
extern char tmp_log[2][TMP_BUF_LEN];				// ログを一時的に記録
const int KIFU_LOG_TOP_MAX = 5;
extern char kifu_log_top[KIFU_LOG_TOP_MAX][TMP_BUF_LEN];	// 最初のコメントを記録
const int USI_POS_MAX_SIZE = 8192;
extern char PlayerName[2][64];	// 先手、後手の名前
extern char sGameDate[];
#define KIF_BUF_MAX (65536*16)
extern unsigned long nKifBufSize;	// バッファにロードしたサイズ
extern unsigned long nKifBufNum;	// バッファの先頭位置を示す
extern char KifBuf[KIF_BUF_MAX];	// バッファ
extern int fSkipLoadKifBuf;

extern int KomaOti;
extern int nSennititeMax;
extern char Kisen[];
extern int  nKifDateYMD;


#define CLOCKS_PER_SEC_MS (1000)	// CLOCKS_PER_SEC を統一。linuxではより小さい.

/** from "yss_init" **/

extern int maisu_kati[8][18];	// 持駒の枚数による、駒自体の価値
extern int mo_fuka_kati[8][18];	// 持駒の枚数による付加価値

extern  int m_kati[8][32];
extern  int   kati[16];
extern  int n_kati[16];
extern  int k_kati[16];
extern const int z8[8];
extern const int oute_can[4][16];
extern const int oute_z[2][8];

extern int total_time[2];		// 先手、後手の累計思考時間。


extern int utikiri_time;			// 思考を打ち切る時間(秒)
extern int saigomade_depth;			// 0...詰みあがりまで指す、1...3手詰程度で投げる。
	extern int saigomade_flag;			// AI将棋のInterfaceのためだけに存在
	extern int saigomade_read_max;		// 同じく
extern int read_max_limit;			// この深さまで反復深化法で読む。
extern int capture_limit;			// 序盤で駒取り延長をかける駒の価値。(= 香車)だと香が取れるなら延長。

extern int base[BAN_SIZE];


extern int senkei;					// 戦型を示す
extern int kibou_senkei;			// COMにさせたい戦型。普通は矢倉。
extern int shuuban;					// 終盤を示すフラグ

extern int Utikiri_Time_Up;			// 製品版用にグローバル変数にする。レベルの設定で代入。
extern int Jissen_Stop;				// 実戦詰将棋が停止する回数 K6-III 450MHz で 1000回=約0.053秒か

extern int thinking_utikiri_flag;	// 思考時間切れの打ち切りフラグ。
extern int fPassWindows;			// Windowsに制御を返すか？（同時に予測探索処理中を意味する---> 予測思考中は別）
extern int seo_view_off;			// 確率計算時に余計な情報を吐き出さない時に1。
extern int fPrtCut;					// 強制的にPRT出力を止める
extern int fGotekara;

extern int ignore_moves[TOP_SST_MAX][4];
extern int ignore_moves_num;

extern int root_nokori;			// 初手で生成した手数

extern unsigned int hash_ban[32][BAN_SIZE][2];	// ハッシュ用乱数データ。32ビット×2で配列が２つある。
extern const unsigned int hash_mask[3][8];		// [0]..value, [1]..mask, [2]..shift
#define HASH_MOTIGOMA_SIGN_BITS 0x9222220		// hash_mask[][]の定義を参照のこと

extern int hash_use;				/**  ハッシュ使用数  **/
extern int hash_over;				/**  再ハッシュ回数  **/
extern int horizon_hash_use;		// 地平線効果用のハッシュ登録数

extern int totta_koma[18][16];
extern int utta_koma[19][8];
extern int m_koma_plus[18][8];		// m_kati[][n] - m_kati[][n-1];


extern const int mo_start[2][4];	// 持ち駒が王手になる始まり（位置による）飛角の打ちは後で読む、kikituke専用
extern const int bit_oute[8][16];	// 駒打王手をパターン認識で判定



typedef	struct {	// 指し手データの構造体（ハッシュテーブルでのみ使用！型は char )
	unsigned char bz;
	unsigned char az;
	unsigned char tk;
	unsigned char nf;
} TE;


// int は 32ビットを前提にしている。hash の合計は32バイトになるように調節.。64bit+mo(20bit)+v(16bit)+mv(24bit)+dp(10bit)
typedef struct HASH {	// タグ名を付けないと前方参照できなくて不便
	// clear する時にアクセスする部分が 1 DWORD になるように配置調整 ---> 武藤さんによる。
	// char が連続するように。
	// flag を最初に参照するので最初に置く（CPUのメモリのバースト転送を考慮）
	// ---> lock[1]を最初に。Win64でアセンブラが使えないので。singleでは速度変わらず。
	volatile char lock[1];		// SMPでのロック用。hashのみで使用。LockChar()を使う。
	unsigned char hit_num;		// このハッシュを参照した回数。(4バイトにするためのダミー)
	unsigned short int flag;	// この評価値が正確な値か、もしかは上限下限か。0で未登録。
								// 0000 0000 
								// 実際の探索用4bit	  00... 未使用 01...正確な値 10 ...上限、又は下限 11...ハッシュOVER	100...最善手のみ登録
								// 脊尾詰com   2bit	  00... 未使用 01...使用     10 ...打歩詰フラグON
								// 脊尾詰man   2bit
								// 通常詰com   1bit	  0 ... 未使用 1 ... 使用
								// 通常詰man   1bit

								// 自分だけが使用しているか、又は誰も使用していなければ書きこみ可
								// if ( (flag & 11110011)==0 ) OK!


	unsigned int hash_code1;	// ハッシュコード 01。後手番は反転させる
	unsigned int hash_code2;	// ハッシュコード 02。計64ビット。2^32局面を作って誤認確率は１％。
	unsigned int motigoma;

	union {		// 無名共用体（今のところ一つだけなのでコメント）
		struct {
			unsigned int pn;
			unsigned int dn;
			unsigned int nodes;		// この情報を得るのに探索した局面数
			unsigned short min_distance;	// 最小距離
			unsigned short dummy;
		};
		struct {
			int value;					// 評価値
			int depth;					// 探索深さ（相対的な深さ）256の倍数で小数点を表す
			union {
				struct {
					unsigned char man_depth;	// 詰将棋MAN用探索深さ（相対的）
					unsigned char com_depth;	//       COM用
					unsigned char man_tume;		// 詰め将棋の評価値（詰，切れ，不明，・・自由度）
					unsigned char com_tume;
				};
				struct {
					unsigned int seo_nodes;		// この情報を得るのに探索した局面数
				};
			};
			TE best;					// この局面での最善手 
		};
	};
} HASH;



extern HASH sennitite[KIFU_MAX];		// ハッシュで以前の局面を記憶

// ハッシュテーブルを検索した時の返す構造体
// flag = 1...正確な値。 2...上限、又は下限。 0...未登録。 3...ハッシュオーバーで未登録。
// flag = 8...打歩詰になる。
typedef struct {
	int value;	// 評価値
	int depth;	// その局面から探索した深さ
	int flag;	// 評価値が正確な値か、もしかは上限下限か、又は未登録の局面か
	unsigned int nodes;	// 探索した局面数
	union {		// 無名共用体
		struct {
			int bz;		// 最善手
			int az;
			int tk;
			int nf;
		};
		struct {
			int pn;
			int dn;
			int dummy1;
			int dummy2;
		};
	};
	short static_value;
} HASH_RET;


#define TUMI	 	25000		/* 詰んだ時の値（COMが詰むときは-TUMI）*/
#define FUMEI 		32000		/* 詰め将棋で不明の時（COMでもMANでも一緒）*/

#define DEPTH_LIMIT	(D_MAX-12)	// この深さを超えたら探索はしない。指し手生成が64手の深さまでだから。
								// +10手ぐらいはワークエリアで使うので安全のために


#endif	//] INCLUDE_YSS_EXT_H_GUARD
