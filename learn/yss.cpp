// 2019 Team AobaZero
// This source code is in the public domain.
/***************************************************************************/
/*                    YSS 13  (Yamashita Shogi System)                     */
/*                                         Programmed by Hiroshi Yamashita */
/*                                                                         */
/* ESS       1984/03 ESS by Hiroshi Wakabayashi in MICRO                   */
/* YSS  1.0  1990/08 PC-8801mkIISR (  Z80  4MHz) Basic                     */
/* YSS  2.0  1991/12 PC-6601       (  Z80  4MHz) Assembler   CSA  2nd 5/ 9 */
/* YSS  3.0  1992/12 PC-386V       (80386 20MHz) C   (TC2.0) CSA  3rd 4/10 */
/* YSS  4.0  1993/12 PC-9821Ap     (80486 66MHz) C   (BC3.1) CSA  4th 4/14 */
/* YSS  5.0  1994/12 PC-9821Xn     (Pent. 90MHz) C   (BC3.1) CSA  5th 3/22 */
/* YSS  6.0  1996/01 DOS/V         (Pent.166MHz) C   (VC1.5) CSA  6th 4/25 */
/* YSS  7.0  1997/02 VT-Alpha     (Alpha 500MHz) C   (VC4.0) CSA  7th 1/33 */
/* YSS  8.0  1998/02 DEC-600a     (Alpha 600MHz) C   (VC4.0) CSA  8th 5/35 */
/* YSS  9.0  1999/03 XP1000  (Alpha21264 500MHz) C   (VC6.0) CSA  9th 2/40 */
/* YSS  9.4  1999/06 FMV           (K6-2 350MHz) C   (VC5.0) ISF  1st 1/ 8 */
/* YSS  9.87 2000/03 DOS/V     (Pentium3 840MHz) C   (VC5.0) CSA 10th 2/45 */
/* YSS 10.15 2000/08 DOS/V                (????) C   (VC5.0) MSO 2000 1/ 3 */
/* YSS 10.39 2001/03 DOS/V       (Athlon 1.2GHz) C   (VC5.0) CSA 11th 5/55 */
/* YSS 10.68 2002/05     (AthlonXP2100+ 1.73GHz) C++ (VC6.0) CSA 12th 8/51 */
/* YSS 10.91 2002/10 FMVnote(Pentium3-M 1.13GHz) C++ (VC6.0) ISF  2nd 1/ 8 */
/* YSS 10.94 2002/11 X24    (Pentium3-M 1.13GHz) C++ (VC6.0) GPW 2002 1/ 8 */
/* YSS 11.55 2003/05 DOS/V     (Xeon x2 3.06GHz) C++ (VC6.0) CSA 13th 2/45 */
/* YSS 11.59 2003/11 X24    (Pentium3-M 1.13GHz) C++ (VC6.0) GPW 2003 2/ 9 */
/* YSS 11.59 2003/11 DOS/V    (Pentium4 2.40GHz) C++ (VC6.0) Olym 8th 1/ 3 */
/* YSS 12.05 2004/05     (Opteron248 x2 2.2 GHz) C++ (VC6.0) CSA 14th 1/43 */
/* YSS 12.40 2005/05     (Opteron852 x4 2.6 GHz) C++ (VC6.0) CSA 15th 4/39 */
/* YSS 12.44 2005/09 X24    (Pentium3-M 1.13GHz) C++ (VC6.0) Olym10th 2/ 4 */
/* YSS 12.54c2005/10 FMV-C20N (Pentium4 2.8 GHz) C++ (VC6.0) ISF  3rd 1/ 8 */
/* YSS 12.65 2006/05     (Opteron852 x4 2.6 GHz) C++ (VC6.0) CSA 16th 2/43 */
/* YSS 12.65b2006/05     (CoreDuo T2600 2.16GHz) C++ (VC6.0) Olym11th 1/ 3 */
/* YSS 12.95 2007/05   (Xeon 5355 x2 8c 2.66GHz) C++ (VC8.0) CSA 17th 1/40 */
/* YSS 13.03 2008/05   (Xeon 5355 x2 8c 2.66GHz) C++ (VC6.0) CSA 18th 4/39 */
/* YSS 13.13 2009/05   (Xeon 5355 x2 8c 2.66GHz) C++ (VC6.0) CSA 19th 7/42 */
/* YSS 13.40 2010/05  (i7 980X EE x1 6c 4.00GHz) C++ (VC2010)CSA 20th 8/42 */
/* YSS 13.62 2011/05 (980 4.0GHz + X3680 3.3GHz) C++ (Linux) CSA 21th 8/37 */
/* YSS 13.67b2012/05 (980 4.0GHz + X3680 3.3GHz) C++ (Linux) CSA 22th 7/41 */
/* YSS 13.73c2013/05  980 3.3GHz + X3680 3.3GHz  C++ (Linux) CSA 23th 8/40 */
/* YSS 13.84a2014/05 E5-2680 2.8GHz x2, x16台    C++ (Linux) CSA 24th 3/38 */
/* YSS 13.88 2015/05 E5-2680 2.8GHz x2, x16台    C++ (Linux) CSA 25th10/?? */
/***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "yss_ext.h"     /**  load extern **/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss_int.h"     /**           shoki data no load          **/
						 /**  ex.  ban_init, mo_m & c , kati etc.. **/

#include "yss.h"		// クラスの定義
#include "yss_dcnn.h"


int shuuban = 0;		// 終盤になった事を示すフラグ


int senkei;				// 戦型を示す

#ifdef CSA_TOURNAMENT
int kibou_senkei = 0;	// COMにさせたい戦型。普通は矢倉。「０...ランダム。１...居飛車。２...振飛車」
#else
int kibou_senkei = 0;	// 普通はランダム
#endif


int read_max_limit = 6;			// この深さまで反復深化法で読む。

int utikiri_time = 2;		// 思考を打ち切る制限時間

int saigomade_depth = 1;		// 0...詰みあがりまで指す、1...3手詰程度で投げる(RM=1の手は採用しない）。
	int saigomade_flag     = 0;		// もう使ってない。AI将棋のInterfaceのためだけに存在。
	int saigomade_read_max = 2;		// 同じく。

int total_time[2];		// 先手、後手の累計思考時間。

char PlayerName[2][64];	// 先手、後手の名前
char sGameDate[TMP_BUF_LEN];
char kifu_comment[KIFU_MAX][TMP_BUF_LEN];	// 棋譜に思考のコメントを出す
char kifu_log[KIFU_MAX][2][TMP_BUF_LEN];	// 1手ごとのログを記録
char tmp_log[2][TMP_BUF_LEN];				// ログを一時的に記録
char kifu_log_top[KIFU_LOG_TOP_MAX][TMP_BUF_LEN];	// 最初のコメントを記録
int KomaOti = 0;			// 0で平手
int nSennititeMax = 0;		// 同型になった最大数(1で同じ局面2回、3で4回なので千日手)
char Kisen[TMP_BUF_LEN];
//char sKifDate[TMP_BUF_LEN];
int  nKifDateYMD;

int seo_view_off = 0;	// 確率計算時に余計な情報を吐き出さない時に1。
int fPrtCut = 0;		// 強制的にPRT出力を止める
int fGotekara = 0;		// 後手から指している場合（駒落ち下手）	



// YSS の初期化処理
void shogi::initialize_yss()
{
	copy_org_ban();		// 初期局面をセット

	init();           /** koma bangou no shokika **/
					  /** hash_motigoma          **/

	allkaku();        /** kiki wo kaku dake!     **/
	fukasa = 0;
	tesuu = 0;
	hash_ban_init();	// ハッシュ用乱数テーブル hash_ban[32][BAN_SIZE][2] の初期化。乱数をぶち込む。

	hash_calc( 0, 0 );	// ハッシュ値を唯一の値に決定する。盤面反転なし、手番反転なし。
	make_move_is_check_table();
	make_move_is_tobi_check_table();

	check_kn();

//	taikyokukan();

#ifdef LEARNING_EVAL
//	init_learning_evaluation();
#endif

	kyokumen = 0;
	make_unique_from_to();
	make_move_id_c_y_x();

//	hyouji();
	PRT("size(shogi)=%d,hash=%d,int=%d,long=%d,float=%d,double=%d\n",sizeof(shogi),sizeof(HASH),sizeof(int),sizeof(long),sizeof(float),sizeof(double));
	PRT("kb_c[][]=%d,kb_c[]=%d,kb_c[][]=%d\n",sizeof(kb_c),sizeof(kb_c[0]),sizeof(kb_c[0][0]) );
}

void shogi::init_file_load_yss_engine()
{
	static int fDone = 0;
	if ( fDone ) return;
	fDone = 1;
	initialize_yss();		// 初期化処理

//	hash_all_clear();		// 動的確保なので最初に確保
	check_kn();				// 盤面が正当かチェック
}


#if !defined(_MSC_VER)


#include <unistd.h> // for readlink
int GetSystemMemoryMB()
{
    int sys_memory = 0;
//  int ret = readlink("/proc/meminfo", buf, sizeof(buf));
	FILE *fp = fopen("/proc/meminfo", "r");
    if ( fp ) {
		char buf[1024];
		int nLen = fread(buf,1,sizeof(buf),fp);
        if ( nLen ) {
			buf[nLen-1] = '\0';
    		char find[] = "MemTotal:";
			char *p = strstr(buf,find);
			if ( p && sscanf(p+strlen(find),"%d", &sys_memory) == 1 ) sys_memory /= 1024; // KB単位なのでMBに
		}
		fclose(fp);
	}
	PRT("sys_memory= %d MB\n",sys_memory);
	return sys_memory;
}

const char *get_kdbDirSL() { return ""; }	// NULLではない。長さが0の文字列の先頭アドレスを返す

int get_learning_dir_number() { DEBUG_PRT("must be programmed!\n"); return 0; }

int change_dir(const char* /* sDir */) { return 0; }
void return_dir() {}
int get_next_file(char* /* filename */, const char* /* ext */) { return 0; }
int open_one_file(char *filename)
{
	if ( fSkipLoadKifBuf==0 ) {
		FILE *fp = fopen(filename,"r");
		if ( fp==NULL ) return 0;
		nKifBufSize = fread((void *)KifBuf, 1, KIF_BUF_MAX, fp);
		fclose(fp);
	}
	str_sjis2euc(KifBuf);
	

	PS->hirate_ban_init(0);
	nKifBufNum = 0;		// バッファの先頭位置を初期化
	InitGameInfo();		// 後手から指してるか、対局者名、などを初期化
	ClearKifuComment();

	PRT_OFF();

	int fCSA = 0;
	char *p = strrchr(filename,'.');
	if ( p ) {
		if ( strcmp(p+1,"csa")==0 || strcmp(p+1,"CSA")==0 ) fCSA = 1;
	}
	if ( fCSA ) {
		PS->LoadCSA();	// CSA形式の棋譜を読み込む
	} else {
		PS->LoadKaki();	// 柿木形式
	}

	PS->SetSennititeKif();	// 千日手も棋譜情報に登録する

	PRT_ON();
	return 1;
}

void SleepMsec(int msec)
{
    struct timespec req;
    req.tv_sec = msec / 1000;
    req.tv_nsec = (msec % 1000) * 1000000;
    nanosleep(&req, 0);
}
int IsNan(double /*dx*/) { return 0; }

void write_down_file_prt_and_die() { return; }

#else



void SleepMsec(int msec)
{
	Sleep(msec);
}
int IsNan(double dx) { return _isnan(dx); }


#include <io.h>
#include <direct.h>
static char sLocalDir[MAX_PATH];

int change_dir(const char *sDir)
{
	GetCurrentDirectory(MAX_PATH, sLocalDir);
	if ( _chdir(sDir) ) { PRT("Error chdir to %s\n",sDir); return 0; }
	return 1;
}
void return_dir()
{
	if ( _chdir(sLocalDir) ) PRT("Error chdir to local %s\n",sLocalDir);
}

int get_next_file(char *filename, const char *ext)
{
	struct _finddata_t c_file;
	static FIND_FILE_HANDLE hFile = -1L;
	static int count = 0;

	if ( hFile == -1L ) {
		/* カレント ディレクトリ内の最初の .sgf ファイルを探します。*/
		hFile = _findfirst( ext, &c_file );	// ext = "*.kif";
		if ( hFile == -1L ) {
			PRT( "カレント ディレクトリには %s ファイルは存在しません。\n",ext );
			return 0;
		}
	} else {
		if ( _findnext( hFile, &c_file ) == -1 ) {
			_findclose( hFile );
			hFile = -1L;
			return_dir();
			return 0;
		}
	}
//	PRT( "%4d: %-12s %.24s  %9ld\n",++count, c_file.name, ctime( &( c_file.time_write ) ), c_file.size );
	strcpy(filename,c_file.name);
	return 1;
}

int get_next_file_from_dirs(char *filename, const char *ext, const char *dir_list[])
{
	static const char *p_current_dir = NULL;
	static int dir_i = 0;
	
	for (;;) {
		if ( p_current_dir == NULL ) {
			p_current_dir = dir_list[dir_i];
			dir_i++;
			if ( p_current_dir == NULL ) {
				dir_i = 0;
				return 0;
			}
			if ( change_dir(p_current_dir) == 0 ) {
				DEBUG_PRT("fail change dir=%s\n",p_current_dir);
			}
		}

		if ( get_next_file(filename,ext)!=0 ) return 1;	// success
		p_current_dir = NULL;
	}
	return 0;
}

// 棋譜を一つ読み込む
int open_one_file(char *filename)
{
	extern char InputDir[TMP_BUF_LEN];	// 棋譜の自動処理用のディレクトリパス（入力）

	PS->hirate_ban_init(0);
	strcpy( InputDir, filename );	// 自動入力ファイル名をセット
	
	PRT_OFF();
	{ extern int fShinpo; fShinpo = 1; }	// menuに登録しない
	int fRet = PS->KifuOpen();
	PRT_ON();
	InputDir[0] = 0;
	return fRet;
}
#endif


#if defined(_MSC_VER)
int getpid_YSS() { return _getpid(); }
int read_YSS(int handle, void *buf, unsigned int count) { return _read( handle, buf, count); }
int fileno_YSS( void *stream ) { return _fileno( (FILE*)stream ); }
void PRT_FILE(int flag) { extern int fFileWrite; fFileWrite = flag; }
const char *getCPUInfo() { return getYssCPU(); }
#else 
void PRT_FILE(int flag) { return; }
int getpid_YSS() { return getpid(); }
int read_YSS(int handle, void *buf, unsigned int count) { return read( handle, buf, count); }
int fileno_YSS( void *stream ) { return fileno( (FILE*)stream ); }
const char *getCPUInfo() { return ""; }
#endif


// fPassWindowsフラグを0にするだけ。この変数、なんか意味不明・・・。
void PassWindowsFlagOff()
{
#if !defined(AISHOGI) && defined(_MSC_VER)
	fPassWindows = 0;
#endif
}

void PRT_OFF() { fPrtCut = 1; }
void PRT_ON()  { fPrtCut = 0; }

// 情報出力、SMPの場合はロックをかける
// 可変個数の引数をそのまま、別の関数に渡すことはできないので、文字列に変換する
int PRT(const char *fmt, ...)	// printf() の書式文字列は const
{
	va_list ap;
	int len;
	static char text[PRT_LEN_MAX];

	if ( 0 ) {
		va_start(ap, fmt);
		len = vsprintf( text, fmt, ap );
		va_end(ap);
		FILE *fp = fopen("yssprt.txt","a");
		if ( fp ) {
			fprintf(fp,"%s",text);
			fclose(fp);
		}
	}

	if ( fPrtCut ) return 0;
#ifdef SMP
	if ( lock_io_init_done==0 ) return 0;
	LockY(lock_io);
#endif

	va_start(ap, fmt);
	len = vsprintf( text, fmt, ap );	// ここで落ちる可能性大（次には行かない）---> vsnprintf()に変更 はLIBの追加がいるのでやめ 
	va_end(ap);

#if !defined(_MSC_VER)
//	str_euc2sjis(text);
	if ( 0 ) {
		FILE *fp = fopen("yssprt.txt","a");
		if ( fp ) {
			fprintf(fp,"%s",text);
			fclose(fp);
		}
	}
	if ( len >= 0 ) fprintf(stderr,"%s",text);
#else
	if ( len < 0 || len >= PRT_LEN_MAX ) {
		PRT_sub("PRT len over!\n");
		DEBUG_PRT("PRT len over!");
	}
	PRT_sub("%s",text);	// PRT(text) だとtext="%CHUDAN"が"HUDAN"になってしまう。
#endif

//	SendMessage(ghWindow, WM_COMMAND, IDM_PRT_SMP, (LPARAM)text );	// 表示内容を送る。PostMessage()はだめ。

#ifdef SMP
	UnLock(lock_io);
#endif
	return 0;
}

int PRT_CHAR_ALL(char *s)
{
	int nLen = strlen_int(s);
	int i; 
	for (i=0;i<nLen;i++) {
		char c = s[i];
		PRT("%c",c);
	}
	return nLen;
}

static char debug_str[TMP_BUF_LEN];

void debug_set(const char *file, int line)
{
	char str[TMP_BUF_LEN];
	strncpy(str, file, TMP_BUF_LEN-1);	// 開発用Dirは恥ずかしいので消す
	const char *p = strrchr(str, '\\');
	if ( p == NULL ) p = file;
	else p++;
	sprintf(debug_str,"%s %d行目\n\n",p,line);
}

void debug_print(const char *fmt, ... )
{
	va_list ap;
	static char text[TMP_BUF_LEN];
	va_start(ap, fmt);
#if defined(_MSC_VER)
	_vsnprintf( text, TMP_BUF_LEN-1, fmt, ap );
#else
	 vsnprintf( text, TMP_BUF_LEN-1, fmt, ap );
#endif
	va_end(ap);
	static char text_out[TMP_BUF_LEN*2];
	sprintf(text_out,"%s%s",debug_str,text);
	PRT_ON();
	PRT("%s\n",text_out);
	debug();
}



// 文字列の長さをint型を返すだけ。Win64での警告対策。
int strlen_int(const char *p)
{
	return (int)strlen(p);
}

// 手を進めて棋譜をセット
int shogi::kifu_set_move(TE Te, int t)
{
	return kifu_set_move(Te.bz,Te.az,Te.tk,Te.nf,t);
}
int shogi::kifu_set_move(int bz,int az,int tk,int nf, int t)
{
//	char retp[20];
//	change(bz,az,tk,nf,retp);
//	PRT("%s, bz,az,tk,nf = %x,%x,%x,%x\n",retp,bz,az,tk,nf);
	if ( tesuu >= KIFU_MAX-1 ) { DEBUG_PRT("KIFU_MAX Err\n"); }
	move_hit(bz,az,tk,nf);
	tesuu++; all_tesuu = tesuu;
	kifu[tesuu][0] = bz;
	kifu[tesuu][1] = az;
	kifu[tesuu][2] = tk;
	kifu[tesuu][3] = nf;
	kifu[tesuu][4] = t;	// 思考時間（累計ではない！）
	return 0;
}

void shogi::kifu_set_move_sennitite(int bz,int az,int tk,int nf, int t)
{
	kifu_set_move(bz, az, tk, nf, t);
	set_sennitite_info();
}
void shogi::kifu_set_move_sennitite(int te, int t)
{
	int bz,az,tk,nf;
	unpack_te(&bz,&az,&tk,&nf, te);
	kifu_set_move_sennitite(bz,az,tk,nf, t);
}

int count_same_position()
{
	int i,same = 0;
	HASH *p0 = &sennitite[PS->tesuu];
	for (i=PS->tesuu-2;i>=1;i-=2) {
		HASH *p = &sennitite[i];
		if ( p0->hash_code1 == p->hash_code1 && p0->hash_code2 == p->hash_code2 && p0->motigoma == p->motigoma ) same++;
	}
	return same;
}

#include <string>
using namespace std;

unsigned long nKifBufSize;	// バッファにロードしたサイズ
unsigned long nKifBufNum;	// バッファの先頭位置を示す
char KifBuf[KIF_BUF_MAX];	// バッファ
int fSkipLoadKifBuf = 0;

// KIFのバッファに書き出す
int KIF_OUT(const char *fmt, ...)
{
	va_list ap;
	int n;
	char text[TMP_BUF_LEN];

	va_start(ap, fmt);
	vsprintf( text , fmt, ap );
	va_end(ap);

	n = strlen_int(text);
	if ( nKifBufNum + n >= KIF_BUF_MAX ) { PRT("KifBuf over\n"); return -1; }

	strncpy(KifBuf+nKifBufNum,text,n);
	nKifBufNum += n;
	KifBuf[nKifBufNum] = 0;
	nKifBufSize = nKifBufNum;	// 一応代入
	return n;
}

// KIF用のバッファから1バイト読み出す。
char GetKifBuf(int *RetByte)
{
	*RetByte = 0;
	if ( nKifBufNum >= nKifBufSize ) return 0;
	*RetByte = 1;
	return KifBuf[nKifBufNum++];
}

int ReadOneLineStr(string &str)
{
	int mac = 0;
	int n = 0;
	for ( ;; ) {
		int RetByte;
		char c = GetKifBuf(&RetByte);
		if ( RetByte == 0 ) break;	// EOF;

		// MACでは改行が \r のみ。Winでは \r\n と2つ続く。
		if ( mac == 1 && c != '\n' ) {	// Macのファイルか？
			nKifBufNum--;	// ポインタを一つ戻す。
			str[n-1] = '\n';
//			PRT("mac形式？\n");
			break;
		}

//		if ( c == '\r' ) c = '\n';	// Mac対策
		if ( c == '\r' ) { mac = 1; c = ' '; }	// 小細工。たいした意味なし
		else mac = 0;

		str.append(1, c);
		n++;
		if ( c == '\r' || c == '\n' ) break;	// 改行はNULLに
	}
	return str.length();
}

const int TMP_BUF_LEN_AOBA_ZERO = TMP_BUF_LEN * 32;

// 一行読み込む
int ReadOneLine(char *lpLine) 
{
	string str;
	int n = ReadOneLineStr(str);
#ifdef AOBA_ZERO
	if ( n >= (TMP_BUF_LEN_AOBA_ZERO-1) ) n = TMP_BUF_LEN_AOBA_ZERO-1;
#else
	if ( n >= (TMP_BUF_LEN-1) ) n = TMP_BUF_LEN-1;
#endif
	//	str.copy(lpLine,n,0);
	strncpy(lpLine, str.c_str(), n);
	lpLine[n] = 0;	// 最後にNULLをいれること！
	return n;
}

// 盤面だけを出力
void shogi::save_banmen()
{
	print_kakinoki_banmen(KIF_OUT,PlayerName[0],PlayerName[1]);
}

const int KomaWariNum = 8;
const char *KomaWari[KomaWariNum] = { "平手","香落ち","角落ち","飛車落ち","二枚落ち","四枚落ち","六枚落ち","右香落ち" };

void shogi::SaveCSA(char *sTagInfo, char *sKifDate)
{
	int bz,az,nf,tk,i,k,x,y;
	int fKomaochi = 0;
#ifdef CSA_SAVE_KOMAOCHI	// 駒落ちの棋譜をCSAで保存する場合に、反転して保存（とりあえず）
	fKomaochi = 1;
#endif
	const int fShort = 1;
	
	nKifBufNum = 0;

	for ( i=0;i<all_tesuu;i++) back_move();	// 初期盤面に戻す。
//	sente_time = gote_time = 0;				// 先手、後手の累計時間

	if ( fShort ) {
		KIF_OUT("%s\r\n",sKifDate);
	} else {
		KIF_OUT("' YSS棋譜(CSA形式),%s\r\n",getCPUInfo());
		KIF_OUT("' %s\r\n",sTagInfo);
		KIF_OUT("' 対局日：%s\r\n",sKifDate);
			
		KIF_OUT("N+%s\r\n",PlayerName[(0+fKomaochi)&1]);
		KIF_OUT("N-%s\r\n",PlayerName[(1+fKomaochi)&1]);
	}
	
	if ( fKomaochi == 0 ) {
		KIF_OUT("PI\r\n");
		KIF_OUT("+\r\n" );
	} else {
		for (y=1;y<=9;y++) {
			KIF_OUT("P%d",y);
			for (x=1;x<=9;x++) {
				k = init_ban[(10-y)*16+(10-x)];
				if ( k==0 ) KIF_OUT(" * ");
				else {
					if ( k > 0x80 ) KIF_OUT("+");
					else            KIF_OUT("-");
					KIF_OUT("%s",koma[k&0x0f]);
				}
			}
			KIF_OUT("\r\n");
		}
		KIF_OUT("-\r\n");
	}


	for ( i=1;i<all_tesuu+1;i++) {                /*** sasite wo saishoni modosu ***/
		bz = kifu[i][0];
		az = kifu[i][1];
		tk = kifu[i][2];
		nf = kifu[i][3];

		if ((i+fKomaochi)&1) KIF_OUT("+");
		else                 KIF_OUT("-");

		if ( bz == 0xff ) {
			bz = 0x00;
			k = tk & 0x07;
		} else {
			k = init_ban[bz] & 0x0f;
			x = 10 - (bz & 0x0f);
			bz = (bz>>4) + (x<<4);
			if ( nf ) k += 8;
		}
		x = 10 - (az & 0x0f);
		az = (az>>4) + (x<<4);
		if ( fKomaochi ) {
			if ( bz ) bz = 0xaa - bz;
			az = 0xaa - az;
		}

		if ( fShort ) {
			KIF_OUT("%02X%02X%s",bz,az,koma[k]);
			if ( (i%10)==0 ) {
				KIF_OUT("\r\n");
			} else {
				KIF_OUT(",");
			}
		} else {
			KIF_OUT("%02X%02X%s,T%d\r\n",bz,az,koma[k],kifu[i][4]);
		}
		// コメント、Logを出力
		if ( strlen_int(kifu_comment[i]) ) {
			KIF_OUT("'%s\r\n",kifu_comment[i]);	// 千日手のコメント出力
		}
		if ( strlen_int(kifu_log[i][0]) ) { KIF_OUT("\'"); KIF_OUT("%s",kifu_log[i][0]+1); }
		if ( strlen_int(kifu_log[i][1]) ) { KIF_OUT("\'"); KIF_OUT("%s",kifu_log[i][1]+1); }

		forth_move();	// 実際に１手進める。
	}
	if ( fShort ) {
		KIF_OUT("%s\r\n",sTagInfo);
	} else {
		KIF_OUT("%%CHUDAN\r\n");
	}
}

void shogi::SaveCSATume()
{
	int bz,az,nf,tk,i,k,x,y;

	nKifBufNum = 0;

	for ( i=0;i<all_tesuu;i++) back_move();	// 初期盤面に戻す。

	// 詰将棋用に盤面を駒ごとに指定
	char *p = strstr(kifu_log_top[1],"詰手数= ");
	if ( p ) {
		char *q = strstr(p,"\n");
		if ( q ) *q = 0;
		if ( q && q > p+1 && *(q-1)==' ' ) *(q-1) = 0;
		KIF_OUT("'%s\n",p+8);
	}

	int n[3];
	n[0] = n[1] = n[2] = 0;
	for (i=0;i<2;i++) {
		for (y=1;y<=9;y++) for (x=1;x<=9;x++) {
			int k = init_ban[y*16+x];
			if ( k==0 ) continue;
			if ( k > 0x80 ) {
				if ( i==1 ) continue;
				if ( n[i]==0 ) KIF_OUT("P-");
				n[0]++;
			} else { 
				if ( i==0 ) continue;
				if ( n[i]==0 ) KIF_OUT("P+");
				n[1]++;
			}
			KIF_OUT("%d%d%s",10-x,y,koma[k&0x0f]);
		}
		if ( n[i] ) KIF_OUT("\n");
	}
	for (i=1;i<8;i++) {
		if ( mo_m[i] == 0 ) continue;
		if ( n[2]==0 ) KIF_OUT("P+");
		n[2]++;
		int j;
		for (j=0;j<mo_m[i];j++) KIF_OUT("00%s",koma[i]);
	}
	if ( n[2] ) KIF_OUT("\n");
	
	KIF_OUT("P-00AL\n+\n");

	for (i=1;i<all_tesuu+1;i++) {
		bz = kifu[i][0];	az = kifu[i][1];	tk = kifu[i][2];	nf = kifu[i][3];
		if (i&1) KIF_OUT("+");
		else     KIF_OUT("-");
		if ( bz == 0xff ) {
			bz = 0x00;
			k = tk & 0x07;
		} else {
			k = init_ban[bz] & 0x0f;
			x = 10 - (bz & 0x0f);
			bz = (bz>>4) + (x<<4);
			if ( nf ) k += 8;
		}
		x = 10 - (az & 0x0f);
		az = (az>>4) + (x<<4);
		KIF_OUT("%02X%02X%s\n",bz,az,koma[k]);
		forth_move();
	}
}


void shogi::SaveKaki(char *sTagInfo, char *sKifDate)
{
	int bz,az,nf,tk,i;
	unsigned int hour,minute,second;
	unsigned int sente_time,gote_time,all_time,jikan;
	char retp[20];

	nKifBufNum = 0;

	for (i=0;i<all_tesuu;i++) back_move();	// 初期盤面に戻す。

	KIF_OUT("# YSS棋譜(柿木形式),%s\r\n",getCPUInfo());
	KIF_OUT("# %s\r\n",sTagInfo);
	KIF_OUT("対局日：%s\r\n",sKifDate);
	if ( Kisen[0] ) KIF_OUT("棋戦：%s\r\n",Kisen);

	if ( is_hirate_ban() == 1 ) {	// 平手の場合
		KIF_OUT("手合割：%s\r\n",KomaWari[KomaOti]);
		KIF_OUT("先手：%s\r\n",PlayerName[0]);
		KIF_OUT("後手：%s\r\n",PlayerName[1]);
	} else {
		save_banmen();
	}
	KIF_OUT("手数----指手---------消費時間--\r\n");


	sente_time = gote_time = 0;				// 先手、後手の累計時間

	for ( i=1;i<all_tesuu+1;i++) {                /*** sasite wo saishoni modosu ***/
		bz = kifu[i][0];
		az = kifu[i][1];
		tk = kifu[i][2];
		nf = kifu[i][3];

		KIF_OUT("%4d ",i);
		if ( KomaOti != 0 ) {
			hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
			hanten();		// 盤面反転
			change(bz,az,tk,nf,retp);
			hanten();		// 盤面反転
			hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
		} else {
			change(bz,az,tk,nf,retp);
		}
		if ( i > 1 && az == kifu[i-1][1] ) {
			strncpy( retp,"同　",4);	/* 同　・・にする */
		}
		KIF_OUT("%s",retp);


		jikan = kifu[i][4];
		if ( (i & 1)==1 ) {
			sente_time += jikan;
			all_time = sente_time;
		} else {
			gote_time += jikan;
			all_time = gote_time;
		}

		minute = jikan / 60;
		second = jikan - minute*60;
		KIF_OUT(" (%02d:%02d/",minute,second);
		hour   =  all_time / 3600;
		minute = (all_time - hour*3600) / 60;
		second =  all_time - hour*3600 - minute*60;
		KIF_OUT(" %d:%02d:%02d)",hour,minute,second);
		if ( strlen_int(kifu_comment[i]) ) KIF_OUT(" #%s\r\n",kifu_comment[i]);	// コメントを出力
		else                               KIF_OUT("\r\n");

		for (int j=0;j<2;j++) {
			char *p = kifu_log[i][j];
			if ( *p != '*' && *p != 0 ) *p = '*';	// 柿木形式のコメントに。 
			KIF_OUT("%s",p);
		}

		forth_move();	// 実際に１手進める。
	}
	KIF_OUT("%4d 中断         (00:00/ 0:00:00)\r\n",i);
//	if ( TMP_BUF_LEN >= MAX_PATH ) { PRT("文字配列長\n"); debug(); } 
}

// 棋譜のコメント、ログをクリアする。
void ClearKifuComment()
{
	int i;
	for (i=0;i<KIFU_MAX;i++) {
		kifu_comment[i][0] = 0;
		kifu_log[i][0][0] = 0;
		kifu_log[i][1][0] = 0;
	}
	for (i=0;i<KIFU_LOG_TOP_MAX;i++) {
		kifu_log_top[i][0] = 0;
	}
}

int get_csa_koma(char *p)
{
	int i;
	for (i=1; i<16; i++) {
		if ( *(p+0) == *(koma[i]) && *(p+1) == *(koma[i]+1) ) break;
	}
	if ( i==16 ) { PRT("駒種別エラー%c%c,",*(p+0),*(p+1)); i = 0; }
	return i;
}
int shogi::getMoveFromCsaStr(int *bz, int *az, int *tk, int *nf, char *str)
{
	*tk = 0;
	*nf = 0;
	int x = str[2] - '0';
	int y = str[3] - '0';
	*az = y*16 + 10 - x;
	x = str[0] - '0';
	y = str[1] - '0';
	*bz = y*16 + 10 - x;
			
	if ( x == 0 ) {	// 駒打ち
		*bz = 0xff;
		int i = get_csa_koma(str+4);
		if ( i==0 || i>=8 ) { PRT("駒打ちエラー"); return 0;}
		*tk = i + (0x80)*((tesuu+fGotekara)&1);
	} else {
		int i = get_csa_koma(str+4);
		if ( i==16 ) return 0;
		if ( (init_ban[*bz] & 0x0f) == i ) *nf = 0;
		else *nf = 0x08;
		*tk = init_ban[*az];
	}
	return 1;
}

// CSA形式の棋譜を読み込む
int shogi::LoadCSA()
{
#ifdef AOBA_ZERO
	char lpLine[TMP_BUF_LEN_AOBA_ZERO];
	free_zero_db_struct(&zdb_one);
	zdb_one.moves = -1;
#else
	char lpLine[TMP_BUF_LEN];
#endif
	int i,j,k,x,y,z, nLen;
	int bz,az,tk,nf;
	char *lpCopy;
	char c;
	int prt_flag = 1;
	int fShortCSA = 0;	// 盤面を座標で指定する詰将棋用
	char sIndex[256];

	tesuu = 0;
	hirate_ban_init(KomaOti);		// 盤面の初期化　平手の状態へ

	nLen = 0;
	for ( ;; ) {
		if ( nLen <= 7 ) nLen = ReadOneLine(lpLine);
		else {
			for (i=0;i<nLen;i++) {
				if ( lpLine[i]==',' ) {	// マルチステートメント
					for (j=i;j<nLen+1;j++) lpLine[j-i] = lpLine[j+1];	// スクロール
					break;
				}
			}
			if ( i==nLen ) {
				nLen = 0;
				continue;
			}
			nLen = strlen_int(lpLine);
		}
		if ( nLen == 0 ) break; // EOF
		lpLine[nLen] = 0;
//		PRT("%s, nLen=%d\n",lpLine,nLen);
		if ( lpLine[0] == 'T' ) {
			kifu[tesuu][4] = atoi( lpLine+1 );
			continue;	// 思考時間
		}
		if ( (lpLine[0] == '+' || lpLine[0] == '-') && nLen <= 3 ) {	// これより手順
			if ( lpLine[0] == '+' ) {
				PRT("+ 先手番\n");
			} else {
				PRT("- 後手番\n"); 
				fGotekara = 1;
			}
			prt_flag = 0;
			ban_saikousei();	// 盤面の再構成。
			check_kn();			// 盤面の状態が正常化チェック

			// 駒落ち判定
			ZERO_DB *pz = &zdb_one;
			pz->handicap = get_handicap_from_board();
			if ( pz->handicap && fGotekara==0 ) DEBUG_PRT("pz->handiacp=%d\n",pz->handicap);
		}

		// csa形式のコメントを取り込む
		if ( lpLine[0] == '\'' ) {
#ifdef AOBA_ZERO
//			int n = strlen_int(lpLine);
//			for (i=0;i<n;i++) PRT("%c",lpLine[i]);
			if ( tesuu == 0 ) {
				if ( strncmp(lpLine,"'no",3)==0 ) {
					strncpy(sIndex,lpLine,255);
				}
				if ( strncmp(lpLine,"'w ",3)==0 ) {
					char *p = strchr(lpLine,',');
					if ( p && p - lpLine < (int)strlen(lpLine) ) {
						*p = 0;
						ZERO_DB *pz = &zdb_one;
						pz->weight_n = atoi(lpLine+3);
					}
				}
			} else {
				ZERO_DB *pz = &zdb_one;
//				PRT("%s\n",lpLine);
				vector <unsigned int> v;
				pz->vv_move_visit.push_back(v);
				if ( pz->vv_move_visit.size() != (size_t)tesuu ) {
					DEBUG_PRT("pz->vv_move_visit.size()=%d,tesuu=%d Err\n",pz->vv_move_visit.size(),tesuu);
				}
				back_move();
				char *p = lpLine + 1;
				int count = 0, all_visit = 0, sum_visit = 0;
				int b0 = 0,b1 = 0;
				bool has_root_score = false;
				for (;;) {
					char c;
					char str[10];
					int n = 0;
					for (;;) {
						if ( n>=10 ) { DEBUG_PRT("Err csa move str >= %d,w=%d,%s\n",n,pz->weight_n,sIndex); }
						c = *p++;
						str[n++] = c;
						if ( c==',' || c=='\r' || c =='\n' || c==0 ) break;
					}
					str[n-1] = 0;
					if ( count==0 ) {
						if ( strstr(str,"v=") ) {
							count--;
							float score = atof(str+2);
							int s = (int)(score * 10000);
							if ( s < 0 || s > 10000 ) DEBUG_PRT("Err s=%d,v=%s\n",s,str);
							pz->v_score_x10k.push_back((unsigned short)s);
							has_root_score = true;
						} else {
							all_visit = atoi(str);
							pz->v_playouts_sum.push_back(all_visit);
							if ( has_root_score == false ) pz->v_score_x10k.push_back(NO_ROOT_SCORE);
						}
					} else {
						if ( (count&1)== 0 ) {
							if ( b0==0 && b1==0 ) DEBUG_PRT("");
							int v = atoi(str);
							if ( v > 0xffff ) v = 0xffff;
							sum_visit += v;
							unsigned short m = (((unsigned char)b0) << 8) | ((unsigned char)b1); 
							int move_visit = (m << 16) | v;
							pz->vv_move_visit[tesuu].push_back(move_visit); 
							b0 = b1 = 0;
						} else {
							if ( getMoveFromCsaStr(&bz, &az, &tk, &nf, str)==0 ) DEBUG_PRT("");
							int c = (tesuu+(pz->handicap!=0))&1;
							if ( is_pseudo_legalYSS((Move)pack_te(bz,az,tk,nf), (Color)(c==1) ) == false ) {
								DEBUG_PRT("move Err %3d:%s\n",tesuu,str);
							}
							pack_from4_to_2_KDB(&b0,&b1, bz, az, tk, nf);
						}
					}
//					PRT("%s\n",str);
					count++;
					if ( c != ',' ) break;
				}
				forth_move();
//				PRT("%3d:count=%3d, (%d,%d)\n",tesuu,count,all_visit,sum_visit);
				if ( 0 && tesuu == 375 ) {
					for (i=0;i<(int)pz->vv_move_visit[tesuu-1].size();i++) {
						unsigned int x = pz->vv_move_visit[tesuu-1][i];
						PRT("(%2d,%2d)%3d,",x>>24,(x>>16)&0xff,x&0xffff);
					}
				}
			}
#else
			static int n = 0;
			lpLine[0] = '*';
			strcpy(kifu_log[tesuu][n],lpLine);
			change_log_pv(kifu_log[tesuu][n]);	// floodgate用

			n++;
			if ( n==2 ) n = 0;
			lpLine[0] = '\'';
#endif
		}

		if ( lpLine[0] == '\'' && prt_flag == 0 ) { nLen = 0; continue; }	// 最初のコメントしか表示しない
		if ( strncmp(lpLine,"$START_TIME:",12)==0 ) {
			int n = strlen_int(lpLine);
			lpLine[n-1]=0;
			sprintf(sGameDate,"%s",lpLine+12);
		}
		if ( lpLine[0] == '$' || lpLine[1] == 'V' ) { PRT("%s",lpLine); continue; }	// 付加情報

		if ( lpLine[0] == 'P' ) {	// 盤面図
/*
P6 *  *  *  *  * +KA * -TO+FU
P7 *  *  *  *  *  *  *  *  * 
'  先手持駒
P+00KE00KE00FU
'  後手持駒
P-00HI
P-00AL
*/
/*
P-21KE22OU33KE23FU
P+32GI14FU26KE
P+00KI
P-00AL
+
+2634KE
-2232OU
+0042KI
*/
/*
PI                          平手
PI11KY                      香落ち
PI22KA                      角落ち
PI82HI                      飛車落ち
PI82HI22KA                  2枚落ち
PI82HI22KA11KY91KY          4枚落ち
PI82HI22KA11KY91KY21KE81KE  6枚落ち
*/

			y = lpLine[1];
			if ( y == '+' || y == '-' ) {	// 持駒
				for (lpCopy=lpLine+2; lpCopy-lpLine<nLen-4; lpCopy+=4) {
					if ( !(*(lpCopy+0) == '0' && *(lpCopy+1) == '0') ) {	// 座標指定
						int xx = *(lpCopy+0) - '0';
						int yy = *(lpCopy+1) - '0';
						if ( yy<1 || yy>9 || xx<1 || xx>9 ) continue;
						if ( fShortCSA == 0 ) {
							int x,y;
							for (y=0;y<9;y++) for (x=0;x<9;x++) {
								init_ban[(y+1)*16+x+1] = 0;
							}
						}
						fShortCSA = 1;
						int i = get_csa_koma(lpCopy+2);
						if ( i==0 ) { PRT("持駒種別エラー='%c%c'\n",*(lpCopy+2),*(lpCopy+3)); }
						init_ban[yy*16+(10-xx)] = i + (y=='-')*(0x80);
						continue;
					}
					if ( *(lpCopy+2) == 'A' && *(lpCopy+3) == 'L' ) {	// 残りの持駒全て
						for ( i=1; i<8; i++ ) {
							k = 0;
							for (z=0;z<BAN_SIZE;z++) k += ((init_ban[z] & 0x77)==i);	// 盤外 0xff を意識して 
							k += mo_c[i] + mo_m[i];
							if ( i==1 )      k = 18 - k;
							else if ( i>=6 ) k =  2 - k;
							else             k =  4 - k;
							if ( y == '+' ) mo_m[i] = k;	// 先手の持駒
							if ( y == '-' ) mo_c[i] = k;	// 後手の持駒
						}
						break;	// P-00AL の時はこれ以上探さない
					}

					int i = get_csa_koma(lpCopy+2);
					if ( i==0 || i>=8 ) { PRT("持駒種別エラー='%c%c'\n",*(lpCopy+2),*(lpCopy+3)); }
					else if ( y == '+' ) mo_m[i]++; // 先手の持駒
					else if ( y == '-' ) mo_c[i]++;	// 後手の持駒
				}
			}
			if ( y == 'I' ) {	// 初期設定配置
				for (lpCopy=lpLine+2; lpCopy-lpLine<nLen-4; lpCopy+=4) {
					int xx = *(lpCopy+0) - '0';
					int yy = *(lpCopy+1) - '0';
					if ( yy<1 || yy>9 || xx<1 || xx>9 ) { DEBUG_PRT("PI Err\n"); }
					int i = get_csa_koma(lpCopy+2);
					if ( i==0 ) { DEBUG_PRT("持駒種別エラー='%c%c'\n",*(lpCopy+2),*(lpCopy+3)); }
					init_ban[yy*16+(10-xx)] = 0;
				}
			}

			y = lpLine[1] - '0';
			if ( 1<=y && y<=9 ) {	// 1行の駒
				lpCopy = lpLine+2;
				for ( x=1;x<10;x++,lpCopy+=3 ) {
					z = y*16+x;
					c = *(lpCopy+0);
					k = 0x00;
					if ( c=='+' ) ;					// 先手の駒	
					else if ( c=='-' ) k = 0x80;	// 後手の駒	
					else {
						init_ban[z] = 0;
						continue;
					}
					i = get_csa_koma(lpCopy+1);
					if ( i==0 ) break;
//					if ( k+i ==0x08 ) i = 0;	// 詰将棋で先手王を消す
					init_ban[z] = k + i;
				}
			}
			continue;	// 盤面図
		}
		if ( lpLine[0] != '+' && lpLine[0] != '-' ) PRT("%s",lpLine);
		if ( lpLine[0] == '\'' ) { nLen = 0; continue; }	// コメント
		if ( lpLine[0] == 'V' ) continue;	// バージョン
		if ( lpLine[0] == 'N' ) {
			if ( lpLine[1] == '+' ) {
				strncpy(PlayerName[0],&lpLine[2],strlen_int(lpLine)-3);	// 先手の名前
				PlayerName[0][strlen_int(lpLine)-3] = 0;	
			}
			if ( lpLine[1] == '-' ) {
				strncpy(PlayerName[1],&lpLine[2],strlen_int(lpLine)-3);	// 後手の名前
				PlayerName[1][strlen_int(lpLine)-3] = 0;	
			}
			continue;
		}
		if ( lpLine[0] == '%' ) {
#ifdef AOBA_ZERO
			ZERO_DB *pz = &zdb_one;
			pz->moves = tesuu;
			pz->result = ZD_DRAW;
			pz->result_type = RT_NONE;
			int is_gote_turn = (tesuu + (pz->handicap!=0))& 1;

			if ( strstr(lpLine,"TORYO") ) {
				if ( is_gote_turn ) {
					pz->result = ZD_S_WIN;
				} else {
					pz->result = ZD_G_WIN;
				}
				pz->result_type = RT_TORYO;
			}
			if ( strstr(lpLine,"KACHI") ) {
				if ( is_gote_turn ) {
					pz->result = ZD_G_WIN;
				} else {
					pz->result = ZD_S_WIN;
				}
				pz->result_type = RT_KACHI;
			}
			// %+ILLEGAL_ACTION 先手(下手)の反則行為により、後手(上手)の勝ち
			// %-ILLEGAL_ACTION 後手(上手)の反則行為により、先手(下手)の勝ち
			if ( strstr(lpLine,"+ILLEGAL_ACTION") ) {	// 2020/03/27 連続王手の後、王側が逃げて千日手確定のみ
				pz->result = ZD_G_WIN;
				pz->result_type = RT_S_ILLEGAL_ACTION;
			}
			if ( strstr(lpLine,"-ILLEGAL_ACTION") ) {
				pz->result = ZD_S_WIN;
				pz->result_type = RT_G_ILLEGAL_ACTION;
			}
			if ( strstr(lpLine,"SENNICHITE") ) {
				pz->result_type = RT_SENNICHITE;
			}
			if ( strstr(lpLine,"CHUDAN") ) {
				pz->result_type = RT_CHUDAN;
			}

			int i,sum=0;
			for (i=0;i<(int)pz->vv_move_visit.size();i++) {
				sum += pz->vv_move_visit[i].size();
			}
			PRT("handicap=%d,moves=%d,result=%d, mv_sum=%d,%.1f\n",pz->handicap,pz->moves,pz->result,sum, (double)sum/(tesuu+0.00001f));
			if ( pz->result_type == RT_NONE ) DEBUG_PRT("");
#endif
			break;		// 読み込み終了
		}
		if ( nLen < 4 ) continue;			// 手番
	
		prt_flag = 0;
		if ( getMoveFromCsaStr(&bz, &az, &tk, &nf, &lpLine[1])==0 ) break;
		if ( is_pseudo_legalYSS((Move)pack_te(bz,az,tk,nf), (Color)(lpLine[0]=='-')) == false ) { PRT("move Err %3d:%s\n",tesuu+1,lpLine); break; }

#ifdef AOBA_ZERO
		int b0,b1;
		pack_from4_to_2_KDB(&b0,&b1, bz, az, tk, nf);
		unsigned short m = (((unsigned char)b0) << 8) | ((unsigned char)b1); 
		zdb_one.v_kif.push_back(m); 
#endif
		kifu_set_move(bz,az,tk,nf,0);

		if ( tesuu == KIFU_MAX-1 ) break;
	}
	PRT("読み込んだ手数=%d (CSA形式)\n",tesuu);
	if ( prt_flag == 1 ) {	// 手順が入っていない場合、ここで初期化
		ban_saikousei();	// 盤面の再構成。
		check_kn();			// 盤面の状態が正常化チェック
	}
#ifdef AOBA_ZERO
	if ( zdb_one.moves < 0 ) {
		PRT("file read err\n");
		return 0;
	}
#endif
	return 1;
}

// Shotest形式の棋譜を読み込む
void shogi::LoadShotest(void)
{
	char lpLine[TMP_BUF_LEN];
	int i,x,y,t, nLen;
	int bz,az,tk,nf;
	char c;

	tesuu = 0;
	hirate_ban_init(KomaOti);		// 盤面の初期化　平手の状態へ

	nLen = 0;
	for ( ;; ) {
		nLen = ReadOneLine(lpLine);
		if ( nLen == 0 ) break; // EOF
		lpLine[nLen] = 0;
//		PRT("%s, nLen=%d\n",lpLine,nLen);
		c = lpLine[0];
		if ( c == ';' || c == ':' || c == '=' || c == '*' ) continue;

/*
先頭 ';' はコメント
先頭 ':' は盤面
先頭 '=' は持ち駒
先頭 '*' は？

116. S 8fx7g+  399  14  98848 5  54  4 15  1 0 5  56  5
117. G 7hx7g     0   2      - -   -  -  -  - - -   -  -
118. S  * 3h   564   1  76103 5 100  9 15  1 0 5  17  9
*/				
		if ( nLen < 13 ) continue;			// 指し手ではないね。

		tk = 0;
		nf = 0;
		x = lpLine[10] - '0';
		y = lpLine[11] - 'a' + 1;
		az = y*16 + 10 - x;	// az を先に読むのはtkの取った駒を知りたいから

		x = lpLine[7] - '0';
		y = lpLine[8] - 'a' + 1;
		if ( lpLine[7] == ' ' ) {	// 駒打ち
			bz = 0xff;
			c = lpLine[5];
			// P L N S G B R
			{
				static char drop_shotest[] = "PLNSGBR";	// 
				char *pdest;

				pdest = strchr( drop_shotest, c );
				if ( pdest == NULL ) { DEBUG_PRT("持ち駒発見ミス shotest\n"); }
				i = pdest - drop_shotest + 1;
			}
			tk = i + (0x80)*(tesuu&1);
		} else {					// 駒移動
			tk = init_ban[az];
			bz = y*16 + 10 - x;
			if ( lpLine[12] == '+' && (init_ban[bz] & 0x0f) < 0x08 ) nf = 0x08;
		}
		// 思考時間を取り出す
		lpLine[0] = lpLine[19];
		lpLine[1] = lpLine[20];
		lpLine[2] = lpLine[21];
		lpLine[3] = 0;
		t = atoi(lpLine);

		kifu_set_move(bz,az,tk,nf,t);
		if ( tesuu == KIFU_MAX-1 ) break;
	}
	PRT("読み込んだ手数=%d (Shotest形式)\n",tesuu);
}

// output出力の盤面を読み込む
void shogi::LoadOutBan(void)
{
	char str[TMP_BUF_LEN],*p,c,cs[2];
	int k,x,y,z,i,nLen;
	
	hirate_ban_init(KomaOti);		// 盤面の初期化　平手の状態へ

	nLen = 0;
	y = 0x10;
	for ( ;; ) {
		nLen = ReadOneLine(str);
		if ( nLen == 0 ) break; // EOF
		str[nLen] = 0;
//		PRT("%s, nLen=%d\n",str,nLen);
		p = strstr(str,"0xff");
		if ( p == NULL ) continue;
		p += 8;
		for (x=1;x<10;x++) {
			for (i=0;i<2;i++) {
				c = *(p+(x-1)*5+2 + i);
				if ( c >= 'a' ) c = c - 'a' + 10;
				else c = c - '0';
				cs[i] = c;
			}
			k = cs[0]*16 + cs[1];
			z = x + y;
			init_ban[z] = k;
		}
		y+=0x10;
		if ( y!=0xa0 ) continue;
		for (int n=0;n<2;n++) {
			nLen = ReadOneLine(str);
			if ( strstr(str,"mo_c")!= NULL ) for (i=0;i<8;i++) mo_c[i] = *(str+20 + i*3) - '0';
			else                             for (i=0;i<8;i++) mo_m[i] = *(str+20 + i*3) - '0';
		}
	}
	ban_saikousei();	// 盤面の再構成。
	check_kn();			// 盤面の状態が正常化チェック
}

void shogi::LoadTextBan()
{
	char str[TMP_BUF_LEN],*p;
	int k,x,y,z,i,nLen;
	
	hirate_ban_init(KomaOti);		// 盤面の初期化　平手の状態へ

	nLen = 0;
	y = 1;
	for ( ;; ) {
		nLen = ReadOneLine(str);
		if ( nLen == 0 ) break; // EOF
		str[nLen] = 0;
//		PRT("%s, nLen=%d\n",str,nLen);
		char find[TMP_BUF_LEN];
		sprintf(find,"%d│",y);
		p = strstr(str,find);
		if ( p == NULL ) continue;
		p += 3;
		for (x=1;x<10;x++) {
			for (i=0;i<32;i++) {
				if ( strncmp(p+(x-1)*2,koma_kanji[i],2)==0 ) break;
			}
			k = i;
			if ( i > 16 ) k += 0x70;
			if ( i==32 ) k = 0;
			z = x + y*16;
			init_ban[z] = k;
		}

		if ( y==1 ) {	// COM :霓 0:霖 0:霰 0:靄 0:靉 0:靦 0:靱 0:
			for (i=1;i<8;i++) {
				sprintf(find,"%s",koma_kanji[i+16]);
				char *q = strstr(p+2*9,find);
				if ( q==NULL) { DEBUG_PRT("not found 駒\n"); }
				mo_c[i] = *(q+3) - '0';
			}
		}
   		y++;
		if ( y==10 ) break;
	}

	// 余った駒をMANの持ち駒に
	int ksum[8][2] = { {0,2}, {0,18}, {0,4}, {0,4}, {0,4}, {0,4}, {0,2}, {0,2} };
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int z = (y+1)*16 + (x+1);
		k = init_ban[z];
		ksum[k&0x07][0]++;
	}
	for (i=1;i<8;i++)  ksum[i][0] += mo_c[i]; 
	for (i=1;i<8;i++) mo_m[i] = ksum[i][1] - ksum[i][0]; 

	ban_saikousei();	// 盤面の再構成。
	check_kn();			// 盤面の状態が正常化チェック

	hanten_with_hash();
#if defined(_MSC_VER)
	save_clipboard(1);	// クリップボードに棋譜をコピー
#endif
}


const char *sSengoSankaku[3] = {"▲","△","▽"};

int shogi::IsSenteTurn() { DEBUG_PRT(""); return ( ((fGotekara+tesuu)&1) == 0 ); }	// 駒落ちで正しくない
int shogi::GetSennititeMax() { return nSennititeMax; }

// 柿木形式の棋譜を読み込む
void shogi::LoadKaki()
{
	char lpLine[TMP_BUF_LEN];
	int i,j,k,x,y,z,t, nLen;
	int bz,az,tk,nf;
	const char *lpCopy,*lpKanji;
	char c;
	int sente_motigoma_flag = 0;	// 先手の持駒を示す
	int fKI2 = 0;	// KI2形式の場合
//	int fKomaoti = 0;				// 駒落ちの棋譜の場合1
	string sOneLine;
	int top_comment_n = 0;

	// 一行をロードする
	// "手" が見つかるまでロード
/*
# ファイル名：A:\Program Files\KShogi95\詰将棋２\1-2.kif
対局日：1997/07/30(水) 13:47:41
手合割：平手　　
後手の持駒：飛二　角　金二　銀四　桂三　香四　歩十五　
  ９ ８ ７ ６ ５ ４ ３ ２ １
+---------------------------+
| ・ ・ ・ ・ ・ ・ ・ ・ ・|一
| ・ ・ ・ ・ ・ ・ ・ ・ ・|二
| ・ ・ ・ ・ ・ ・ ・ ・v歩|三
| ・ ・ ・ ・ ・ ・ 角 ・ ・|四
| ・ ・ ・ ・ ・ ・v桂 金 ・|五
| ・ ・ ・ ・ ・ ・ ・ 歩v玉|六
| ・ ・ ・ ・ ・ ・ 金 ・vと|七
| ・ ・ ・ ・ ・ ・ ・ ・ ・|八
| ・ ・ ・ ・ ・ ・ ・ ・ ・|九
+---------------------------+
先手の持駒：なし
先手：
後手：
手数----指手---------消費時間--
*/

	y = 0;	// 盤面読み込み用
	hirate_ban_init(0);		// 盤面の初期化　平手の状態へ
//	for (i=0;i<8;i++) mo_c[i] = mo_m[i] = 0;
	
	for ( ;; ) {
		nLen = ReadOneLine(lpLine);
		if ( nLen == 0 ) {
			PRT("EOF - 読み込み失敗。「手数」が見つからない\n");
			break;
		}
		lpLine[nLen] = 0;
		PRT("%s",lpLine);
	 	// 漢字の「手」を探す
		if ( strstr(lpLine,"手数----") != NULL ) break;
//		if ( (unsigned char)lpLine[0] == 0x8e && (unsigned char)lpLine[1] == 0xe8 &&
//		     (unsigned char)lpLine[2] == 0x90 && (unsigned char)lpLine[3] == 0x94 ) break;

		// 行の先頭が▲、△、▽ならki2形式。
		for (i=0;i<3;i++) if ( strncmp(lpLine,sSengoSankaku[i],2)==0 ) break;
		if ( i!=3 ) {
			fKI2 = 1;
			break;
		}

		if ( (unsigned char)lpLine[0] == '#' ) {
			strncpy(kifu_log_top[top_comment_n],lpLine,TMP_BUF_LEN-1);
			top_comment_n++;
			if ( top_comment_n==KIFU_LOG_TOP_MAX ) top_comment_n = 0;
		}

		// 盤面データを読み込む。
		if ( (unsigned char)lpLine[0] == '|' || (unsigned char)lpLine[1] == '|' ) {
			x = 1;
			y++;
//			PRT("y=%d",y);
			if ( y >= 10 ) {PRT("盤面エラー。盤が大きすぎます。");break;}
			lpCopy = lpLine + 1;
			if ( (unsigned char)lpLine[1] == '|' ) lpCopy++;	// 脊尾詰形式？
			for ( i=1;; ) {
				c = *lpCopy;
				k = 0xff;
				if ( c == ' ' || c == '^' || c== '+' ) k = 0x00;	// 先手の駒
				if ( c == 'V' || c == 'v' || c== '-' ) k = 0x80;	// 後手の駒
				if (  k == 0xff ) {PRT("盤面エラー");break;}
	
				lpCopy++;i++;
				if ( i >= nLen ) break;		// 読み込んだ文字数を超えた。

				for ( j=1;j<16;j++) {
					if ( *(lpCopy) == *(koma_kanji[j]) && *(lpCopy+1) == *(koma_kanji[j]+1) ) break;
				}
				if ( j==16 ) {	// "王","竜" のチェック	
					lpKanji = "王";
					if ( *(lpCopy) == *(lpKanji) && *(lpCopy+1) == *(lpKanji+1) ) j = 8;
					lpKanji = "竜";
					if ( *(lpCopy) == *(lpKanji) && *(lpCopy+1) == *(lpKanji+1) ) j = 15;
				}
				if ( j==16 ) k = 0;
				else k = k + j;

//				PRT("c=%c, j=%d,k=%x,x=%d\n",c,j,k,x);

				z = y *16 + x;
				init_ban[z] = k;
				x++;
				if ( x > 9 ) break;
				lpCopy++;i++;
				lpCopy++;i++;
				if ( i >= nLen ) break;		// 読み込んだ文字数を超えた。
			}
		}

		// 持ち駒を読み込む
		// 持駒と持駒の間は全角の空白1文字、または半角で2文字で区切りをつける。それ以外は不可。
		sente_motigoma_flag = 0;	//持駒データではない。

		lpKanji = "先手の持駒：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 1;	//先手の持駒
		lpKanji = "下手の持駒：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 1;	//先手の持駒
		lpKanji = "持駒：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 3;	//先手の持駒で後手は全部
		lpKanji = "後手の持駒：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 2;	//後手の持駒
		lpKanji = "上手の持駒：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 2;	//後手の持駒

		if ( sente_motigoma_flag != 0 ) {
			i = 0;
			lpCopy = lpLine + i;
//			PRT("smf=%d\n",sente_motigoma_flag);
	
			for ( ;; ) {
				for ( j=1;j<8;j++) {
					if ( strncmp(lpCopy,koma_kanji[j],2)==0 ) break;
				}
				lpCopy++;i++;
				lpCopy++;i++;
				if ( i >= nLen ) break;		// 読み込んだ文字数を超えた。

				if ( j == 8 ) continue;
				k = j;
				
				// 枚数を読み込む

				for ( j=1; j<11; j++ ) {
					if ( strncmp(lpCopy,num_kanji[j],2)==0 || strncmp(lpCopy,num_zenkaku[j],2)==0) break;
				}

				if ( j==11 ) {
					j = 1;	// 持駒は最低でも1枚
					if ( !( strncmp(lpCopy,"　",2)==0 || strncmp(lpCopy,"  ",2)==0 ) ) {	// 全角と半角の空白
//						PRT("次の文字が空白ではない"); 
						lpCopy--;i--;
						lpCopy--;i--;
					}
				} else if ( j==10 ) {
					for ( j=1; j<11; j++ ) {
						if ( strncmp(lpCopy+2,num_kanji[j],2)==0 ) break;
					}
					if ( j==11 ) j = 10;
					else j = 10 + j;
					lpCopy++;i++;
					lpCopy++;i++;
				}

				if ( sente_motigoma_flag == 2 ) mo_c[k] += j;
				else                            mo_m[k] += j;

				lpCopy++;i++;
				lpCopy++;i++;
				if ( i >= nLen ) break;		// 読み込んだ文字数を超えた。
			}
		}
		if ( sente_motigoma_flag == 3 ) {	// 先手の持駒で後手は全部
			mo_c[0] = 2;
			mo_c[1] = 18;
			mo_c[2] = 4;
			mo_c[3] = 4;
			mo_c[4] = 4;
			mo_c[5] = 4;
			mo_c[6] = 2;
			mo_c[7] = 2;
			for (z=0x11;z<0x9a;z++) {
				k = init_ban[z];
				if ( k == 0 || k == 0xff ) continue;
				mo_c[k&0x07]--;
			}
			for (i=1;i<8;i++) mo_c[i] -= mo_m[i];
//			for (i=1;i<8;i++) PRT("mo_c[%d]=%d\n",i,mo_c[i]);
		}

		// 対局者名を読み込む
		const char *pSGName[4] = { "先手：","下手：","後手：","上手：" };
		for (i=0;i<4;i++) {
			lpKanji = pSGName[i];
			j = i/2;
			if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
				strncpy(PlayerName[j],&lpLine[6],strlen_int(lpLine)-7);	// 先手の名前
				PlayerName[j][strlen_int(lpLine)-7] = 0;	
			}
		}

		// 駒落ちの棋譜では駒打ちを先後反転させる。
		// 手合割：平手　　手合割：香落ち　手合割：二枚落ち 手合割：四枚落ち 手合割：四枚落ち
		lpKanji = "手合割：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
			lpKanji = "落";
			if ( strstr( lpLine, lpKanji ) != NULL ) fGotekara = 1;
			// 0...平手、1...香落、2...角落、3...飛落、4...２枚、5...４枚、6...６枚
			for (i=KomaWariNum-1;i>=1;i--) {
				if ( strstr( lpLine, KomaWari[i] ) != NULL ) { KomaOti = i; break; }
			}
			if ( KomaOti ) {
				hirate_ban_init(KomaOti);		// 盤面の初期化　平手の状態へ
				hanten_with_hash();				// 完全盤面反転
			}
//			PRT("%s,%s\n",lpLine,lpKanji);	PRT("手合割は %d\n",KomaOti);
		}
		lpKanji = "後手番";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) fGotekara = 1;

		lpKanji = "棋戦：";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
			strncpy(Kisen,&lpLine[6],strlen_int(lpLine)-7);
			Kisen[strlen_int(lpLine)-7] = 0;	
		}
		const char *pSGDate[2] = { "対局日：","開始日時：" };
		for (i=0;i<2;i++) {
			lpKanji = pSGDate[i];
			int nLen = strlen_int(lpKanji);
			if ( strncmp( lpKanji, lpLine, nLen ) ==  0 ) {
				char s2[TMP_BUF_LEN];
				strcpy(s2,&lpLine[nLen]);
				s2[4] = 0;
				int y = atoi(s2);
				s2[7] = 0;
				int m = atoi(s2+5);
				s2[10] = 0;
				int d = atoi(s2+8);
				int ymd = y*10000+m*100+d;
				nKifDateYMD = ymd;
			}
		}

		
	}

	ban_saikousei();	// 盤面の再構成。
	check_kn();			// 盤面の状態が正常化チェック

	if ( nLen == 0 ) return;
	
	tesuu = 0;
	int comment_n = 0;
	if ( fKI2 ) {
		int n = 0;
		for (;;) {
			// 柿木2形式の棋譜を読み込む。0...読み込みエラー。1...1行が終わった。2...1手進んだ。3...次の文字を読むべし。
			int flag = ReadOneMoveKi2(lpLine, &n);
			if ( flag == 0 ) break;
			if ( flag == 1 ) {
				n = 0;
				nLen = ReadOneLine( lpLine );
				if ( nLen == 0 ) break; // EOF
			}
		}

	} else for ( ;; ) {
		nLen = ReadOneLine( lpLine );
//		if ( nLen >= TMP_BUF_LEN-1 ) { ReadOneLine( lpLine ); continue; }
		if ( nLen == 0 ) break; // EOF
		lpLine[nLen] = 0;
		if ( lpLine[0] == '*' || lpLine[0] == '#' ) {
//			PRT("%s",lpLine);
			if ( strstr(lpLine,"#--separator--" ) ) break;	// 分岐棋譜は無視
			// 柿木形式のコメントを2行だけ取り込む
			if ( lpLine[0] == '*') {
				strncpy(kifu_log[tesuu][comment_n],lpLine,TMP_BUF_LEN-1);
				comment_n++;
				if ( comment_n==2 ) comment_n = 0;
			}
			continue;	// コメント
		}
//		PRT("%s",lpLine);


		if ( nLen < 11 ) continue;
//		sscanf(lpLine,"%d",&n);
//		PRT("n = %d\n",n);
		// 24の棋譜用に、最初が数字だったら最初の半角空白の次から調べる。	
		if ( *lpLine != ' ' ) {
			lpCopy = strstr(lpLine," ");
			if ( lpCopy == NULL ) continue;
			lpCopy++;
		} else {
			lpCopy = lpLine + 5;
		}

		for ( x=1; x<10; x++ ) {
			if ( *(lpCopy + 0) == *(num_zenkaku[x]) && *(lpCopy + 1) == *(num_zenkaku[x] + 1) ) break;
		}
		for ( y=1; y<10; y++ ) {
			if ( *(lpCopy + 2) == *(num_kanji[y]) && *(lpCopy + 3) == *(num_kanji[y] + 1) ) break;
		}
		if ( x==10 || y==10 ) {
			const unsigned char sFind[] = "同";
			for ( i=0; i<10; i++ ) {	// 「同」を探す
				if ( (unsigned char)*(lpCopy+i) == sFind[0] && (unsigned char)*(lpCopy+i+1) == sFind[1] ) break;
//				if ( (unsigned char)*(lpCopy+i) == 0x93 && (unsigned char)*(lpCopy+i+1) == 0xaf ) break;
			}
			if ( i == 10 ) {break;}
			az = kifu[tesuu][1];
		} else az = y*16 + 10 - x;
		nf = 0;
		tk = 0;
		const unsigned char sFind[] = "成";
		for ( i=6; i<10; i++ ) {	// 「成」を探す
			if ( (unsigned char)*(lpCopy+i) == sFind[0] && (unsigned char)*(lpCopy+i+1) == sFind[1] ) nf = 0x08;
//			if ( (unsigned char)*(lpCopy+i) == 0x90 && (unsigned char)*(lpCopy+i+1) == 0xac ) nf = 0x08;
		}
		x = y = 0;
		for ( i=6; i<10; i++ ) {	// 「(」を探す
			if ( *(lpCopy + i) == '(' ) {
				x = *(lpCopy+i+1) - '0';
				y = *(lpCopy+i+2) - '0';
				break;
			}
		}
		if ( x == 0 ) {	// 駒打ち
			bz = 0xff;
			for ( i=1; i<8; i++ ) {
				if ( *(lpCopy+4) == *(koma_kanji[i]) && *(lpCopy+5) == *(koma_kanji[i]+1) ) break;
			}
			if ( i==8 ) {PRT("駒打ちエラー");break;}
			tk = i + (0x80)*((tesuu+fGotekara)&1);
		} else {
			bz = y*16 + 10 - x;
			tk = init_ban[az];
		}
		// 昔のYSSの反転棋譜を読み込む場合。
//		hanten_sasite(&bz, &az, &tk, &nf);	if ( bz != 0xff ) tk = init_ban[az]; else tk = (tk & 0x07) + (0x80)*((tesuu+0)&1);

		// 消費時間を読み取る
		t = 0;
		for ( i=11; i<16; i++ ) {	// 「(」を探す
			if ( *(lpCopy + i) == '(' ) {
				lpCopy = lpCopy +i+1;
				// ':' があるまでが分
				for (j=0;j<5;j++) {
					k = *(lpCopy+j);
					if ( k == ':' ) break;
				}
				if ( j==5 ) break;
				x = *(lpCopy+j-1) - '0';
				y = *(lpCopy+j-2) - '0';
				z = *(lpCopy+j-3) - '0';
				if ( 0<=x && x<=9 ) t  = x;
				if ( 0<=y && y<=9 ) t += y*10;
				if ( 0<=z && z<=9 ) t += z*100;
				t = t*60;
				// '/' があるまでが秒
				lpCopy = lpCopy + j +1;
				for (j=0;j<5;j++) {
					k = *(lpCopy+j);
					if ( k == '/' ) break;
				}
				if ( j==5 ) break;
				x = *(lpCopy+j-1) - '0';
				y = *(lpCopy+j-2) - '0';
				if ( 0<=x && x<=9 ) t += x;
				if ( 0<=y && y<=9 ) t += y*10;
//				PRT("%s x=%d,y=%d,  t=%d\n",lpCopy,x,y,t);
				break;
			}
		}

//		if ( t >= 30 ) { if ( strstr(PlayerName[(tesuu) & 1],"YSS") ) {PRT("%3d手,t=%d over!!!\n",tesuu+1,t); debug();} }	// 王者戦用の時間チェック

		if ( is_pseudo_legalYSS( (Move)pack_te(bz,az,tk,nf), (Color)(tesuu&1) )==0 ) {
			PRT("エラーの手\n");
			break;
		}

		kifu_set_move(bz,az,tk,nf,t);
		if ( tesuu == KIFU_MAX-1 ) break;
	}
	if ( tesuu ) PRT("読み込んだ手数=%d\n",tesuu);
}

// 柿木2形式の棋譜を読み込む。0...読み込みエラー。1...1行が終わった。2...1手進んだ。3...次の文字を読むべし。
int shogi::ReadOneMoveKi2(char *top_p, int *p_next_n)
{
	int i,n,nLen,nLen2,k,x,y,bz,az,tk,nf,flag,sum,loop,k_n,kk;
	int *pkb,*pmo;
	int bz_stoc[KB_MAX];
	char *p;

	const char *koma_2moji[16] = {
	  "王","歩",  "香",  "桂",  "銀","金","角","飛",
	  "玉","と","成香","成桂","成銀","金","馬","龍"
	};

	const char *koma_dir[10] = {
	//					 右、以降は組み合わせが生じる。
	// 	0x01 0x02 0x04 0x08 0x10 0x20 0x40 0x80 0x100
		"打","直","寄","行","右","左","上","引","成","不成"
	};

	nLen = strlen_int(top_p);
	n = *p_next_n;
	if ( n >= nLen ) return 1;	// 次の行へ
	p = top_p + n;
	 
	if ( n == 0 ) {	// 行の先頭から調べる時、
		if ( *top_p == '*' || *top_p == '#' ) {	// コメント
			PRT("%s",top_p);
			return 1;
		}
//		// 最初に先後の符号を探す。
//		for (i=0;i<3;i++) if ( strncmp(p,sSengoSankaku[i],2)==0 ) break;
	}

	// 最初に先後の符号を探す。
	for (i=0;i<3;i++) if ( strncmp(p,sSengoSankaku[i],2)==0 ) break;
	if ( i==3 ) {
		n++;
		*p_next_n = n;
		return 3;	// 次の文字を調べる。
	}

	int fSenteTurn = 0;
	if ( i==0 ) fSenteTurn = 1;	

	// 座標、以下を読み取る。
	n += 2;	p = top_p + n;
	if ( n+2 >= nLen ) return 1;	// 次の行へ

	for (x=1;x<10;x++) if ( strncmp(p  ,num_zenkaku[x], 2)==0 ) break;
	for (y=1;y<10;y++) if ( strncmp(p+2,num_kanji[y],2)==0 ) break;
	if ( x==10 || y==10 ) {
		if ( strncmp(p,"同",2)!=0 ) { PRT("座標読み取りErr x=%d,y=%d,n=%d\n",x,y,n); return 0; }
		if ( strncmp(p+2,"　",2)!=0 && strncmp(p+2,"  ",2)!=0 ) n -= 2;	// 同香、などと空白がない場合。
		az = kifu[tesuu][1];
	} else {
		az = y*16 + 10 - x;
	}
	// 動いた駒を読み取る。
	n += 4;	p = top_p + n;
	if ( n >= nLen ) return 1;	// 次の行へ

	for (k=0;k<16;k++) {
		const char *p2 = koma_2moji[k];
		nLen2 = strlen_int(p2);
		if ( strncmp(p,p2,nLen2)==0 ) break;
	}
	if ( k==16 ) { PRT("駒取得失敗\n"); return 0; }
	if ( k==0 ) k = 8;	// 王は玉に。
	// 右、左、上、寄、引、直、行、打、成、不成を読み取る。▲５三銀右引不成
	// 打、は必ず最初に来る。成、不成は最後に付く。
	// 
	// 読み取れない文字が来るか、1行が終わるか、次の△が来るまで読み込む。
	// 右、左、寄る、引、直、行、打、は必ず
	n += nLen2;	p = top_p + n;
	flag = 0;
	for (;;) {
		if ( n >= nLen ) break;	// 行の終わり

		for (i=0;i<10;i++) {	// 「右」などを探す
			const char *p2 = koma_dir[i];
			nLen2 = strlen_int(p2);
			if ( strncmp(p,p2,nLen2)==0 ) break;
		}
		if ( i==10 ) break;
		flag |= 1 << i;	// bitで保存。

		n += nLen2;	p = top_p + n;
	}
	// 全部の情報が読み取れた。これで着手を決定する。
//	PRT("%3d:x=%2d,y=%2d,az=%02x,k=%2d,flag=%4x,n=%d\n",tesuu,x,y,az,k,flag,n);
	if ( flag & 0x100 ) nf = 0x08;
	else                nf = 0x00;
	tk = init_ban[az];
	bz = 0;
	// そのマス目に移動可能な駒を調べる。
	sum = 0;
	if ( fSenteTurn ) {
		loop = kiki_m[az];
		pkb  = kb_m[az];
		pmo  = mo_m;
	} else {
		loop = kiki_c[az];
		pkb  = kb_c[az];
		pmo  = mo_c;
	}

	for (i=0;i<loop;i++) {
		k_n = *(pkb+i);
		if ( k_n > 0x80 ) continue;	// 影の利き
		kk = kn[k_n][0];
		bz = kn[k_n][1];
		if ( (kk & 0x0f) != k ) continue;	// 違う駒の利き
		bz_stoc[sum++] = bz;
	}
	if ( sum == 0 || (flag & 0x01) ) {	// 移動可能手がない。または「打」が指定。駒を打つしかないはず。
		if ( *(pmo+k) == 0 ) { PRT("打つ駒がない,bz=%02x,az=%02x,tk=%02x,flag=%d\n",bz,az,tk,flag); return 0; }
		bz = 0xff;
		tk = k;
		if ( fSenteTurn==0 ) tk += 0x80;
	} else if ( sum == 1 ) {	// 唯一の手
		bz = bz_stoc[0];
	} else {		// 複数可能手がある。やっかい。
//		PRT("複数の可能手！\n");	
		// 	0x01 0x02 0x04 0x08 0x10 0x20 0x40 0x80 0x100
		//	"打","直","寄","行","右","左","上","引","成","不成"
		// 条件に当てはまる場合にボーナスを。一番点数が高い手を選択。
		int max = -1;
		int max_i = -1;
		for (i=0;i<sum;i++) {
			bz = bz_stoc[i];
			int bbz,aaz;
			if ( fSenteTurn ) {
				bbz = bz;
				aaz = az;
			} else {
				bbz = az;
				aaz = bz;
			}

			int add = 0;
			int bx =  bbz & 0x0f;
			int by = (bbz & 0xf0) >> 4;
			int ax =  aaz & 0x0f;
			int ay = (aaz & 0xf0) >> 4;
			if ( bx >  ax  && (flag&0x10) ) add+=2;	// 右
			if ( bx == ax  && (flag&0x10) ) add++;	// 右
			if ( bx <  ax  && (flag&0x20) ) add+=2;	// 左 (▲71馬▲61馬のとき▲72馬は「▲72馬左」)
			if ( bx == ax  && (flag&0x20) ) add++;	// 左 (同じX座標でも少しだけ優遇)
			if ( by >  ay  && (flag&0x40) ) add+=2;	// 上
			if ( by <  ay  && (flag&0x80) ) add+=2;	// 引
			if ( by == ay  && (flag&0x04) ) add+=2;	// 寄
			if ( by >  ay  && (flag&0x08) ) add+=2;	// 行(まっすぐ、だけでなく、前方に進むのもOKらしい）
			if ( bx == ax && by == ay+1 && (flag&0x02) ) add+=2;	// 直
			if ( add > max ) {
				max = add;
				max_i = i;
			}
		}
		bz = bz_stoc[max_i];
	}

	if ( fSenteTurn ) fukasa++;
	flag = able_move(bz,az,tk,nf);
	if ( fSenteTurn ) fukasa--;
	if ( flag==0 ) {
		PRT("ルール違反(%3d手目)\n",tesuu);
		return 0;
	}

	// 実際に手を進める。
	kifu_set_move(bz,az,tk,nf,0);
	if ( tesuu == KIFU_MAX-1 ) return 0;

	*p_next_n = n;
	return 2;	// 1手進んだ
}

// 千日手も棋譜情報に登録する
void shogi::SetSennititeKif()
{
	int i,j;

	// 千日手を登録する。
	for ( i=0;i<all_tesuu;i++) back_move();
	for ( i=0;i<all_tesuu;i++) {
		forth_move();
		set_sennitite_info();

		// 回数チェック
		int sum=0;
		const int ss_max = 40;
		int ss[ss_max];

		for (j=0;j<tesuu;j++) { 	
			HASH *psen = &sennitite[j];
			if ( hash_code1 == psen->hash_code1 && hash_code2 == psen->hash_code2 ) {
				if ( hash_motigoma == psen->motigoma ) ss[sum++] = j;
			}
			if ( sum >= ss_max ) break;
		}
		if ( sum ) {
			PRT("手数=%d,同形 = %d 回 ---> ",tesuu,sum);
			for (j=0;j<sum;j++) PRT("%d手目,",ss[j]);
			PRT("\n");
			if ( sum > nSennititeMax ) nSennititeMax = sum;
		}

	}
	hash_calc(0,1);	// ハッシュ値を作りなおす
}

// 後手から指してるか、対局者名、などを初期化
void InitGameInfo()
{
	fGotekara = 0;
	KomaOti = 0;
	PlayerName[0][0] = 0;
	PlayerName[1][0] = 0;
	Kisen[0] = 0;
	nSennititeMax = 0;
	nKifDateYMD = 0;
}






// 棋譜に従って局面を進め、戻す。---> 思考ルーチンの定跡生成でも呼んでいる。
void shogi::back_move(void)
{
	if ( tesuu == 0 ) return;
	remove_hit(&kifu[tesuu][0]);
	tesuu--;
}
void shogi::forth_move(void)
{
	if ( tesuu == all_tesuu ) return;
	tesuu++;
	move_hit(&kifu[tesuu][0]);
}

// 指定した手数へ飛ぶ
void shogi::jump_move(int new_tesuu)
{
	int n = new_tesuu - tesuu;
	if ( n == 0 ) return;
	int i;
	if ( n > 0 ) {
		for (i=0;i<n;i++) forth_move(); 
	} else {
		n = -n;
		for (i=0;i<n;i++) back_move();
	}
}

void shogi::clear_all_data()
{
#if 0
	memset(this, 0, sizeof(shogi));		// 結構未初期化の変数を参照してるぞ・・・。
#else
	clear_kb_kiki_kn();
#endif
}

// init_ban[] に org_ban[]をコピー
void shogi::copy_org_ban(void)
{
	int i,z;
	
	for ( z=0; z<BAN_SIZE; z++ ) init_ban[z] = org_ban[z];
	for ( i=0; i<8;i++) {
		mo_m[i] = org_mo_m[i];
		mo_c[i] = org_mo_c[i];
	}
}



/************************** ban init ********************************/
int org_ban[BAN_SIZE]= {
 0xff,   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,   0xff,0,0,0,0,0,

 0xff,   0x82,0x83,0x00,0x85,0x00,0x00,0x00,0x00,0x82,   0xff,0,0,0,0,0,
 0xff,   0x00,0x88,0x84,0x00,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x81,0x81,0x00,0x0f,0x00,0x81,0x00,   0xff,0,0,0,0,0,
 0xff,   0x81,0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x03,0x04,0x81,0x00,0x00,0x01,0x81,   0xff,0,0,0,0,0,
 0xff,   0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x86,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x08,0x04,0x00,0x00,0x89,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x02,0x03,0x00,0x05,0x00,0x00,0x00,0x03,0x02,   0xff,0,0,0,0,0,

 0xff,   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,   0xff,0,0,0,0,0};
		     /***     FU KY KE GI KI KA HI    ***/
int org_mo_c[8] = { 0, 4, 0, 0, 0, 1, 1, 1 };
int org_mo_m[8] = { 0, 1, 0, 0, 1, 1, 0, 0 };
/********************************************************************/


int main(int argc, char *argv[])
{
	InitLockYSS();

 	int no_prt = 0;
 	int i;
	for (i=1;i<argc;i++) {
		if ( strcmp( argv[i],"-no_prt")     ==0 ) no_prt = 1;
	}
	PRT("no_prt=%d\n",no_prt);
	if ( no_prt ) PRT_OFF();
	PS->init_file_load_yss_engine();

	start_zero_train(&argc,&argv);

	return 0;
}
