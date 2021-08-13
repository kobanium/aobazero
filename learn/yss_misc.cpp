// 2019 Team AobaZero
// This source code is in the public domain.
// yss_misc.cpp
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "yss_ext.h"     /**  load extern **/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss.h"		// ���饹�����

HASH sennitite[KIFU_MAX];		/* �ϥå���ǰ����ζ��̤򵭲� */

#ifdef SMP
shogi          local[MAX_BLOCKS+1];	// õ���ǻȤ����饹���Ρ�
#else
shogi          local[1];			// õ���ǻȤ����饹���Ρ�
#endif

shogi *PS = &local[0];	// ��ʸ����PS�ϥ����Х�ǻȤ���



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
		if ( i==2 && z == 0x5d ) continue;	// �;���
		ks[ k & 0x07 ]++;
		if ( ban[z] != i || init_ban[z] != k ) {
			print_kn();
			hyouji();
			PRT("check_kn(%x) Error !!! tesuu=%d fukasa=%d\n",i,tesuu,fukasa);
			PRT("ban[%x]=%02x, init_b[%x]=%x\n",z,ban[z],z,init_ban[z]);
			print_kb();
			debug();
		}
	}
	for (i=1;i<8;i++) {
		k = mo_c[i]+mo_m[i]+ks[i];
		if ( mo_c[i]<0 || mo_m[i]<0 || k < 0 || (i==1 && k>18) ||
		 ( (i==2||i==3||i==4||i==5) && k>4 ) || ( (i==6||i==7) && k>2 ) ) {
			PRT("mo_ Error !\n");
			debug();
		}
	}
}

void shogi::clear_kb_kiki_kn()
{
#if 1
	// �㴳®��
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

// ��������ȿž�����̤�ȿž�����ƥϥå������ľ����
void shogi::hanten_with_hash(void)
{
	hanten();
	hash_init();
}

// ������ȿž�����롣�ճ���������ʬ���ä����򤤤��⡣
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

/*** ���̤������֤��᤹�ʶ�����դ��� ***/
/*** ��꤬��COM¦������Ȥ�            ***/
/*** 0...ʿ�ꡢ1...���2...���3...���4...���硢5...���硢6...���� ***/
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

//	PRT("n=%d\n",n);
	if ( n == 7           )   init_ban[0x99] = 0;						// �����ä�
	if ( n == 6           ) { init_ban[0x92] = 0; init_ban[0x98] = 0; }	// ���Ϥ�ä�
	if ( n >= 5 && n <= 6 ) { init_ban[0x91] = 0; init_ban[0x99] = 0; }	// ��֤�ä�	
	if ( n >= 4 && n <= 6 ) { init_ban[0x82] = 0; init_ban[0x88] = 0; }	// ���Ѥ�ä�
	if ( n == 3           )   init_ban[0x88] = 0;						// ���֤�ä�
	if ( n == 2           )   init_ban[0x82] = 0;						// �ѡ���ä�
	if ( n == 1           )   init_ban[0x91] = 0;						// �����ä�

	init();			/** kn[] ni kakikomu **/
	allkaku();
	tesuu = all_tesuu = 0;
}


// ���ߤλ��������̤ξ���(ban_init)�򸵤����̾��֤�ƹ������롣
void shogi::ban_saikousei()
{
	clear_kb_kiki_kn();

	init();			/** kn[] ni kakikomu **/
	allkaku();
	tesuu = all_tesuu = 0;
}

// ʿ������̤�
int shogi::is_hirate_ban()
{
	int z;
	
	for (z=0x11; z<0x9a; z++) if ( init_ban[z] != hirate_ban[z] ) break;
	if ( z == 0x9a ) return 1;	// ʿ��
	return 0;
}

void shogi::hanten_with_hash_kifu()
{
	hanten_with_hash();
	
	int i;
	for (i=0;i<all_tesuu;i++) {
		int *p = &kifu[i+1][0];
		int bz = *(p+0), az = *(p+1), tk = *(p+2), nf = *(p+3);
		hanten_sasite(&bz,&az,&tk,&nf);	// �ؤ�������ȿž
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

int get_localtime(int *year, int *month, int *day, int *week, int *hour, int *minute, int *second)
{
	time_t timer = time(NULL);				/* ����μ��� */
	struct tm *tblock = localtime(&timer);	/* ���ա������¤�Τ��Ѵ����� */

	*year = *month = *day = *week = *hour = *minute = *second = 0;	// ��������Ƥ�����

	if ( tblock == NULL ) { fprintf(stderr,"localtime() == NULL!\n"); return 0; }
	*second = tblock->tm_sec;			/* �� */
	*minute = tblock->tm_min;			/* ʬ */
	*hour   = tblock->tm_hour;			/* �� (0��23) */
	*day    = tblock->tm_mday;			/* ������̤����� (1��31) */
	*month  = tblock->tm_mon + 1;		/* �� (0��11) */
	*year   = tblock->tm_year + 1900;	/* ǯ (���� - 1900) */
	*week   = tblock->tm_wday;			/* ���� (0��6; ���� = 0) */
	return (int)timer;
}

// msec ñ�̤ηв���֤��֤���clock()��linux�Υޥ������åɤǤϤ��������ʤ롣�ץ������Τξ�����֤ʤΤ�2�ܤλ��֤ˡ�
int get_clock()
{
#if defined(_MSC_VER)
	if ( CLOCKS_PER_SEC_MS != CLOCKS_PER_SEC ) { PRT("CLOCKS_PER_SEC=%d Err. not Windows OS?\n"); debug(); }
	return clock();
//	return GetTickCount();
#else
	struct timeval  val;
	struct timezone zone;
	if ( gettimeofday( &val, &zone ) == -1 ) { PRT("time err\n"); debug(); }
//	return tv.tv_sec + (double)tv.tv_usec*1e-6;
	return val.tv_sec*1000 + (val.tv_usec / 1000);
#endif
}

double get_spend_time(int ct1)
{
	return (double)(get_clock()+1 - ct1) / 1000.0;	// +1��Ĥ���0�ˤʤ�ʤ��褦�ˡ�
}


// ����������2�Х��Ȥ�4�Х��Ȥ��Ѵ���num�ϸ��ߤμ����num=0����Ϥޤ롣
void shogi::trans_4_to_2_KDB(int b0,int b1, int num, int *bz,int *az, int *tk, int *nf)
{
	int k;
//	b0 = p->kifu[i][0];			  // b0 = 11-99, 0x81-0x87
//	b1 = p->kifu[i][1];
	*bz = *az = *tk = *nf = 0;
		
	if ( b0 > 0x80 ) {	// ���Ǥ�
		*tk = b0 - 0x80;
		if ( (num & 1) ) *tk |= 0x80;	// COM
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

// KDB�����ʴ��������ˤΰ��̴�����Ѵ���
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
lock_t          lock_io;	// PRT()��ɽ����
pthread_attr_t  pthread_attr;
pthread_t       p_thread[CPUS];		// ����åɤΥϥ�ɥ�
int             lock_io_init_done = 0;

void InitLockYSS()
{
	static int fDone = 0;
	if ( fDone ) return;
	fDone = 1;

//	LockInit(lock_smp);
//	LockInit(lock_root);
	LockInit(lock_io);		// PRT()�Τ������yss_win.cpp�ǽ�������Ƥ롣2�ٽ�����ϴ�����
//	LockInit(lock_fish_alloc);
	
	lock_io_init_done = 1;	// fish�ǥ��饹�������ݤ˥��󥹥ȥ饯����PRT()���ƤФ��Τ��ɻ�
}



void shogi::hyouji()
{
   int x,y,i;
   PRT("    1 2 3 4 5 6 7 8 9  \n");
   for (y=0;y<9;y++) {
	  PRT("%d��",y+1);
	  for (x=0;x<9;x++) {
		 int n = ban[ (y+1)*16+x+1 ];
		 int k = kn[n][0];
		 if (k>0x80) k-=0x70;
//		 PRT("%s",koma[i]);
		 PRT("%s",koma_kanji[k]);
	  }  PRT("��");
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

	if ( sName1 ) func_prt("��ꡧ%s\r\n",sName1);
	func_prt("���λ���");
	for (i=7;i>0;i--) {
		if ( (k = mo_c[i]) == 0 ) continue;
		func_prt(koma_kanji[i]);
		if ( k > 1 ) func_prt(num_kanji[k]);
		func_prt("��");
	}
	func_prt("\r\n");
	func_prt("  �� �� �� �� �� �� �� �� ��\r\n");
	func_prt("+---------------------------+\r\n");
	for (y=1;y<=9;y++) {
		func_prt("|");
		for (x=1;x<=9;x++) {
			z = y*16+x;
			k = init_ban[z];
			if ( k == 0 ) {
				func_prt(" ��");
				continue;
			}
			if ( k < 0x80 ) func_prt(" %s",koma_kanji[k]);
       		else            func_prt("v%s",koma_kanji[k&0x0f]);
		}
		func_prt("|%s",num_kanji[y]);
		if (y==5) {
			func_prt("   ���=%d,IsSenteTurn()=%d",tesuu,IsSenteTurn());
			if ( tesuu > 0 ) {
				char tmp[TMP_BUF_LEN];
				change_sg( kifu[tesuu][0],kifu[tesuu][1],kifu[tesuu][2],kifu[tesuu][3], (IsSenteTurn()+1)&1, tmp );
				func_prt("  %s",tmp);
			}
		}
		func_prt("\r\n");
	}
	func_prt("+---------------------------+\r\n");

	func_prt("���λ���");
	for (i=7;i>0;i--) {
		if ( (k = mo_m[i]) == 0 ) continue;
		func_prt(koma_kanji[i]);
		if ( k > 1 ) func_prt(num_kanji[k]);
		func_prt("��");
	}
	func_prt("\r\n");
	if ( sName0 ) func_prt("��ꡧ%s\r\n",sName0);
	// ���֤����
	if ( all_tesuu ) {
		int bz = kifu[1][0];
//		int az = kifu[1][1];
		if ( bz!=0xff && init_ban[bz] > 0x80 ) func_prt("�����\r\n");
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
	WaitKeyInput();	// Windows�Ѥ�MessageBox��ɽ�����롣
#endif
	exit(1);
}

void shogi::debug()
{
	int i;
	PRT_ON();
	PRT("debug()!!! tume_hyouka=%d,����=%d\n",tume_hyouka,kyokumen);
	PRT("��=%d,RM=%d,fukasa_add=%d, ���=%d\n",fukasa,read_max,fukasa_add,tesuu);

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

// ���饹�ǤϤʤ����顼
void debug()
{
	PRT_ON();
	PRT("Non Class Error!\n");
	PS->hyouji();
	debug_exit();
}


// EUC����SJIS�ؤ��Ѵ�
void euc2sjis(unsigned char *c1, unsigned char *c2) {
	// �裲�Х��Ȥ��Ѵ�
	if( ( *c1 % 2 ) == 0 ) *c2 -= 0x02;
	else {
		*c2 -= 0x61;
		if ( *c2 > 0x7e ) ++ *c2;
	}

	// �裱�Х��Ȥ��Ѵ�
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

// ʸ�����EUC����SJIS��
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

// S-JIS����EUC�ؤ��Ѵ��ץ���� 
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

// ��������Ƭ�Х��Ȥ���Ƚ����ˤ� char �� unsigned �ˤ��Ƥ��뤳�ȡ�
int IsKanji(unsigned char c)
{
	return (( 0x80<(c) && (c)<0xa0 ) || ( 0xdf<(c) && (c)<0xfd ));
}

// ʸ�����SJIS����EUC��
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



/*** able_move ���λؤ��꤬����ǽ�ʾ��ϣ��ʳ����֤� *******************/
/*** man ,com ���� ������������ �Ǥ���ͤ��̵�� ***/
int shogi::able_move( int bz, int az, int tk, int nf )
{
	if ( bz == 0xff ) {	// ���Ǥ��λ�
		if ( init_ban[az] != 0 ) return(0);	// ���˶𤬤���Ȥ�����ǤȤ��Ȥ��Ƥ��롣
		if ( tk > 0x80 ) {
			if ( (fukasa & 1) == 1 ) {
				PRT("MAN �μ��֤� COM �ζ���ǤȤ��Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��

				PRT("bz,az,tk,nf=%x,%x,%x,%x :(%d/%d)\n",bz,az,tk,nf,fukasa,read_max );
				print_tejun();
				{int i,*p,ret;	ret = ret_fukasa[fukasa];		// ɾ���ͤ����ꤷ��������
					for ( i=fukasa+0; i<=ret; i++ ) {	// = ���դ�
						p = saizen[fukasa][i];	bz = *(p+0); az = *(p+1); tk = *(p+2); nf = *(p+3);	PRT("%x,%x,%x,%x: :",bz,az,tk,nf);
					}
					PRT("\n");
				}

				return(0);
			}
			if ( mo_c[tk & 0x0f] == 0 ) return(0);
			if ( tk == 0x81 && nifu_c(az) == 0  ) return(0);
			if ( tk <= 0x82 && az > 0x90 ) return(0);	// �Ԥ��꤬�ʤ�
			if ( tk == 0x83 && az > 0x80 ) return(0);
		} else {
			if ( (fukasa & 1) == 0 ) {
				PRT("COM �μ��֤� MAN �ζ���ǤȤ��Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
				return(0);
			}
			if ( mo_m[tk       ] == 0 ) return(0);
			if ( tk == 0x01 && nifu_m(az) == 0  ) return(0);
			if ( tk <= 0x02 && az < 0x20 ) return(0);	// �Ԥ��꤬�ʤ�
			if ( tk == 0x03 && az < 0x30 ) return(0);
		}
		return (1);
	}

	int k = init_ban[bz];
	if ( k == 0 ) return(0);	// �׾�ˤʤ����ư�������Ȥ��Ƥ��롣

	if ( nf == 0x08 && (k & 0x0f) > 0x08 ) {
//		PRT("����𤬤ʤäƤ��롪 move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return 0;
	}

	if ( init_ban[az] != tk	) {
//		PRT("�׾�ˤʤ�������Ȥ��Ƥ��롪\n");
/*	
hyouji();
PRT("bz,az,tk,nf=%x,%x,%x,%x :read_max=%d \n",bz,az,tk,nf,read_max );
print_tejun();
PRT("hash_code1,2 = %x,%x hash_motigoma = %x\n",hash_code1,hash_code2,hash_motigoma);
*/
		return(0);
	}

	int n = ban[bz];	// ���ֹ�

	if ( k > 0x80 ) {	// COM �ζ�
		if ( (fukasa & 1) == 1 ) {
//			PRT("MAN �μ��֤� COM �ζ��ư�������Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
			return(0);
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return(0);	// ����Ȳ��ȶ������ʤ�
			if ( az < 0x70 && bz < 0x70 ) return(0);	// ����ʤ���
		} else {
			if ( k == 0x83 && az > 0x80 ) return(0);	// �����ϵ�����ʤ���
			if ( k <= 0x82 && az > 0x90 ) return(0);	// �����ϵ�����ʤ���
		}
	
		if ( tk > 0x80 ) return (0); // ��ʬ�ζ���äƤ��롣

		int i, loop = kiki_c[az];
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return(1);	// OK! ư�����롪
		}
	} else {
		if ( (fukasa & 1) == 0 ) {
//			PRT("COM �μ��֤� MAN �ζ��ư�������Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
			return(0);
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return(0);	// ����Ȳ��ȶ������ʤ�
			if ( az > 0x40 && bz > 0x40 ) return(0);	// ����ʤ���
		} else {
			if ( k == 0x03 && az < 0x30 ) return(0);	// �����ϵ�����ʤ���---> �ʤ��������Ȥˤ��Ƥ�������ͳ�ϡ�
			if ( k <= 0x02 && az < 0x20 ) return(0);	// �����ϵ�����ʤ���---> �ʤ��������Ȥˤ��Ƥ�������ͳ�ϡ�
		}

		if ( tk && tk < 0x80 ) return (0);	// ��ʬ�ζ���äƤ��롣

		int i,loop = kiki_m[az];
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return(1);
		}
	}
	return (0);	// ư���ʤ�
}

// �Ȥꤢ��������ǽ������Ƚ�ꡣ�ؤ�����ޤǤ��Τ��
bool shogi::is_pseudo_legalYSS(Move m, Color sideToMove)
{
	int bz = get_bz(m);
	int az = get_az(m);
	int tk = get_tk(m);
	int nf = get_nf(m);

	if ( bz == 0xff ) {	// ���Ǥ��λ�
		if ( init_ban[az] != 0 ) return false;	// ���˶𤬤���Ȥ�����ǤȤ��Ȥ��Ƥ��롣
		if ( az > 0x99 || (az&0x0f) > 9 ) return false; // 
		if ( nf ) return false;
		if ( tk > 0x80 ) {
			if ( tk > 0x87 ) return false;
			if ( sideToMove == WHITE ) {
//				PRT("MAN �μ��֤� COM �ζ���ǤȤ��Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
				return false;
			}
			if ( mo_c[tk & 0x0f] == 0 ) return false;
			if ( tk == 0x81 && nifu_c(az) == 0 ) return false;
			if ( tk <= 0x82 && az > 0x90 ) return false;	// �Ԥ��꤬�ʤ�
			if ( tk == 0x83 && az > 0x80 ) return false;
		} else {
			if ( tk == 0 || tk > 0x07 ) return false;
			if ( sideToMove == BLACK ) {
//				PRT("COM �μ��֤� MAN �ζ���ǤȤ��Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
				return false;
			}
			if ( mo_m[tk       ] == 0 ) return false;
			if ( tk == 0x01 && nifu_m(az) == 0 ) return false;
			if ( tk <= 0x02 && az < 0x20 ) return false;	// �Ԥ��꤬�ʤ�
			if ( tk == 0x03 && az < 0x30 ) return false;
		}
		return true;
	}

	if ( bz == az ) return false;
	if ( az > 0x99 || (az&0x0f) > 9 ) return false;
	if ( bz > 0x99 || (bz&0x0f) > 9 ) return false;
	if ( tk == 0xff ) return false;
	
	const int k = init_ban[bz];
	if ( k == 0 || k == 0xff ) return false;	// �׾�ˤʤ����ư�������Ȥ��Ƥ��롣

	if ( nf != 0 && nf != 0x08 ) return false;
	
	if ( nf == 0x08 && (k & 0x0f) > 0x07 ) {
//		PRT("����𤬤ʤäƤ��롪 move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return false;
	}

	if ( init_ban[az] != tk	) {
//		PRT("�׾�ˤʤ�������Ȥ��Ƥ��롪\n");
		return false;
	}

	const int n = ban[bz];	// ���ֹ�
	if ( kn[n][0] != k  ) return false;
	if ( kn[n][1] != bz ) return false;

	if ( k > 0x80 ) {	// COM �ζ�
		if ( sideToMove == WHITE ) {
//			PRT("MAN �μ��֤� COM �ζ��ư�������Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
			return false;
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return false;	// ����Ȳ��ȶ������ʤ�
			if ( az < 0x70 && bz < 0x70 ) return false;	// ����ʤ���
		} else {
			if ( k == 0x83 && az > 0x80 ) return false;	// �����ϵ�����ʤ���
			if ( k <= 0x82 && az > 0x90 ) return false;	// �����ϵ�����ʤ���
		}
	
		if ( tk > 0x80 ) return false; // ��ʬ�ζ���äƤ��롣

		const int loop = kiki_c[az];
		int i;
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return true;	// OK! ư�����롪
		}
	} else {
		if ( sideToMove == BLACK ) {
//			PRT("COM �μ��֤� MAN �ζ��ư�������Ȥ��Ƥ��롪�ϥå����Ʊ��뤫����\n");	// ErrorȽ��
			return false;
		}
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return false;	// ����Ȳ��ȶ������ʤ�
			if ( az > 0x40 && bz > 0x40 ) return false;	// ����ʤ���
		} else {
			if ( k == 0x03 && az < 0x30 ) return false;	// �����ϵ�����ʤ�
			if ( k <= 0x02 && az < 0x20 ) return false;	// �����ϵ�����ʤ�
		}

		if ( tk && tk < 0x80 ) return false;	// ��ʬ�ζ���äƤ��롣

		const int loop = kiki_m[az];
		int i;
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return true;
		}
	}
	return false;	// ư���ʤ�
}

// �Ȥꤢ��������ǽ�ʼ꤬���ؤ�����˲�����ȴ����ʤ���
bool shogi::is_pl_move_is_legalYSS(Move /*m*/, Color /*sideToMove*/)
{
	return true;
/*
	int bz = get_bz(m);
	int az = get_az(m);
	int tk = get_tk(m);
	int nf = get_nf(m);
	
	// �����������ʤ� -> ���̤Ϥ��ꤨ��ʡ�����
	
	// ���꤬�����äƤ�������ɤ��ʤ�
	PRT("is_pl_move_is_legalYSS");

	if ( bz == 0xff ) {	// ���Ǥ��λ�
		return true;
	}
	return true;
*/
}

static unsigned short move_is_check_table[81][81];
static unsigned short move_is_tobi_check_table[81][81];

int shogi::is_move_attack_sq256(Move m, Color sideToMove, Square256 attack_z)
{
	// ���������̵�롣��ư��ζ�ΰ��֤�Ũ�������뤫�ɤ����Τߤ�Ĵ�٤롣���ѹ��Υ�첦���̵�롣
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
	// 99%�ϲ��꤫�ɤ����ǸƤФ�롣node��5�ܤ�ƤФ�롣���ʤ�¿����
//	{ static int sum,all; all++; if ( (init_ban[teki_ou_z] & 0x0f)==0x08 ) sum++; if ( (all&0xffff)==0 ) PRT("%d/%d\n",sum,all); }
#if 0
	int k7 = k & 0x07;
	if ( k==0x02 || k7==0x06 || k7==0x07 ) {	// k=2,6,7,14,15
		int k3 = k7 - 2;
		k3 = (k3>>2) + (k3&1); // 2,6,7 -> 0,4,5 -> 0,1,2        10 110 111      00 100 101
		int tc = (move_is_tobi_check_table[ou_z81][az81] >> (k3*5)) & 0x1f;
		if ( tc ) {
			int n  = tc & 0x07;
			int dz = z8[((tc>>3) & 0x03)*2+(k3&1)];  // (���� 2bit, ��Υ 3bit) x3 ���ѹ�
			dz = dz*( 1 + (sideToMove == BLACK)*(-2));
			int z = teki_ou_z + dz;	
//			{ print_teban(!sideToMove); print_te(bz,az,tk,nf); PRT("����=%04x(%2x),k3=%d,n=%d,dz=%+3d,z=%02x\n",tc,teki_ou_z,k3,n,dz,z); }
			int i;
			for (i=0;i<n;i++,z+=dz) if ( init_ban[z] ) return 0;
//			PRT("����\n"); hyouji();
			return 1;	
		}		
	}
#endif
//	if ( sideToMove	== BLACK ) { hyouji(); print_te(bz,az,tk,nf); PRT("����=%d\n",(move_is_check_table[ou_z81][az81] >> k) & 1); }
	return (move_is_check_table[ou_z81][az81] >> k) & 1;
}


void make_move_is_check_table()
{
//	PRT("\n\nư���ǧ��\n\n");
	memset(move_is_check_table, 0, sizeof(move_is_check_table));
	int x,y;
	for (y=0;y<9;y++) for (x=0;x<9;x++) {
		int ou_z = (y+1)*16+(x+1);
		int i;
		for (i=0;i<8;i++) {
			int az   = ou_z + oute_z[0][i];		// com���˲���򤫤���
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
			for (az+=dz,n=0; n<7; n++,az+=dz) {		// ���ܤ�����֤����Ф�
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














#define REHASH_MAX (2048*1)		// �ƥϥå���κǹ����ʼ�����Ͽ�ˤ�Ȥ���5���ܤ�̵��̣�ơ��֥�Ǥ⡣593�ʾ塣

int rehash[REHASH_MAX-1];		// �Х��åȤ����ͤ����ݤκƥϥå����Ѥξ�����������ǡ���
int rehash_flag[REHASH_MAX];	// �ǽ�˺������뤿���

int hash_use;
int hash_over;			/* ���κƥϥå�����ٷ����֤����� */

static int seed;			// ����μ����͡�1 ���� 2^31-1 �ޤǡ�
int rand_yss_enable = 1;	// �������������(����)���׹ͤμ¸��������ߤ᤿�����ˡ��׹ͤ�����ľ����OFF�ˤ��롣

// �ϥå���������ơ��֥�ν������
void hash_ban_init(void)
{
	int p,i,j,k;
	int f = 0,f_lo,f_hi;

	/* �¹Ԥ��뤿�Ӥ˰㤦�ͤ�������褦�ˡ����ߤλ����ͤ�Ȥä���������ͥ졼�����������ޤ���*/
//	srand( (unsigned)time( NULL ) );

	seed = 1;
//	seed = 1043618065;
	
//	init_rnd(1);	// �ͷ�������ν���� seed ��Ȥ�

	for (j=0;j<1;j++) {
		p =0;
//		k0 = k1 = 0;
		for (i=0;i<10000;i++ ) {
			// �����Ʊˡ�ϸġ��ΥӥåȤ򸫤�ȡ����Ф��ꡢ�Ȥ�������¿����-> �������̥ӥåȤ��������������������OK��
			f = rand_yss();			// 31 bit �ʤΤ� 0x80000000 �Ͼ�ˣ�
			if ( (i&1) && (f & 1) ) p++;
		}
//		PRT("\n0=%d, 1=%d\n",k0,k1);	
//		PRT("%6d: seed = %11d, random=0x%08x ,p=%d \n",i*(j+1),seed,f,p);
	}

//	PRT("10000��Υ롼�פ� seed = 1043618065 �ʤ�����������\n");

	if ( seed != 1043618065 ) PRT("���Υ����ƥ�δĶ��Ǥ��������������������Ƥ��ޤ���int �� 32bit �ʾ��ɬ�פ�����ޤ���\n");
//	PRT("UseMemory= %d MB, Hash_Table_Size=%d, Hash_Stop=%d, Hash_Byte=%d \n",(Hash_Table_Size * sizeof(hash_table[0]))>>20, Hash_Table_Size, Hash_Stop, sizeof(hash_table[0]) );
	
	///////////////////////////////////////////////////////////////

	for (i=0; i<32; i++) {
		for (j=0; j<BAN_SIZE; j++) {
			for (k=0; k<2; k++) {

/*
				// �Ǿ�̥ӥåȤ���Ф��ƣ����Ĥʤ�٤ư�Ĥ��������
				// �Ĥ��Ǥ˥����ƥ������ä��Ƥߤ� ---> �ϥå���Ʊ�줷�ޤ��ꡪ
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
				if ( f_hi &          1 ) f += 0x80000000;	// ����ʤ�Ǿ�̥ӥåȤ�Ω�Ƥ롣�줷ʶ�졣
//				if ( f_hi & 0x40000000 ) f += 0x80000000;	// �Ǿ�̥ӥåȤ�ä��롩

//				PRT("%08x:",f); int m; for (m=0;m<32;m++) PRT("%d",(f>>(31-m))&1);	PRT("\n");	// 2�ʿ���ɽ��
//				PRT(" %08x,",f);
				hash_ban[i][j][k] = f;
			}
		}
	}

/*
	// Ʊ��ο��������뤫�����å�
	for (i=0; i<32; i++) for (j=0; j<BAN_SIZE; j++) for (k=0; k<2; k++) {
		unsigned int ff = hash_ban[i][j][k];
		int p,q,r;
		for (p=0; p<32; p++) for (q=0; q<BAN_SIZE; q++) for (r=0; r<2; r++) {
			if ( ff == hash_ban[p][q][r] && i!=p && j!=q && r!=k ) PRT("Ʊ��!!!!!!!!!!\n");
		}
	}
*/
	// �ƥϥå�����������
	i = 0;
	rehash_flag[0] = 1;	// 0 �ϻȤ�ʤ�
	for ( ;; ) {
		k = (rand_yss() >> 8) & (REHASH_MAX-1);
		if ( rehash_flag[k] ) continue;	// ������Ͽ�Ѥ�
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
	seed += timer;	// ��������ñ��Ǥ褷��
	rand_yss();	// seed ��������
	PRT("rand_seed =%11d\n",seed );
//	PRT("���ߤηв��ä� %ld \n",time(NULL));
}

int print_time()
{
	int year,month,day,week,hour,minute,second;
	int timer = get_localtime(&year, &month, &day, &week, &hour, &minute, &second);
	PRT("%4d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
	return timer;
}


// �����Ʊˡ�ˤ����������������ξ�硢1 ���� 2^31 - 2 �ޤǤ�����������롣������ 2^31 -1 �Ǥ��롣 
// bit 1993-04 ��ꡣ������������ȯ����int �� 32bit �ʾ�ʤ�������ư��롣
int rand_yss()
{
	static const int a = 16807;				// 7^5
	static const int m = 2147483647;		// 2^31 - 1;
	static const int q = 127773;			// 3^2 * 14197;
	static const int r = 2836;

	if ( rand_yss_enable == 0 ) { PRT("rand_yss()=0\n"); return 0; }	// ���ȯ������ߡ�

#if 0	// 46bit�ʾ夢��Ķ��ǤϤ����OK�� 32bit�Ķ��Ǥ�2���٤���
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
	return( seed );		// ������ seed �򤽤Τޤ��֤��Ƥ�����--->���������餤���Τ�
}


// �ͷ�������ˤ����ˡ�ʥ��르�ꥺ�༭ŵ����¼��ɧ�����˼��� 2^521 - 1 ��¾����ʬ�ۤˤ�ͥ�줿��ˡ
#define M32(x)  (0xffffffff & (x))
static int rnd521_count = 521;	// �����˺��ξ��Υ����å���
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
			u = ( u >> 1 ) | (u_seed & (1UL << 31));	// �Ǿ��bit����32�ļ��Ф��Ƥ���
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
		if ( rnd521_count == 522 ) init_rnd521(4357);	// ��������Ƥʤ��ä�����Fail Safe
		rnd521_refresh();
		rnd521_count = 0;
	}
	return mx521[rnd521_count];
}

void shogi::hash_init(void)
{
	int k,z;

	koma_koukan_table_init();	// ��򴹤򤷤���������򡢴��ܤζ�β��ͤ�����ľ����

	// YSS�����ǡ����̤�ȿž�����Ȥ����������оΤΰ��֤ˤ�����ư�����С�
	// XOR�Ǹ��Υϥå����ͤ���äƤ��ޤ����ʹ֤�78��Ȼؤ�������32��Ȼؤ��С�
	// ����Τߤ���ϥå����ͤ���ľ�������֤�̵�뤷�Ƥ⤤�����Ϥ���
	// ���Υϥå����ͤϡ����ȿž���Ƥ��ʤ����֡ʼ��־����ޤޤʤ���
	hash_code1 = 0x00000000;
	hash_code2 = 0x00000000;

	for (z=0x11;z<0x9a;z++) {
		k = init_ban[z];
		if ( k==0 || k==0xff ) continue;

		if ( k > 0x80 ) k -= 0x70;	// ���� 0x01-0x8f -> 0x01-0x1f
		HashXor(k,z);
	}
}


// ���Υϥå���Ʒ׻����Ѵؿ���YSS�Ǥ��Ѥ��롣From 98/05/01
// ���δؿ������Ƥ��뤳��
// ���̤����ƤϤ��Τޤޤˡ��ϥå����ͤΤߤ�����ȿž���֡��ޤ��ϡ��ӥå�ȿž�ʼ��ָ���ˤ�Ԥ���
// YSS�Ǥϡ�sennitite[] ����Ͽ������Τߤ˻��ѡ�YSS_WIN.C �򻲾ȡ� YSS�Ǥϡ����complement�ϣ�(���֤Ϥ��Τޤޡˤ�

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

	// �ϥå��女�������Τ��ľ�����׾�˲���𤬤ʤ��Ȥ������Ȥ��롣
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
	hash_calc( tesuu & 1, 0 );	// �ϥå����ͤ���ľ���ʼ��=1��ȿž�����
	HASH *psen = &sennitite[tesuu];
	psen->hash_code1 = hash_code1;		// ���̤򵭲� 
	psen->hash_code2 = hash_code2;		// ���̤򵭲� 
	psen->motigoma   = hash_motigoma;
	psen->flag       = ((tesuu&1)==0)*kiki_c[kn[1][1]] + ((tesuu&1)==1)*kiki_m[kn[2][1]];	// ���꤫�ɤ���
}









char *get_str_zahyo(int z)
{
	static const char *suuji[10] = {
	  "��","��","��","��","��","��","��","��","��","��"
	};
	static const char *moji[10] = {
	  "  ","��","��","��","��","��","ϻ","��","Ȭ","��"
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
	  "  ","��",  "��",  "��",  "��","��","��","��",
	  "��","��","����","����","����","  ","��","ζ"
	};
	if ( n < 0 || n >= 16 ) debug();
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
		if ( nf ) strcat(retp,"��");

		strcpy(p,"(00)");
		p[1]+=bx;
		p[2]+=by;
		strcat(retp,p);
		if ( nf==0 && ( k<0x0a || k>0x0d ) ) strcat(retp,"  ");
	}
	else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "��    ");
	}
}

void shogi::change(int *p,char retp[])
{
	change(*(p+0),*(p+1),*(p+2),*(p+3),retp);
}

/*** û���Ѵ����� ***/
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
	if ( bz < 0 || bz > 0xff || az <= 0 || az > 0x99 || nf < 0 || nf > 0x08 || tk < 0 || tk > 0x8f ) { PRT("Err bz,az,tk,nf=%02x,%02x,%02x,%02x\n",bz,az,tk,nf); debug(); }

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
		if ( nf ) strcat(retp,"��");

		strcpy(p,"(00)");
		p[1]+=bx;
		p[2]+=by;
		strcat(retp,p);
//		if ( nf==0 && ( k<0x0a || k>0x0d ) ) strcat(retp,"  ");
	} else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "��");
	}
}

// ��76�⡢��ɽ�����Ѵ����롣���� "��53����(42)" ��12ʸ����retp ��13ɬ��
void shogi::change_sg(int bz,int az,int tk,int nf,int depth, char retp[])
{
	static const char *suuji[10] = {
	  "0","1","2","3","4","5","6","7","8","9"
	};
	int bx,by,ax,ay,k;

	if ( depth & 1 ) sprintf(retp,"��");
	else             sprintf(retp,"��");

	if ( bz == 0 ) {
		strcat(retp,"PASS");
		return;	
	}
	if ( bz < 0 || bz > 0xff || az <= 0 || az > 0x99 || nf < 0 || nf > 0x08 || tk < 0 || tk > 0x8f ) { PRT("Err sg bz,az,tk,nf=%02x,%02x,%02x,%02x\n",bz,az,tk,nf); debug(); }

	ay = az/16;
	ax = 10-(az-ay*16);
	by = bz/16;
	bx = 10-(bz-by*16);
	k = 0;	// ư������bz�˶𤬤ʤ�����az�򸫤�
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

		// ��������ľ��������Ĥ��롣
		// ��ư������ˡ�Ʊ������ζ�2�������Ƥ����顣
		for (;;) {
			int i,n,dep,sum,loop,sk[32];
	//		static char *sayu[4] = { "��","��","ľ","��" };
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
			// sk[0] �˼ºݤ˻ؤ�����sk[1]����Ӥ�����
#if 1
			strcat(retp,"(");
			strcat(retp,suuji[bx]);
			strcat(retp,suuji[by]);
			strcat(retp,")");
#else
			dx = (sk[0] & 0x0f) - (sk[1] & 0x0f);
			dy = (sk[0] & 0xf0) - (sk[1] & 0xf0);
			if ( dx ) {	// ��������ͥ�衣
				if ( dx > 0 ) strcat(retp, sayu[1-dep]);
				else          strcat(retp, sayu[  dep]);
			} else {
				if ( dy > 0 ) strcat(retp, sayu[2+1-dep]);
				else          strcat(retp, sayu[2+  dep]);
			}
#endif
			break;
		}

		if ( nf ) strcat(retp,"��");
	} else {
		strcat(retp, koma_kanji_str(tk & 0x07) );
		strcat(retp, "��");
	}
}

void shogi::change_log_pv(char *str)
{
	// ** 121 -4445FU +6766FU -6364FU +8899OU -5263KI +5867KI -7374FU +7988GI -8173KE +6979KI
	// ** 121 ��45�⢥66��...         ��45���� ... �Ǥ�¿����Ķ���롣����ʤ���(+8899OU-5263KI)
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

				const char *sSG[2] = { "��","��" };
				char ss[16];
				sprintf(ss,"%s%d%d%s",sSG[*(p+0)=='-'],ax,ay,koma_kanji_str(j));
				if ( bx==0 ) strcat(ss,"��");
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

// ���ɽ���������
void shogi::print_te(int bz, int az,int tk,int nf)
{
	char retp[20];
	change_small(bz,az,tk,nf,retp);
	PRT("%s ",retp);
}
// �ϥå���μ��ɽ����
void shogi::print_te(HASH_RET *phr)
{
	print_te(phr->bz, phr->az, phr->tk, phr->nf);
}
// sasite[]�ʤɤμ��ɽ����
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

// �����ˤ�ä�4ʸ�������񤯤���
void print_4tab(int dep)
{
	for (int i=0;i<dep;i++) PRT("    ");
}

// ���֤ˤ�ä����ޡ�����ɽ���������
void print_teban(int fukasa_teban)
{
	if ( fukasa_teban & 1 ) PRT("��");
	else                    PRT("��");
}

// ���ߤޤǤ�õ������ɽ������ؿ��������Ƥ����ΤǤ����ǤޤȤ���
void shogi::print_tejun(void)
{
	int i,*p;
	char retp[20];
#if 1
	int bz,az,tk,nf;
	// Ĺ�������ɽ���������㤰����ʤΤ�hash_move���ᤷ�Ƥ��줤��ɽ���ˤ��롣
	// �����ǡ����Ϥ�����ʤ��ΤǱƶ��Ϥʤ��������Ϥ���
	for (i=fukasa-1; i >= 0; i--) {	// ���ä����᤹��
		p=tejun[i];
		if ( p != NULL ) { bz = *(p+0); az = *(p+1); tk = *(p+2); nf = *(p+3); }
		else bz = 0;
		if ( bz == 0 ) ; else if ( bz == 0xff ) rhit_hash(az); else rmove_hash(az,bz,nf,tk);
	}

	for (i=0; i<fukasa; i++) {		// �ʤ��
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
	for (i=fukasa-1; i >= 0; i--) {	// ���ä����᤹��
		int m = path[i];
		if ( m ) undo_moveYSS((Move)m);
	}

	for (i=0; i<fukasa; i++) {		// �ʤ��
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

// able_move ���纹�ʤ��������֤ˤ������å��򳰤���---> ���ߤ� YSS_KI2.C ����Ƥ�Ǥ��������������ǥХå���
int shogi::check_move_sub( int bz, int az, int tk, int nf, int kiki_check_flag )
{
	int i,k,n,loop;

	if ( bz == 0xff ) {	// ���Ǥ��λ�
		if ( init_ban[az] != 0 ) return(0);	// ���˶𤬤���Ȥ�����ǤȤ��Ȥ��Ƥ��롣
		if ( tk > 0x80 ) {
			if ( mo_c[tk & 0x0f] == 0 ) return(0);
			if ( tk == 0x81 && nifu_c(az) == 0  ) return(0);
			if ( tk <= 0x82 && az > 0x90 ) return(0);	// �Ԥ��꤬�ʤ�
			if ( tk == 0x83 && az > 0x80 ) return(0);
		} else {
			if ( mo_m[tk       ] == 0 ) return(0);
			if ( tk == 0x01 && nifu_m(az) == 0  ) return(0);
			if ( tk <= 0x02 && az < 0x20 ) return(0);	// �Ԥ��꤬�ʤ�
			if ( tk == 0x03 && az < 0x30 ) return(0);
		}
		return (1);
	}

	if ( nf == 0x08 && (init_ban[bz] & 0x0f) > 0x08 ) {
		PRT("����𤬤ʤäƤ��ޤ������� move Error!. bz,az,nf = %x,%x,%x \n",bz,az,nf);
		return 0;
	}

	k = init_ban[bz];
	if ( k == 0 ) return(0);	// �׾�ˤʤ����ư�������Ȥ��Ƥ��롣
	if ( tk == 0xff ) return 0;	// �ɤ��ͤù��ࡣ
	if ( init_ban[az] != tk	) {
		PRT("�׾�ˤʤ�������Ȥ��Ƥ��롪\n");
/*	
hyouji();
PRT("bz,az,tk,nf=%x,%x,%x,%x :read_max=%d \n",bz,az,tk,nf,read_max );
print_tejun();
PRT("hash_code1,2 = %x,%x hash_motigoma = %x\n",hash_code1,hash_code2,hash_motigoma);
*/
		return(0);
	}

	n = ban[bz];	// ���ֹ�

	if ( k > 0x80 ) {	// COM �ζ�
		if ( nf == 0x08 ) {
			if ( k >= 0x88 || k == 0x85 ) return(0);	// ����Ȳ��ȶ������ʤ�
			if ( az < 0x70 && bz < 0x70 ) return(0);	// ����ʤ���
		} else {
			if ( k == 0x83 && az > 0x80 ) return(0);	// �����ϵ�����ʤ���
			if ( k <= 0x82 && az > 0x90 ) return(0);	// �����ϵ�����ʤ���
		}

		if ( tk > 0x80 ) return (0);	// ��ʬ�ζ���äƤ��롣

		if ( kiki_check_flag == 0 ) return 1;	// �����Υǡ���������å����ʤ����
		loop = kiki_c[az];
		for (i=0; i<loop; i++) {
			if ( kb_c[az][i] == n ) return(1);	// OK! ư�����롪
		}
	} else {
		if ( nf == 0x08 ) {
			if ( k >= 0x08 || k == 0x05 ) return(0);	// ����Ȳ��ȶ������ʤ�
			if ( az > 0x40 && bz > 0x40 ) return(0);	// ����ʤ���
		} else {
			if ( k == 0x03 && az < 0x30 ) return(0);	// �����ϵ�����ʤ���---> �ʤ��������Ȥˤ��Ƥ�������ͳ�ϡ�
			if ( k <= 0x02 && az < 0x20 ) return(0);	// �����ϵ�����ʤ���---> �ʤ��������Ȥˤ��Ƥ�������ͳ�ϡ�
		}

		if ( tk && tk < 0x80 ) return (0);	// ��ʬ�ζ���äƤ��롣

		if ( kiki_check_flag == 0 ) return 1;	// �����Υǡ���������å����ʤ����
		loop = kiki_m[az];
		for (i=0; i<loop; i++) {
			if ( kb_m[az][i] == n ) return(1);
		}
	}
	return (0);	// ư���ʤ�
}
#endif
