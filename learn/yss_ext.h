// 2019 Team AobaZero
// This source code is in the public domain.
// Yamashita Shogi System    "yss_ext.h"
#ifndef INCLUDE_YSS_EXT_H_GUARD	//[
#define INCLUDE_YSS_EXT_H_GUARD


#define LEARNING_EVAL		// �ؽ�ɾ���ؿ���Ȥ�(����)
#define AOBA_ZERO


#define DRAW_VALUE (+0)		// ����������������긢�ξ�硢�����ˤ�ä��Ѥ�롣�̾��0��������ʬ���Ǥ⾡���ʤ�(+15000)

#define NIFU_TABLE			// 2��ǡ������˥ơ��֥���ݻ������硣0.6%(����)��1.2%(����)�ι�®����12.4��---> 12.2�á�47.2��--->47.0�á���

/************************ initialize *************************/

#ifdef _DEBUG
const int DEBUG_FLAG = 1;
#else
const int DEBUG_FLAG = 0;
#endif

// AI�����ǤϤ��Υե�������̤ξ�꤫�鸫�Ƥ�Τ��ѹ������Ȥ��ϴ�����ӥ�ɤ�(����Ǥ�yss2005���ӥ��)
#if defined(AISHOGI)	//[
#define D_MAX 64			// ����õ������  AI�����ε;����Ǽ�(611��)���򤱤�褦�ˡ�SMP����DLL�������������С�����ࡣ
#else
//#define D_MAX 1024			
#define D_MAX 64			// ����õ������  64��Ǹ��� ...2048����saizen[]��64MB�⿩����  
#endif	//]
#define SST_MAX 256			// ����������� 128��Ǹ��� ... 64����������ͤι��Ǥ��դ�롣
#define TOP_SST_MAX 600		// root�Ǥκ������������������κ����593�ꡣ
#define SST_REAL_MAX 593	// �����κ���ʬ������
#define RML_MAX 20			// ����ȿ��������20��ޤ�ȿ��������Ԥ���
#if defined(AISHOGI)
#define KB_MAX	32			// kb_c[][32] ��18���ѹ����ƥ������ν̾����ߤ롣2.0%��®�����ʤä��������͡ˡ���õ���Ǥ�0.4%�٤��ʤä���
#else
#define KB_MAX	16
#endif
#define BAN_SIZE 256		// [256] �Ǥʤ����Ǿ��� 11*16=160+16= [176] �ˤ��Ƥߤ�ȡ�---> 256������®��

#define TADASUTE_OK_DEPTH 1		// (fukasa<=TADASUTE_OK_DEPTH)    �ʤ�Ф����ǼΤƤ��������
#define TADASUTE_FU_OK_DEPTH 3	// (fukasa<=TADASUTE_FU_OK_DEPTH) ��ʿ���к��򤷤Ƥ��뿼����3���ܡ�4���ܤ����餷��

#define KAKO_BEST_SAME_MAX 5	// Ʊ���ȿ�������Ǻ��粿��ޤǺ����꤬�����ؤ�ä��������ݻ����뤫��

extern int org_ban[BAN_SIZE];	// ������֤Τߤ˻���
extern int org_mo_m[8];
extern int org_mo_c[8];
 
extern const char *koma[32];
extern const char *koma_kanji[32];
extern const char *num_kanji[];
extern const char *num_zenkaku[];

#define KIFU_MAX 2048	// ����μ���Ϻ����2048���1529���,�ߥ������⥹���θ��

#define PRT_LEN_MAX 512 // ���٤�ɽ����ǽ�ʺ���ʸ����
#define TMP_BUF_LEN 256	// ���Ū��ʸ�����Ĺ����
extern char kifu_comment[KIFU_MAX][TMP_BUF_LEN];	// ���ΤȤ���������Τߤ�Ͽ	
extern char kifu_log[KIFU_MAX][2][TMP_BUF_LEN];		// 1�ꤴ�ȤΥ���Ͽ
extern char tmp_log[2][TMP_BUF_LEN];				// ������Ū�˵�Ͽ
const int KIFU_LOG_TOP_MAX = 5;
extern char kifu_log_top[KIFU_LOG_TOP_MAX][TMP_BUF_LEN];	// �ǽ�Υ����Ȥ�Ͽ
const int USI_POS_MAX_SIZE = 8192;
extern char PlayerName[2][64];	// ��ꡢ����̾��
extern char sGameDate[];
#define KIF_BUF_MAX (65536*16)
extern unsigned long nKifBufSize;	// �Хåե��˥��ɤ���������
extern unsigned long nKifBufNum;	// �Хåե�����Ƭ���֤򼨤�
extern char KifBuf[KIF_BUF_MAX];	// �Хåե�
extern int fSkipLoadKifBuf;

extern int KomaOti;
extern int nSennititeMax;
extern char Kisen[];
extern int  nKifDateYMD;


#define CLOCKS_PER_SEC_MS (1000)	// CLOCKS_PER_SEC �����졣linux�ǤϤ�꾮����.

/** from "yss_init" **/

extern int maisu_kati[8][18];	// ���������ˤ�롢���Τβ���
extern int mo_fuka_kati[8][18];	// ���������ˤ���ղò���

extern  int m_kati[8][32];
extern  int   kati[16];
extern  int n_kati[16];
extern  int k_kati[16];
extern const int z8[8];
extern const int oute_can[4][16];
extern const int oute_z[2][8];

extern int total_time[2];		// ��ꡢ�����߷׻׹ͻ��֡�


extern int utikiri_time;			// �׹ͤ��Ǥ��ڤ����(��)
extern int saigomade_depth;			// 0...�ͤߤ�����ޤǻؤ���1...3������٤��ꤲ�롣
	extern int saigomade_flag;			// AI������Interface�Τ��������¸��
	extern int saigomade_read_max;		// Ʊ����
extern int read_max_limit;			// ���ο����ޤ�ȿ������ˡ���ɤࡣ
extern int capture_limit;			// ���פǶ����Ĺ�򤫤����β��͡�(= ���)���ȹ᤬����ʤ��Ĺ��

extern int base[BAN_SIZE];


extern int senkei;					// �﷿�򼨤�
extern int kibou_senkei;			// COM�ˤ��������﷿�����̤����ҡ�
extern int shuuban;					// ���פ򼨤��ե饰

extern int Utikiri_Time_Up;			// �������Ѥ˥����Х��ѿ��ˤ��롣��٥�������������
extern int Jissen_Stop;				// ����;�������ߤ����� K6-III 450MHz �� 1000��=��0.053�ä�

extern int thinking_utikiri_flag;	// �׹ͻ����ڤ���Ǥ��ڤ�ե饰��
extern int fPassWindows;			// Windows��������֤�������Ʊ����ͽ¬õ����������̣����---> ͽ¬�׹�����̡�
extern int seo_view_off;			// ��Ψ�׻�����;�פʾ�����Ǥ��Ф��ʤ�����1��
extern int fPrtCut;					// ����Ū��PRT���Ϥ�ߤ��
extern int fGotekara;

extern int ignore_moves[TOP_SST_MAX][4];
extern int ignore_moves_num;

extern int root_nokori;			// ���������������

extern unsigned int hash_ban[32][BAN_SIZE][2];	// �ϥå���������ǡ�����32�ӥåȡ�2�����󤬣��Ĥ��롣
extern const unsigned int hash_mask[3][8];		// [0]..value, [1]..mask, [2]..shift
#define HASH_MOTIGOMA_SIGN_BITS 0x9222220		// hash_mask[][]������򻲾ȤΤ���

extern int hash_use;				/**  �ϥå�����ѿ�  **/
extern int hash_over;				/**  �ƥϥå�����  **/
extern int horizon_hash_use;		// ��ʿ�������ѤΥϥå�����Ͽ��

extern int totta_koma[18][16];
extern int utta_koma[19][8];
extern int m_koma_plus[18][8];		// m_kati[][n] - m_kati[][n-1];


extern const int mo_start[2][4];	// �����𤬲���ˤʤ�Ϥޤ�ʰ��֤ˤ������Ѥ��Ǥ��ϸ���ɤࡢkikituke����
extern const int bit_oute[8][16];	// ���ǲ����ѥ�����ǧ����Ƚ��



typedef	struct {	// �ؤ���ǡ����ι�¤�Ρʥϥå���ơ��֥�ǤΤ߻��ѡ����� char )
	unsigned char bz;
	unsigned char az;
	unsigned char tk;
	unsigned char nf;
} TE;


// int �� 32�ӥåȤ�����ˤ��Ƥ��롣hash �ι�פ�32�Х��Ȥˤʤ�褦��Ĵ��.��64bit+mo(20bit)+v(16bit)+mv(24bit)+dp(10bit)
typedef struct HASH {	// ����̾���դ��ʤ����������ȤǤ��ʤ�������
	// clear ������˥�������������ʬ�� 1 DWORD �ˤʤ�褦������Ĵ�� ---> ��ƣ����ˤ�롣
	// char ��Ϣ³����褦�ˡ�
	// flag ��ǽ�˻��Ȥ���ΤǺǽ���֤���CPU�Υ���ΥС�����ž�����θ��
	// ---> lock[1]��ǽ�ˡ�Win64�ǥ�����֥餬�Ȥ��ʤ��Τǡ�single�Ǥ�®���Ѥ�餺��
	volatile char lock[1];		// SMP�ǤΥ�å��ѡ�hash�Τߤǻ��ѡ�LockChar()��Ȥ���
	unsigned char hit_num;		// ���Υϥå���򻲾Ȥ��������(4�Х��Ȥˤ��뤿��Υ��ߡ�)
	unsigned short int flag;	// ����ɾ���ͤ����Τ��ͤ����⤷���Ͼ�²��¤���0��̤��Ͽ��
								// 0000 0000 
								// �ºݤ�õ����4bit	  00... ̤���� 01...���Τ��� 10 ...��¡����ϲ��� 11...�ϥå���OVER	100...������Τ���Ͽ
								// ������com   2bit	  00... ̤���� 01...����     10 ...����ͥե饰ON
								// ������man   2bit
								// �̾��com   1bit	  0 ... ̤���� 1 ... ����
								// �̾��man   1bit

								// ��ʬ���������Ѥ��Ƥ��뤫������ï����Ѥ��Ƥ��ʤ���н񤭤��߲�
								// if ( (flag & 11110011)==0 ) OK!


	unsigned int hash_code1;	// �ϥå��女���� 01������֤�ȿž������
	unsigned int hash_code2;	// �ϥå��女���� 02����64�ӥåȡ�2^32���̤��äƸ�ǧ��Ψ�ϣ���
	unsigned int motigoma;

	union {		// ̵̾�����Ρʺ��ΤȤ����Ĥ����ʤΤǥ����ȡ�
		struct {
			unsigned int pn;
			unsigned int dn;
			unsigned int nodes;		// ���ξ��������Τ�õ���������̿�
			unsigned short min_distance;	// �Ǿ���Υ
			unsigned short dummy;
		};
		struct {
			int value;					// ɾ����
			int depth;					// õ������������Ū�ʿ�����256���ܿ��Ǿ�������ɽ��
			union {
				struct {
					unsigned char man_depth;	// �;���MAN��õ������������Ū��
					unsigned char com_depth;	//       COM��
					unsigned char man_tume;		// �ͤᾭ����ɾ���͡ʵ͡��ڤ졤������������ͳ�١�
					unsigned char com_tume;
				};
				struct {
					unsigned int seo_nodes;		// ���ξ��������Τ�õ���������̿�
				};
			};
			TE best;					// ���ζ��̤Ǥκ����� 
		};
	};
} HASH;



extern HASH sennitite[KIFU_MAX];		// �ϥå���ǰ����ζ��̤򵭲�

// �ϥå���ơ��֥�򸡺����������֤���¤��
// flag = 1...���Τ��͡� 2...��¡����ϲ��¡� 0...̤��Ͽ�� 3...�ϥå��奪���С���̤��Ͽ��
// flag = 8...����ͤˤʤ롣
typedef struct {
	int value;	// ɾ����
	int depth;	// ���ζ��̤���õ����������
	int flag;	// ɾ���ͤ����Τ��ͤ����⤷���Ͼ�²��¤�������̤��Ͽ�ζ��̤�
	unsigned int nodes;	// õ���������̿�
	union {		// ̵̾������
		struct {
			int bz;		// ������
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


#define TUMI	 	25000		/* �ͤ�������͡�COM���ͤ�Ȥ���-TUMI��*/
#define FUMEI 		32000		/* �ͤᾭ���������λ���COM�Ǥ�MAN�Ǥ����*/

#define DEPTH_LIMIT	(D_MAX-12)	// ���ο�����Ķ������õ���Ϥ��ʤ����ؤ���������64��ο����ޤǤ����顣
								// +10�ꤰ�餤�ϥ�����ꥢ�ǻȤ��Τǰ����Τ����


#endif	//] INCLUDE_YSS_EXT_H_GUARD
