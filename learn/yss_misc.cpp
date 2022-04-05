// 2019 Team AobaZero
// This source code is in the public domain.
// yss_misc.cpp
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "yss_ext.h"     /**  load extern **/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss.h"		// クラスの定義

HASH sennitite[KIFU_MAX];		/* ハッシュで以前の局面を記憶 */

#ifdef SMP
shogi          local[MAX_BLOCKS+1];	// 探索で使うクラス本体。
#else
shogi          local[1];			// 探索で使うクラス本体。
#endif

shogi *PS = &local[0];	// 大文字のPSはグローバルで使う。



void shogi::check_kn(void)
{
	int i,k,z;
	int *p;
	int ks[8];

	for ( i=1;i<8;i++) ks[i]=0;

	p = kn[1];
	for (i=1;i<41;i++) {
		k = *p++;
		z = *p++;
		if ( k==0 ) continue;
		if ( i==2 && z == 0x5d ) continue;	// 詰将棋
		ks[ k & 0x07 ]++;
		if ( ban[z] != i || init_ban[z] != k ) {
			print_kn();
			hyouji();
			PRT("check_kn(%x) Error !!! tesuu=%d fukasa=%d\n",i,tesuu,fukasa);
			PRT("ban[%x]=%02x, init_b[%x]=%x\n",z,ban[z],z,init_ban[z]);
			print_kb();
			DEBUG_PRT("");
		}
	}
	for (i=1;i<8;i++) {
		k = mo_c[i]+mo_m[i]+ks[i];
		if ( mo_c[i]<0 || mo_m[i]<0 || k < 0 || (i==1 && k>18) ||
		 ( (i==2||i==3||i==4||i==5) && k>4 ) || ( (i==6||i==7) && k>2 ) ) {
			PRT("mo_ Error !\n");
			DEBUG_PRT("");
		}
	}
}

void shogi::clear_kb_kiki_kn()
{
#if 1
	// 若干速い
	memset(kb_c,0,sizeof(kb_c));
	memset(kb_m,0,sizeof(kb_m));
	memset(kiki_c,0,sizeof(kiki_c));
	memset(kiki_m,0,sizeof(kiki_m));
	memset(kn,0,sizeof(kn));
	return;
#else
	int i,z,*p;
//	p = kb_m[0];					/*** kb_m[256][8]  no clear ***/
	for (z=0;z<BAN_SIZE;z++) {
		for (i=0;i<8;i++) {
			kb_c[z][i] = 0;
			kb_m[z][i] = 0;
		}
	}
	for (i=0;i<BAN_SIZE;i++) {           /*** kiki_m[256] ***/
		kiki_m[i] = 0;
		kiki_c[i] = 0;
	}
	p = kn[0];            			/*** kn[41][2] ***/
	for (i=0;i<82;i++) *p++ = 0;
#endif
}
void shogi::clear_mo()
{
	int i;
	for (i=1;i<8;i++) {            				/*** motigoma ***/
		mo_c[i] = 0;
		mo_m[i] = 0;
	}
}
void shogi::clear_init_ban()
{
	int i;
	for (i=0;i<BAN_SIZE;i++) {
		if ( init_ban[i] != 0xff ) init_ban[i] = 0;
	}
}


/***************** hanten ( kiki no irekae ) ******************/
void shogi::hanten(void)
{
	int i,x,y,xe,zc,zm,kc,km,dummy;

	clear_kb_kiki_kn();

	for (y=1;y<6;y++) {							/** init_ban[] no hanten **/
		if ( y==5 ) xe=6;
		else xe=10;
		for (x=1;x<xe;x++) {
			zc = (     y * 0x10 ) +     x;
			zm = ( (10-y)* 0x10 ) + (10-x);
			kc = init_ban[zc];
			km = init_ban[zm];

			if ( kc == 0 ) kc = 0;
			else if ( kc > 0x80 ) kc -= 0x80;
			else kc += 0x80;

			if ( km == 0 ) km = 0;
			else if ( km > 0x80 ) km -= 0x80;
			else km += 0x80;

			init_ban[zc] = km;
			init_ban[zm] = kc;
		}
	}
	for (i=1;i<8;i++) {            				/*** motigoma ***/
		dummy = mo_c[i];
		mo_c[i] = mo_m[i];
		mo_m[i] = dummy;
	}
	init();			/** kn[] ni kakikomu **/
	allkaku();
}

// 完全盤面反転。局面を反転させてハッシュも作り直す。
void shogi::hanten_with_hash(void)
{
	hanten();
	hash_init();
}

// 左右を反転させる。意外と盲点が分かって面白いかも。
void shogi::hanten_sayuu(void)
{
	int x,y,zc,zm,kc,km;

	clear_kb_kiki_kn();

	for (y=1;y<10;y++) {
		for (x=1;x<5;x++) {
			zc = y * 0x10 +     x;
			zm = y * 0x10 + (10-x);
			kc = init_ban[zc];
			km = init_ban[zm];
			init_ban[zc] = km;
			init_ban[zm] = kc;
		}
	}
	init();			/** kn[] ni kakikomu **/
	allkaku();
}

static int hirate_ban[BAN_SIZE]= {
 0xff,   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,   0xff,0,0,0,0,0,

 0xff,   0x82,0x83,0x84,0x85,0x88,0x85,0x84,0x83,0x82,   0xff,0,0,0,0,0,
 0xff,   0x00,0x87,0x00,0x00,0x00,0x00,0x00,0x86,0x00,   0xff,0,0,0,0,0,
 0xff,   0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,   0xff,0,0,0,0,0,
 0xff,   0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,   0xff,0,0,0,0,0,
 0xff,   0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x07,0x00,   0xff,0,0,0,0,0,
 0xff,   0x02,0x03,0x04,0x05,0x08,0x05,0x04,0x03,0x02,   0xff,0,0,0,0,0,

 0xff,   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,   0xff,0,0,0,0,0};

/*** 盤面を初期状態に戻す（駒落ち付き） ***/
/*** 後手が（COM側が）落とす            ***/
/*** 0...平手、1...香落、2...角落、3...飛落、4...２枚、5...４枚、6...６枚 ***/
void shogi::hirate_ban_init(int n)
{
	int x,y,z;

	clear_kb_kiki_kn();
	clear_mo();

	for (y=1;y<10;y++) {				/** init_ban[] no hanten **/
		for (x=1;x<10;x++) {
			z = y*16+x;
			init_ban[z] = hirate_ban[z];
		}
	}
/*
//	PRT("n=%d\n",n);
	if ( n == 7           )   init_ban[0x99] = 0;						// 右香を消す
	if ( n == 6           ) { init_ban[0x92] = 0; init_ban[0x98] = 0; }	// 桂馬を消す
	if ( n >= 5 && n <= 6 ) { init_ban[0x91] = 0; init_ban[0x99] = 0; }	// 香車を消す
	if ( n >= 4 && n <= 6 ) { init_ban[0x82] = 0; init_ban[0x88] = 0; }	// 飛角を消す
	if ( n == 3           )   init_ban[0x88] = 0;						// 飛車を消す
	if ( n == 2           )   init_ban[0x82] = 0;						// 角　を消す
	if ( n == 1           )   init_ban[0x91] = 0;						// 左香を消す
*/
	if ( n == 6           ) { init_ban[0x12] = 0; init_ban[0x18] = 0; }	// 桂馬を消す
	if ( n >= 5 && n <= 6 ) { init_ban[0x11] = 0; init_ban[0x19] = 0; }	// 香車を消す
	if ( n >= 4 && n <= 6 ) { init_ban[0x22] = 0; init_ban[0x28] = 0; }	// 飛角を消す
	if ( n == 3           )   init_ban[0x22] = 0;						// 飛車を消す
	if ( n == 2           )   init_ban[0x28] = 0;						// 角　を消す
	if ( n == 1           )   init_ban[0x19] = 0;						// 左香を消す

	init();			/** kn[] ni kakikomu **/
	allkaku();
	tesuu = all_tesuu = 0;
}

int shogi::get_handicap_from_board()
{
	int m[256];
	int sum = 0, mo_sum = 0;
	for (int i=1;i<8;i++) {
		mo_sum += mo_m[i] + mo_c[i];
	}
	if ( mo_sum ) return 0;
	int x,y;
	for (y=1;y<10;y++) for (x=1;x<10;x++) {
		int z = y*16+x;
		if ( init_ban[z] == hirate_ban[z] ) continue;
		m[sum++] = z;
	}
	if ( sum==1 && m[0]==0x19 ) return 1;	// ky
	if ( sum==1 && m[0]==0x28 ) return 2;	// ka
	if ( sum==1 && m[0]==0x22 ) return 3;	// hi
	if ( sum==2 && m[0]==0x22 && m[1]==0x28 ) return 4;	// 2mai
	if ( sum==4 && m[0]==0x11 && m[1]==0x19 && m[2]==0x22 && m[3]==0x28 ) return 5;	// 4mai
	if ( sum==6 && m[0]==0x11 && m[1]==0x12 && m[2]==0x18 && m[3]==0x19 && m[4]==0x22 && m[5]==0x28 ) return 6;	// 6mai
	return 0;
}

// 現在の持ち駒、盤面の状態(ban_init)を元に盤面状態を再構成する。
void shogi::ban_saikousei()
{
	clear_kb_kiki_kn();

	init();			/** kn[] ni kakikomu **/
	allkaku();
	tesuu = all_tesuu = 0;
}

// 平手の盤面か
int shogi::is_hirate_ban()
{
	int z;
	
	for (z=0x11; z<0x9a; z++) if ( init_ban[z] != hirate_ban[z] ) break;
	if ( z == 0x9a ) return 1;	// 平手
	return 0;
}

void shogi::hanten_with_hash_kifu()
{
	hanten_with_hash();
	
	int i;
	for (i=0;i<all_tesuu;i++) {
		int *p = &kifu[i+1][0];
		int bz = *(p+0), az = *(p+1), tk = *(p+2), nf = *(p+3);
		hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
		set_te(p,bz,az,tk,nf);
	}
}

void hanten_sasite( int *bz, int *az, int *tk, int *nf )
{
	int x,y;
	if ( *bz == 0 ) return;

	if ( *bz != 0xff ) {
		y = 0xa0 - (*bz & 0xf0);
		x = 0x0a - (*bz & 0x0f);
		*bz = y+x;
	}
	y = 0xa0 - (*az & 0xf0);
	x = 0x0a - (*az & 0x0f);
	*az = y+x;
	if ( *tk ) {
		if ( *tk > 0x80 ) *tk -= 0x80;
		else *tk += 0x80;
	}
	*nf = *nf;
}

void hanten_sasite_sayuu( int *bz, int *az, int *tk, int *nf )
{
	if ( *bz == 0 ) return;

	int y = *bz & 0xf0;
	int x = *bz & 0x0f;
	if ( *bz != 0xff ) *bz = y | (10 - x);

	y = *az & 0xf0;
	x = *az & 0x0f;
	*az = y | (10 - x);
}

int get_localtime(int *year, int *month, int *day, int *week, int *hour, int *minute, int *second)
{
	time_t timer = time(NULL);				/* 時刻の取得 */
	struct tm *tblock = localtime(&timer);	/* 日付／時刻を構造体に変換する */

	*year = *month = *day = *week = *hour = *minute = *second = 0;	// 初期化しておく。

	if ( tblock == NULL ) { fprintf(stderr,"localtime() == NULL!\n"); return 0; }
	*second = tblock->tm_sec;			/* 秒 */
	*minute = tblock->tm_min;			/* 分 */
	*hour   = tblock->tm_hour;			/* 時 (0〜23) */
	*day    = tblock->tm_mday;			/* 月内の通し日数 (1〜31) */
	*month  = tblock->tm_mon + 1;		/* 月 (0〜11) */
	*year   = tblock->tm_year + 1900;	/* 年 (西暦 - 1900) */
	*week   = tblock->tm_wday;			/* 曜日 (0〜6; 日曜 = 0) */
	return (int)timer;
}

// msec 単位の経過時間を返す。clock()はlinuxのマルチスレッドではおかしくなる。プロセス全体の消費時間なので2倍の時間に。
int get_clock()
{
#if defined(_MSC_VER)
	if ( CLOCKS_PER_SEC_MS != CLOCKS_PER_SEC ) { PRT("CLOCKS_PER_SEC=%d Err. not Windows OS?\n"); DEBUG_PRT(""); }
	return clock();
//	return GetTickCount();
#else
	struct timeval  val;
	struct timezone zone;
	if ( gettimeofday( &val, &zone ) == -1 ) { PRT("time err\n"); DEBUG_PRT(""); }
//	return tv.tv_sec + (double)tv.tv_usec*1e-6;
	return val.tv_sec*1000 + (val.tv_usec / 1000);
#endif
}

double get_spend_time(int ct1)
{
	return (double)(get_clock()+1 - ct1) / 1000.0;	// +1をつけて0にならないように。
}


// 棋泉形式の2バイトを4バイトに変換。numは現在の手数。num=0から始まる。
void shogi::trans_4_to_2_KDB(int b0,int b1, bool bGoteTurn, int *bz,int *az, int *tk, int *nf)
{
	int k;
//	b0 = p->kifu[i][0];			  // b0 = 11-99, 0x81-0x87
//	b1 = p->kifu[i][1];
	*bz = *az = *tk = *nf = 0;
		
	if ( b0 > 0x80 ) {	// 駒打ち
		*tk = b0 - 0x80;
		if ( bGoteTurn ) *tk |= 0x80;	// COM
		*bz = 0xff;
	} else {
		k = b0 / 10;
		*bz = k*16 + (b0-k*10);
	}

	if ( b1 > 0x80 ) {
		*nf = 0x08;
		b1 = b1 - 0x80;
	} else *nf = 0;

	k = b1 / 10;
	*az = k*16 + (b1-k*10);

	if ( *bz != 0xff ) *tk = init_ban[*az];
}

// KDB形式（棋泉形式）の圧縮棋譜に変換）
void pack_from4_to_2_KDB(int *pb0,int *pb1, int bz, int az, int tk, int nf)
{
	int b0 = 0;
	int b1 = 0;
	if ( bz == 0xff ) {
		b0 = 0x80 + (tk & 0x07);
	} else {
		b0 = (bz >> 4)*10 + (bz & 0x0f);	// b0 = 11-99, 0x81-0x87	
	}

	b1 = (az >> 4)*10 + (az & 0x0f);		// b1 = 11-99, 0x80+ 11-99
	if ( nf ) b1 += 0x80;
	
	*pb0 = b0;
	*pb1 = b1;
}


//lock_t          lock_fish_alloc;
lock_t          lock_io;	// PRT()の表示。
pthread_attr_t  pthread_attr;
pthread_t       p_thread[CPUS];		// スレッドのハンドル
int             lock_io_init_done = 0;

void InitLockYSS()
{
	static int fDone = 0;
	if ( fDone ) return;
	fDone = 1;

//	LockInit(lock_smp);
//	LockInit(lock_root);
	LockInit(lock_io);		// PRT()のため既にyss_win.cppで初期化してる。2度初期化は危険か？
//	LockInit(lock_fish_alloc);
	
	lock_io_init_done = 1;	// fishでクラスが作られる際にコンストラクタでPRT()が呼ばれるので防止
}



void shogi::hyouji()
{
   int x,y,i;
   PRT("    1 2 3 4 5 6 7 8 9  \n");
   for (y=0;y<9;y++) {
	  PRT("%d│",y+1);
	  for (x=0;x<9;x++) {
		 int n = ban[ (y+1)*16+x+1 ];
		 int k = kn[n][0];
		 if (k>0x80) k-=0x70;
//		 PRT("%s",koma[i]);
		 PRT("%s",koma_kanji[k]);
	  }  PRT("│");
	  if (y==0) {
		 PRT("   COM :");
//		 for (i=1;i<8;i++) PRT("%s %x:",koma[i+16],mo_c[i]);
		 for (i=1;i<8;i++) PRT("%s %x:",koma_kanji[i+16],mo_c[i]);
	  }
	  if (y==8) {
		 PRT("   MAN :");
//		 for (i=1;i<8;i++) PRT("%s %x:",koma[i],mo_m[i]);
		 for (i=1;i<8;i++) PRT("%s %x:",koma_kanji[i],mo_m[i]);
	  }
	  PRT("\n");
   }
   PRT("- - - - - - - - - - - - - - - - - - - - \n");
}

void shogi::print_kakinoki_banmen(int (*func_prt)(const char *, ... ), char *sName0,char *sName1)
{
	int i,k,x,y,z;

	if ( sName1 ) func_prt("後手：%s\r\n",sName1);
	func_prt("後手の持駒：");
	for (i=7;i>0;i--) {
		if ( (k = mo_c[i]) == 0 ) continue;
		func_prt(koma_kanji[i]);
		if ( k > 1 ) func_prt(num_kanji[k]);
		func_prt("　");
	}
	func_prt("\r\n");
	func_prt("  ９ ８ ７ ６ ５ ４ ３ ２ １\r\n");
	func_prt("+---------------------------+\r\n");
	for (y=1;y<=9;y++) {
		func_prt("|");
		for (x=1;x<=9;x++) {
			z = y*16+x;
			k = init_ban[z];
			if ( k == 0 ) {
				func_prt(" ・");
				continue;
			}
			if ( k < 0x80 ) func_prt(" %s",koma_kanji[k]);
       		else            func_prt("v%s",koma_kanji[k&0x0f]);
		}
		func_prt("|%s",num_kanji[y]);
		if (y==5) {
			func_prt("   手数=%d,IsSenteTurn()=%d",tesuu,IsSenteTurn());
			if ( tesuu > 0 ) {
				char tmp[TMP_BUF_LEN];
				change_sg( kifu[tesuu][0],kifu[tesuu][1],kifu[tesuu][2],kifu[tesuu][3], (IsSenteTurn()+1)&1, tmp );
				func_prt("  %s",tmp);
			}
		}
		func_prt("\r\n");
	}
	func_prt("+---------------------------+\r\n");

	func_prt("先手の持駒：");
	for (i=7;i>0;i--) {
		if ( (k = mo_m[i]) == 0 ) continue;
		func_prt(koma_kanji[i]);
		if ( k > 1 ) func_prt(num_kanji[k]);
		func_prt("　");
	}
	func_prt("\r\n");
	if ( sName0 ) func_prt("先手：%s\r\n",sName0);
	// 手番も出力
	if ( all_tesuu ) {
		int bz = kifu[1][0];
//		int az = kifu[1][1];
		if ( bz!=0xff && init_ban[bz] > 0x80 ) func_prt("後手番\r\n");
	}
}

void shogi::print_kb(void)
{
   int x,y,i;
   for (y=1;y<10;y++) {
	  PRT("y=%1d :",y);
	  for (x=1;x<10;x++) {
		 for (i=0;i<3;i++)
			PRT("%02x", kb_m[y*16+x][i] );
		 PRT(" ");
	  }
	  PRT("\n");
   }
   PRT("-----------------------------------------\n");
   for (y=1;y<10;y++) {
	  PRT("y=%1d :",y);
	  for (x=1;x<10;x++) {
		 for (i=0;i<4;i++)
			PRT("%02x", kb_c[y*16+x][i] );
		 PRT(" ");
	  }
	  PRT("\n");
   }
   PRT("-----------------------------------------\n");
}

void shogi::print_kn(void)
{
	int i,j;

	for (i=1;i<41;i+=4) {
		for (j=0;j<4;j++)
			PRT("kn[%02x] %02x,  %02x   ",i+j,kn[i+j][0],kn[i+j][1] );
		PRT("\n");
	}
}

void shogi::print_kiki(void)
{
	int x,y,i;

	PRT("------ print out ( kiki(yx) ) -------\n");
	for (y=0;y<9;y++) {
		PRT("y=%1d:",y+1);
		for (x=0;x<9;x++) {
			i=kiki_m[ (y+1)*16+x+1 ];
			PRT("%1x ",i);
		}
		PRT("    /////    ");
		for (x=0;x<9;x++) {
			i=kiki_c[ (y+1)*16+x+1 ];
			PRT("%1x ",i);
		}
		PRT("\n");
	}
}

void debug_exit()
{
#ifdef YSSFISH
	write_down_file_prt_and_die();
//	if ( GuiThreadId != GetCurrentThreadId() ) thread_exit();
#endif
#if !defined(AISHOGI) && defined(_MSC_VER)
	WaitKeyInput();	// Windows用のMessageBoxを表示する。
#endif
	exit(1);
}

void shogi::debug()
{
	int i;
	PRT_ON();
	PRT("debug()!!! tume_hyouka=%d,局面=%d\n",tume_hyouka,kyokumen);
	PRT("深=%d,RM=%d,fukasa_add=%d, 手数=%d\n",fukasa,read_max,fukasa_add,tesuu);

	for (i=0;i<fukasa;i++) {
		int *p = tejun[i];
		if ( p!=NULL ) PRT("%02x,%02x,%02x,%02x: ",*p,*(p+1),*(p+2),*(p+3));
		else PRT("PASS: ");
	}
	PRT("\n");

	for (i=0;i<fukasa;i++) {
		char retp[20];
		int *p = tejun[i];
		if ( p!=NULL ) change_sg(*p,*(p+1),*(p+2),*(p+3),i,retp);
		else           change_sg(0,0,0,0,i,retp);
		PRT("%s",retp);
	}
	PRT("\n");

//	print_kiki();
	hyouji();
//	print_init_ban();
#if defined(AISHOGI)	//[[
	::debug();
#else	//][
	debug_exit();
#endif	//]]
}

// クラスではないエラー
void debug()
{
	PRT_ON();
	PRT("Non Class Error!\n");
	PS->hyouji();
	debug_exit();
}


// EUCからSJISへの変換
void euc2sjis(unsigned char *c1, unsigned char *c2) {
	// 第２バイトの変換
	if( ( *c1 % 2 ) == 0 ) *c2 -= 0x02;
	else {
		*c2 -= 0x61;
		if ( *c2 > 0x7e ) ++ *c2;
	}

	// 第１バイトの変換
	if( *c1 < 0xdf ) {
		++ *c1;
		*c1 /= 2;
		*c1 += 0x30;
	} else {
		++ *c1;
		*c1 /= 2;
		*c1 += 0x70;
	}
}

// 文字列をEUCからSJISへ
void str_euc2sjis(char *p) {
	int i;
	int nLen = strlen_int(p);
	for (i=0;i<nLen-1;i++) {
		unsigned char c = *(p+i);
		if ( 0xa1 <= c && c <= 0xfe ) {
			euc2sjis((unsigned char *)p+i, (unsigned char *)p+i+1 );
			i++;
		}
	}
}

// S-JISからEUCへの変換プログラム 
void sjis2euc( unsigned char *c1, unsigned char *c2)
{
	if( *c2 < 0x9f ) {
		if( *c1 < 0xa0 ) {
			*c1 -= 0x81;
			*c1 *= 2;
			*c1 += 0xa1;
        } else {
			*c1 -= 0xe0;
			*c1 *= 2;
			*c1 += 0xdf;
        }
		if( *c2 > 0x7f ) -- *c2;
	    *c2 += 0x61;
	} else {
		if( *c1 < 0xa0 ) {
			*c1 -= 0x81;
			*c1 *= 2;
			*c1 += 0xa2;
		} else {
			*c1 -= 0xe0;
			*c1 *= 2;
			*c1 += 0xe0;
		}
		*c2 += 2;
	}
}

// 漢字の先頭バイトか？判定時には char を unsigned にしていること！
int IsKanji(unsigned char c)
{
	return (( 0x80<(c) && (c)<0xa0 ) || ( 0xdf<(c) && (c)<0xfd ));
}

// 文字列をSJISからEUCへ
void str_sjis2euc(char *p) {
	int i,nLen;
	unsigned char c;
	
	nLen = strlen(p);
	for (i=0;i<nLen-1;i++) {
		c = *(p+i);
		if ( IsKanji(c) ) {
			sjis2euc( (unsigned char*)(p+i), (unsigned char*)(p+i+1) );
			i++;
		}
	}
}



/*** able_move その指し手が着手可能な場合は０以外を返す *******************/
/*** man ,com 共用 ・・・・・・ 打ち歩詰めは無視 ***/
int shogi::able_move( int bz, int az, int tk, int nf )
{
	if ( bz == 0xff ) {	// 駒打ちの時
		if ( init_ban[az] != 0 ) return(0);	// 既に駒があるところに打とうとしている。
		if ( tk > 0x80 ) {
			if ( (fukasa & 1) == 1 ) {
				PRT("MAN の手番で COM の駒を打とうとしている！ハッシュの同一視か？！\n");	// Error判定

				PRT("bz,az,tk,nf=%x,%x,%x,%x :(%d/%d)\n",bz,az,tk,nf,fukasa,read_max );
				print_tejun();
				{int i,*p,ret;	ret = ret_fukasa[fukasa];		// 評価値が決定した深さ。
					for ( i=fukasa+0; i<=ret; i++ ) {	// = が付く
						p = saizen[fukasa][i];	bz = *(p+0); az = *(p+1); tk = *(p+2); nf = *(p+3);	PRT("%x,%x,%x,%x: :",bz,az,tk,nf);
					}
					PRT("\n");
				}

				return(0);
			}
			if ( mo_c[tk & 0x0f] == 0 ) return(0);
			if ( tk == 0x81 && nifu_c(az) == 0  ) return(0);
			if ( tk <= 0x82 && az > 0x90 ) return(0);	// 行き所がない
			if ( tk == 0x83 && az > 0x80 ) return(0);
		} else {
			if ( (fukasa & 1) == 0 ) {
				PRT("COM の手番で MAN の駒を打とうとしている！ハッシュの同一視か？！\n");	// Error判定
				return(0);
			}
			if ( mo_m[tk       ] == 0 ) return(0);
			if ( tk == 0x01 && nifu_m(az) == 0  ) return(0);
			if ( tk <= 0x02 && az < 0x20 ) return(0);	// 行き所がない
			if ( tk == 0x03 && az < 0x30 ) return(0);
		}
		return (1);
	}

	int k = init_ban[bz];
	if ( k == 0 ) return(0);	// 盤上にない駒を動かそうとしている。

	if ( nf == 0x08 && (k & 0x0f) > 0x08 ) {
//		PRT("成り駒がなっている！ move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return 0;
	}

	if ( init_ban[az] != tk	) {
//		PRT("盤上にない駒を取ろうとしている！\n");
/*	
hyouji();
PRT("bz,az,tk,nf=%x,%x,%x,%x :read_max=%d \n",bz,az,tk,nf,read_max );
print_tejun();
PRT("hash_code1,2 = %x,%x hash_motigoma = %x\n",hash_code1,hash_code2,hash_motigoma);
*/
		return(0);
	}

	int n = ban[bz];	// 駒番号

	if ( k > 0x80 ) {	// COM の駒
		if ( (fukasa & 1) == 1 ) {
//			PRT("MAN の手番で COM の駒を動かそうとしている！ハッシュの同一視か？！\n");	// Error判定
			return(0);
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return(0);	// 成駒と王と金は成れない
			if ( az < 0x70 && bz < 0x70 ) return(0);	// 成れない。
		} else {
			if ( k == 0x83 && az > 0x80 ) return(0);	// 不成は許されない。
			if ( k <= 0x82 && az > 0x90 ) return(0);	// 不成は許されない。
		}
	
		if ( tk > 0x80 ) return (0); // 自分の駒を取っている。

		int i, loop = kiki_c[az];
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return(1);	// OK! 動かせる！
		}
	} else {
		if ( (fukasa & 1) == 0 ) {
//			PRT("COM の手番で MAN の駒を動かそうとしている！ハッシュの同一視か？！\n");	// Error判定
			return(0);
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return(0);	// 成駒と王と金は成れない
			if ( az > 0x40 && bz > 0x40 ) return(0);	// 成れない。
		} else {
			if ( k == 0x03 && az < 0x30 ) return(0);	// 不成は許されない。---> なぜかコメントにしていた。理由は？
			if ( k <= 0x02 && az < 0x20 ) return(0);	// 不成は許されない。---> なぜかコメントにしていた。理由は？
		}

		if ( tk && tk < 0x80 ) return (0);	// 自分の駒を取っている。

		int i,loop = kiki_m[az];
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return(1);
		}
	}
	return (0);	// 動けない
}

// とりあえず着手可能か、の判定。指した後までは知らん
bool shogi::is_pseudo_legalYSS(Move m, Color sideToMove)
{
	int bz = get_bz(m);
	int az = get_az(m);
	int tk = get_tk(m);
	int nf = get_nf(m);

	if ( bz == 0xff ) {	// 駒打ちの時
		if ( init_ban[az] != 0 ) return false;	// 既に駒があるところに打とうとしている。
		if ( az > 0x99 || (az&0x0f) > 9 ) return false; // 
		if ( nf ) return false;
		if ( tk > 0x80 ) {
			if ( tk > 0x87 ) return false;
			if ( sideToMove == WHITE ) {
//				PRT("MAN の手番で COM の駒を打とうとしている！ハッシュの同一視か？！\n");	// Error判定
				return false;
			}
			if ( mo_c[tk & 0x0f] == 0 ) return false;
			if ( tk == 0x81 && nifu_c(az) == 0 ) return false;
			if ( tk <= 0x82 && az > 0x90 ) return false;	// 行き所がない
			if ( tk == 0x83 && az > 0x80 ) return false;
		} else {
			if ( tk == 0 || tk > 0x07 ) return false;
			if ( sideToMove == BLACK ) {
//				PRT("COM の手番で MAN の駒を打とうとしている！ハッシュの同一視か？！\n");	// Error判定
				return false;
			}
			if ( mo_m[tk       ] == 0 ) return false;
			if ( tk == 0x01 && nifu_m(az) == 0 ) return false;
			if ( tk <= 0x02 && az < 0x20 ) return false;	// 行き所がない
			if ( tk == 0x03 && az < 0x30 ) return false;
		}
		return true;
	}

	if ( bz == az ) return false;
	if ( az > 0x99 || (az&0x0f) > 9 ) return false;
	if ( bz > 0x99 || (bz&0x0f) > 9 ) return false;
	if ( tk == 0xff ) return false;
	
	const int k = init_ban[bz];
	if ( k == 0 || k == 0xff ) return false;	// 盤上にない駒を動かそうとしている。

	if ( nf != 0 && nf != 0x08 ) return false;
	
	if ( nf == 0x08 && (k & 0x0f) > 0x07 ) {
//		PRT("成り駒がなっている！ move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return false;
	}

	if ( init_ban[az] != tk	) {
//		PRT("盤上にない駒を取ろうとしている！\n");
		return false;
	}

	const int n = ban[bz];	// 駒番号
	if ( kn[n][0] != k  ) return false;
	if ( kn[n][1] != bz ) return false;

	if ( k > 0x80 ) {	// COM の駒
		if ( sideToMove == WHITE ) {
//			PRT("MAN の手番で COM の駒を動かそうとしている！ハッシュの同一視か？！\n");	// Error判定
			return false;
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return false;	// 成駒と王と金は成れない
			if ( az < 0x70 && bz < 0x70 ) return false;	// 成れない。
		} else {
			if ( k == 0x83 && az > 0x80 ) return false;	// 不成は許されない。
			if ( k <= 0x82 && az > 0x90 ) return false;	// 不成は許されない。
		}
	
		if ( tk > 0x80 ) return false; // 自分の駒を取っている。

		const int loop = kiki_c[az];
		int i;
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return true;	// OK! 動かせる！
		}
	} else {
		if ( sideToMove == BLACK ) {
//			PRT("COM の手番で MAN の駒を動かそうとしている！ハッシュの同一視か？！\n");	// Error判定
			return false;
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return false;	// 成駒と王と金は成れない
			if ( az > 0x40 && bz > 0x40 ) return false;	// 成れない。
		} else {
			if ( k == 0x03 && az < 0x30 ) return false;	// 不成は許されない
			if ( k <= 0x02 && az < 0x20 ) return false;	// 不成は許されない
		}

		if ( tk && tk < 0x80 ) return false;	// 自分の駒を取っている。

		const int loop = kiki_m[az];
		int i;
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return true;
		}
	}
	return false;	// 動けない
}

// とりあえず着手可能な手が、指した後に王が素抜かれないか
bool shogi::is_pl_move_is_legalYSS(Move /*m*/, Color /*sideToMove*/)
{
	return true;
/*
	int bz = get_bz(m);
	int az = get_az(m);
	int tk = get_tk(m);
	int nf = get_nf(m);
	
	// 王が自爆しない -> 普通はありえんな・・・
	
	// 王手がかかっている場合に防がない
	PRT("is_pl_move_is_legalYSS");

	if ( bz == 0xff ) {	// 駒打ちの時
		return true;
	}
	return true;
*/
}

static unsigned short move_is_check_table[81][81];
static unsigned short move_is_tobi_check_table[81][81];

int shogi::is_move_attack_sq256(Move m, Color sideToMove, Square256 attack_z)
{
	// 空き王手は無視。移動後の駒の位置に敵王がいるかどうかのみを調べる。飛角香の離れ王手も無視。
	int bz = get_bz(m);
	int az = get_az(m);
	
//	Color them = ~sideToMove;
//	int teki_ou_z = attack_z;
	
	int k;
	if ( bz==0xff ) {
		int tk = get_tk(m);
		k = tk & 0x0f;
	} else {
		int nf = get_nf(m);
		k = (init_ban[bz] + nf) & 0x0f;
	}
	int ou_z81 = get81(attack_z);
	int az81   = get81(az);
	if ( sideToMove == BLACK ) {
		ou_z81 = flip(ou_z81);
		az81 = flip(az81);
	}
	// 99%は王手かどうかで呼ばれる。nodeの5倍も呼ばれる。かなり多い。
//	{ static int sum,all; all++; if ( (init_ban[teki_ou_z] & 0x0f)==0x08 ) sum++; if ( (all&0xffff)==0 ) PRT("%d/%d\n",sum,all); }
#if 0
	int k7 = k & 0x07;
	if ( k==0x02 || k7==0x06 || k7==0x07 ) {	// k=2,6,7,14,15
		int k3 = k7 - 2;
		k3 = (k3>>2) + (k3&1); // 2,6,7 -> 0,4,5 -> 0,1,2        10 110 111      00 100 101
		int tc = (move_is_tobi_check_table[ou_z81][az81] >> (k3*5)) & 0x1f;
		if ( tc ) {
			int n  = tc & 0x07;
			int dz = z8[((tc>>3) & 0x03)*2+(k3&1)];  // (方向 2bit, 距離 3bit) x3 飛角香
			dz = dz*( 1 + (sideToMove == BLACK)*(-2));
			int z = teki_ou_z + dz;	
//			{ print_teban(!sideToMove); print_te(bz,az,tk,nf); PRT("王手=%04x(%2x),k3=%d,n=%d,dz=%+3d,z=%02x\n",tc,teki_ou_z,k3,n,dz,z); }
			int i;
			for (i=0;i<n;i++,z+=dz) if ( init_ban[z] ) return 0;
//			PRT("王手\n"); hyouji();
			return 1;	
		}		
	}
#endif
//	if ( sideToMove	== BLACK ) { hyouji(); print_te(bz,az,tk,nf); PRT("王手=%d\n",(move_is_check_table[ou_z81][az81] >> k) & 1); }
	return (move_is_check_table[ou_z81][az81] >> k) & 1;
}


void make_move_is_check_table()
{
//	PRT("\n\n動作確認！\n\n");
	memset(move_is_check_table, 0, sizeof(move_is_check_table));
	int x,y;
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int ou_z = (y+1)*16+(x+1);
		int i;
		for (i=0;i<8;i++) {
			int az   = ou_z + oute_z[0][i];		// com王に王手をかける
			int oute_number = oute_z[1][i];
			if ( is_out_of_board(az) ) continue;
			int k;
			for (k=1;k<16;k++) {
				if ( oute_can[oute_number][k] == 0 ) continue;
				move_is_check_table[get81(ou_z)][get81(az)] |= (1 << k); 
			}
		}
		int az = ou_z + 0x1f;
		for (i=0;i<2;i++,az+=0x02) {
			if ( is_out_of_board(az) ) continue;
			move_is_check_table[get81(ou_z)][get81(az)] |= (1 << 0x03); 
		}
	}
	
	if ( 0 ) for (y=0;y<3;y++) for (x=0;x<9;x++) {
		int xx,yy,ou_z = (y+1)*16+(x+1);
		PRT("ou_z=%02x\n",ou_z);
		for (yy=0;yy<9;yy++) for (xx=0;xx<9;xx++) {
			int az =  (yy+1)*16+(xx+1);
			PRT("%04x ",move_is_check_table[get81(ou_z)][get81(az)]);
			if ( xx==8 ) PRT("\n");
		}
	}
}

void make_move_is_tobi_check_table()
{
	memset(move_is_tobi_check_table, 0, sizeof(move_is_tobi_check_table));
	int x,y;
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int ou_z = (y+1)*16+(x+1);
		int i;
		for (i=0;i<8;i++) {
			int dz = z8[i];
			int az = ou_z + dz;
			if ( is_out_of_board(az) ) continue;
			int n;
			for (az+=dz,n=0; n<7; n++,az+=dz) {		// 隣接する位置は飛ばす
				if ( is_out_of_board(az) ) break;
				int k = (((i>>1)<<3) + ((n+1)));	// 2bit + 3bit
				if ( (i&1)==0 ) k = k << 10;
				if ( (i&1)==1 ) k = k << 5;
				if ( i==2 ) k |= k >> 10;
				move_is_tobi_check_table[get81(ou_z)][get81(az)] |= k;
			}
		}
	}
	
	if ( 0 ) for (y=0;y<3;y++) for (x=0;x<9;x++) {
		int xx,yy,ou_z = (y+1)*16+(x+1);
		PRT("ou_z=%02x\n",ou_z);
		for (yy=0;yy<9;yy++) for (xx=0;xx<9;xx++) {
			int az =  (yy+1)*16+(xx+1);
			PRT("%04x ",move_is_tobi_check_table[get81(ou_z)][get81(az)]);
			if ( xx==8 ) PRT("\n");
		}
	}
}














#define REHASH_MAX (2048*1)		// 再ハッシュの最高回数（手順の登録にも使う）5手目の無意味テーブルでも。593以上。

int rehash[REHASH_MAX-1];		// バケットが衝突した際の再ハッシュ用の場所を求めるランダムデータ
int rehash_flag[REHASH_MAX];	// 最初に作成するために

int hash_use;
int hash_over;			/* ｎ回の再ハッシュを何度繰り返したか */

static int seed;			// 乱数の種。初期値、1 から 2^31-1 まで。
int rand_yss_enable = 1;	// 乱数を生成する(基本)。思考の実験で乱数を止めたい時に、思考に飛ぶ直前にOFFにする。

// ハッシュ用乱数テーブルの初期化。
void hash_ban_init(void)
{
	int p,i,j,k;
	int f = 0,f_lo,f_hi;

	/* 実行するたびに違う値が得られるように、現在の時刻値を使って乱数ジェネレータを初期化します。*/
//	srand( (unsigned)time( NULL ) );

	seed = 1;
//	seed = 1043618065;
	
//	init_rnd(1);	// Ｍ系列乱数の初期化 seed を使う

	for (j=0;j<1;j++) {
		p =0;
//		k0 = k1 = 0;
		for (i=0;i<10000;i++ ) {
			// 乗数合同法は個々のビットを見ると、０ばかり、とかが非常に多い。-> 嘘。下位ビットが。しかし、この乱数はOK。
			f = rand_yss();			// 31 bit なので 0x80000000 は常に０
			if ( (i&1) && (f & 1) ) p++;
		}
//		PRT("\n0=%d, 1=%d\n",k0,k1);	
//		PRT("%6d: seed = %11d, random=0x%08x ,p=%d \n",i*(j+1),seed,f,p);
	}

//	PRT("10000回のループで seed = 1043618065 なら乱数列は正解！\n");

	if ( seed != 1043618065 ) PRT("このシステムの環境では乱数が正しく生成されていません。int が 32bit 以上の必要があります。\n");
//	PRT("UseMemory= %d MB, Hash_Table_Size=%d, Hash_Stop=%d, Hash_Byte=%d \n",(Hash_Table_Size * sizeof(hash_table[0]))>>20, Hash_Table_Size, Hash_Stop, sizeof(hash_table[0]) );
	
	///////////////////////////////////////////////////////////////

	for (i=0; i<32; i++) {
		for (j=0; j<BAN_SIZE; j++) {
			for (k=0; k<2; k++) {

/*
				// 最上位ビットを取り出して３２個ならべて一つの乱数を作る
				// ついでにシステム乱数も加えてみる ---> ハッシュ同一しまくり！
				{
					int m,n;
					f = 0;
					for (m=0;m<32;m++) {
						n = (rand_yss() >> 30) & 1;
						// n = (n + (rand() & 2)>>1) & 1;
						f += n << m;
					}
				}
*/

				f_lo = rand_yss();
				f_hi = rand_yss();
				f = f_lo;
				if ( f_hi &          1 ) f += 0x80000000;	// 奇数なら最上位ビットを立てる。苦し紛れ。
//				if ( f_hi & 0x40000000 ) f += 0x80000000;	// 最上位ビットを加える？

//				PRT("%08x:",f); int m; for (m=0;m<32;m++) PRT("%d",(f>>(31-m))&1);	PRT("\n");	// 2進数で表示
//				PRT(" %08x,",f);
				hash_ban[i][j][k] = f;
			}
		}
	}

/*
	// 同一の数字があるかチェック
	for (i=0; i<32; i++) for (j=0; j<BAN_SIZE; j++) for (k=0; k<2; k++) {
		unsigned int ff = hash_ban[i][j][k];
		int p,q,r;
		for (p=0; p<32; p++) for (q=0; q<BAN_SIZE; q++) for (r=0; r<2; r++) {
			if ( ff == hash_ban[p][q][r] && i!=p && j!=q && r!=k ) PRT("同一!!!!!!!!!!\n");
		}
	}
*/
	// 再ハッシュ配列を作成
	i = 0;
	rehash_flag[0] = 1;	// 0 は使わない
	for ( ;; ) {
		k = (rand_yss() >> 8) & (REHASH_MAX-1);
		if ( rehash_flag[k] ) continue;	// 既に登録済み
		rehash[i] = k;
		rehash_flag[k] = 1;
		i++;
		if ( i == REHASH_MAX-1 ) break;
	}
//	for (i=0;i<REHASH_MAX-1;i++) {	PRT("%4d,",rehash[i]);		if ( ((i+1) % 16)==0 ) PRT("\n");	} PRT("\n");

	int year,month,day,week,hour,minute,second;
	int timer = get_localtime(&year, &month, &day, &week, &hour, &minute, &second);
	PRT("Date %4d-%02d-%02d %02d:%02d:%02d,", year, month, day, hour, minute, second);
//	seed += second + minute*60 + hour*3600 + day*86400 + month*2678400 + year*32140800;
	seed += timer;	// この方が単純でよし。
	rand_yss();	// seed を整数に
	PRT("rand_seed =%11d\n",seed );
//	PRT("現在の経過秒は %ld \n",time(NULL));
}

int print_time()
{
	int year,month,day,week,hour,minute,second;
	int timer = get_localtime(&year, &month, &day, &week, &hour, &minute, &second);
	PRT("%4d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
	return timer;
}


// 乗数合同法による乱数の生成。この場合、1 から 2^31 - 2 までの乱数列が得られる。周期は 2^31 -1 である。 
// bit 1993-04 より。最低基準乱数列の発生。int は 32bit 以上なら正しく動作する。
int rand_yss()
{
	static const int a = 16807;				// 7^5
	static const int m = 2147483647;		// 2^31 - 1;
	static const int q = 127773;			// 3^2 * 14197;
	static const int r = 2836;

	if ( rand_yss_enable == 0 ) { PRT("rand_yss()=0\n"); return 0; }	// 乱数発生を停止。

#if 0	// 46bit以上ある環境ではこれでOK。 32bit環境では2倍遅い。
	__int64 seed64 = seed;
	seed64 = a * seed64;
	seed64 = seed64 % m;
	seed = (int)seed64;
#else
	int lo,hi,tt;

	hi = seed / q;
	lo = seed % q;

	tt = a*lo - r*hi;
	if ( tt > 0 ) seed = tt;
	else seed = tt + m;

//    seed *= 69069;
#endif
	return( seed );		// 以前は seed をそのまま返していた！--->整数だからいいのか
}


// Ｍ系列乱数による方法（アルゴリズム辞典、奥村晴彦著より）周期 2^521 - 1 の他次元分布にも優れた方法
#define M32(x)  (0xffffffff & (x))
static int rnd521_count = 521;	// 初期化忘れの場合のチェック用
static unsigned long mx521[521];

void rnd521_refresh(void)
{
	int i;
	for (i= 0;i< 32;i++) mx521[i] ^= mx521[i+489];
	for (i=32;i<521;i++) mx521[i] ^= mx521[i- 32];
}

void init_rnd521(unsigned long u_seed)
{
	int i,j;
	unsigned long u = 0;
	for (i=0;i<=16;i++) {
		for (j=0;j<32;j++) {
			u_seed = u_seed*1566083941UL + 1;
			u = ( u >> 1 ) | (u_seed & (1UL << 31));	// 最上位bitから32個取り出している
		}
		mx521[i] = u;
	}
	mx521[16] = M32(mx521[16] << 23) ^ (mx521[0] >> 9) ^ mx521[15];
	for (i=17;i<=520;i++) mx521[i] = M32(mx521[i-17] << 23) ^ (mx521[i-16] >> 9) ^ mx521[i-1];
	rnd521_refresh();	rnd521_refresh();	rnd521_refresh();	// warm up
	rnd521_count = 520;
}

unsigned long rand_m521(void)
{
	if ( ++rnd521_count >= 521 ) { 
		if ( rnd521_count == 522 ) init_rnd521(4357);	// 初期化してなかった場合のFail Safe
		rnd521_refresh();
		rnd521_count = 0;
	}
	return mx521[rnd521_count];
}

void shogi::hash_init(void)
{
	int k,z;

	koma_koukan_table_init();	// 駒交換をした場合の配列を、基本の駒の価値から作り直す。

	// YSSが先手で、盤面を反転したとき、相手と点対称の位置にある駒を動かせば、
	// XORで元のハッシュ値に戻ってしまう。人間が78金と指した時、32金と指せば。
	// 初形のみからハッシュ値を作り直す。手番は無視してもいい、はず。
	// このハッシュ値は、常に反転していない状態（手番情報を含まない）
	hash_code1 = 0x00000000;
	hash_code2 = 0x00000000;

	for (z=0x11;z<0x9a;z++) {
		k = init_ban[z];
		if ( k==0 || k==0xff ) continue;

		if ( k > 0x80 ) k -= 0x70;	// 種類 0x01-0x8f -> 0x01-0x1f
		HashXor(k,z);
	}
}


// このハッシュ再計算専用関数はYSSでも用いる。From 98/05/01
// この関数がしていること
// 盤面の内容はそのままに、ハッシュ値のみを、盤面反転状態、または、ビット反転（手番交代）を行う。
// YSSでは、sennitite[] に登録する時のみに使用。YSS_WIN.C を参照！ YSSでは、常にcomplementは０(手番はそのまま）で

void shogi::hash_calc( int upsidedown, int complement )
{
	unsigned int code1, code2, motigoma;
	int i,k,x,y,z;
	unsigned int *p;

   /******* init.. hash_motigoam for hash_table ********/
	motigoma = 0;
	for (i=1;i<8;i++) {
		motigoma += hash_mask[0][i] * (upsidedown ? mo_m[i] : mo_c[i]);
	}

	// ハッシュコード全体を書き直す。盤上に何も駒がないときが０とする。
	code1 = 0x00000000;
	code2 = 0x00000000;
	for ( y=1;y<10;y++ ) {
		for ( x=1;x<10;x++) {
			z = y*16+x;
		    k = init_ban[(upsidedown ? ((10-y)*16+(10-x)) : (y*16+x))];
			if ( k == 0 ) continue;
			if ( upsidedown ) k ^= 0x80;
			if ( k > 0x80 ) k -= 0x70;	/** shurui 0x01-0x8f -> 0x01-0x1f  (0x81 -> 0x11) **/

			p = hash_ban[k][z];
			code1 ^= *p;
			code2 ^= *(p+1);
		}
	}
	if (complement) {
		code1 = ~code1;
		code2 = ~code2;
	}
	hash_motigoma = motigoma;
	hash_code1 = code1;
	hash_code2 = code2;
}

void shogi::set_sennitite_info()
{
	hash_calc( tesuu & 1, 0 );	// ハッシュ値を作り直す（手数=1で反転する）
	HASH *psen = &sennitite[tesuu];
	psen->hash_code1 = hash_code1;		// 局面を記憶 
	psen->hash_code2 = hash_code2;		// 局面を記憶 
	psen->motigoma   = hash_motigoma;
	psen->flag       = ((tesuu&1)==0)*kiki_c[kn[1][1]] + ((tesuu&1)==1)*kiki_m[kn[2][1]];	// 王手かどうか
}









char *get_str_zahyo(int z)
{
	static const char *suuji[10] = {
	  "０","１","２","３","４","５","６","７","８","９"
	};
	static const char *moji[10] = {
	  "  ","一","二","三","四","五","六","七","八","九"
	};
	static char str[5];
	int y =  z/16;
	int x = 10-(z-y*16);
	str[0] = 0;
	if ( x<0 || x>9 || y<1 || y>9 ) return str;
	sprintf(str,"%s%s",suuji[x],moji[y]);
	return str;
}

const char *koma_kanji_str(int n)
{
	static const char *koma_kanji2[16] = {
	  "  ","歩",  "香",  "桂",  "銀","金","角","飛",
	  "玉","と","成香","成桂","成銀","  ","馬","龍"
	};
	if ( n < 0 || n >= 16 ) DEBUG_PRT("");
	return koma_kanji2[n];
}

/************************************************/
void shogi::change(int bz,int az,int tk,int nf,char retp[])
{
	int bx,by,k;
	char p[5];

	if ( bz == 0 ) {
		sprintf(retp,"PASS        ");
		return;	
	}
//	ay = az/16;
//	ax = 10-(az-ay*16);
	by = bz/16;
	bx = 10-(bz-by*16);
	k = kn[ban[bz]][0] & 0x0f;
	if ( k==0 && bz!=0xff ) k = kn[ban[az]][0] & 0x0f;
	sprintf(retp,"%s",get_str_zahyo(az));
//	strcpy(retp,suuji[ax]);
//	strcat(retp, moji[ay]);
	if ( bz != 0xff ) {
		if ( nf==0x08 ) k=k & 0x07;
		strcat(retp, koma_kanji_str(k));
		if ( nf ) strcat(retp,"成");

		strcpy(p,"(00)");
		p[1]+=bx;
		p[2]+=by;
		strcat(retp,p);
		if ( nf==0 && ( k<0x0a || k>0x0d ) ) strcat(retp,"  ");
	}
	else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "打    ");
	}
}

void shogi::change(int *p,char retp[])
{
	change(*(p+0),*(p+1),*(p+2),*(p+3),retp);
}

/*** 短く変換する ***/
void shogi::change_small(int bz,int az,int tk,int nf,char retp[])
{
	static const char *suuji[10] = {
	  "0","1","2","3","4","5","6","7","8","9"
	};

	int bx,by,ax,ay,k;
	char p[5];

	if ( bz == 0 ) {
		sprintf(retp,"PASS    ");
		return;	
	}
	if ( bz < 0 || bz > 0xff || az <= 0 || az > 0x99 || nf < 0 || nf > 0x08 || tk < 0 || tk > 0x8f ) { DEBUG_PRT("Err bz,az,tk,nf=%02x,%02x,%02x,%02x\n",bz,az,tk,nf); }

	ay = az/16;
	ax = 10-(az-ay*16);
	by = bz/16;
	bx = 10-(bz-by*16);
	k = init_ban[bz] & 0x0f; // kn[ban[bz]][0] & 0x0f;
	if ( k==0 && bz!=0xff ) k = init_ban[az] & 0x0f;	// kn[ban[az]][0] & 0x0f;
	strcpy(retp,suuji[ax]);
	strcat(retp,suuji[ay]);
	if ( bz != 0xff ) {
		if ( nf==0x08 ) k=k & 0x07;
		strcat(retp, koma_kanji_str(k));
		if ( nf ) strcat(retp,"成");

		strcpy(p,"(00)");
		p[1]+=bx;
		p[2]+=by;
		strcat(retp,p);
//		if ( nf==0 && ( k<0x0a || k>0x0d ) ) strcat(retp,"  ");
	} else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "打");
	}
}

// ▲76歩、の表記に変換する。最大 "▲53銀成(42)" で12文字。retp は13必要
void shogi::change_sg(int bz,int az,int tk,int nf,int depth, char retp[])
{
	static const char *suuji[10] = {
	  "0","1","2","3","4","5","6","7","8","9"
	};
	int bx,by,ax,ay,k;

	if ( depth & 1 ) sprintf(retp,"▲");
	else             sprintf(retp,"△");

	if ( bz == 0 ) {
		strcat(retp,"PASS");
		return;	
	}
	if ( bz < 0 || bz > 0xff || az <= 0 || az > 0x99 || nf < 0 || nf > 0x08 || tk < 0 || tk > 0x8f ) { DEBUG_PRT("Err sg bz,az,tk,nf=%02x,%02x,%02x,%02x\n",bz,az,tk,nf); }

	ay = az/16;
	ax = 10-(az-ay*16);
	by = bz/16;
	bx = 10-(bz-by*16);
	k = 0;	// 動いた駒。bzに駒がない場合はazを見る
	if ( bz!=0xff ) {
		if ( init_ban[bz] ) {
			k = kn[ban[bz]][0] & 0x0f;
		} else {
			k = kn[ban[az]][0] & 0x0f;
		}
	}
	strcat(retp,suuji[ax]);
	strcat(retp,suuji[ay]);
	if ( bz != 0xff ) {
		if ( nf==0x08 ) k = k & 0x07;
		strcat(retp, koma_kanji_str(k));

		// 右、左、直、引、をつける。
		// 移動する場所に、同じ種類の駒が2つ利いていたら。
		for (;;) {
			int i,n,dep,sum,loop,sk[32];
	//		static char *sayu[4] = { "右","左","直","引" };
			dep = depth & 1;
			sum = 0;
			if ( dep ) loop = kiki_m[az];
			else       loop = kiki_c[az];
			for (i=0;i<loop;i++) {
				if ( dep ) n = kb_m[az][i] & 0x7f;
				else       n = kb_c[az][i] & 0x7f;
				if ( n <= 0 || n >= 42 ) continue;
				k = kn[n][0];
				if ( k == kn[ban[bz]][0] ) {
					sk[sum] = kn[n][1];
					sum++;
				}
			}
			if ( sum <= 1 ) break;
			for (i=0;i<sum;i++) {
				if ( sk[i] != bz ) continue;
				if ( i == 0 ) break;
				k = sk[0];
				sk[0] = sk[i];
				sk[i] = k;
				break;
			}
			if ( i==sum ) break;
			// sk[0] に実際に指した駒。sk[1]が比較される駒。
#if 1
			strcat(retp,"(");
			strcat(retp,suuji[bx]);
			strcat(retp,suuji[by]);
			strcat(retp,")");
#else
			dx = (sk[0] & 0x0f) - (sk[1] & 0x0f);
			dy = (sk[0] & 0xf0) - (sk[1] & 0xf0);
			if ( dx ) {	// 右、左を優先。
				if ( dx > 0 ) strcat(retp, sayu[1-dep]);
				else          strcat(retp, sayu[  dep]);
			} else {
				if ( dy > 0 ) strcat(retp, sayu[2+1-dep]);
				else          strcat(retp, sayu[2+  dep]);
			}
#endif
			break;
		}

		if ( nf ) strcat(retp,"成");
	} else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "打");
	}
}

void shogi::change_log_pv(char *str)
{
	// ** 121 -4445FU +6766FU -6364FU +8899OU -5263KI +5867KI -7374FU +7988GI -8173KE +6979KI
	// ** 121 △45歩▲66歩...         △45歩打 ... 打が多いと超える。空白なしで(+8899OU-5263KI)
	char sTmp[TMP_BUF_LEN];
	memset(sTmp,0,sizeof(sTmp));
	int i,n = strlen_int(str);
	int found_pv = 0;
	for (i=0;i<n;i++) {
		char *p = str + i;
		for (;;) {
			if ( n-i < 7 ) break;
			if ( !(*(p+0)=='+' || *(p+0)=='-') ) break;
			int bx = *(p+1) - '0';
			int by = *(p+2) - '0';
			int ax = *(p+3) - '0';
			int ay = *(p+4) - '0';
			if ( 0 <= bx && bx <= 9 && 0 <= by && by <= 9 && 1 <= ax && ax <= 9 && 1 <= ay && ay <= 9 ) {
				int j;
				for (j=0;j<16;j++) if ( *(koma[j]+0) == *(p+5) && *(koma[j]+1) == *(p+6) ) break;
				if ( j==16 ) break;

				const char *sSG[2] = { "▲","△" };
				char ss[16];
				sprintf(ss,"%s%d%d%s",sSG[*(p+0)=='-'],ax,ay,koma_kanji_str(j));
				if ( bx==0 ) strcat(ss,"打");
				found_pv = 1;
				strcat(sTmp,ss);
			}
			break;
		}
		if ( found_pv == 0 ) sTmp[i] = str[i];
		if ( i > TMP_BUF_LEN-10 || strlen_int(sTmp) > TMP_BUF_LEN-10 ) break;
	}
	int nLen = strlen_int(sTmp); 
	if ( nLen < TMP_BUF_LEN-1 && nLen>0 && sTmp[nLen-1]!='\n' ) strcat(sTmp,"\n");
	strcpy(str,sTmp);
//	PRT("%s",str);
}

// 手を表示するだけ
void shogi::print_te(int bz, int az,int tk,int nf)
{
	char retp[20];
	change_small(bz,az,tk,nf,retp);
	PRT("%s ",retp);
}
// ハッシュの手を表示。
void shogi::print_te(HASH_RET *phr)
{
	print_te(phr->bz, phr->az, phr->tk, phr->nf);
}
// sasite[]などの手を表示。
void shogi::print_te(int *p)
{
	print_te(*(p+0),*(p+1),*(p+2),*(p+3));
}
void shogi::print_te(int pack)
{
	int bz,az,tk,nf;
	unpack_te(&bz,&az,&tk,&nf, pack);
	print_te(bz,az,tk,nf);
}
void shogi::print_te_no_space(int pack)
{
	int bz,az,tk,nf;
	unpack_te(&bz,&az,&tk,&nf, pack);
	char retp[20];
	change_small(bz,az,tk,nf,retp);
	PRT("%s",retp);
}
void shogi::print_te_width(int pack,int depth, int width)
{
	char retp[20];
	int bz,az,tk,nf;
	unpack_te(&bz,&az,&tk,&nf, pack);
	change_sg(bz,az,tk,nf, depth, retp);
	if ( width ) PRT("%-8s",retp);
}

// 深さによって4文字空白を書くだけ
void print_4tab(int dep)
{
	for (int i=0;i<dep;i++) PRT("    ");
}

// 手番によって先後マークを表示するだけ
void print_teban(int fukasa_teban)
{
	if ( fukasa_teban & 1 ) PRT("▲");
	else                    PRT("△");
}

// 現在までの探索手順を表示する関数（増えてきたのでここでまとめる）
void shogi::print_tejun(void)
{
	int i,*p;
	char retp[20];
#if 1
	int bz,az,tk,nf;
	// 長手数だと表示がぐちゃぐちゃなのでhash_moveで戻してきれいな表示にする。
	// 利きデータはいじらないので影響はない・・・はず。
	for (i=fukasa-1; i >= 0; i--) {	// いったん戻す。
		p=tejun[i];
		if ( p != NULL ) { bz = *(p+0); az = *(p+1); tk = *(p+2); nf = *(p+3); }
		else bz = 0;
		if ( bz == 0 ) ; else if ( bz == 0xff ) rhit_hash(az); else rmove_hash(az,bz,nf,tk);
	}

	for (i=0; i<fukasa; i++) {		// 進める
		p=tejun[i];
		if ( p != NULL ) { bz = *(p+0); az = *(p+1); tk = *(p+2); nf = *(p+3); }
		else bz = az = tk = nf = 0;
		change_sg( bz,az,tk,nf,i,retp );
		PRT("%s",retp);
		if ( bz == 0 ) ; else if ( bz == 0xff ) hit_hash(az,tk); else move_hash(az,bz,nf);
	}
	PRT("\n");

#else

	for (i=0;i<fukasa;i++) {
		p=tejun[i];
#if 1
		if ( p!=NULL ) {
			change_sg(*p,*(p+1),*(p+2),*(p+3),i,retp);
			PRT("%s",retp);
		} else {
			change_sg(0,0,0,0,i,retp);
			PRT("%s",retp);
		}
#else
		if ( p!=NULL ) {
			change_small(*p,*(p+1),*(p+2),*(p+3),retp);
			PRT("%s:",retp);
		} else PRT("PASS        :");
#endif
	}
	PRT("\n");
#endif
}

void shogi::print_path()
{
	int i;
	for (i=fukasa-1; i >= 0; i--) {	// いったん戻す。
		int m = path[i];
		if ( m ) undo_moveYSS((Move)m);
	}

	for (i=0; i<fukasa; i++) {		// 進める
		int m = path[i];
		print_te_no_space(m); PRT(":");
		if ( m ) do_moveYSS((Move)m);
	}
	PRT("\n");
}


#ifdef _DEBUG
int shogi::check_move( int bz, int az, int tk, int nf )
{
	return check_move_sub( bz, az, tk, nf, 1 );
}
int shogi::check_move_hash( int bz, int az, int tk, int nf )
{
//	return 1;
	return check_move_sub( bz, az, tk, nf, 0 );
}

// able_move と大差ないが、手番によるチェックを外す。---> 現在は YSS_KI2.C から呼んでいるだけ！しかもデバッグ用
int shogi::check_move_sub( int bz, int az, int tk, int nf, int kiki_check_flag )
{
	int i,k,n,loop;

	if ( bz == 0xff ) {	// 駒打ちの時
		if ( init_ban[az] != 0 ) return(0);	// 既に駒があるところに打とうとしている。
		if ( tk > 0x80 ) {
			if ( mo_c[tk & 0x0f] == 0 ) return(0);
			if ( tk == 0x81 && nifu_c(az) == 0  ) return(0);
			if ( tk <= 0x82 && az > 0x90 ) return(0);	// 行き所がない
			if ( tk == 0x83 && az > 0x80 ) return(0);
		} else {
			if ( mo_m[tk       ] == 0 ) return(0);
			if ( tk == 0x01 && nifu_m(az) == 0  ) return(0);
			if ( tk <= 0x02 && az < 0x20 ) return(0);	// 行き所がない
			if ( tk == 0x03 && az < 0x30 ) return(0);
		}
		return (1);
	}

	if ( nf == 0x08 && (init_ban[bz] & 0x0f) > 0x08 ) {
		PRT("成り駒がなっています！！！ move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return 0;
	}

	k = init_ban[bz];
	if ( k == 0 ) return(0);	// 盤上にない駒を動かそうとしている。
	if ( tk == 0xff ) return 0;	// 壁に突っ込む。
	if ( init_ban[az] != tk	) {
		PRT("盤上にない駒を取ろうとしている！\n");
/*	
hyouji();
PRT("bz,az,tk,nf=%x,%x,%x,%x :read_max=%d \n",bz,az,tk,nf,read_max );
print_tejun();
PRT("hash_code1,2 = %x,%x hash_motigoma = %x\n",hash_code1,hash_code2,hash_motigoma);
*/
		return(0);
	}

	n = ban[bz];	// 駒番号

	if ( k > 0x80 ) {	// COM の駒
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return(0);	// 成駒と王と金は成れない
			if ( az < 0x70 && bz < 0x70 ) return(0);	// 成れない。
		} else {
			if ( k == 0x83 && az > 0x80 ) return(0);	// 不成は許されない。
			if ( k <= 0x82 && az > 0x90 ) return(0);	// 不成は許されない。
		}

		if ( tk > 0x80 ) return (0);	// 自分の駒を取っている。

		if ( kiki_check_flag == 0 ) return 1;	// 利きのデータをチェックしない場合
		loop = kiki_c[az];
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return(1);	// OK! 動かせる！
		}
	} else {
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return(0);	// 成駒と王と金は成れない
			if ( az > 0x40 && bz > 0x40 ) return(0);	// 成れない。
		} else {
			if ( k == 0x03 && az < 0x30 ) return(0);	// 不成は許されない。---> なぜかコメントにしていた。理由は？
			if ( k <= 0x02 && az < 0x20 ) return(0);	// 不成は許されない。---> なぜかコメントにしていた。理由は？
		}

		if ( tk && tk < 0x80 ) return (0);	// 自分の駒を取っている。

		if ( kiki_check_flag == 0 ) return 1;	// 利きのデータをチェックしない場合
		loop = kiki_m[az];
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return(1);
		}
	}
	return (0);	// 動けない
}
#endif
