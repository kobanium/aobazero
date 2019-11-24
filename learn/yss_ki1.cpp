// 2019 Team AobaZero
// This source code is in the public domain.
// yss_ki1.cpp 
#include "yss_ext.h"        /*** load extern ***/
#include "yss_prot.h"		/* load prototype */
#include "yss.h"			// クラスの定義
#include <stdio.h>

const char *koma[32] = {
"  ","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","","UM","RY",
"  ","fu","ky","ke","gi","ki","ka","hi","ou","to","ny","nk","ng","","um","ry"
};

const char *koma_kanji[32] = {
"  ","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","","UM","RY",
"  ","fu","ky","ke","gi","ki","ka","hi","ou","to","ny","nk","ng","","um","ry"
//"  ","歩","香","桂","銀","金","角","飛","玉","と","杏","圭","全","","馬","龍",
//"  ","霓","霖","霰","靄","靉","靦","靱","鞆","霑","霤","霽","靈","","勒","鞅"
//   95e0 8d81 8c6a 8be2 8be0 8a70 94f2 8bca 82c6 88c7 8c5c 9153    946e 97b4 漢字コード
//   f0bf f0c3 f0c7 f0cb f0cf f0d3 f0d7 f0dd f0c1 f0c5 f0c9 f0cd    f0d5 f0d9 外字コード（標準の外字）
//"  ","\xf0\xbf","\xf0\xc3","\xf0\xc7","\xf0\xcb","\xf0\xcf","\xf0\xd3","\xf0\xd7","\xf0\xdd","\xf0\xc1","\xf0\xc5","\xf0\xc9","\xf0\xcd","","\xf0\xd5","\xf0\xd9"
};

const char *num_kanji[] = {
  "零",  "一",  "二",  "三",  "四",  "五",  "六",  "七",  "八",  "九",
  "十","十一","十二","十三","十四","十五","十六","十七","十八"
};

const char *num_zenkaku[]  = {
		  "０","１","２","３","４","５","６","７","８","９",
		  "10","11","12","13","14","15","16","17","18"      };

			  /***   fu ky ke gi ki ka hi ou to ny nk ng // um ry   ***/
static int kt[4][16] = {
				  { 0, 1, 2, 0, 1, 1, 0, 2, 0, 1, 1, 1, 1, 0, 1, 2 },
				  { 0, 0, 0, 0, 0, 1, 0, 2, 0, 1, 1, 1, 1, 0, 1, 2 },
				  { 0, 0, 0, 0, 1, 1, 2, 0, 0, 1, 1, 1, 1, 0, 2, 1 },
				  { 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 1 }
				};


#define KIKI_D_FOR_LOOP	// 利きの消去をエラーチェックなしの無限ループにする場合。実行サイズは減るはず。
// 消費したのクロックサイクル数 : 2123 [cycles] ... 初型から▲76歩△84歩に▲33角成の場合で。通常。
// 消費したのクロックサイクル数 : 2005 [cycles] 5.8%も速くなった?!
// 
// 74.4 秒(74)です。 69667局面/秒 read_max=8 ... 通常。	PentiumM 1.13GHz のバッテリーモードで。
// 71.8 秒(71)です。 72164局面/秒 read_max=8 ... ループ。4.2%も高速化！されたよ...おい。
//
// 593920 ... 現状
// 569344 ... ki1.c をループに。うひゃー、かなりEXEサイズは減るね。やっぱり。

#ifdef KIKI_D_FOR_LOOP	// エラーチェックなしでloopに。
#define KIKI_DM(x) p  = kb_m[x];             \
				   pk = --kiki_m[x] + p;	 \
				   for (;z != *p; p++) ;	 \
				   *p = *pk;
#else
/* 18回検索して見つからなかったらエラー */
/* z = ban[z]; で駒番号 */
#define KIKI_DM(x)  p=kb_m[x];                              \
					pk=--kiki_m[x]+p;                       \
					if (z-*p)                               \
				     if (z-*++p)                            \
					  if (z-*++p)                           \
					   if (z-*++p)                          \
					    if (z-*++p)                         \
					     if (z-*++p)                        \
						  if (z-*++p)                       \
						   if (z-*++p)                      \
						    if (z-*++p)                     \
						     if (z-*++p)                    \
						      if (z-*++p)                   \
							   if (z-*++p)                  \
							    if (z-*++p)                 \
								 if (z-*++p)                \
								  if (z-*++p)               \
								   if (z-*++p)              \
								    if (z-*++p)             \
									 if (z-*++p) debug();   \
					*p=*pk;                                 \
//					*pk=0;
#endif

#define KIKI_WM(x) kb_m[x][ kiki_m[x]++ ] = z;


#ifdef KIKI_D_FOR_LOOP	// エラーチェックなしでloopに。
#define LKIKIDM p  = kb_m[zz];                                      \
				pk = p + kiki_m[zz];    /* -1 を付けてif文を減らしても遅くなる。*/ \
				if ( p > --pk ) break;	/* 利きがないとき */        \
search_again:														\
				if ( z == (*p & 0x7f) ) {	/* 見つけた */			\
					*p = *pk; 										\
					--kiki_m[zz];									\
					/* loopを続ける */								\
				} else {											\
					if ( ++p > pk ) break;	/* 見つからなかった。*/	\
					goto search_again;								\
				}
#else

/* 利きが見つかったら if文ループを抜けて利きを消去 */
#define LKIKIDM p  = kb_m[zz];                                      \
				pk = p + kiki_m[zz];                                \
				if ( p > --pk ) break;	/* 利きがないとき */        \
				else if (z-(*p & 0x7f))                             \
					 if (++p>pk) break;                             \
				 else if (z-(*p & 0x7f))                            \
					  if (++p>pk) break;                            \
				  else if (z-(*p & 0x7f))                           \
					   if (++p>pk) break;                           \
				   else if (z-(*p & 0x7f))                          \
					    if (++p>pk) break;                          \
					else if (z-(*p & 0x7f))                         \
						 if (++p>pk) break;                         \
					 else if (z-(*p & 0x7f))                        \
						  if (++p>pk) break;                        \
					  else if (z-(*p & 0x7f))                       \
						   if (++p>pk) break;                       \
					   else if (z-(*p & 0x7f))                      \
							if (++p>pk) break;                      \
						else if (z-(*p & 0x7f))                     \
							 if (++p>pk) break;                     \
						 else if (z-(*p & 0x7f))                    \
							  if (++p>pk) break;                    \
						  else if (z-(*p & 0x7f))                   \
							   if (++p>pk) break;                   \
						   else if (z-(*p & 0x7f))                  \
								if (++p>pk) break;                  \
							else if (z-(*p & 0x7f))                 \
								 if (++p>pk) break;                 \
							 else if (z-(*p & 0x7f))                \
								  if (++p>pk) break;                \
							  else if (z-(*p & 0x7f))               \
								   if (++p>pk) break;               \
							   else if (z-(*p & 0x7f))              \
									if (++p>pk) break;              \
								else if (z-(*p & 0x7f))             \
									 if (++p>pk) break;             \
								 else if (z-(*p & 0x7f)) break;		\
				*p=*pk;                                             \
				--kiki_m[zz];
//				*pk=0;                                              
#endif

/******* com no define ********/

#ifdef KIKI_D_FOR_LOOP	// エラーチェックなしでloopに。
#define KIKI_DC(x) p  = kb_c[x];             \
				   pk = --kiki_c[x] + p;	 \
				   for (;z != *p;p++) ;		 \
				   *p=*pk;

#else
#define KIKI_DC(x) p=kb_c[x];                               \
				   pk=--kiki_c[x]+p;                        \
					if (z-*p)                               \
				     if (z-*++p)                            \
					  if (z-*++p)                           \
					   if (z-*++p)                          \
					    if (z-*++p)                         \
					     if (z-*++p)                        \
						  if (z-*++p)                       \
						   if (z-*++p)                      \
						    if (z-*++p)                     \
						     if (z-*++p)                    \
						      if (z-*++p)                   \
							   if (z-*++p)                  \
							    if (z-*++p)                 \
								 if (z-*++p)                \
								  if (z-*++p)               \
								   if (z-*++p)              \
								    if (z-*++p)             \
									 if (z-*++p) debug();   \
					*p=*pk;                                 \
//					*pk=0;
#endif

#define KIKI_WC(x) kb_c[x][ kiki_c[x]++ ] = z;


#ifdef KIKI_D_FOR_LOOP	// エラーチェックなしでloopに。
#define LKIKIDC p  = kb_c[zz];                                      \
				pk = p + kiki_c[zz];                                \
				if ( p > --pk ) break;	/* 利きがないとき */        \
search_again:														\
				if ( z == (*p & 0x7f) ) {	/* 見つけた */			\
					*p = *pk; 										\
					--kiki_c[zz];									\
					/* loopを続ける */								\
				} else {											\
					if ( ++p > pk ) break;	/* 見つからなかった。*/	\
					goto search_again;								\
				}
#else

#define LKIKIDC p=kb_c[zz];                                         \
                pk=p+kiki_c[zz];                                    \
				if (p>--pk) break;                                  \
				else if (z-(*p & 0x7f))                             \
					 if (++p>pk) break;                             \
				 else if (z-(*p & 0x7f))                            \
					  if (++p>pk) break;                            \
				  else if (z-(*p & 0x7f))                           \
					   if (++p>pk) break;                           \
				   else if (z-(*p & 0x7f))                          \
					    if (++p>pk) break;                          \
					else if (z-(*p & 0x7f))                         \
						 if (++p>pk) break;                         \
					 else if (z-(*p & 0x7f))                        \
						  if (++p>pk) break;                        \
					  else if (z-(*p & 0x7f))                       \
						   if (++p>pk) break;                       \
					   else if (z-(*p & 0x7f))                      \
							if (++p>pk) break;                      \
						else if (z-(*p & 0x7f))                     \
							 if (++p>pk) break;                     \
						 else if (z-(*p & 0x7f))                    \
							  if (++p>pk) break;                    \
						  else if (z-(*p & 0x7f))                   \
							   if (++p>pk) break;                   \
						   else if (z-(*p & 0x7f))                  \
								if (++p>pk) break;                  \
							else if (z-(*p & 0x7f))                 \
								 if (++p>pk) break;                 \
							 else if (z-(*p & 0x7f))                \
								  if (++p>pk) break;                \
							  else if (z-(*p & 0x7f))               \
								   if (++p>pk) break;               \
							   else if (z-(*p & 0x7f))              \
									if (++p>pk) break;              \
								else if (z-(*p & 0x7f))             \
									 if (++p>pk) break;             \
								 else if (z-(*p & 0x7f)) break;		\
				*p=*pk;                                             \
				--kiki_c[zz];
//				*pk=0;                                              
#endif

// kn[]を初期化して再設定。hash_code と tume_hyouka を再設定
void shogi::init(void)
{
	init_without_koma_koukan_table_init();
	koma_koukan_table_init();
}

void shogi::init_without_koma_koukan_table_init(void)
{
   int i,j,k;

   for ( i=0 ; i<8 ; i++ ) {
	  kn_stn[i] = &kn_stoc[i][0];
   }

   for ( i=0 ; i<4 ; i++ ) {
	  *kn_stn[2]++ = i+19;     /** kyou **/
	  *kn_stn[3]++ = i+15;     /** kei  **/
	  *kn_stn[4]++ = i+11;     /** gin  **/
	  *kn_stn[5]++ = i+7 ;     /** kin  **/
   }
   for ( i=0 ; i<2 ; i++ ) {
	  *kn_stn[6]++ = i+5 ;     /** kaku **/
	  *kn_stn[7]++ = i+3 ;     /** hi   **/
   }
   for ( i=0 ; i<18 ; i++ ) {
	  *kn_stn[1]++ = i+23;     /** fu   **/
   }

   kn[0][0] = kn[0][1] = 0;
   
   for ( i=0 ; i<BAN_SIZE; i++ ) {
	  j=init_ban[i];
	  if ( j==0 || j==0xff ) {
		 ban[i]=j;
		 continue;
	  }
	  if ( j==0x08 ) {
		 kn[1][0]=j;
		 kn[1][1]=i;
		 ban[i]=1;
		 continue;
	  }
	  if ( j==0x88 ) {
		 kn[2][0]=j;
		 kn[2][1]=i;
		 ban[i]=2;
		 continue;
	  }
	  int n=j & 0x07;
	  int k=*--kn_stn[n];
	  ban[i]=k;
	  kn[k][0]=j;
	  kn[k][1]=i;
   }

#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
	// 2歩テーブルを初期化
	for (k=0;k<16;k++) for (i=0;i<10;i++) {
		nifu_table_com[k][i] = 1;
		nifu_table_man[k][i] = 1;
	}
	for (i=0; i<BAN_SIZE; i++) {
		int k = init_ban[i];
		if ( k==0x81 ) nifu_table_com[1][i&0x0f] = 0;	// 歩が打てない
		if ( k==0x01 ) nifu_table_man[1][i&0x0f] = 0;
	}
#endif
}

// 駒交換をした場合の配列を、基本の駒の価値から作り直す。
void shogi::koma_koukan_table_init(void)
{
   int i,j,k;
   
   // なぜここでhash_motigomaを初期化してる？
	hash_motigoma = 0;
	for (i=1;i<8;i++) hash_motigoma += hash_mask[0][i] * mo_c[i];

#if 0
	// 駒を取ったりした場合に、その増分だけで処理できるように変換
	for (i=0;i<18;i++) for (j=1;j<16;j++) {
		totta_koma[i][j] = maisu_kati[j&0x07][i]*2 + mo_fuka_kati[j&0x07][i];
	}
	for (i=1;i<19;i++) for (j=1;j<8;j++) {
		utta_koma[i][j]  = -mo_fuka_kati[j][i-1];
	}
	for (i=0;i<18;i++) for (j=1;j<8;j++) {		// 持ち駒の差額。取られMAXの是正。
		m_koma_plus[i][j] = maisu_kati[j][i]*2 + mo_fuka_kati[j][i];
	}

#else
	// 駒を取ったりした場合に、その増分だけで処理できるように変換
	for (i=0;i<18;i++) for (j=1;j<16;j++) {
		totta_koma[i][j]=kati[j]+( m_kati[j&0x07][i+1] - m_kati[j&0x07][i] );
	}
	for (i=1;i<19;i++) for (j=1;j<8;j++) {
		utta_koma[i][j]=kati[j] - ( m_kati[j][i] - m_kati[j][i-1] );
	}
	for (i=0;i<18;i++) for (j=1;j<8;j++) {		// 持ち駒の差額。取られMAXの是正。
		m_koma_plus[i][j] = (m_kati[j][i+1] - m_kati[j][i]) - kati[j];
	}
#endif

#if 0
	for (j=1;j<8;j++) {
		PRT("[%d]=",j);
		int m_koma_max[8] = { 0,18,4,4,4,4,2,2 };
		for (i=0;i<m_koma_max[j];i++) PRT("%4d,",m_koma_plus[i][j]);
		PRT("\n");
	}
	for (i=1;i<8;i++) {
		if ( i!=5 && kati[i+8] - kati[i] != n_kati[i  ] ) { PRT("n_kati[%d] Err\n",i); debug(); }
		if (         kati[i]*2           != k_kati[i  ] ) { PRT("k_kati[%d] Err\n",i); debug(); }
		if ( i!=5 && kati[i]+kati[i+8]   != k_kati[i+8] ) { PRT("k_kati[%d]+Err\n",i); debug(); }
	}
#endif

   // init.. tume_hyouka
	tume_hyouka = 0;
	for (i=0;i<BAN_SIZE;i++) {
		k = init_ban[i];
		if ( k==0 || k==0xff ) continue;
		if ( k > 0x80 ) tume_hyouka += kati[k & 0x0f];
		else            tume_hyouka -= kati[k       ];
//		PRT("tume_hyouka=%d,k=%02x\n",tume_hyouka,k);
	}
	for (i=1;i<8;i++) tume_hyouka += m_kati[i][mo_c[i]] - m_kati[i][mo_m[i]];
//	PRT("tume_hyouka=%d,k=%02x\n",tume_hyouka,k);
}

/************************ kiki no write & delete **********************/


void shogi::fu_wm( register int z )
{
   register int zz;

   zz=z-0x10;
   z=ban[z];
   KIKI_WM(zz)     /* kb_m[ zz ][ kiki[ zz ]++ ]=ban[z] */
}
void shogi::fu_dm( register int z )
{
   register int *p;
   int *pk;
   int zz;

   zz=z-0x10;
   z=ban[z];
   KIKI_DM(zz)
}
void shogi::kei_wm( register int z )
{
   register int zz;

   zz=z-0x21;
   z=ban[z];
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
}
void shogi::kei_dm( int z )
{
   register int *p,*pk;
   int zz;

   zz=z-0x21;
   z=ban[z];
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
}
void shogi::gin_wm(register int z)
{
   register int zz;

   zz=z+0x0f;
   z=ban[z];
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
   zz-=0x20;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
}
void shogi::gin_dm(int z)
{
   register int *p,*pk;
   int zz;

   zz=z+0x0f;
   z=ban[z];
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
   zz-=0x20;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
}
void shogi::kin_wm(register int z)
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
   zz+=0x0f;
   KIKI_WM(zz)
   zz-=0x1f;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
}
void shogi::kin_dm(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
   zz+=0x0f;
   KIKI_DM(zz)
   zz-=0x1f;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
}

void shogi::ou_wm(register int z)                 /* ou no kiki kaku */
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
   zz+=0x10;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
   zz-=0x1e;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
   zz--;
   KIKI_WM(zz)
}
void shogi::ou_dm(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
   zz+=0x10;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
   zz-=0x1e;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
   zz--;
   KIKI_DM(zz)
}
void shogi::um4_wm(register int z)
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
   zz+=0x0f;
   KIKI_WM(zz)
   zz-=0x20;
   KIKI_WM(zz)
}
void shogi::um4_dm(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
   zz+=0x0f;
   KIKI_DM(zz)
   zz-=0x20;
   KIKI_DM(zz)
}
void shogi::ryu4_wm(register int z)
{
   register int zz;

   zz=z+0x11;
   z=ban[z];
   KIKI_WM(zz)
   zz-=0x02;
   KIKI_WM(zz)
   zz-=0x20;
   KIKI_WM(zz)
   zz+=0x02;
   KIKI_WM(zz)
}
void shogi::ryu4_dm(int z)
{
   register int *p,*pk;
   int zz;

   zz=z+0x11;
   z=ban[z];
   KIKI_DM(zz)
   zz-=0x02;
   KIKI_DM(zz)
   zz-=0x20;
   KIKI_DM(zz)
   zz+=0x02;
   KIKI_DM(zz)
}


// 香の利きを書く（飛車の前方も兼ねている）
void shogi::yari_wm(int z)
{
	register int k,zz;

	for (zz=z-0x10,z=ban[z]; zz > 0x10; zz -= 0x10) {
		KIKI_WM(zz)
		k = init_ban[zz];
		if ( k==0 ) continue;
		if ( k < 0x80 ) {
			if ( k=kt[0][k] , k == 0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				if ( zz > 0x20 ) {
					zz -= 0x10;
					KIKI_WM(zz)
				}
				break;
			}
		} else if (k-0x88) break;
	}
}

/*
void yari_wm(register int z)
{
	register int k,n;
	int endf = 0;

	n = ban[z];	// 香の駒番号
	for ( z = z - 0x10; z > 0x10; z -= 0x10 ) {	// 最初の zz > 0x10 は不要だが・・・。（飛車の時、有効）
		kb_m[z][ kiki_m[z]++ ] = n;		// KIKI_WM(zz)
		if (endf) break;
		k = init_ban[z];		// そこにある駒の種類
		if ( k == 0 ) continue;	// 何もなかったら次へ
		if ( k < 0x80 ) {		// 人間の駒であれば
			n |= 0x80;			// 間接の利きフラグを立てる
			k = kt[0][k];		// 利きが通過できるフラグを調べる（人間の香車か飛車なら通過)
			if ( k == 0 ) break;// 通過できない。
			if ( k == 1 ) {		// 歩や金。1回書いて終わり。
				endf++;
//				if ( z > 0x20 ) {
//					z -= 0x10;
//					kb_m[z][ kiki_m[z]++ ] = n;		// KIKI_WM(zz)	長い利きは盤外には絶対書かないこと！
//				}
//				break;			// 歩、金、銀、馬の時。一つ前方にだけ利く。
			}
		} else if (k-0x88) break;	// コンピュータの王だけは通過できる。
	}
}
*/
void shogi::yari_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x10,z=ban[z] ;; zz-=0x10) {
	  LKIKIDM
   }
}

void shogi::hir_wm(int z)
{
	register int k,zz;

	for (zz=z+0x01,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz++) {
		KIKI_WM(zz)
		if ( k==0 ) continue;
		if ( k < 0x80 ) {
			if ( k=kt[1][k] , k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz++;
				if ( init_ban[zz] != 0xff ) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::hir_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x01,z=ban[z] ;; zz++) {
	  LKIKIDM
   }
}

void shogi::hil_wm(int z)
{
	register int k,zz;

	for (zz=z-0x01,z=ban[z] ; k=init_ban[zz], k != 0xff ; zz--) {
		KIKI_WM(zz)
		if ( k==0 ) continue;
		if ( k<0x80 ) {
			if ( k=kt[1][k],k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz--;
				if ( init_ban[zz] != 0xff ) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::hil_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x01,z=ban[z] ;; zz--) {
	  LKIKIDM
   }
}
void shogi::hid_wm(int z)                       /* hisha no shita kiki kesu */
{
	register int k,zz;

	for (zz=z+0x10,z=ban[z] ; zz < 0x9a; zz += 0x10 ) {
		KIKI_WM(zz)
		k = init_ban[zz];
		if ( k==0 ) continue;
		if ( k < 0x80 ) {
			if (k=kt[1][k],k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				if ( zz < 0x90 ) {
					zz += 0x10;
					KIKI_WM(zz)
				}
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::hid_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x10,z=ban[z] ;; zz+=0x10) {
	  LKIKIDM
   }
}

void shogi::kaur_wm(int z)                 /* kaku no miginaname kiki kesu */
{
	register int k,zz;

	for (zz=z-0x0f,z=ban[z] ; k=init_ban[zz],k != 0xff; zz-=0x0f) {
		KIKI_WM(zz)
		if ( k==0 ) continue;
		if ( k<0x80 ) {
			if ( k=kt[2][k],k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz -= 0x0f;
				if ( init_ban[zz] != 0xff) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::kaur_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x0f,z=ban[z] ;; zz-=0x0f) {
	  LKIKIDM
   }
}

void shogi::kaul_wm(int z)
{
	register int k,zz;

	for (zz=z-0x11,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz-=0x11) {
		KIKI_WM(zz)
		if ( k==0 ) continue;
		if ( k<0x80 ) {
			if ( k=kt[2][k],k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz -= 0x11;
				if ( init_ban[zz] != 0xff ) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::kaul_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x11,z=ban[z] ;; zz-=0x11) {
	  LKIKIDM
   }
}

void shogi::kadr_wm(int z)
{
	register int k,zz;

	for (zz=z+0x11,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz+=0x11) {
		KIKI_WM(zz)
		if (k==0) continue;
		if ( k<0x80 ) {
			if ( k=kt[3][k],k==0 ) break;
			z |= 0x80;
			if (k==1) {
				zz += 0x11;
				if ( init_ban[zz] != 0xff) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::kadr_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x11,z=ban[z] ;; zz+=0x11) {
	  LKIKIDM
   }
}

void shogi::kadl_wm(int z)
{
	register int k,zz;

	for (zz=z+0x0f,z=ban[z] ; k=init_ban[zz], k != 0xff ; zz+=0x0f) {
		KIKI_WM(zz)
		if (k==0) continue;
		if ( k<0x80 ) {
			if ( k=kt[3][k],k==0 ) break;
			z|=0x80;
			if (k==1) {
				zz += 0x0f;
				if ( init_ban[zz] != 0xff ) KIKI_WM(zz)
				break;
			}
		} else if (k-0x88) break;
	}
}

void shogi::kadl_dm(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x0f,z=ban[z] ;; zz+=0x0f) {
	  LKIKIDM
   }
}

/*****************   computer no kiki routine ****************/


void shogi::fu_wc(register int z)
{
   register int zz;

   zz=z+0x10;
   z=ban[z];
   KIKI_WC(zz)
}
void shogi::fu_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z+0x10;
   z=ban[z];
   KIKI_DC(zz)
}
void shogi::kei_wc(register int z)
{
   register int zz;

   zz=z+0x1f;
   z=ban[z];
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
}
void shogi::kei_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z+0x1f;
   z=ban[z];
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
}
void shogi::gin_wc(register int z)
{
   register int zz;

   zz=z-0x11;
   z=ban[z];
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
   zz+=0x20;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
}
void shogi::gin_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x11;
   z=ban[z];
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
   zz+=0x20;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
}
void shogi::kin_wc(register int z)
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
   zz-=0x11;
   KIKI_WC(zz)
   zz+=0x21;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
}
void shogi::kin_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
   zz-=0x11;
   KIKI_DC(zz)
   zz+=0x21;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
}

void shogi::ou_wc(register int z)                 /* ou no kiki kaku */
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
   zz+=0x10;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
   zz-=0x1e;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
   zz--;
   KIKI_WC(zz)
}
void shogi::ou_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
   zz+=0x10;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
   zz-=0x1e;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
   zz--;
   KIKI_DC(zz)
}
void shogi::um4_wc(register int z)
{
   register int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
   zz+=0x0f;
   KIKI_WC(zz)
   zz-=0x20;
   KIKI_WC(zz)
}
void shogi::um4_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z-0x01;
   z=ban[z];
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
   zz+=0x0f;
   KIKI_DC(zz)
   zz-=0x20;
   KIKI_DC(zz)
}
void shogi::ryu4_wc(register int z)
{
   register int zz;

   zz=z+0x11;
   z=ban[z];
   KIKI_WC(zz)
   zz-=0x02;
   KIKI_WC(zz)
   zz-=0x20;
   KIKI_WC(zz)
   zz+=0x02;
   KIKI_WC(zz)
}
void shogi::ryu4_dc(int z)
{
   register int *p,*pk;
   int zz;

   zz=z+0x11;
   z=ban[z];
   KIKI_DC(zz)
   zz-=0x02;
   KIKI_DC(zz)
   zz-=0x20;
   KIKI_DC(zz)
   zz+=0x02;
   KIKI_DC(zz)
}

/*
void yari_wc(int z)
{
   register int k,zz;
   int endf=0;

   for (zz=z+0x10,z=ban[z] ; k=ban[zz],k != 0xff ; zz+=0x10) {
	  KIKI_WC(zz)
	  if (endf) break;
	  else if (k==0) continue;
	  else if ( k=kn[k][0],k>0x80 ) {
		 z|=0x80;
		 k&=0x0f;
		 if ( k=kt[0][k],k==0 ) break;
		 else if (k==1) endf++;
		 }
	  else if (k-0x08) break;
	  }
}
*/
void shogi::yari_wc(int z)
{
	register int k,zz;

	for (zz = z + 0x10,z = ban[z]; zz < 0x9a; zz += 0x10) {	// z には元の駒番号が入る。分かりづらい・・・。
		KIKI_WC(zz)	// kb_c[x][ kiki_c[x]++ ] = z;	// 
		k = init_ban[zz];
		if ( k==0 ) continue;
		if ( k > 0x80 ) {
			k = kt[0][k & 0x0f];
			if ( k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {		// COMの金銀歩などは1つだけ突き抜ける
				if ( zz < 0x90 ) {
					zz += 0x10;
					KIKI_WC(zz)
				}
				break;
			}
		} else if (k-0x08) break;
	}
}


void shogi::yari_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x10,z=ban[z] ;; zz+=0x10) {
	  LKIKIDC
   }
}

/*
void yari_dc(int z)
{
	register int *p,*pk;
	int zz;

	n = ban[z];	// 香(飛)の駒番号。間接の場合は 0x80 とOR

	for ( z += 0x10; z < 0x9a ;z+=0x10) {	// 長い利きは「絶対に」盤外に1つも書かない
		if ( kiki_c[zz] == 0 ) break;		// 利きがなければ終わり
		for (i = 0; i < kiki_c[zz]; i++) {
			if ( n == kb_c[zz][i] & 0x7f ) {	// 見つけた
				*p = *pk;                                             
				*pk = 0;                                              
				--kiki_c[zz];
			
				break;
			}
		}


		p = kb_c[zz];        // 最初のポインタ                                
        pk = p + kiki_c[zz]; // 最後のポインタ                                  
		// z ... 消したい駒番号
		if ( p > --pk ) break;                         
		if ( z != (*p & 0x7f) ) {
			if ( ++p > pk) break;                    
			if ( z != (*p & 0x7f) ) {
				if ( ++p > pk ) break;                   
				if ( z != (*p & 0x7f) ) {
					if ( ++p > pk ) break;                   
					if ( z != (*p & 0x7f) ) {
						if ( ++p > pk ) break;                   
						if ( z != (*p & 0x7f) ) {
							if ( ++p > pk ) break;                   
							if ( z != (*p & 0x7f) ) break;
						}
					}
				}
			}
		}
		// break; で落ちなかったらこの行を実行する		               
		*p = *pk;                                             
		*pk = 0;                                              
		--kiki_c[zz];
	}
}
*/
void shogi::hir_wc(int z)
{
	register int k,zz;

	for (zz=z-0x01,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz--) {
		KIKI_WC(zz)
	 	if ( k == 0 ) continue;
		if ( k > 0x80 ) {
			k = kt[1][k & 0x0f];
			if ( k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz--;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::hir_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x01,z=ban[z] ;; zz--) {
	  LKIKIDC
   }
}

void shogi::hil_wc(int z)
{
	register int k,zz;

	for (zz=z+0x01,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz++) {
		KIKI_WC(zz)
		if ( k == 0 ) continue;
		if ( k > 0x80 ) {
			k = kt[1][k & 0x0f];
			if ( k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				zz++;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::hil_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x01,z=ban[z] ;; zz++) {
	  LKIKIDC
   }
}
void shogi::hid_wc(int z)           /* hisha no shita kiki kesu */
{
	register int k,zz;

	for (zz=z-0x10,z=ban[z] ; k=init_ban[zz],k != 0xff ;zz -= 0x10) {
		KIKI_WC(zz)
		if ( k == 0 ) continue;
		if ( k > 0x80 ) {
			k=kt[1][k & 0x0f];
			if ( k==0 ) break;
			z |= 0x80;
			if ( k==1 ) {
				if ( zz > 0x20 ) {
					zz -= 0x10;
					KIKI_WC(zz)
				}
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::hid_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x10,z=ban[z] ;; zz-=0x10) {
	  LKIKIDC
   }
}

void shogi::kaur_wc(int z)       /* kaku no miginaname kiki kesu */
{
	register int k,zz;

	for (zz=z+0x0f,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz+=0x0f) {
		KIKI_WC(zz)
		if (k==0) continue;
		if ( k>0x80 ) {
			k&=0x0f;
			if ( k=kt[2][k],k==0 ) break;
			z |= 0x80;
			if (k==1) {
				zz += 0x0f;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::kaur_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x0f,z=ban[z] ;; zz+=0x0f) {
	  LKIKIDC
   }
}

void shogi::kaul_wc(int z)
{
	register int k,zz;

	for (zz=z+0x11,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz+=0x11) {
		KIKI_WC(zz)
		if ( k==0 ) continue;
		if ( k>0x80 ) {
			k&=0x0f;
			if ( k=kt[2][k],k==0 ) break;
			z |= 0x80;
			if (k==1) {
				zz += 0x11;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::kaul_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z+0x11,z=ban[z] ;; zz+=0x11) {
	  LKIKIDC
   }
}

void shogi::kadr_wc(int z)
{
	register int k,zz;

	for (zz=z-0x11,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz-=0x11) {
		KIKI_WC(zz)
		if (k==0) continue;
		if ( k>0x80 ) {
			k&=0x0f;
			if ( k=kt[3][k],k==0 ) break;
			z|=0x80;
			if (k==1) {
				zz -= 0x11;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::kadr_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x11,z=ban[z] ;; zz-=0x11) {
	  LKIKIDC
   }
}

void shogi::kadl_wc(int z)
{
	register int k,zz;

	for (zz=z-0x0f,z=ban[z] ; k=init_ban[zz],k != 0xff ; zz-=0x0f) {
		KIKI_WC(zz)
		if (k==0) continue;
		if ( k>0x80 ) {
			k&=0x0f;
			if ( k=kt[3][k],k==0 ) break;
			z |= 0x80;
			if (k==1) {
				zz -= 0x0f;
				if ( init_ban[zz] != 0xff ) KIKI_WC(zz)
				break;
			}
		} else if (k-0x08) break;
	}
}

void shogi::kadl_dc(int z)
{
   register int *p,*pk;
   int zz;

   for (zz=z-0x0f,z=ban[z] ;; zz-=0x0f) {
	  LKIKIDC
   }
}






// 利きを書く
#if 0
void kikiw(register int z)         // z= y*16+x  ex. z=0x95 (ou)
{
   register int s;

   s=init_ban[z];
   if (s==0) {
	  PRT("kiki(%02X)=0 Error of kikiw()!!!!!! \n",z);
	  debug();
   }

   if (s<0x80) {
	  if (s<0x08)
		 switch (s) {
			case 0x01:
			   fu_wm(z);
			   break;
			case 0x02:
			   yari_wm(z);
			   break;
			case 0x03:
			   kei_wm(z);
			   break;
			case 0x04:
			   gin_wm(z);
			   break;
			case 0x05:
			   kin_wm(z);
			   break;
			case 0x06:
			   kaur_wm(z);
			   kaul_wm(z);
			   kadr_wm(z);
			   kadl_wm(z);
			   break;
			case 0x07:
			   yari_wm(z);
			   hir_wm(z);
			   hil_wm(z);
			   hid_wm(z);
			   break;

		 }
	  else
		 switch (s) {
			case 0x08:
			   ou_wm(z);
			   break;
			case 0x0e:
			   kaur_wm(z);
			   kaul_wm(z);
			   kadr_wm(z);
			   kadl_wm(z);
			   um4_wm(z);
			   break;
			case 0x0f:
			   yari_wm(z);
			   hir_wm(z);
			   hil_wm(z);
			   hid_wm(z);
			   ryu4_wm(z);
			   break;
			default:
			   kin_wm(z);
			   break;
		 }
   }

   else if (s<0x88)               /*  com no toki */
		 switch (s) {
			case 0x81:
			   fu_wc(z);
			   break;
			case 0x82:
			   yari_wc(z);
			   break;
			case 0x83:
			   kei_wc(z);
			   break;
			case 0x84:
			   gin_wc(z);
			   break;
			case 0x85:
			   kin_wc(z);
			   break;
			case 0x86:
			   kaur_wc(z);
			   kaul_wc(z);
			   kadr_wc(z);
			   kadl_wc(z);
			   break;
			case 0x87:
			   yari_wc(z);
			   hir_wc(z);
			   hil_wc(z);
			   hid_wc(z);
			   break;

		 }
	  else
		 switch (s) {
			case 0x88:
			   ou_wc(z);
			   break;
			case 0x8e:
			   kaur_wc(z);
			   kaul_wc(z);
			   kadr_wc(z);
			   kadl_wc(z);
			   um4_wc(z);
			   break;
			case 0x8f:
			   yari_wc(z);
			   hir_wc(z);
			   hil_wc(z);
			   hid_wc(z);
			   ryu4_wc(z);
			   break;
			default:
			   kin_wc(z);
			   break;
		 }

}
#endif

// 角の利きを全て書く
void shogi::ka_wm(int z)
{
	kaur_wm(z);
	kaul_wm(z);
	kadr_wm(z);
	kadl_wm(z);
}
// 飛
void shogi::hi_wm(int z)
{
	yari_wm(z);
	hir_wm(z);
	hil_wm(z);
	hid_wm(z);
}
// 馬
void shogi::um_wm(int z)
{
	kaur_wm(z);
	kaul_wm(z);
	kadr_wm(z);
	kadl_wm(z);
	um4_wm(z);
}
// 龍
void shogi::ryu_wm(int z)
{
	yari_wm(z);
	hir_wm(z);
	hil_wm(z);
	hid_wm(z);
	ryu4_wm(z);
}

// 角 COM
void shogi::ka_wc(int z)
{
	kaur_wc(z);
	kaul_wc(z);
	kadr_wc(z);
	kadl_wc(z);
}
// 飛
void shogi::hi_wc(int z)
{
	yari_wc(z);
	hir_wc(z);
	hil_wc(z);
	hid_wc(z);
}
// 馬
void shogi::um_wc(int z)
{
	kaur_wc(z);
	kaul_wc(z);
	kadr_wc(z);
	kadl_wc(z);
	um4_wc(z);
}
// 龍
void shogi::ryu_wc(int z)
{
	yari_wc(z);
	hir_wc(z);
	hil_wc(z);
	hid_wc(z);
	ryu4_wc(z);
}

void shogi::ekw(int z)		// err_kiki_write
{
	PRT("\nkikiw(%x)=%x Error !!!!!!\n",z,init_ban[z]);
	debug();
}

// 高速化のために関数を配列にしてON GOTO を真似る。---> 3%の高速化

// 関数へのポインタを格納する配列
//extern int func();
//int (*fp)() = func;


// メンバ関数へのポインタ配列 "&shogi::fu_wm" と&を付けるべき？

#if 0
typedef void (shogi::*PFN_kiki_write)(int);
typedef int BOOL;
typedef void (*VOIDFNC)(void);

void abc(void) {}
void (*def)(void) = abc;

VOIDFNC def2 = abc;	

PFN_kiki_write func_kiki_write[0x90] = {	// 関数へのポインタはこういう風に書いたほうがきれい。

#else
void (shogi::*func_kiki_write[0x90])(int) = {
#endif
	&shogi::ekw,	// 0x00
	&shogi::fu_wm,
	&shogi::yari_wm,
	&shogi::kei_wm,
	&shogi::gin_wm,
	&shogi::kin_wm,
	&shogi::ka_wm,
	&shogi::hi_wm,
	&shogi::ou_wm,	// 0x08
	&shogi::kin_wm,
	&shogi::kin_wm,
	&shogi::kin_wm,
	&shogi::kin_wm,
	&shogi::ekw,
	&shogi::um_wm,	// 0x0e
	&shogi::ryu_wm,	// 0x0f

	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x10)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x20)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x30)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x40)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x50)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x60)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 	// (0x70)
	&shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, &shogi::ekw, 

	&shogi::ekw,	// 0x80
	&shogi::fu_wc,
	&shogi::yari_wc,
	&shogi::kei_wc,
	&shogi::gin_wc,
	&shogi::kin_wc,
	&shogi::ka_wc,
	&shogi::hi_wc,
	&shogi::ou_wc,	// 0x88
	&shogi::kin_wc,
	&shogi::kin_wc,
	&shogi::kin_wc,
	&shogi::kin_wc,
	&shogi::ekw,
	&shogi::um_wc,	// 0x8e
	&shogi::ryu_wc,	// 0x8f
};




#if 0
void kikiw(register int z)         // z = y*16+x  ex. z=0x95 (ou)
{
	register int s;

	s = init_ban[z];
//	if ( s > 0x80 ) s -= 0x70;	// 0x81-0x8f ---> 0x11-0x1f に変更
	(*func_kiki_write[s])(z);	// 関数での一発ジャンプ！（分かりにくいけど・・・）
}
#endif

// さらにマクロにする（呼ぶ回数が詰将棋だと最大なので）
//#define kikiw(z) (*func_kiki_write[ init_ban[z] ])(z)


// 利きを消す

// 角の利きを全て消す
void shogi::ka_dm(int z)
{
	kaur_dm(z);
	kaul_dm(z);
	kadr_dm(z);
	kadl_dm(z);
}
// 飛
void shogi::hi_dm(int z)
{
	yari_dm(z);
	hir_dm(z);
	hil_dm(z);
	hid_dm(z);
}
// 馬
void shogi::um_dm(int z)
{
	kaur_dm(z);
	kaul_dm(z);
	kadr_dm(z);
	kadl_dm(z);
	um4_dm(z);
}
// 龍
void shogi::ryu_dm(int z)
{		 
	yari_dm(z);
	hir_dm(z);
	hil_dm(z);
	hid_dm(z);
	ryu4_dm(z);
}

// 角 COM
void shogi::ka_dc(int z)
{
	kaur_dc(z);
	kaul_dc(z);
	kadr_dc(z);
	kadl_dc(z);
}
// 飛
void shogi::hi_dc(int z)
{
	yari_dc(z);
	hir_dc(z);
	hil_dc(z);
	hid_dc(z);
}
// 馬
void shogi::um_dc(int z)
{
	kaur_dc(z);
	kaul_dc(z);
	kadr_dc(z);
	kadl_dc(z);
	um4_dc(z);
}
// 龍
void shogi::ryu_dc(int z)
{
	yari_dc(z);
	hir_dc(z);
	hil_dc(z);
	hid_dc(z);
	ryu4_dc(z);
}

void shogi::ekd(int z)	// err_kiki_delete
{
	PRT("\nkikid(%x)=%x Error !!!!!! in ekd()\n",z,init_ban[z]);
	debug();
}

// 「型を返さずintの引数を1つもつ」関数へのポインタ、の配列
// 利きを消す関数の配列 ... -0x70 を引きたくないのでべたでテーブルを作る

void (shogi::*func_kiki_delete[0x90])(int) = {
	&shogi::ekd,	// 0x00
	&shogi::fu_dm,	// 0x01
	&shogi::yari_dm,
	&shogi::kei_dm,
	&shogi::gin_dm,
	&shogi::kin_dm,
	&shogi::ka_dm,
	&shogi::hi_dm,
	&shogi::ou_dm,	// 0x08
	&shogi::kin_dm,
	&shogi::kin_dm,
	&shogi::kin_dm,
	&shogi::kin_dm,
	&shogi::ekd,
	&shogi::um_dm,	// 0x0e
	&shogi::ryu_dm,	// 0x0f

	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x10)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x20)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x30)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x40)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x50)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x60)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, // (0x70)
	&shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, &shogi::ekd, 

	&shogi::ekd,	// 0x80
	&shogi::fu_dc,
	&shogi::yari_dc,
	&shogi::kei_dc,
	&shogi::gin_dc,
	&shogi::kin_dc,
	&shogi::ka_dc,
	&shogi::hi_dc,
	&shogi::ou_dc,	// 0x88
	&shogi::kin_dc,
	&shogi::kin_dc,
	&shogi::kin_dc,
	&shogi::kin_dc,
	&shogi::ekd,
	&shogi::um_dc,	// 0x8e
	&shogi::ryu_dc,	// 0x8f
};

#if 0
void kikid(register int z)
{
	register int s;

	s = init_ban[z];
//	if ( s > 0x80 ) s -= 0x70;	// 0x81-0x8f ---> 0x11-0x1f に変更
	(*func_kiki_delete[s])(z);	// 関数への一発ジャンプ！（分かりにくいけど・・・）
}
#endif

//#define kikid(z) (*func_kiki_delete[ init_ban[z] ])(z)

#if 0
void kikid(register int z)
{
	register int s;

	s = init_ban[z];
#ifdef _DEBUG
	if ( s==0 ) { PRT("\nkiki(%x)=0 Error !!!!!! of kikid() \n",z); debug(); }
#endif
	if ( s > 0x80 ) s -= 0x70;	// 0x81-0x8f ---> 0x11-0x1f に変更

	// switch はif文が並んで遅いように見えるが、最適化されると on goto ... 形式になって速い
	// ---> 最初にsの範囲のチェックをしてしまうのでちょっと遅い・・・。
	switch (s) {
		case 0x00:	// これでもいいかな？（最適化失敗なら消す）
			{ PRT("\nkiki(%x)=0 Error !!!!!! of kikid() \n",z); debug(); }
			break;
		case 0x01:
			fu_dm(z);
			break;
		case 0x02:
			yari_dm(z);
			break;
		case 0x03:
			kei_dm(z);
			break;
		case 0x04:
			gin_dm(z);
			break;
		case 0x05:
			kin_dm(z);
			break;
		case 0x06:
			kaur_dm(z);
			kaul_dm(z);
			kadr_dm(z);
			kadl_dm(z);
			break;
		case 0x07:
			yari_dm(z);
			hir_dm(z);
			hil_dm(z);
			hid_dm(z);
			break;
		case 0x08:
			ou_dm(z);
			break;
		case 0x09:
			kin_dm(z);
			break;
		case 0x0a:
			kin_dm(z);
			break;
		case 0x0b:
			kin_dm(z);
			break;
		case 0x0c:
			kin_dm(z);
			break;
		case 0x0d:		// これも不要か？
			debug();	
			break;
		case 0x0e:
			kaur_dm(z);
			kaul_dm(z);
			kadr_dm(z);
			kadl_dm(z);
			um4_dm(z);
			break;
		case 0x0f:
			yari_dm(z);
			hir_dm(z);
			hil_dm(z);
			hid_dm(z);
			ryu4_dm(z);
			break;
		case 0x11:
			fu_dc(z);
			break;
		case 0x12:
			yari_dc(z);
			break;
		case 0x13:
			kei_dc(z);
			break;
		case 0x14:
			gin_dc(z);
			break;
		case 0x15:
			kin_dc(z);
			break;
		case 0x16:
			kaur_dc(z);
			kaul_dc(z);
			kadr_dc(z);
			kadl_dc(z);
			break;
		case 0x17:
			yari_dc(z);
			hir_dc(z);
			hil_dc(z);
			hid_dc(z);
			break;
		case 0x18:
			ou_dc(z);
			break;
		case 0x19:
			kin_dc(z);
			break;
		case 0x1a:
			kin_dc(z);
			break;
		case 0x1b:
			kin_dc(z);
			break;
		case 0x1c:
			kin_dc(z);
			break;
		case 0x8d:
			debug();
			break;
		case 0x1e:
			kaur_dc(z);
			kaul_dc(z);
			kadr_dc(z);
			kadl_dc(z);
			um4_dc(z);
			break;
		case 0x1f:
			yari_dc(z);
			hir_dc(z);
			hil_dc(z);
			hid_dc(z);
			ryu4_dc(z);
			break;
	}
}
#endif
