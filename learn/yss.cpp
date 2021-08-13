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
/* YSS 13.84a2014/05 E5-2680 2.8GHz x2, x16��    C++ (Linux) CSA 24th 3/38 */
/* YSS 13.88 2015/05 E5-2680 2.8GHz x2, x16��    C++ (Linux) CSA 25th10/?? */
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

#include "yss.h"		// ���饹�����
#include "yss_dcnn.h"


int shuuban = 0;		// ���פˤʤä����򼨤��ե饰


int senkei;				// �﷿�򼨤�

#ifdef CSA_TOURNAMENT
int kibou_senkei = 0;	// COM�ˤ��������﷿�����̤����ҡ��֣�...�����ࡣ��...�����֡���...�����֡�
#else
int kibou_senkei = 0;	// ���̤ϥ�����
#endif


int read_max_limit = 6;			// ���ο����ޤ�ȿ������ˡ���ɤࡣ

int utikiri_time = 2;		// �׹ͤ��Ǥ��ڤ����»���

int saigomade_depth = 1;		// 0...�ͤߤ�����ޤǻؤ���1...3������٤��ꤲ��(RM=1�μ�Ϻ��Ѥ��ʤ��ˡ�
	int saigomade_flag     = 0;		// �⤦�ȤäƤʤ���AI������Interface�Τ��������¸�ߡ�
	int saigomade_read_max = 2;		// Ʊ������

int total_time[2];		// ��ꡢ�����߷׻׹ͻ��֡�

char PlayerName[2][64];	// ��ꡢ����̾��
char sGameDate[TMP_BUF_LEN];
char kifu_comment[KIFU_MAX][TMP_BUF_LEN];	// ����˻׹ͤΥ����Ȥ�Ф�
char kifu_log[KIFU_MAX][2][TMP_BUF_LEN];	// 1�ꤴ�ȤΥ���Ͽ
char tmp_log[2][TMP_BUF_LEN];				// ������Ū�˵�Ͽ
char kifu_log_top[KIFU_LOG_TOP_MAX][TMP_BUF_LEN];	// �ǽ�Υ����Ȥ�Ͽ
int KomaOti = 0;			// 0��ʿ��
int nSennititeMax = 0;		// Ʊ���ˤʤä������(1��Ʊ������2��3��4��ʤΤ�������)
char Kisen[TMP_BUF_LEN];
//char sKifDate[TMP_BUF_LEN];
int  nKifDateYMD;

int seo_view_off = 0;	// ��Ψ�׻�����;�פʾ�����Ǥ��Ф��ʤ�����1��
int fPrtCut = 0;		// ����Ū��PRT���Ϥ�ߤ��
int fGotekara = 0;		// ��꤫��ؤ��Ƥ�����ʶ���������	



// YSS �ν��������
void shogi::initialize_yss()
{
	copy_org_ban();		// ������̤򥻥å�

	init();           /** koma bangou no shokika **/
					  /** hash_motigoma          **/

	allkaku();        /** kiki wo kaku dake!     **/
	fukasa = 0;
	tesuu = 0;
	hash_ban_init();	// �ϥå���������ơ��֥� hash_ban[32][BAN_SIZE][2] �ν�����������֤����ࡣ

	hash_calc( 0, 0 );	// �ϥå����ͤ�ͣ����ͤ˷��ꤹ�롣����ȿž�ʤ�������ȿž�ʤ���
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
	initialize_yss();		// ���������

//	hash_all_clear();		// ưŪ���ݤʤΤǺǽ�˳���
	check_kn();				// ���̤������������å�
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
			if ( p && sscanf(p+strlen(find),"%d", &sys_memory) == 1 ) sys_memory /= 1024; // KBñ�̤ʤΤ�MB��
		}
		fclose(fp);
	}
	PRT("sys_memory= %d MB\n",sys_memory);
	return sys_memory;
}

const char *get_kdbDirSL() { return ""; }	// NULL�ǤϤʤ���Ĺ����0��ʸ�������Ƭ���ɥ쥹���֤�

int get_learning_dir_number() { PRT("must be programmed!\n"); debug(); return 0; }

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
	nKifBufNum = 0;		// �Хåե�����Ƭ���֤�����
	InitGameInfo();		// ��꤫��ؤ��Ƥ뤫���жɼ�̾���ʤɤ�����
	ClearKifuComment();

	PRT_OFF();

	int fCSA = 0;
	char *p = strrchr(filename,'.');
	if ( p ) {
		if ( strcmp(p+1,"csa")==0 || strcmp(p+1,"CSA")==0 ) fCSA = 1;
	}
	if ( fCSA ) {
		PS->LoadCSA();	// CSA�����δ�����ɤ߹���
	} else {
		PS->LoadKaki();	// ���ڷ���
	}

	PS->SetSennititeKif();	// ����������������Ͽ����


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
		/* ������ �ǥ��쥯�ȥ���κǽ�� .sgf �ե������õ���ޤ���*/
		hFile = _findfirst( ext, &c_file );	// ext = "*.kif";
		if ( hFile == -1L ) {
			PRT( "������ �ǥ��쥯�ȥ�ˤ� %s �ե������¸�ߤ��ޤ���\n",ext );
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
				PRT("fail change dir=%s\n",p_current_dir); debug();
			}
		}

		if ( get_next_file(filename,ext)!=0 ) return 1;	// success
		p_current_dir = NULL;
	}
	return 0;
}

// ��������ɤ߹���
int open_one_file(char *filename)
{
	extern char InputDir[TMP_BUF_LEN];	// ����μ�ư�����ѤΥǥ��쥯�ȥ�ѥ������ϡ�

	PS->hirate_ban_init(0);
	strcpy( InputDir, filename );	// ��ư���ϥե�����̾�򥻥å�
	
	PRT_OFF();
	{ extern int fShinpo; fShinpo = 1; }	// menu����Ͽ���ʤ�
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


// fPassWindows�ե饰��0�ˤ�������������ѿ����ʤ󤫰�̣������������
void PassWindowsFlagOff()
{
#if !defined(AISHOGI) && defined(_MSC_VER)
	fPassWindows = 0;
#endif
}

void PRT_OFF() { fPrtCut = 1; }
void PRT_ON()  { fPrtCut = 0; }

// ������ϡ�SMP�ξ��ϥ�å��򤫤���
// ���ѸĿ��ΰ����򤽤Τޤޡ��̤δؿ����Ϥ����ȤϤǤ��ʤ��Τǡ�ʸ������Ѵ�����
int PRT(const char *fmt, ...)	// printf() �ν�ʸ����� const
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
	len = vsprintf( text, fmt, ap );	// ������������ǽ����ʼ��ˤϹԤ��ʤ���---> vsnprintf()���ѹ� ��LIB���ɲä�����ΤǤ�� 
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
		debug();
	}
	PRT_sub("%s",text);	// PRT(text) ����text="%CHUDAN"��"HUDAN"�ˤʤäƤ��ޤ���
#endif

//	SendMessage(ghWindow, WM_COMMAND, IDM_PRT_SMP, (LPARAM)text );	// ɽ�����Ƥ����롣PostMessage()�Ϥ��ᡣ

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
	strncpy(str, file, TMP_BUF_LEN-1);	// ��ȯ��Dir���Ѥ��������ΤǾä�
	const char *p = strrchr(str, '\\');
	if ( p == NULL ) p = file;
	else p++;
	sprintf(debug_str,"%s %d����\n\n",p,line);
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
	PRT("%s\n",text_out);
	debug();
}



// ʸ�����Ĺ����int�����֤�������Win64�Ǥηٹ��к���
int strlen_int(const char *p)
{
	return (int)strlen(p);
}

// ���ʤ�ƴ���򥻥å�
int shogi::kifu_set_move(TE Te, int t)
{
	return kifu_set_move(Te.bz,Te.az,Te.tk,Te.nf,t);
}
int shogi::kifu_set_move(int bz,int az,int tk,int nf, int t)
{
//	char retp[20];
//	change(bz,az,tk,nf,retp);
//	PRT("%s, bz,az,tk,nf = %x,%x,%x,%x\n",retp,bz,az,tk,nf);
	if ( tesuu >= KIFU_MAX-1 ) { PRT("KIFU_MAX Err\n"); debug(); }
	move_hit(bz,az,tk,nf);
	tesuu++; all_tesuu = tesuu;
	kifu[tesuu][0] = bz;
	kifu[tesuu][1] = az;
	kifu[tesuu][2] = tk;
	kifu[tesuu][3] = nf;
	kifu[tesuu][4] = t;	// �׹ͻ��֡��߷פǤϤʤ�����
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

unsigned long nKifBufSize;	// �Хåե��˥��ɤ���������
unsigned long nKifBufNum;	// �Хåե�����Ƭ���֤򼨤�
char KifBuf[KIF_BUF_MAX];	// �Хåե�
int fSkipLoadKifBuf = 0;

// KIF�ΥХåե��˽񤭽Ф�
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
	nKifBufSize = nKifBufNum;	// �������
	return n;
}

// KIF�ѤΥХåե�����1�Х����ɤ߽Ф���
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

		// MAC�Ǥϲ��Ԥ� \r �Τߡ�Win�Ǥ� \r\n ��2��³����
		if ( mac == 1 && c != '\n' ) {	// Mac�Υե����뤫��
			nKifBufNum--;	// �ݥ��󥿤����᤹��
			str[n-1] = '\n';
//			PRT("mac������\n");
			break;
		}

//		if ( c == '\r' ) c = '\n';	// Mac�к�
		if ( c == '\r' ) { mac = 1; c = ' '; }	// ���ٹ�������������̣�ʤ�
		else mac = 0;

		str.append(1, c);
		n++;
		if ( c == '\r' || c == '\n' ) break;	// ���Ԥ�NULL��
	}
	return str.length();
}

const int TMP_BUF_LEN_AOBA_ZERO = TMP_BUF_LEN * 32;

// ����ɤ߹���
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
	lpLine[n] = 0;	// �Ǹ��NULL�򤤤�뤳�ȡ�
	return n;
}

// ���̤��������
void shogi::save_banmen()
{
	print_kakinoki_banmen(KIF_OUT,PlayerName[0],PlayerName[1]);
}

const int KomaWariNum = 8;
const char *KomaWari[KomaWariNum] = { "ʿ��","�����","�����","�������","�������","�������","ϻ�����","�������" };

void shogi::SaveCSA(char *sTagInfo, char *sKifDate)
{
	int bz,az,nf,tk,i,k,x,y;
	int fKomaochi = 0;
#ifdef CSA_SAVE_KOMAOCHI	// ������δ����CSA����¸������ˡ�ȿž������¸�ʤȤꤢ������
	fKomaochi = 1;
#endif
	const int fShort = 1;
	
	nKifBufNum = 0;

	for ( i=0;i<all_tesuu;i++) back_move();	// ������̤��᤹��
//	sente_time = gote_time = 0;				// ��ꡢ�����߷׻���

	if ( fShort ) {
		KIF_OUT("%s\r\n",sKifDate);
	} else {
		KIF_OUT("' YSS����(CSA����),%s\r\n",getCPUInfo());
		KIF_OUT("' %s\r\n",sTagInfo);
		KIF_OUT("' �ж�����%s\r\n",sKifDate);
			
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
		// �����ȡ�Log�����
		if ( strlen_int(kifu_comment[i]) ) {
			KIF_OUT("'%s\r\n",kifu_comment[i]);	// ������Υ����Ƚ���
		}
		if ( strlen_int(kifu_log[i][0]) ) { KIF_OUT("\'"); KIF_OUT("%s",kifu_log[i][0]+1); }
		if ( strlen_int(kifu_log[i][1]) ) { KIF_OUT("\'"); KIF_OUT("%s",kifu_log[i][1]+1); }

		forth_move();	// �ºݤˣ���ʤ�롣
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

	for ( i=0;i<all_tesuu;i++) back_move();	// ������̤��᤹��

	// �;����Ѥ����̤�𤴤Ȥ˻���
	char *p = strstr(kifu_log_top[1],"�ͼ��= ");
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

	for (i=0;i<all_tesuu;i++) back_move();	// ������̤��᤹��

	KIF_OUT("# YSS����(���ڷ���),%s\r\n",getCPUInfo());
	KIF_OUT("# %s\r\n",sTagInfo);
	KIF_OUT("�ж�����%s\r\n",sKifDate);
	if ( Kisen[0] ) KIF_OUT("���%s\r\n",Kisen);

	if ( is_hirate_ban() == 1 ) {	// ʿ��ξ��
		KIF_OUT("���䡧%s\r\n",KomaWari[KomaOti]);
		KIF_OUT("��ꡧ%s\r\n",PlayerName[0]);
		KIF_OUT("��ꡧ%s\r\n",PlayerName[1]);
	} else {
		save_banmen();
	}
	KIF_OUT("���----�ؼ�---------�������--\r\n");


	sente_time = gote_time = 0;				// ��ꡢ�����߷׻���

	for ( i=1;i<all_tesuu+1;i++) {                /*** sasite wo saishoni modosu ***/
		bz = kifu[i][0];
		az = kifu[i][1];
		tk = kifu[i][2];
		nf = kifu[i][3];

		KIF_OUT("%4d ",i);
		if ( KomaOti != 0 ) {
			hanten_sasite(&bz,&az,&tk,&nf);	// �ؤ�������ȿž
			hanten();		// ����ȿž
			change(bz,az,tk,nf,retp);
			hanten();		// ����ȿž
			hanten_sasite(&bz,&az,&tk,&nf);	// �ؤ�������ȿž
		} else {
			change(bz,az,tk,nf,retp);
		}
		if ( i > 1 && az == kifu[i-1][1] ) {
			strncpy( retp,"Ʊ��",4);	/* Ʊ�������ˤ��� */
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
		if ( strlen_int(kifu_comment[i]) ) KIF_OUT(" #%s\r\n",kifu_comment[i]);	// �����Ȥ����
		else                               KIF_OUT("\r\n");

		for (int j=0;j<2;j++) {
			char *p = kifu_log[i][j];
			if ( *p != '*' && *p != 0 ) *p = '*';	// ���ڷ����Υ����Ȥˡ� 
			KIF_OUT("%s",p);
		}

		forth_move();	// �ºݤˣ���ʤ�롣
	}
	KIF_OUT("%4d ����         (00:00/ 0:00:00)\r\n",i);
//	if ( TMP_BUF_LEN >= MAX_PATH ) { PRT("ʸ������Ĺ\n"); debug(); } 
}

// ����Υ����ȡ����򥯥ꥢ���롣
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
	if ( i==16 ) { PRT("����̥��顼%c%c,",*(p+0),*(p+1)); i = 0; }
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
			
	if ( x == 0 ) {	// ���Ǥ�
		*bz = 0xff;
		int i = get_csa_koma(str+4);
		if ( i==0 || i>=8 ) { PRT("���Ǥ����顼"); return 0;}
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


// CSA�����δ�����ɤ߹���
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
	int fShortCSA = 0;	// ���̤��ɸ�ǻ��ꤹ��;�����

	tesuu = 0;
	hirate_ban_init(KomaOti);		// ���̤ν������ʿ��ξ��֤�

	nLen = 0;
	for ( ;; ) {
		if ( nLen <= 7 ) nLen = ReadOneLine(lpLine);
		else {
			for (i=0;i<nLen;i++) {
				if ( lpLine[i]==',' ) {	// �ޥ�����ơ��ȥ���
					for (j=i;j<nLen+1;j++) lpLine[j-i] = lpLine[j+1];	// ��������
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
			continue;	// �׹ͻ���
		}
		if ( (lpLine[0] == '+' || lpLine[0] == '-') && nLen <= 3 ) {	// ��������
			if ( lpLine[0] == '+' ) {
				PRT("+ �����\n");
			} else {
				PRT("- �����\n"); 
				fGotekara = 1;
			}
			prt_flag = 0;
			ban_saikousei();	// ���̤κƹ�����
			check_kn();			// ���̤ξ��֤����ﲽ�����å�
/*
			if ( fShinpo == 2 ) {	// ��¼�������꽸�ξ��
				ReadOneLine(lpLine); 
				ReadOneLine(lpLine);
				ReadOneLine(lpLine);
				ReadOneLine(lpLine);
				ReadOneLine(lpLine);
				PRT("����=%s",lpLine);
				strcpy(sYoshi660Seikai,lpLine);
				break;
			}
*/
		}

		// csa�����Υ����Ȥ������
		if ( lpLine[0] == '\'' ) {
#ifdef AOBA_ZERO
//			int n = strlen_int(lpLine);
//			for (i=0;i<n;i++) PRT("%c",lpLine[i]);
			if ( tesuu == 0 ) {
				if ( strncmp(lpLine,"'no",3)==0 ) {
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
				for (;;) {
					char c;
					char str[10];
					int n = 0;
					for (;;) {
						if ( n>=10 ) { PRT("Err csa move str >= 10.\n"); debug(); }
						c = *p++;
						str[n++] = c;
						if ( c==',' || c=='\r' || c =='\n' || c==0 ) break;
					}
					str[n-1] = 0;
					if ( count==0 ) {
						if ( strstr(str,"v=") ) {
							count--;
						} else {
							all_visit = atoi(str);
							pz->v_playouts_sum.push_back(all_visit);
						}
					} else {
						if ( (count&1)== 0 ) {
							if ( b0==0 && b1==0 ) debug();
							int v = atoi(str);
							if ( v > 0xffff ) v = 0xffff;
							sum_visit += v;
							unsigned short m = (((unsigned char)b0) << 8) | ((unsigned char)b1); 
							int move_visit = (m << 16) | v;
							pz->vv_move_visit[tesuu].push_back(move_visit); 
							b0 = b1 = 0;
						} else {
							if ( getMoveFromCsaStr(&bz, &az, &tk, &nf, str)==0 ) debug();
							if ( is_pseudo_legalYSS((Move)pack_te(bz,az,tk,nf), (Color)((tesuu&1)==1)) == false ) {
								PRT("move Err %3d:%s\n",tesuu,str); debug();
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
			change_log_pv(kifu_log[tesuu][n]);	// floodgate��

			n++;
			if ( n==2 ) n = 0;
			lpLine[0] = '\'';
#endif
		}

		if ( lpLine[0] == '\'' && prt_flag == 0 ) { nLen = 0; continue; }	// �ǽ�Υ����Ȥ���ɽ�����ʤ�
		if ( strncmp(lpLine,"$START_TIME:",12)==0 ) {
			int n = strlen_int(lpLine);
			lpLine[n-1]=0;
			sprintf(sGameDate,"%s",lpLine+12);
		}
		if ( lpLine[0] == '$' || lpLine[1] == 'V' ) { PRT("%s",lpLine); continue; }	// �ղþ���

		if ( lpLine[0] == 'P' ) {	// ���̿�
/*
P6 *  *  *  *  * +KA * -TO+FU
P7 *  *  *  *  *  *  *  *  * 
'  ������
P+00KE00KE00FU
'  ������
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

			y = lpLine[1];
			if ( y == '+' || y == '-' ) {	// ����
				for (lpCopy=lpLine+2; lpCopy-lpLine<nLen-4; lpCopy+=4) {
					if ( !(*(lpCopy+0) == '0' && *(lpCopy+1) == '0') ) {	// ��ɸ����
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
						if ( i==0 ) { PRT("������̥��顼='%c%c'\n",*(lpCopy+2),*(lpCopy+3)); }
						init_ban[yy*16+(10-xx)] = i + (y=='-')*(0x80);
						continue;
					}
					if ( *(lpCopy+2) == 'A' && *(lpCopy+3) == 'L' ) {	// �Ĥ�λ�������
						for ( i=1; i<8; i++ ) {
							k = 0;
							for (z=0;z<BAN_SIZE;z++) k += ((init_ban[z] & 0x77)==i);	// �׳� 0xff ��ռ����� 
							k += mo_c[i] + mo_m[i];
							if ( i==1 )      k = 18 - k;
							else if ( i>=6 ) k =  2 - k;
							else             k =  4 - k;
							if ( y == '+' ) mo_m[i] = k;	// ���λ���
							if ( y == '-' ) mo_c[i] = k;	// ���λ���
						}
						break;	// P-00AL �λ��Ϥ���ʾ�õ���ʤ�
					}

					int i = get_csa_koma(lpCopy+2);
					if ( i==0 || i>=8 ) { PRT("������̥��顼='%c%c'\n",*(lpCopy+2),*(lpCopy+3)); }
					else if ( y == '+' ) mo_m[i]++; // ���λ���
					else if ( y == '-' ) mo_c[i]++;	// ���λ���
				}
			}
			y = lpLine[1] - '0';
			if ( 1<=y && y<=9 ) {	// 1�Ԥζ�
				lpCopy = lpLine+2;
				for ( x=1;x<10;x++,lpCopy+=3 ) {
					z = y*16+x;
					c = *(lpCopy+0);
					k = 0x00;
					if ( c=='+' ) ;					// ���ζ�	
					else if ( c=='-' ) k = 0x80;	// ���ζ�	
					else {
						init_ban[z] = 0;
						continue;
					}
					i = get_csa_koma(lpCopy+1);
					if ( i==0 ) break;
//					if ( k+i ==0x08 ) i = 0;	// �;�������겦��ä�
					init_ban[z] = k + i;
				}
			}
			continue;	// ���̿�
		}
		if ( lpLine[0] != '+' && lpLine[0] != '-' ) PRT("%s",lpLine);
		if ( lpLine[0] == '\'' ) { nLen = 0; continue; }	// ������
		if ( lpLine[0] == 'V' ) continue;	// �С������
		if ( lpLine[0] == 'N' ) {
			if ( lpLine[1] == '+' ) {
				strncpy(PlayerName[0],&lpLine[2],strlen_int(lpLine)-3);	// ����̾��
				PlayerName[0][strlen_int(lpLine)-3] = 0;	
			}
			if ( lpLine[1] == '-' ) {
				strncpy(PlayerName[1],&lpLine[2],strlen_int(lpLine)-3);	// ����̾��
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
			if ( strstr(lpLine,"TORYO") ) {
				if ( tesuu & 1 ) {
					pz->result = ZD_S_WIN;
				} else {
					pz->result = ZD_G_WIN;
				}
				pz->result_type = RT_TORYO;
			}
			if ( strstr(lpLine,"KACHI") ) {
				if ( tesuu & 1 ) {
					pz->result = ZD_G_WIN;
				} else {
					pz->result = ZD_S_WIN;
				}
				pz->result_type = RT_KACHI;
			}
			// %+ILLEGAL_ACTION ���(����)��ȿ§�԰٤ˤ�ꡢ���(���)�ξ���
			// %-ILLEGAL_ACTION ���(���)��ȿ§�԰٤ˤ�ꡢ���(����)�ξ���
			if ( strstr(lpLine,"+ILLEGAL_ACTION") ) {	// 2020/03/27 Ϣ³����θ塢��¦��ƨ�������������Τ�
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
			PRT("moves=%d,result=%d, mv_sum=%d,%.1f\n",pz->moves,pz->result,sum, (double)sum/(tesuu+0.00001f));
#endif
			break;		// �ɤ߹��߽�λ
		}
		if ( nLen < 4 ) continue;			// ����
	
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
	PRT("�ɤ߹�������=%d (CSA����)\n",tesuu);
	if ( prt_flag == 1 ) {	// ��礬���äƤ��ʤ���硢�����ǽ����
		ban_saikousei();	// ���̤κƹ�����
		check_kn();			// ���̤ξ��֤����ﲽ�����å�
	}
#ifdef AOBA_ZERO
	if ( zdb_one.moves < 0 ) {
		PRT("file read err\n");
		return 0;
	}
#endif
	return 1;
}

// Shotest�����δ�����ɤ߹���
void shogi::LoadShotest(void)
{
	char lpLine[TMP_BUF_LEN];
	int i,x,y,t, nLen;
	int bz,az,tk,nf;
	char c;

	tesuu = 0;
	hirate_ban_init(KomaOti);		// ���̤ν������ʿ��ξ��֤�

	nLen = 0;
	for ( ;; ) {
		nLen = ReadOneLine(lpLine);
		if ( nLen == 0 ) break; // EOF
		lpLine[nLen] = 0;
//		PRT("%s, nLen=%d\n",lpLine,nLen);
		c = lpLine[0];
		if ( c == ';' || c == ':' || c == '=' || c == '*' ) continue;

/*
��Ƭ ';' �ϥ�����
��Ƭ ':' ������
��Ƭ '=' �ϻ�����
��Ƭ '*' �ϡ�

116. S 8fx7g+  399  14  98848 5  54  4 15  1 0 5  56  5
117. G 7hx7g     0   2      - -   -  -  -  - - -   -  -
118. S  * 3h   564   1  76103 5 100  9 15  1 0 5  17  9
*/				
		if ( nLen < 13 ) continue;			// �ؤ���ǤϤʤ��͡�

		tk = 0;
		nf = 0;
		x = lpLine[10] - '0';
		y = lpLine[11] - 'a' + 1;
		az = y*16 + 10 - x;	// az ������ɤ�Τ�tk�μ�ä�����Τꤿ������

		x = lpLine[7] - '0';
		y = lpLine[8] - 'a' + 1;
		if ( lpLine[7] == ' ' ) {	// ���Ǥ�
			bz = 0xff;
			c = lpLine[5];
			// P L N S G B R
			{
				static char drop_shotest[] = "PLNSGBR";	// 
				char *pdest;

				pdest = strchr( drop_shotest, c );
				if ( pdest == NULL ) { PRT("������ȯ���ߥ� shotest\n"); debug(); }
				i = pdest - drop_shotest + 1;
			}
			tk = i + (0x80)*(tesuu&1);
		} else {					// ���ư
			tk = init_ban[az];
			bz = y*16 + 10 - x;
			if ( lpLine[12] == '+' && (init_ban[bz] & 0x0f) < 0x08 ) nf = 0x08;
		}
		// �׹ͻ��֤���Ф�
		lpLine[0] = lpLine[19];
		lpLine[1] = lpLine[20];
		lpLine[2] = lpLine[21];
		lpLine[3] = 0;
		t = atoi(lpLine);

		kifu_set_move(bz,az,tk,nf,t);
		if ( tesuu == KIFU_MAX-1 ) break;
	}
	PRT("�ɤ߹�������=%d (Shotest����)\n",tesuu);
}

// output���Ϥ����̤��ɤ߹���
void shogi::LoadOutBan(void)
{
	char str[TMP_BUF_LEN],*p,c,cs[2];
	int k,x,y,z,i,nLen;
	
	hirate_ban_init(KomaOti);		// ���̤ν������ʿ��ξ��֤�

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
	ban_saikousei();	// ���̤κƹ�����
	check_kn();			// ���̤ξ��֤����ﲽ�����å�
}

void shogi::LoadTextBan()
{
	char str[TMP_BUF_LEN],*p;
	int k,x,y,z,i,nLen;
	
	hirate_ban_init(KomaOti);		// ���̤ν������ʿ��ξ��֤�

	nLen = 0;
	y = 1;
	for ( ;; ) {
		nLen = ReadOneLine(str);
		if ( nLen == 0 ) break; // EOF
		str[nLen] = 0;
//		PRT("%s, nLen=%d\n",str,nLen);
		char find[TMP_BUF_LEN];
		sprintf(find,"%d��",y);
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

		if ( y==1 ) {	// COM :� 0:�� 0:�� 0:�� 0:�� 0:�� 0:�� 0:
			for (i=1;i<8;i++) {
				sprintf(find,"%s",koma_kanji[i+16]);
				char *q = strstr(p+2*9,find);
				if ( q==NULL) { PRT("not found ��\n"); debug(); }
				mo_c[i] = *(q+3) - '0';
			}
		}
   		y++;
		if ( y==10 ) break;
	}

	// ;�ä����MAN�λ������
	int ksum[8][2] = { {0,2}, {0,18}, {0,4}, {0,4}, {0,4}, {0,4}, {0,2}, {0,2} };
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int z = (y+1)*16 + (x+1);
		k = init_ban[z];
		ksum[k&0x07][0]++;
	}
	for (i=1;i<8;i++)  ksum[i][0] += mo_c[i]; 
	for (i=1;i<8;i++) mo_m[i] = ksum[i][1] - ksum[i][0]; 

	ban_saikousei();	// ���̤κƹ�����
	check_kn();			// ���̤ξ��֤����ﲽ�����å�

	hanten_with_hash();
#if defined(_MSC_VER)
	save_clipboard(1);	// ����åץܡ��ɤ˴���򥳥ԡ�
#endif
}


const char *sSengoSankaku[3] = {"��","��","��"};

int shogi::IsSenteTurn() { return ( ((fGotekara+tesuu)&1) == 0 ); }	// ��������������ʤ�
int shogi::GetSennititeMax() { return nSennititeMax; }

// ���ڷ����δ�����ɤ߹���
void shogi::LoadKaki()
{
	char lpLine[TMP_BUF_LEN];
	int i,j,k,x,y,z,t, nLen;
	int bz,az,tk,nf;
	const char *lpCopy,*lpKanji;
	char c;
	int sente_motigoma_flag = 0;	// ���λ���򼨤�
	int fKI2 = 0;	// KI2�����ξ��
//	int fKomaoti = 0;				// ������δ���ξ��1
	string sOneLine;
	int top_comment_n = 0;

	// ��Ԥ���ɤ���
	// "��" �����Ĥ���ޤǥ���
/*
# �ե�����̾��A:\Program Files\KShogi95\�;�����\1-2.kif
�ж�����1997/07/30(��) 13:47:41
���䡧ʿ�ꡡ��
���λ������󡡳ѡ����󡡶�͡��˻�����͡��⽽�ޡ�
  �� �� �� �� �� �� �� �� ��
+---------------------------+
| �� �� �� �� �� �� �� �� ��|��
| �� �� �� �� �� �� �� �� ��|��
| �� �� �� �� �� �� �� ��v��|��
| �� �� �� �� �� �� �� �� ��|��
| �� �� �� �� �� ��v�� �� ��|��
| �� �� �� �� �� �� �� ��v��|ϻ
| �� �� �� �� �� �� �� ��v��|��
| �� �� �� �� �� �� �� �� ��|Ȭ
| �� �� �� �� �� �� �� �� ��|��
+---------------------------+
���λ��𡧤ʤ�
��ꡧ
��ꡧ
���----�ؼ�---------�������--
*/

	y = 0;	// �����ɤ߹�����
	hirate_ban_init(0);		// ���̤ν������ʿ��ξ��֤�
//	for (i=0;i<8;i++) mo_c[i] = mo_m[i] = 0;
	
	for ( ;; ) {
		nLen = ReadOneLine(lpLine);
		if ( nLen == 0 ) {
			PRT("EOF - �ɤ߹��߼��ԡ��ּ���פ����Ĥ���ʤ�\n");
			break;
		}
		lpLine[nLen] = 0;
		PRT("%s",lpLine);
	 	// �����Ρּ�פ�õ��
		if ( strstr(lpLine,"���----") != NULL ) break;
//		if ( (unsigned char)lpLine[0] == 0x8e && (unsigned char)lpLine[1] == 0xe8 &&
//		     (unsigned char)lpLine[2] == 0x90 && (unsigned char)lpLine[3] == 0x94 ) break;

		// �Ԥ���Ƭ�������������ʤ�ki2������
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

		// ���̥ǡ������ɤ߹��ࡣ
		if ( (unsigned char)lpLine[0] == '|' || (unsigned char)lpLine[1] == '|' ) {
			x = 1;
			y++;
//			PRT("y=%d",y);
			if ( y >= 10 ) {PRT("���̥��顼���פ��礭�����ޤ���");break;}
			lpCopy = lpLine + 1;
			if ( (unsigned char)lpLine[1] == '|' ) lpCopy++;	// �����ͷ�����
			for ( i=1;; ) {
				c = *lpCopy;
				k = 0xff;
				if ( c == ' ' || c == '^' || c== '+' ) k = 0x00;	// ���ζ�
				if ( c == 'V' || c == 'v' || c== '-' ) k = 0x80;	// ���ζ�
				if (  k == 0xff ) {PRT("���̥��顼");break;}
	
				lpCopy++;i++;
				if ( i >= nLen ) break;		// �ɤ߹����ʸ������Ķ������

				for ( j=1;j<16;j++) {
					if ( *(lpCopy) == *(koma_kanji[j]) && *(lpCopy+1) == *(koma_kanji[j]+1) ) break;
				}
				if ( j==16 ) {	// "��","ε" �Υ����å�	
					lpKanji = "��";
					if ( *(lpCopy) == *(lpKanji) && *(lpCopy+1) == *(lpKanji+1) ) j = 8;
					lpKanji = "ε";
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
				if ( i >= nLen ) break;		// �ɤ߹����ʸ������Ķ������
			}
		}

		// ��������ɤ߹���
		// ����Ȼ���δ֤����Ѥζ���1ʸ�����ޤ���Ⱦ�Ѥ�2ʸ���Ƕ��ڤ��Ĥ��롣����ʳ����Բġ�
		sente_motigoma_flag = 0;	//����ǡ����ǤϤʤ���

		lpKanji = "���λ���";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 1;	//���λ���
		lpKanji = "����λ���";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 1;	//���λ���
		lpKanji = "����";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 3;	//���λ���Ǹ�������
		lpKanji = "���λ���";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 2;	//���λ���
		lpKanji = "���λ���";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) sente_motigoma_flag = 2;	//���λ���

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
				if ( i >= nLen ) break;		// �ɤ߹����ʸ������Ķ������

				if ( j == 8 ) continue;
				k = j;
				
				// ������ɤ߹���

				for ( j=1; j<11; j++ ) {
					if ( strncmp(lpCopy,num_kanji[j],2)==0 || strncmp(lpCopy,num_zenkaku[j],2)==0) break;
				}

				if ( j==11 ) {
					j = 1;	// ����Ϻ���Ǥ�1��
					if ( !( strncmp(lpCopy,"��",2)==0 || strncmp(lpCopy,"  ",2)==0 ) ) {	// ���Ѥ�Ⱦ�Ѥζ���
//						PRT("����ʸ��������ǤϤʤ�"); 
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
				if ( i >= nLen ) break;		// �ɤ߹����ʸ������Ķ������
			}
		}
		if ( sente_motigoma_flag == 3 ) {	// ���λ���Ǹ�������
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

		// �жɼ�̾���ɤ߹���
		const char *pSGName[4] = { "��ꡧ","���ꡧ","��ꡧ","��ꡧ" };
		for (i=0;i<4;i++) {
			lpKanji = pSGName[i];
			j = i/2;
			if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
				strncpy(PlayerName[j],&lpLine[6],strlen_int(lpLine)-7);	// ����̾��
				PlayerName[j][strlen_int(lpLine)-7] = 0;	
			}
		}

		// ������δ���Ǥ϶��Ǥ������ȿž�����롣
		// ���䡧ʿ�ꡡ�����䡧����������䡧������� ���䡧������� ���䡧�������
		lpKanji = "���䡧";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
			lpKanji = "��";
			if ( strstr( lpLine, lpKanji ) != NULL ) fGotekara = 1;
			// 0...ʿ�ꡢ1...���2...���3...���4...���硢5...���硢6...����
			for (i=KomaWariNum-1;i>=1;i--) {
				if ( strstr( lpLine, KomaWari[i] ) != NULL ) { KomaOti = i; break; }
			}
			if ( KomaOti ) {
				hirate_ban_init(KomaOti);		// ���̤ν������ʿ��ξ��֤�
				hanten_with_hash();				// ��������ȿž
			}
//			PRT("%s,%s\n",lpLine,lpKanji);	PRT("����� %d\n",KomaOti);
		}
		lpKanji = "�����";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) fGotekara = 1;

		lpKanji = "���";
		if ( strncmp( lpKanji, lpLine, strlen_int(lpKanji) ) ==  0 ) {
			strncpy(Kisen,&lpLine[6],strlen_int(lpLine)-7);
			Kisen[strlen_int(lpLine)-7] = 0;	
		}
		const char *pSGDate[2] = { "�ж�����","����������" };
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

	ban_saikousei();	// ���̤κƹ�����
	check_kn();			// ���̤ξ��֤����ﲽ�����å�

	if ( nLen == 0 ) return;
	
	tesuu = 0;
	int comment_n = 0;
	if ( fKI2 ) {
		int n = 0;
		for (;;) {
			// ����2�����δ�����ɤ߹��ࡣ0...�ɤ߹��ߥ��顼��1...1�Ԥ�����ä���2...1��ʤ����3...����ʸ�����ɤ�٤���
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
			if ( strstr(lpLine,"#--separator--" ) ) break;	// ʬ�������̵��
			// ���ڷ����Υ����Ȥ�2�Ԥ���������
			if ( lpLine[0] == '*') {
				strncpy(kifu_log[tesuu][comment_n],lpLine,TMP_BUF_LEN-1);
				comment_n++;
				if ( comment_n==2 ) comment_n = 0;
			}
			continue;	// ������
		}
//		PRT("%s",lpLine);


		if ( nLen < 11 ) continue;
//		sscanf(lpLine,"%d",&n);
//		PRT("n = %d\n",n);
		// 24�δ����Ѥˡ��ǽ餬�������ä���ǽ��Ⱦ�Ѷ���μ�����Ĵ�٤롣	
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
			const unsigned char sFind[] = "Ʊ";
			for ( i=0; i<10; i++ ) {	// ��Ʊ�פ�õ��
				if ( (unsigned char)*(lpCopy+i) == sFind[0] && (unsigned char)*(lpCopy+i+1) == sFind[1] ) break;
//				if ( (unsigned char)*(lpCopy+i) == 0x93 && (unsigned char)*(lpCopy+i+1) == 0xaf ) break;
			}
			if ( i == 10 ) {break;}
			az = kifu[tesuu][1];
		} else az = y*16 + 10 - x;
		nf = 0;
		tk = 0;
		const unsigned char sFind[] = "��";
		for ( i=6; i<10; i++ ) {	// �����פ�õ��
			if ( (unsigned char)*(lpCopy+i) == sFind[0] && (unsigned char)*(lpCopy+i+1) == sFind[1] ) nf = 0x08;
//			if ( (unsigned char)*(lpCopy+i) == 0x90 && (unsigned char)*(lpCopy+i+1) == 0xac ) nf = 0x08;
		}
		x = y = 0;
		for ( i=6; i<10; i++ ) {	// ��(�פ�õ��
			if ( *(lpCopy + i) == '(' ) {
				x = *(lpCopy+i+1) - '0';
				y = *(lpCopy+i+2) - '0';
				break;
			}
		}
		if ( x == 0 ) {	// ���Ǥ�
			bz = 0xff;
			for ( i=1; i<8; i++ ) {
				if ( *(lpCopy+4) == *(koma_kanji[i]) && *(lpCopy+5) == *(koma_kanji[i]+1) ) break;
			}
			if ( i==8 ) {PRT("���Ǥ����顼");break;}
			tk = i + (0x80)*((tesuu+fGotekara)&1);
		} else {
			bz = y*16 + 10 - x;
			tk = init_ban[az];
		}
		// �Τ�YSS��ȿž������ɤ߹����硣
//		hanten_sasite(&bz, &az, &tk, &nf);	if ( bz != 0xff ) tk = init_ban[az]; else tk = (tk & 0x07) + (0x80)*((tesuu+0)&1);

		// ������֤��ɤ߼��
		t = 0;
		for ( i=11; i<16; i++ ) {	// ��(�פ�õ��
			if ( *(lpCopy + i) == '(' ) {
				lpCopy = lpCopy +i+1;
				// ':' ������ޤǤ�ʬ
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
				// '/' ������ޤǤ���
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

//		if ( t >= 30 ) { if ( strstr(PlayerName[(tesuu) & 1],"YSS") ) {PRT("%3d��,t=%d over!!!\n",tesuu+1,t); debug();} }	// �������Ѥλ��֥����å�

		if ( is_pseudo_legalYSS( (Move)pack_te(bz,az,tk,nf), (Color)(tesuu&1) )==0 ) {
			PRT("���顼�μ�\n");
			break;
		}

		kifu_set_move(bz,az,tk,nf,t);
		if ( tesuu == KIFU_MAX-1 ) break;
	}
	if ( tesuu ) PRT("�ɤ߹�������=%d\n",tesuu);
}

// ����2�����δ�����ɤ߹��ࡣ0...�ɤ߹��ߥ��顼��1...1�Ԥ�����ä���2...1��ʤ����3...����ʸ�����ɤ�٤���
int shogi::ReadOneMoveKi2(char *top_p, int *p_next_n)
{
	int i,n,nLen,nLen2,k,x,y,bz,az,tk,nf,flag,sum,loop,k_n,kk;
	int *pkb,*pmo;
	int bz_stoc[KB_MAX];
	char *p;

	const char *koma_2moji[16] = {
	  "��","��",  "��",  "��",  "��","��","��","��",
	  "��","��","����","����","����","��","��","ζ"
	};

	const char *koma_dir[10] = {
	//					 �����ʹߤ��Ȥ߹�碌�������롣
	// 	0x01 0x02 0x04 0x08 0x10 0x20 0x40 0x80 0x100
		"��","ľ","��","��","��","��","��","��","��","����"
	};

	nLen = strlen_int(top_p);
	n = *p_next_n;
	if ( n >= nLen ) return 1;	// ���ιԤ�
	p = top_p + n;
	 
	if ( n == 0 ) {	// �Ԥ���Ƭ����Ĵ�٤����
		if ( *top_p == '*' || *top_p == '#' ) {	// ������
			PRT("%s",top_p);
			return 1;
		}
//		// �ǽ����������õ����
//		for (i=0;i<3;i++) if ( strncmp(p,sSengoSankaku[i],2)==0 ) break;
	}

	// �ǽ����������õ����
	for (i=0;i<3;i++) if ( strncmp(p,sSengoSankaku[i],2)==0 ) break;
	if ( i==3 ) {
		n++;
		*p_next_n = n;
		return 3;	// ����ʸ����Ĵ�٤롣
	}

	int fSenteTurn = 0;
	if ( i==0 ) fSenteTurn = 1;	

	// ��ɸ���ʲ����ɤ߼�롣
	n += 2;	p = top_p + n;
	if ( n+2 >= nLen ) return 1;	// ���ιԤ�

	for (x=1;x<10;x++) if ( strncmp(p  ,num_zenkaku[x], 2)==0 ) break;
	for (y=1;y<10;y++) if ( strncmp(p+2,num_kanji[y],2)==0 ) break;
	if ( x==10 || y==10 ) {
		if ( strncmp(p,"Ʊ",2)!=0 ) { PRT("��ɸ�ɤ߼��Err x=%d,y=%d,n=%d\n",x,y,n); return 0; }
		if ( strncmp(p+2,"��",2)!=0 && strncmp(p+2,"  ",2)!=0 ) n -= 2;	// Ʊ�ᡢ�ʤɤȶ��򤬤ʤ���硣
		az = kifu[tesuu][1];
	} else {
		az = y*16 + 10 - x;
	}
	// ư��������ɤ߼�롣
	n += 4;	p = top_p + n;
	if ( n >= nLen ) return 1;	// ���ιԤ�

	for (k=0;k<16;k++) {
		const char *p2 = koma_2moji[k];
		nLen2 = strlen_int(p2);
		if ( strncmp(p,p2,nLen2)==0 ) break;
	}
	if ( k==16 ) { PRT("���������\n"); return 0; }
	if ( k==0 ) k = 8;	// ���϶̤ˡ�
	// ���������塢�󡢰���ľ���ԡ��ǡ������������ɤ߼�롣�������䱦������
	// �ǡ���ɬ���ǽ����롣���������ϺǸ���դ���
	// 
	// �ɤ߼��ʤ�ʸ������뤫��1�Ԥ�����뤫�����΢������ޤ��ɤ߹��ࡣ
	// ����������롢����ľ���ԡ��ǡ���ɬ��
	n += nLen2;	p = top_p + n;
	flag = 0;
	for (;;) {
		if ( n >= nLen ) break;	// �Ԥν����

		for (i=0;i<10;i++) {	// �ֱ��פʤɤ�õ��
			const char *p2 = koma_dir[i];
			nLen2 = strlen_int(p2);
			if ( strncmp(p,p2,nLen2)==0 ) break;
		}
		if ( i==10 ) break;
		flag |= 1 << i;	// bit����¸��

		n += nLen2;	p = top_p + n;
	}
	// �����ξ����ɤ߼�줿�������������ꤹ�롣
//	PRT("%3d:x=%2d,y=%2d,az=%02x,k=%2d,flag=%4x,n=%d\n",tesuu,x,y,az,k,flag,n);
	if ( flag & 0x100 ) nf = 0x08;
	else                nf = 0x00;
	tk = init_ban[az];
	bz = 0;
	// ���Υޥ��ܤ˰�ư��ǽ�ʶ��Ĵ�٤롣
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
		if ( k_n > 0x80 ) continue;	// �Ƥ�����
		kk = kn[k_n][0];
		bz = kn[k_n][1];
		if ( (kk & 0x0f) != k ) continue;	// �㤦�������
		bz_stoc[sum++] = bz;
	}
	if ( sum == 0 || (flag & 0x01) ) {	// ��ư��ǽ�꤬�ʤ����ޤ��ϡ��ǡפ����ꡣ����ǤĤ����ʤ��Ϥ���
		if ( *(pmo+k) == 0 ) { PRT("�ǤĶ𤬤ʤ�,bz=%02x,az=%02x,tk=%02x,flag=%d\n",bz,az,tk,flag); return 0; }
		bz = 0xff;
		tk = k;
		if ( fSenteTurn==0 ) tk += 0x80;
	} else if ( sum == 1 ) {	// ͣ��μ�
		bz = bz_stoc[0];
	} else {		// ʣ����ǽ�꤬���롣��ä�����
//		PRT("ʣ���β�ǽ�ꡪ\n");	
		// 	0x01 0x02 0x04 0x08 0x10 0x20 0x40 0x80 0x100
		//	"��","ľ","��","��","��","��","��","��","��","����"
		// �������ƤϤޤ���˥ܡ��ʥ��򡣰����������⤤�������
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
			if ( bx >  ax  && (flag&0x10) ) add+=2;	// ��
			if ( bx == ax  && (flag&0x10) ) add++;	// ��
			if ( bx <  ax  && (flag&0x20) ) add+=2;	// �� (��71�Ϣ�61�ϤΤȤ���72�Ϥϡ֢�72�Ϻ���)
			if ( bx == ax  && (flag&0x20) ) add++;	// �� (Ʊ��X��ɸ�Ǥ⾯������ͥ��)
			if ( by >  ay  && (flag&0x40) ) add+=2;	// ��
			if ( by <  ay  && (flag&0x80) ) add+=2;	// ��
			if ( by == ay  && (flag&0x04) ) add+=2;	// ��
			if ( by >  ay  && (flag&0x08) ) add+=2;	// ��(�ޤä����������Ǥʤ��������˿ʤ�Τ�OK�餷����
			if ( bx == ax && by == ay+1 && (flag&0x02) ) add+=2;	// ľ
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
		PRT("�롼���ȿ(%3d����)\n",tesuu);
		return 0;
	}

	// �ºݤ˼��ʤ�롣
	kifu_set_move(bz,az,tk,nf,0);
	if ( tesuu == KIFU_MAX-1 ) return 0;

	*p_next_n = n;
	return 2;	// 1��ʤ��
}

// ����������������Ͽ����
void shogi::SetSennititeKif()
{
	int i,j;

	// ���������Ͽ���롣
	for ( i=0;i<all_tesuu;i++) back_move();
	for ( i=0;i<all_tesuu;i++) {
		forth_move();
		set_sennitite_info();

		// ��������å�
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
			PRT("���=%d,Ʊ�� = %d �� ---> ",tesuu,sum);
			for (j=0;j<sum;j++) PRT("%d����,",ss[j]);
			PRT("\n");
			if ( sum > nSennititeMax ) nSennititeMax = sum;
		}

	}
	hash_calc(0,1);	// �ϥå����ͤ���ʤ���
}

// ��꤫��ؤ��Ƥ뤫���жɼ�̾���ʤɤ�����
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






// ����˽��äƶ��̤�ʤᡢ�᤹��---> �׹ͥ롼��������������Ǥ�Ƥ�Ǥ��롣
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

// ���ꤷ�����������
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
	memset(this, 0, sizeof(shogi));		// �빽̤��������ѿ��򻲾Ȥ��Ƥ뤾��������
#else
	clear_kb_kiki_kn();
#endif
}

// init_ban[] �� org_ban[]�򥳥ԡ�
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
