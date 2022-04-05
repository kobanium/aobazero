// 2019 Team AobaZero
// This source code is in the public domain.
/* Yamashita Shogi System */
#include "yss_ext.h"     /*** load extern ***/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss.h"		// クラスの定義

/*******  koma ugokasu.  (man)   ********/

// 「型を返さずintの引数を1つもつ」関数へのポインタ、の配列
extern void (shogi::*func_kiki_write[0x90])(int);	// o_ki1.c で定義
extern void (shogi::*func_kiki_delete[0x90])(int);

// さらにマクロにする（呼ぶ回数が詰将棋だと最大なので）
#define kikiw(z) (this->*func_kiki_write[  init_ban[z] ])(z)
#define kikid(z) (this->*func_kiki_delete[ init_ban[z] ])(z)

//extern void (*func_kiki_delete_hoge[0x90])(int,int);	// 2つ引数を持つ場合

#ifdef NPS_MOVE_COUNT
	int move_count;	// move,hitを呼び出した回数。局面を実際に動かした回数を数える場合
#endif

//#define KI2_HIKAKU	// 飛角（龍馬）の動く方向の書き換えを最小に。実探索では20.1秒で変わらず。脊尾詰では10.3秒と10.4秒で逆に遅くなった。
/*
飛角が動いた時だけ、特別にkikiw()を使いたい。多分2%ぐらい高速化される。
bz-azの情報が欲しい。
ただし、他の駒の時は通常通り、"z" 一箇所で処理する。

素直に書くと、
move(az,bz,nf)
switch (init_ban[bz])

if ( k==0 ) {
	
}


k = init_ban[bz] & 0x0f;

if ( (k & 0x07)==0x07 ) {	// 飛
	dir = hisha_dir[bz - az + 0x99];
	if ( dir == 0 ) 通常（全部消して全部書く）
	上下に進む場合（左右のみ消して書く）
	駒を取る場合、その方向にのみ、伸ばして書く。

	今、自分がいる場所に動ける利きを書く。
	自分が動く場所の利きを消す。
	
	成り駒が動く場合は斜め4マスの利きを消す。bz
	駒が成る場合、元から成っている場合は、成り駒も同時に書く。az
}

駒移動の場合は、利き消しと書きは必ず対になるので同時に処理してもOK。
また駒を取ったとしても利きの情報はなんら変化がないのでOK。（長い利きを除いて）


		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
//		if ( dx && dy ) 通常（龍の動き）
		if ( dy == 0 ) 左右
		else {
			if ( dx == 0 ) 上下
			else 通常
		}
	}
	if ( (k & 0x07)==0x06 ) {	// 角
		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
//		if ( dx && dy ) 通常（龍の動き）
		if ( dy == 0 ) 左右
		else {
			if ( dx == 0 ) 上下
			else 通常
		}
	}
} else 通常

相対位置を一発で配列から持ってこれない？
256x256ならできる。
  bz -  az = 
0x11  0x12	 -0x01
0x11  0x13	 -0x02
0x11  0x14	 -0x03
0x11  0x15	 -0x04
0x11  0x16	 -0x05
0x11  0x17	 -0x06
0x11  0x18	 -0x07
0x11  0x19	 -0x08
0x11  0x21	 -0x10
0x11  0x22	 -0x11
0x11  0x23	 -0x12
0x11  0x24	 -0x13
0x11  0x25	 -0x14
0x11  0x26	 -0x15
...

0x19  0x12	  0x07
0x19  0x13	  0x06
0x19  0x14	  0x05
0x19  0x15	  0x04
0x19  0x16	  0x03
0x19  0x17	  0x02
0x19  0x18	  0x01
0x19  0x19	  0x00
0x19  0x21	 -0x08	---> これが重複する！
0x19  0x22	 -0x09
0x19  0x23	 -0x0a
0x19  0x24	 -0x0b
0x19  0x25	 -0x0c
0x19  0x26	 -0x0d
0x19  0x27	 -0x0e
0x19  0x28	 -0x0f
0x19  0x29	 -0x10
...

しかし実際は・・・飛角の移動できる場所は限られているので、移動元と先の差をとった場合に、
絶対に出てこない数値、というのがある。

例えば飛車は81マスから80マスに動けるわけではない。
81マスから最大16マスにしか動けない。つまり81*16 = 1280+16=1296 通りしか存在しない。
龍だとしても81*(16+4)=81*20=1620通り以下。

その全ての bz - az を計算すると・・・もし、方向と龍の斜め4マスが完全に独立なら差分を取って計算できる。
というか盤面の端から上の端へワープする動きはないので計算できるはず。テーブルは0x11-0x99 = 0x88*2 で十分。300バイト程度か。

王は上下左右は通常は8箇所消して8箇所書く。最小なら4箇所消して4箇所書く。
斜めは6箇所消して6箇所書く。


kikid_move()
#define kikid(bz,az) (this->*func_kiki_move[ init_ban[bz] ])(bz,az)

	&shogi::fu_wm,
	&shogi::yari_wm,
	&shogi::kei_wm,
	&shogi::gin_wm,
	&shogi::kin_wm,
	&shogi::ka_wm,
	&shogi::hi_wm,

*/


/*
// 「型を返さずintの引数を1つもつ」関数へのポインタ、の配列
void (*func_stoc_write_man[8])(int) = {	// write ... man
   kaul_wm,   kaur_wm,   kadl_wm,   kadr_wm,   yari_wm,   hid_wm,   hil_wm,   hir_wm
};
void (*func_stoc_write_com[8])(int) = {	// write ... com
   kaul_wc,   kaur_wc,   kadl_wc,   kadr_wc,   yari_wc,   hid_wc,   hil_wc,   hir_wc
};

void (*func_stoc_delete_man[8])(int) = {// delete ... man
   kaul_dm,   kaur_dm,   kadl_dm,   kadr_dm,   yari_dm,   hid_dm,   hil_dm,   hir_dm
};
void (*func_stoc_delete_com[8])(int) = {// delete ... com
   kaul_dc,   kaur_dc,   kadl_dc,   kadr_dc,   yari_dc,   hid_dc,   hil_dc,   hir_dc
};
*/


void shogi::move(int az,int bz,int nf) //  az... 移動する yx, bz... 元の yx, nf... 成フラグ
{
	int k,n,tk,st=0,i,nnn;
	int *p;
//	int stoc[32];	// ローカルで持つと 71.8 秒 ---> 72.4 秒 0.8%遅くなる。

#ifdef NPS_MOVE_COUNT
	move_count++;
#endif

#ifdef KI2_HIKAKU	// 飛角（龍馬）の動く方向の書き換えを最小に。
	int f_dir;
#endif

#ifdef _DEBUG
	tk = init_ban[az];
//	check_kn();
	if ( check_move( bz, az, tk, nf ) == 0 ) {PRT("ルール違反の指し手! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
//	if ( able_move( bz, az, tk, nf ) == 0 ) {PRT("ルール違反の指し手! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
	check_kn();
#endif

	if ( (tk=ban[az]) != 0 ) kikid(az);		// 取る駒の利きを消す

#ifdef KI2_HIKAKU	// 飛角（龍馬）の動く方向の書き換えを最小に。
	k = init_ban[bz];
	f_dir = 0;
	if ( (k & 0x0f)==0x07 && tk == 0 ) {	// MANの飛
		int dx,dy;
		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
		if      ( dy == 0 ) f_dir = 1;	// 左右の移動
		else if ( dx == 0 ) f_dir = 2;	// 上下
//		else                f_dir = 0;	// 通常
		if ( k > 0x80 ) f_dir += 4;
	} else if ( (k & 0x0f)==0x06 && tk == 0 ) {	// MANの角
		if ( bz < az ) f_dir = 9;		// 下
		else           f_dir = 11;
		if ( (bz & 0x0f) < (az & 0x0f) ) f_dir++;	// 右
		// 12...右上、10...右下、11...左上、9...左下
		if ( k > 0x80 ) f_dir += 4;
	}
	
	if ( f_dir ) {	// 移動先の利きを消す。一箇所だけ
		int *pk;
		k = ban[bz];	// 駒番号
		if ( (f_dir - 1) & 0x04 ) { p=kb_c[az]; pk=--kiki_c[az]+p; }
		else                      { p=kb_m[az]; pk=--kiki_m[az]+p; }
		if (k-*p) if (k-*++p) if (k-*++p) if (k-*++p) if (k-*++p) if (k-*++p) { PRT("hoge\n"); debug(); }
		*p=*pk;                                 
	} else {
		kikid(bz);								// 動かす駒の利きを消す（通常）
	}
#else
	kikid(bz);								// 動かす駒の利きを消す
#endif

											// 元の長い利きを消す

	if ( (i=kiki_m[bz]) != 0 ) {			//  man の利きを stoc へ
		for ( p = kb_m[bz] + i ; i ; i--) {
			k = *--p & 0x7f;				//  k ... 駒番号
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
				case 0x06:
					if ( bz > k ) n = 2;	// ka down
					else          n = 0;
					if ((bz & 0x0f) > (k & 0x0f)) n++; /*  ka right */

					*(stoc + st++) = k;
					*(stoc + st++) = n;

					// 続けて書くとコンパイラが on goto ... で最適化してくれていた。
					switch (n) {			
						case 0:
							kaul_dm(k);
							break;
						case 1:
							kaur_dm(k);
							break;
						case 2:
							kadl_dm(k);
							break;
						case 3:
							kadr_dm(k);
							break;
					}
					break;

				case 0x07:
					if ((bz & 0x0f) == (k & 0x0f)) {	/* hi down or up  */
						if ( bz > k ) n = 5;
						else          n = 4;
					}
					else if ((bz & 0x0f) > (k & 0x0f)) n=7;
					else n=6;							/* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dm(k);
							break;
						case 5:
							hid_dm(k);
							break;
						case 6:
							hil_dm(k);
							break;
						case 7:
							hir_dm(k);
							break;
					}
					break;

				case 0x0e:
					if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) break;

					if ( bz > k ) n = 2;	// um down
					else          n = 0;
					if ((bz & 0x0f) > (k & 0x0f)) n++; /*  um right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dm(k);
							break;
						case 1:
							kaur_dm(k);
							break;
						case 2:
							kadl_dm(k);
							break;
						case 3:
							kadr_dm(k);
							break;
					}
					break;

				case 0x0f:
					if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) ;
					else break;

					if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up */
						if (bz>k) n=5;
						else n=4;
					}
					else if ((bz & 0x0f) > (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dm(k);
							break;
						case 5:
							hid_dm(k);
							break;
						case 6:
							hil_dm(k);
							break;
						case 7:
							hir_dm(k);
							break;
					}	
					break;

				case 0x02:
					*(stoc + st++)=k;
					*(stoc + st++)=4;
					yari_dm(k);
					break;
			}	
		}
	}

	if ( (i=kiki_c[bz]) != 0 ) {				/*  com no kiki stoc  */
		for ( p=kb_c[bz]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];

			switch ( nnn ) {
				case 0x86:
					if ( bz < k ) n = 2;	// ka down
					else          n = 0;
					if ((bz & 0x0f) < (k & 0x0f)) n++; /*  ka right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dc(k);
							break;
						case 1:
							kaur_dc(k);
							break;
						case 2:
							kadl_dc(k);
							break;
						case 3:
							kadr_dc(k);
							break;
					}
					break;

				case 0x87:
					if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up  */
						if (bz<k) n=5;
						else n=4;
					}
					else if ((bz & 0x0f) < (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dc(k);
							break;
						case 5:
							hid_dc(k);
							break;
						case 6:
							hil_dc(k);
							break;
						case 7:
							hir_dc(k);
							break;
					}
					break;

				case 0x8e:
					if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) break;

					if ( bz < k ) n = 2;	// um down
					else          n = 0;
					if ((bz & 0x0f) < (k & 0x0f)) n++; /*  um right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dc(k);
							break;
						case 1:
							kaur_dc(k);
							break;
						case 2:
							kadl_dc(k);
							break;
						case 3:
							kadr_dc(k);
							break;
					}
					break;

				case 0x8f:
					if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) ;
					else break;

					if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up  */
						if (bz<k) n=5;
						else n=4;
					}
					else if ((bz & 0x0f) < (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dc(k);
							break;
						case 5:
							hid_dc(k);
							break;
						case 6:
							hil_dc(k);
							break;
						case 7:
							hir_dc(k);
							break;
					}
					break;

				case 0x82:
					*(stoc + st++)=k;
					*(stoc + st++)=4;
					yari_dc(k);
					break;
			}
		}
	}
    
	// 移動先に利いている長い利きを消す（飛角香が動く場合には先に利きを消す必要がある）。
	if ( (i=kiki_m[az]) != 0 ) {          /*  man no kiki stoc  */
		for ( p=kb_m[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
				case 0x06:
					if ( az > k ) n = 2;	// ka down
					else          n = 0;
					if ((az & 0x0f) > (k & 0x0f)) n++; /*  ka right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dm(k);
							break;
						case 1:
							kaur_dm(k);
							break;
						case 2:
							kadl_dm(k);
							break;
						case 3:
							kadr_dm(k);
							break;
					}
					break;

				case 0x07:
					if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up  */
						if (az>k) n=5;
						else      n=4;
					}
					else if ((az & 0x0f) > (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dm(k);
							break;
						case 5:
							hid_dm(k);
							break;
						case 6:
							hil_dm(k);
							break;
						case 7:
							hir_dm(k);
							break;
					}
					break;

				case 0x0e:
					if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

					if ( az > k ) n = 2;	// um down
					else          n = 0;
					if ((az & 0x0f) > (k & 0x0f)) n++; /*  um right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dm(k);
							break;
						case 1:
							kaur_dm(k);
							break;
						case 2:
							kadl_dm(k);
							break;
						case 3:
							kadr_dm(k);
							break;
					}
					break;

				case 0x0f:
					if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
					else break;

					if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
						if (az>k) n=5;
						else n=4;
					}
					else if ((az & 0x0f) > (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dm(k);
							break;
						case 5:
							hid_dm(k);
							break;
						case 6:
							hil_dm(k);
							break;
						case 7:
							hir_dm(k);
							break;
					}
					break;

				case 0x02:
					*(stoc + st++)=k;
					*(stoc + st++)=4;
					yari_dm(k);
					break;
			}
		}
	}

	if ( (i=kiki_c[az]) != 0 ) {           /*  com no kiki stoc  */
		for ( p=kb_c[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
				case 0x86:
					if ( az < k ) n = 2;	// ka down
					else          n = 0;
					if ((az & 0x0f) < (k & 0x0f)) n++; /*  ka right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dc(k);
							break;
						case 1:
							kaur_dc(k);
							break;
						case 2:
							kadl_dc(k);
							break;
						case 3:
							kadr_dc(k);
							break;
					}
					break;

				case 0x87:
					if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
						if (az<k) n=5;
						else n=4;
					}
					else if ((az & 0x0f) < (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dc(k);
							break;
						case 5:
							hid_dc(k);
							break;
						case 6:
							hil_dc(k);
							break;
						case 7:
							hir_dc(k);
							break;
					}
					break;

				case 0x8e:
					if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

					if ( az < k ) n = 2;	// um down
					else          n = 0;
					if ((az & 0x0f) < (k & 0x0f)) n++; /*  um right */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 0:
							kaul_dc(k);
							break;
						case 1:
							kaur_dc(k);
							break;
						case 2:
							kadl_dc(k);
							break;
						case 3:
							kadr_dc(k);
							break;
					}
					break;

				case 0x8f:
					if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
					else break;

					if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
						if (az<k) n=5;
						else n=4;
					}
					else if ((az & 0x0f) < (k & 0x0f)) n=7;
					else n=6;                    /* hi right or left */

					*(stoc + st++)=k;
					*(stoc + st++)=n;

					switch (n) {
						case 4:
							yari_dc(k);
							break;
						case 5:
							hid_dc(k);
							break;
						case 6:
							hil_dc(k);
							break;
						case 7:
							hir_dc(k);
							break;
					}
					break;

				case 0x82:
					*(stoc + st++)=k;
					*(stoc + st++)=4;

					yari_dc(k);
					break;
			}
		}
	}


	k=ban[bz];                   /*** koma wo totte idou ***/
	kn[k][0]+=nf;
	kn[k][1] =az;                       /** koma bangou **/
	ban[bz]=0;
	ban[az]=k;

	k = init_ban[bz];
									/*** koma no shurui 0x01-0x8f ***/
	init_ban[az]=k+nf;
	init_ban[bz]=0;

	if ( k > 0x80 ) {
		k -= 0x70;						/** shurui 0x01-0x8f -> 0x01-0x1f **/
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x07][az&0x0f] = 1;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka -= n_kati[k];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k      ][az&0x0f] = 1;
#endif
		}
	}
#ifndef YSSFISH_NOHASH
	HashPass();		// 先後判別用のビット反転。24歩 同歩 同飛 23歩 28飛、の局面を区別するため

	HashXor(k,bz);
	HashXor(k+nf,az);
#endif
	if (tk) {
		k=kn[tk][0];              // 取った駒の種類。tkには取った駒の駒番号。
		n = k & 0x07;
		if (k>0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x0f][az&0x0f] = 1;
#endif
			tume_hyouka -= totta_koma[ mo_m[n]++ ][k & 0x0f];	/* motigoma + */
#ifndef YSSFISH_NOHASH
			HashXor(k-0x70,az);
#endif
		} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k       ][az&0x0f] = 1;
#endif
			tume_hyouka += totta_koma[ mo_c[n]++ ][k];
#ifndef YSSFISH_NOHASH
			HashXor(k,az);
#endif
			hash_motigoma += hash_mask[0][n];	/* value */
		}

		*kn_stn[n]++=tk;      /** koma bangou you no stoc ++ **/
		kn[tk][0]=0;
	}

#ifdef KI2_HIKAKU	// 飛角（龍馬）の動く方向の書き換えを最小に。
	if ( f_dir ) {
		if ( (f_dir - 1) & 0x04 ) kb_c[bz][ kiki_c[bz]++ ] = ban[az];	// 元いた場所には動ける。
		else                      kb_m[bz][ kiki_m[bz]++ ] = ban[az];
		ban[bz] = ban[az];
		if ( f_dir == 1 ) {		// 左右に動いた。上下の利きだけを消して書く。
			yari_dm(bz);
			hid_dm(bz);
			ban[bz] = 0;
			yari_wm(az);
			hid_wm(az);
			if ( nf ) ryu4_wm(az);
		} else if ( f_dir == 2 ) {	// 上下。左右を消す。
			hil_dm(bz);
			hir_dm(bz);
			ban[bz] = 0;
			hil_wm(az);
			hir_wm(az);
			if ( nf ) ryu4_wm(az);
		} else if ( f_dir == 5 ) {		// 左右に動いた。上下の利きだけを消して書く。
			yari_dc(bz);
			hid_dc(bz);
			ban[bz] = 0;
			yari_wc(az);
			hid_wc(az);
			if ( nf ) ryu4_wc(az);
		} else if ( f_dir == 6 ) {	// 上下。左右を消す。
			hil_dc(bz);
			hir_dc(bz);
			ban[bz] = 0;
			hil_wc(az);
			hir_wc(az);
			if ( nf ) ryu4_wc(az);
		} else if ( f_dir == 9 ) {		// 角。 12...右上、10...右下、11...左上、9...左下
			kaul_dm(bz);
			kadr_dm(bz);
			ban[bz] = 0;
			kaul_wm(az);
			kadr_wm(az);
			if ( nf ) um4_wm(az);
		} else if ( f_dir == 10 ) {
			kaur_dm(bz);
			kadl_dm(bz);
			ban[bz] = 0;
			kaur_wm(az);
			kadl_wm(az);
			if ( nf ) um4_wm(az);
		} else if ( f_dir == 11 ) {
			kaur_dm(bz);
			kadl_dm(bz);
			ban[bz] = 0;
			kaur_wm(az);
			kadl_wm(az);
			if ( nf ) um4_wm(az);
		} else if ( f_dir == 12 ) {
			kaul_dm(bz);
			kadr_dm(bz);
			ban[bz] = 0;
			kaul_wm(az);
			kadr_wm(az);
			if ( nf ) um4_wm(az);
		} else if ( f_dir == 13 ) {		// 12...右上、10...右下、11...左上、9...左下
			kaul_dc(bz);
			kadr_dc(bz);
			ban[bz] = 0;
			kaul_wc(az);
			kadr_wc(az);
			if ( nf ) um4_wc(az);
		} else if ( f_dir == 14 ) {
			kaur_dc(bz);
			kadl_dc(bz);
			ban[bz] = 0;
			kaur_wc(az);
			kadl_wc(az);
			if ( nf ) um4_wc(az);
		} else if ( f_dir == 15 ) {
			kaur_dc(bz);
			kadl_dc(bz);
			ban[bz] = 0;
			kaur_wc(az);
			kadl_wc(az);
			if ( nf ) um4_wc(az);
		} else if ( f_dir == 16 ) {
			kaul_dc(bz);
			kadr_dc(bz);
			ban[bz] = 0;
			kaul_wc(az);
			kadr_wc(az);
			if ( nf ) um4_wc(az);
		} else PRT("hhhhh\n");
	} else {
		kikiw(az);                             /* kiki kaku */
	}
#else
	kikiw(az);                             /* kiki kaku */
#endif

					  /*****  stoc wo kaku *****/
	for ( ; st ; ) {
		n = *(stoc + --st);        /** number **/
		k = *(stoc + --st);        /** yx     **/

		if ( kn[ ban[k] ][0] < 0x80 ) {   /*  man no kiki kaku */

//			(*func_stoc_write_man[n])(k); // 関数を配列で呼ぶ ---> こっちのほうが遅い・・・。

		 switch (n) {
			case 0:
			   kaul_wm(k);
			   break;
			case 1:
			   kaur_wm(k);
			   break;
			case 2:
			   kadl_wm(k);
			   break;
			case 3:
			   kadr_wm(k);
			   break;
			case 4:
			   yari_wm(k);
			   break;
			case 5:
			   hid_wm(k);
			   break;
			case 6:
			   hil_wm(k);
			   break;
			case 7:
			   hir_wm(k);
			   break;
		 }

		} else {		                    /* com no kiki kaku */
//			(*func_stoc_write_com[n])(k); // 関数を配列で呼ぶ

		switch (n) {
			case 0:
			   kaul_wc(k);
			   break;
			case 1:
			   kaur_wc(k);
			   break;
			case 2:
			   kadl_wc(k);
			   break;
			case 3:
			   kadr_wc(k);
			   break;
			case 4:
			   yari_wc(k);
			   break;
			case 5:
			   hid_wc(k);
			   break;
			case 6:
			   hil_wc(k);
			   break;
			case 7:
			   hir_wc(k);
			   break;
		}


		}

	}	
}

/************  koma modosu.  **************/
void shogi::rmove(int az,int bz,int nf,int tk)
{                           /*   az... ima no yx, bz... moto no yx.  */
							/*   nf... nari flag, tk... totta koma.  */
	int k,n,st=0,*p,i,nnn;

#ifdef _DEBUG
	check_kn();
	if ( init_ban[bz] != 0 || init_ban[az] == 0 ) { PRT("rmove()Err! = %02x,%02x,%02x,%02x\n",bz,az,tk,nf); debug(); }
#endif

   kikid(az);               /*  modosu koma no kiki wo kesu. */

				  /**** moto no long kiki kesu. ****/

   if ( (i=kiki_m[bz]) != 0 )            /*  man no kiki stoc  */
		 for ( p=kb_m[bz]+i ; i ; i--) {

			k=*--p & 0x7f;          /*  k = komabangou  */
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x06:
				  if ( bz>k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((bz & 0x0f) > (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
                     case 2:
                        kadl_dm(k);
                        break;
					 case 3:
                        kadr_dm(k);
                        break;
                  }
				  break;

               case 0x07:
				  if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (bz>k) n=5;
					 else n=4;
                  }
				  else if ((bz & 0x0f) > (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

                  switch (n) {
                     case 4:
                        yari_dm(k);
						break;
                     case 5:
                        hid_dm(k);
						break;
					 case 6:
                        hil_dm(k);
                        break;
					 case 7:
						hir_dm(k);
                        break;
                  }
			   break;

			   case 0x0e:
				  if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) break;

                  if ( bz>k ) n=2;                   /*  um down  */
                  else n = 0;
				  if ((bz & 0x0f) > (k & 0x0f)) n++; /*  um right */

                  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
                     case 0:
						kaul_dm(k);
                        break;
                     case 1:
						kaur_dm(k);
						break;
                     case 2:
                        kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
               break;

               case 0x0f:
                  if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) ;
				  else break;

                  if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up */
                     if (bz>k) n=5;
					 else n=4;
                  }
				  else if ((bz & 0x0f) > (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

                  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
                     case 5:
                        hid_dm(k);
                        break;
                     case 6:
						hil_dm(k);
                        break;
                     case 7:
                        hir_dm(k);
						break;
				  }
               break;

			   case 0x02:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dm(k);
			   break;
			}
		 }

   if ( (i=kiki_c[bz]) != 0 )              /*  com no kiki stoc  */
		 for ( p=kb_c[bz]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];

			switch ( nnn ) {
			   case 0x86:
				  if ( bz<k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((bz & 0x0f) < (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
                        kadl_dc(k);
                        break;
					 case 3:
						kadr_dc(k);
                        break;
                  }
				  break;

			   case 0x87:
                  if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (bz<k) n=5;
					 else n=4;
				  }
				  else if ((bz & 0x0f) < (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
                     case 4:
						yari_dc(k);
                        break;
					 case 5:
                        hid_dc(k);
                        break;
					 case 6:
						hil_dc(k);
                        break;
                     case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x8e:
				  if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) break;

				  if ( bz<k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((bz & 0x0f) < (k & 0x0f)) n++; /*  um right */

                  *(stoc + st++)=k;
                  *(stoc + st++)=n;

                  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
                     case 1:
                        kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
                        break;
                  }
			   break;

               case 0x8f:
                  if ((bz&0x0f)==(k&0x0f) || (bz&0xf0)==(k&0xf0)) ;
                  else break;

				  if ((bz & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (bz<k) n=5;
					 else n=4;
				  }
                  else if ((bz & 0x0f) < (k & 0x0f)) n=7;
                     else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
                     case 4:
                        yari_dc(k);
                        break;
					 case 5:
						hid_dc(k);
                        break;
                     case 6:
                        hil_dc(k);
						break;
					 case 7:
                        hir_dc(k);
						break;
				  }
               break;

			   case 0x82:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
                  yari_dc(k);
               break;
			}
		 }

                        /**** idou saki no long kiki kesu. ****/

   if ( (i=kiki_m[az]) != 0 )            /*  man no kiki stoc  */
		 for ( p=kb_m[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x06:
				  if ( az>k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
                     case 0:
						kaul_dm(k);
						break;
                     case 1:
                        kaur_dm(k);
						break;
					 case 2:
                        kadl_dm(k);
                        break;
					 case 3:
						kadr_dm(k);
                        break;
                  }
				  break;

			   case 0x07:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x0e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

				  if ( az>k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  um right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
					 case 2:
						kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
			   break;

			   case 0x0f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x02:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dm(k);
			   break;
			}
		 }

   if ( (i=kiki_c[az]) != 0 )            /*  com no kiki stoc  */
		 for ( p=kb_c[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x86:
				  if ( az<k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
				  break;

			   case 0x87:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x8e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

				  if ( az<k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  um right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
			   break;

			   case 0x8f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
				  else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x82:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dc(k);
			   break;
			}
		 }

   /*** koma modosu ***/

   k=ban[az];						/*** koma bangou ***/
   kn[k][0]-=nf;
   kn[k][1] =bz;
   ban[bz]=k;

   k = init_ban[az];
   init_ban[bz] = k - nf;		/** koma shurui **/

	if ( k > 0x80 ) {
		k -= 0x70;						/** shurui 0x01-0x8f -> 0x01-0x1f **/
		if ( nf ) {
			tume_hyouka -= n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x07][az&0x0f] = 0;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k & 0x07][az&0x0f] = 0;
#endif
		}
	}
// hash_code = ~hash_code;			/* 先後判別用のビット反転 */
// hash_code ^= hash_ban[k   ][az];
// hash_code ^= hash_ban[k-nf][bz];

#ifndef YSSFISH_NOHASH
   HashPass();		// 先後判別用のビット反転。24歩 同歩 同飛 23歩 28飛、の局面を区別するため

   HashXor(k,az);
   HashXor(k-nf,bz);
#endif

   if (tk) {              // move() では tk = 駒番号、rmove()では駒の種類
	 k = tk & 0x07;
	 if (tk>0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_com[tk & 0x0f][az&0x0f] = 0;
#endif
		tume_hyouka += totta_koma[ --mo_m[k] ][tk & 0x0f];	/* motigoma - */
#ifndef YSSFISH_NOHASH
		HashXor(tk-0x70,az);
#endif
	 }
	 else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_man[tk       ][az&0x0f] = 0;
#endif
		tume_hyouka -= totta_koma[ --mo_c[k] ][tk];
#ifndef YSSFISH_NOHASH
		HashXor(tk,az);
#endif
		hash_motigoma -= hash_mask[0][k];		/* value */
	 }

	 n=*--kn_stn[k];
	 kn[n][0]=tk;
	 kn[n][1]=az;
	 ban[az]=n; 					/** koma bangou **/

	 init_ban[az]=tk;				/** koma shurui **/

	 kikiw(az);                          /* totta koma no kiki kaku */
   }
   else {
	 ban[az]=0;
	 init_ban[az]=0;
   }

   kikiw(bz);                             /* kiki kaku */

					  /*****  stoc wo kaku *****/
   for ( ; st ; ) {
	  n=*(stoc + --st);
	  k=*(stoc + --st);

	  if ( kn[ ban[k] ][0] < 0x80 ) {   /*  man no kiki kaku */
		 switch (n) {
			case 0:
			   kaul_wm(k);
			   break;
			case 1:
			   kaur_wm(k);
			   break;
			case 2:
			   kadl_wm(k);
			   break;
			case 3:
			   kadr_wm(k);
			   break;
			case 4:
			   yari_wm(k);
			   break;
			case 5:
			   hid_wm(k);
			   break;
			case 6:
			   hil_wm(k);
			   break;
			case 7:
			   hir_wm(k);
			   break;
		 }
	  }
	  else switch (n) {                    /* com no kiki kaku */
			case 0:
			   kaul_wc(k);
			   break;
			case 1:
			   kaur_wc(k);
			   break;
			case 2:
			   kadl_wc(k);
			   break;
			case 3:
			   kadr_wc(k);
			   break;
			case 4:
			   yari_wc(k);
			   break;
			case 5:
			   hid_wc(k);
			   break;
			case 6:
			   hil_wc(k);
			   break;
			case 7:
			   hir_wc(k);
			   break;
		 }
   }
}

/******** koma utu.  *********/
void shogi::hit(int az,int uk)
{
							/*   az... utu yx, uk... utu shurui.  */
	int k,n,st=0,*p,i,nnn;

#ifdef NPS_MOVE_COUNT
	move_count++;
#endif
						/**** utu tokoro no long kiki kesu. ****/
#ifdef _DEBUG
//	if ( able_move( 0xff, az, uk, 0 ) == 0 ) {	PRT("ルール違反の指し手! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);	debug(); }
	if ( check_move( 0xff, az, uk, 0 ) == 0 ) {PRT("ルール違反の指し手! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);debug();}
	if ( init_ban[az] != 0 ) {PRT("駒打場所が空白ではない！ az=%x,uk=%x, init_ban[az]=%x\n",az,uk,init_ban[az]);debug();}
	if ( uk > 0x80 && mo_c[uk&0x07] <= 0 ) {PRT("持ち駒を持っていない COM\n");debug();}
	if ( uk < 0x80 && mo_m[uk&0x07] <= 0 ) {PRT("持ち駒を持っていない MAN\n");debug();}
#endif

   if ( (i=kiki_m[az]) != 0 )            /*  man no kiki stoc  */
		 for ( p=kb_m[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x06:
				  if ( az>k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
					 case 2:
						kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
				  break;

			   case 0x07:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x0e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

				  if ( az>k ) n=2;                   /*  um down  */
				  else n= 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  um right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
					 case 2:
						kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
			   break;

			   case 0x0f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x02:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dm(k);
			   break;
			}
		 }

   if ( (i=kiki_c[az]) != 0 )            /*  com no kiki stoc  */
		 for ( p=kb_c[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x86:
				  if ( az<k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
				  break;

			   case 0x87:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x8e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

                  if ( az<k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  um right */

                  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
			   break;

			   case 0x8f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x82:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dc(k);
			   break;
			}
		 }


   k=uk & 0x07;                          /*  koma wo utu     */
   n=*--kn_stn[k];
   kn[n][0]=uk;
   kn[n][1]=az;
   ban[az]=n;

   init_ban[az]=uk;

#ifndef YSSFISH_NOHASH
   HashPass();		// 先後判別用のビット反転。24歩 同歩 同飛 23歩 28飛、の局面を区別するため
#endif

	if (uk<0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_man[k][az&0x0f] = 0;
#endif
		tume_hyouka -= utta_koma[ mo_m[k]-- ][k];	/*    motigoma -    */
#ifndef YSSFISH_NOHASH
		HashXor(uk,az);
#endif
	}
	else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_com[k][az&0x0f] = 0;
#endif
		tume_hyouka += utta_koma[ mo_c[k]-- ][k];
#ifndef YSSFISH_NOHASH
		HashXor(uk-0x70,az);
#endif
		hash_motigoma -= hash_mask[0][k];			/* value */
	}

   kikiw(az);                          /* utta koma no kiki kaku */

					  /*****  stoc wo kaku *****/
   for ( ; st ; ) {
	  n=*(stoc + --st);
	  k=*(stoc + --st);

	  if ( kn[ ban[k] ][0] < 0x80 ) {    /*  man no kiki kaku */
		 switch (n) {
			case 0:
			   kaul_wm(k);
			   break;
			case 1:
			   kaur_wm(k);
			   break;
			case 2:
			   kadl_wm(k);
			   break;
			case 3:
			   kadr_wm(k);
			   break;
			case 4:
			   yari_wm(k);
			   break;
			case 5:
			   hid_wm(k);
			   break;
			case 6:
			   hil_wm(k);
			   break;
			case 7:
			   hir_wm(k);
			   break;
		 }
	  }
	  else switch (n) {                    /* com no kiki kaku */
			case 0:
			   kaul_wc(k);
			   break;
			case 1:
			   kaur_wc(k);
			   break;
			case 2:
			   kadl_wc(k);
			   break;
			case 3:
			   kadr_wc(k);
			   break;
			case 4:
			   yari_wc(k);
			   break;
			case 5:
			   hid_wc(k);
			   break;
			case 6:
			   hil_wc(k);
			   break;
			case 7:
			   hir_wc(k);
			   break;
		 }
   }
}

/******** utigoma modosu. ********/
void shogi::rhit(int az)
{
							/*   az... utta yx   */
   int k,n,st=0,*p,i,nnn;

#ifdef _DEBUG
	k = init_ban[az];
	if ( k == 0 || (k & 0x0f) > 0x08 ) debug();
#endif

   kikid(az);                /* utta koma no kiki kesu. */

						/**** uttta tokoro no long kiki kesu. ****/

   if ( (i=kiki_m[az]) != 0 )            /*  man no kiki stoc  */
		 for ( p=kb_m[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x06:
				  if ( az>k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
					 case 2:
						kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
				  break;

			   case 0x07:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up  */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x0e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

				  if ( az>k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((az & 0x0f) > (k & 0x0f)) n++; /*  um right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dm(k);
						break;
					 case 1:
						kaur_dm(k);
						break;
					 case 2:
						kadl_dm(k);
						break;
					 case 3:
						kadr_dm(k);
						break;
				  }
			   break;

			   case 0x0f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az>k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) > (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dm(k);
						break;
					 case 5:
						hid_dm(k);
						break;
					 case 6:
						hil_dm(k);
						break;
					 case 7:
						hir_dm(k);
						break;
				  }
			   break;

			   case 0x02:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dm(k);
			   break;
			}
		 }

   if ( (i=kiki_c[az]) != 0 )            /*  com no kiki stoc  */
		 for ( p=kb_c[az]+i ; i ; i--) {
			k=*--p & 0x7f;
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
			   case 0x86:
				  if ( az<k ) n=2;                   /*  ka down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  ka right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
				  break;

			   case 0x87:
				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x8e:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) break;

				  if ( az<k ) n=2;                   /*  um down  */
				  else n = 0;
				  if ((az & 0x0f) < (k & 0x0f)) n++; /*  um right */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 0:
						kaul_dc(k);
						break;
					 case 1:
						kaur_dc(k);
						break;
					 case 2:
						kadl_dc(k);
						break;
					 case 3:
						kadr_dc(k);
						break;
				  }
			   break;

			   case 0x8f:
				  if ((az&0x0f)==(k&0x0f) || (az&0xf0)==(k&0xf0)) ;
				  else break;

				  if ((az & 0x0f) == (k & 0x0f)) { /* hi down or up */
					 if (az<k) n=5;
					 else n=4;
				  }
				  else if ((az & 0x0f) < (k & 0x0f)) n=7;
					 else n=6;                    /* hi right or left */

				  *(stoc + st++)=k;
				  *(stoc + st++)=n;

				  switch (n) {
					 case 4:
						yari_dc(k);
						break;
					 case 5:
						hid_dc(k);
						break;
					 case 6:
						hil_dc(k);
						break;
					 case 7:
						hir_dc(k);
						break;
				  }
			   break;

			   case 0x82:
				  *(stoc + st++)=k;
				  *(stoc + st++)=4;
				  yari_dc(k);
			   break;
			}
		 }

   n=ban[az];                            /*  koma modosu  */
   k=kn[n][0];
   nnn = k & 0x07;
	if (k<0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_man[nnn][az&0x0f] = 1;
#endif
		tume_hyouka += utta_koma[ ++mo_m[nnn] ][nnn];   /*    motigoma +    */
#ifndef YSSFISH_NOHASH
		HashXor(k,az);
#endif
	} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_com[nnn][az&0x0f] = 1;
#endif
		tume_hyouka -= utta_koma[ ++mo_c[nnn] ][nnn];
#ifndef YSSFISH_NOHASH
		HashXor(k-0x70,az);
#endif
		hash_motigoma += hash_mask[0][nnn];			/* value */
	}

#ifndef YSSFISH_NOHASH
   HashPass();		// 先後判別用のビット反転。24歩 同歩 同飛 23歩 28飛、の局面を区別するため
#endif

   *kn_stn[nnn]++=n;
   kn[n][0]=0;
   ban[az]=0;
   init_ban[az]=0;
					  /*****  stoc wo kaku *****/
   for ( ; st ; ) {
	  n=*(stoc + --st);
	  k=*(stoc + --st);

	  if ( kn[ ban[k] ][0] < 0x80 ) {     /*  man no kiki kaku */
		 switch (n) {
			case 0:
			   kaul_wm(k);
			   break;
			case 1:
			   kaur_wm(k);
			   break;
			case 2:
			   kadl_wm(k);
			   break;
			case 3:
			   kadr_wm(k);
			   break;
			case 4:
			   yari_wm(k);
			   break;
			case 5:
			   hid_wm(k);
			   break;
			case 6:
			   hil_wm(k);
			   break;
			case 7:
			   hir_wm(k);
			   break;
		 }
	  }
	  else switch (n) {                    /* com no kiki kaku */
			case 0:
			   kaul_wc(k);
			   break;
			case 1:
			   kaur_wc(k);
			   break;
			case 2:
			   kadl_wc(k);
			   break;
			case 3:
			   kadr_wc(k);
			   break;
			case 4:
			   yari_wc(k);
			   break;
			case 5:
			   hid_wc(k);
			   break;
			case 6:
			   hil_wc(k);
			   break;
			case 7:
			   hir_wc(k);
			   break;
		 }
   }
}


/*
2歩データを常に保持するには
テーブルに持つ。歩だけでなく、全部の駒に対して持つ。
歩を打った場合は

nifu_com_table[k&0x07][az&0x0f] = 0;

歩を戻す場合は

nifu_com_table[k&0x07][az&0x0f] = 1;

駒が移動する場合は、
・敵の歩を取る場合、
nifu_com_table[tk&0x0f][az&0x0f] = 1;

・自分の歩が成る場合、

k==0x01 && nf=0x08;
nifu_com_table[k&0x0f][az&0x0f] = 1;

nifu_c() ... 歩が打てれば1を返す。

*/

// ハッシュの確認をするだけなので利きは操作しない（盤面の内容とハッシュコードだけ）
void shogi::move_hash(int az,int bz,int nf)
{
	int k,n,tk;

#ifdef _DEBUG
	// hash_moveでは利きの内容は動かしてないのでチェックは無意味	
	tk = init_ban[az];
	if ( check_move_hash( bz, az, tk, nf ) == 0 ) {PRT("ルール違反の指し手! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
//	if ( able_move( bz, az, tk, nf ) == 0 ) {PRT("\n\nルール違反の指し手! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
#endif

	tk = ban[az];
	k  = ban[bz];                   /*** koma wo totte idou ***/
	kn[k][0] += nf;
	kn[k][1]  = az;                       /** koma bangou **/
	ban[bz] = 0;
	ban[az] = k;

	k = init_ban[bz];
									/*** koma no shurui 0x01-0x8f ***/
	init_ban[az]=k+nf;
	init_ban[bz]=0;

	if ( k > 0x80 ) {
		k -= 0x70;					/** shurui 0x01-0x8f -> 0x01-0x1f **/
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x07][az&0x0f] = 1;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka -= n_kati[k];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k       ][az&0x0f] = 1;
#endif
		}
	}

	HashPass();

	HashXor(k   ,bz);
	HashXor(k+nf,az);

	if (tk) {
		k = kn[tk][0];              /*** totta koma no syurui ***/
		n = k & 0x07;
		if (k>0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x0f][az&0x0f] = 1;
#endif
			tume_hyouka -= totta_koma[ mo_m[n]++ ][k & 0x0f];	/* motigoma + */
			HashXor(k - 0x70,az);
		} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k       ][az&0x0f] = 1;
#endif
			tume_hyouka += totta_koma[ mo_c[n]++ ][k];
			HashXor(k,az);
			hash_motigoma += hash_mask[0][n];	/* value */
		}

		*kn_stn[n]++ = tk;      // 駒番号用のstocに ++
		kn[tk][0]=0;
	}
}

void shogi::rmove_hash(int az,int bz,int nf,int tk)
{
	int k,n;

	k=ban[az];						/*** koma bangou ***/
	kn[k][0]-=nf;
	kn[k][1] =bz;
	ban[bz]=k;

	 k = init_ban[az];
	init_ban[bz] = k - nf;		/** koma shurui **/

	if ( k > 0x80 ) {
		k -= 0x70;						/** shurui 0x01-0x8f -> 0x01-0x1f **/
		if ( nf ) {
			tume_hyouka -= n_kati[k & 0x07];	// 駒を進める場合と、戻す場合で、駒の種類が違うので&を取る値は注意！
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[k & 0x07][az&0x0f] = 0;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[k & 0x07][az&0x0f] = 0;
#endif
		}
	}

	HashPass();		// 先後判別用のビット反転。
	HashXor(k,az);
	HashXor(k-nf,bz);

	if (tk) {					// move() では tk = 駒番号。こちらでは種類。注意！！
		k = tk & 0x07;
		if (tk>0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_com[tk & 0x0f][az&0x0f] = 0;
#endif
			tume_hyouka += totta_koma[ --mo_m[k] ][tk & 0x0f];	/* motigoma - */
			HashXor(tk - 0x70,az);
		} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
			nifu_table_man[tk       ][az&0x0f] = 0;
#endif
			tume_hyouka -= totta_koma[ --mo_c[k] ][tk];
			HashXor(tk,az);
			hash_motigoma -= hash_mask[0][k];		/* value */
		}

		n=*--kn_stn[k];
		kn[n][0]=tk;
		kn[n][1]=az;
		ban[az]=n; 					/** koma bangou **/

		init_ban[az]=tk;			/** koma shurui **/

	} else {
		ban[az]=0;
		init_ban[az]=0;
	}
}


void shogi::hit_hash(int az,int uk)
{
	int k,n;

#ifdef _DEBUG
	// 駒打ちのチェックは利きの内容に関係ないのでOK
	if ( check_move( 0xff, az, uk, 0 ) == 0 ) {PRT("ルール違反の指し手! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);debug();}
#endif

	k = uk & 0x07;                          /*  koma wo utu     */
	n = *--kn_stn[k];
	kn[n][0] = uk;
	kn[n][1] = az;
	ban[az] = n;

	init_ban[az] = uk;

	HashPass();		// 先後判別用のビット反転。24歩 同歩 同飛 23歩 28飛、の局面を区別するため

	if (uk<0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_man[k][az&0x0f] = 0;
#endif
		tume_hyouka -= utta_koma[ mo_m[k]-- ][k];	/*    motigoma -    */
		HashXor(uk,az);
	} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_com[k][az&0x0f] = 0;
#endif
		tume_hyouka += utta_koma[ mo_c[k]-- ][k];
		HashXor(uk-0x70,az);
		hash_motigoma -= hash_mask[0][k];			// COMの持ち駒の枚数を示す
	}
}

void shogi::rhit_hash(int az)
{
	int k,n,nnn;
  
#ifdef _DEBUG
	k = init_ban[az];
	if ( k == 0 || (k & 0x0f) > 0x08 ) debug();
#endif

	n = ban[az];                            /*  koma modosu  */
	k = kn[n][0];
	nnn = k & 0x07;
	if (k<0x80) {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_man[nnn][az&0x0f] = 1;
#endif
		tume_hyouka += utta_koma[ ++mo_m[nnn] ][nnn];   /*    motigoma +    */
		HashXor(k,az);
	} else {
#ifdef NIFU_TABLE		// 2歩データを常にテーブルに保持する場合
		nifu_table_com[nnn][az&0x0f] = 1;
#endif
		tume_hyouka -= utta_koma[ ++mo_c[nnn] ][nnn];
		HashXor(k - 0x70,az);
		hash_motigoma += hash_mask[0][nnn];			/* value */
	}

	HashPass();

	*kn_stn[nnn]++ = n;
	kn[n][0] = 0;
	ban[az] = 0;
	init_ban[az] = 0;
}

/*** 利きを全部書く、消す ***/
void shogi::allkaku()
{
	int x,y,z;
	for (y=1;y<10;y++) for (x=1;x<10;x++) {
		z = y*16 + x;
		if ( ban[z] != 0 ) kikiw(z);
	}
}

void shogi::allkesu()
{
	int x,y,z;
	
	for (y=1;y<10;y++) for (x=1;x<10;x++) {
		z= y*16 + x;
		if ( ban[z] != 0 ) kikid(z);
	}
}

void shogi::kiki_write(int z)  { kikiw(z); }
void shogi::kiki_delete(int z) { kikid(z); }
