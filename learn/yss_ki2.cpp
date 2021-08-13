// 2019 Team AobaZero
// This source code is in the public domain.
/* Yamashita Shogi System */
#include "yss_ext.h"     /*** load extern ***/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss.h"		// ���饹�����

/*******  koma ugokasu.  (man)   ********/

// �ַ����֤���int�ΰ�����1�Ĥ�ġ״ؿ��ؤΥݥ��󥿡�������
extern void (shogi::*func_kiki_write[0x90])(int);	// o_ki1.c �����
extern void (shogi::*func_kiki_delete[0x90])(int);

// ����˥ޥ���ˤ���ʸƤֲ�����;������Ⱥ���ʤΤǡ�
#define kikiw(z) (this->*func_kiki_write[  init_ban[z] ])(z)
#define kikid(z) (this->*func_kiki_delete[ init_ban[z] ])(z)

//extern void (*func_kiki_delete_hoge[0x90])(int,int);	// 2�İ�������ľ��

#ifdef NPS_MOVE_COUNT
	int move_count;	// move,hit��ƤӽФ�����������̤�ºݤ�ư������������������
#endif

//#define KI2_HIKAKU	// ���ѡ�ζ�ϡˤ�ư�������ν񤭴�����Ǿ��ˡ���õ���Ǥ�20.1�ä��Ѥ�餺�������ͤǤ�10.3�ä�10.4�äǵդ��٤��ʤä���
/*
���Ѥ�ư���������������̤�kikiw()��Ȥ�������¿ʬ2%���餤��®������롣
bz-az�ξ����ߤ�����
��������¾�ζ�λ����̾��̤ꡢ"z" ��ս�ǽ������롣

��ľ�˽񤯤ȡ�
move(az,bz,nf)
switch (init_ban[bz])

if ( k==0 ) {
	
}


k = init_ban[bz] & 0x0f;

if ( (k & 0x07)==0x07 ) {	// ��
	dir = hisha_dir[bz - az + 0x99];
	if ( dir == 0 ) �̾�������ä��������񤯡�
	�岼�˿ʤ���ʺ����Τ߾ä��ƽ񤯡�
	������硢���������ˤΤߡ����Ф��ƽ񤯡�

	������ʬ���������ư����������񤯡�
	��ʬ��ư������������ä���
	
	�����ư�����ϼФ�4�ޥ���������ä���bz
	�������硢���������äƤ�����ϡ�������Ʊ���˽񤯡�az
}

���ư�ξ��ϡ������ä��Ƚ񤭤�ɬ���Фˤʤ�Τ�Ʊ���˽������Ƥ�OK��
�ޤ�����ä��Ȥ��Ƥ������ξ���Ϥʤ���Ѳ����ʤ��Τ�OK����Ĺ������������ơ�


		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
//		if ( dx && dy ) �̾��ζ��ư����
		if ( dy == 0 ) ����
		else {
			if ( dx == 0 ) �岼
			else �̾�
		}
	}
	if ( (k & 0x07)==0x06 ) {	// ��
		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
//		if ( dx && dy ) �̾��ζ��ư����
		if ( dy == 0 ) ����
		else {
			if ( dx == 0 ) �岼
			else �̾�
		}
	}
} else �̾�

���а��֤��ȯ�����󤫤���äƤ���ʤ���
256x256�ʤ�Ǥ��롣
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
0x19  0x21	 -0x08	---> ���줬��ʣ���롪
0x19  0x22	 -0x09
0x19  0x23	 -0x0a
0x19  0x24	 -0x0b
0x19  0x25	 -0x0c
0x19  0x26	 -0x0d
0x19  0x27	 -0x0e
0x19  0x28	 -0x0f
0x19  0x29	 -0x10
...

�������ºݤϡ��������Ѥΰ�ư�Ǥ�����ϸ¤��Ƥ���Τǡ���ư������κ���Ȥä����ˡ�
���Ф˽ФƤ��ʤ����͡��Ȥ����Τ����롣

�㤨�����֤�81�ޥ�����80�ޥ���ư����櫓�ǤϤʤ���
81�ޥ��������16�ޥ��ˤ���ư���ʤ����Ĥޤ�81*16 = 1280+16=1296 �̤ꤷ��¸�ߤ��ʤ���
ζ���Ȥ��Ƥ�81*(16+4)=81*20=1620�̤�ʲ���

�������Ƥ� bz - az ��׻�����ȡ������⤷��������ζ�μФ�4�ޥ�����������Ω�ʤ麹ʬ���äƷ׻��Ǥ��롣
�Ȥ��������̤�ü������ü�إ�פ���ư���Ϥʤ��ΤǷ׻��Ǥ���Ϥ����ơ��֥��0x11-0x99 = 0x88*2 �ǽ�ʬ��300�Х������٤���

���Ͼ岼�������̾��8�ս�ä���8�ս�񤯡��Ǿ��ʤ�4�ս�ä���4�ս�񤯡�
�Ф��6�ս�ä���6�ս�񤯡�


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
// �ַ����֤���int�ΰ�����1�Ĥ�ġ״ؿ��ؤΥݥ��󥿡�������
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


void shogi::move(int az,int bz,int nf) //  az... ��ư���� yx, bz... ���� yx, nf... ���ե饰
{
	int k,n,tk,st=0,i,nnn;
	int *p;
//	int stoc[32];	// ������ǻ��Ĥ� 71.8 �� ---> 72.4 �� 0.8%�٤��ʤ롣

#ifdef NPS_MOVE_COUNT
	move_count++;
#endif

#ifdef KI2_HIKAKU	// ���ѡ�ζ�ϡˤ�ư�������ν񤭴�����Ǿ��ˡ�
	int f_dir;
#endif

#ifdef _DEBUG
	tk = init_ban[az];
//	check_kn();
	if ( check_move( bz, az, tk, nf ) == 0 ) {PRT("�롼���ȿ�λؤ���! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
//	if ( able_move( bz, az, tk, nf ) == 0 ) {PRT("�롼���ȿ�λؤ���! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
	check_kn();
#endif

	if ( (tk=ban[az]) != 0 ) kikid(az);		// �����������ä�

#ifdef KI2_HIKAKU	// ���ѡ�ζ�ϡˤ�ư�������ν񤭴�����Ǿ��ˡ�
	k = init_ban[bz];
	f_dir = 0;
	if ( (k & 0x0f)==0x07 && tk == 0 ) {	// MAN����
		int dx,dy;
		dx = (bz & 0x0f) - (az & 0x0f);
		dy = (bz & 0xf0) - (az & 0xf0);
		if      ( dy == 0 ) f_dir = 1;	// �����ΰ�ư
		else if ( dx == 0 ) f_dir = 2;	// �岼
//		else                f_dir = 0;	// �̾�
		if ( k > 0x80 ) f_dir += 4;
	} else if ( (k & 0x0f)==0x06 && tk == 0 ) {	// MAN�γ�
		if ( bz < az ) f_dir = 9;		// ��
		else           f_dir = 11;
		if ( (bz & 0x0f) < (az & 0x0f) ) f_dir++;	// ��
		// 12...���塢10...������11...���塢9...����
		if ( k > 0x80 ) f_dir += 4;
	}
	
	if ( f_dir ) {	// ��ư���������ä�����ս����
		int *pk;
		k = ban[bz];	// ���ֹ�
		if ( (f_dir - 1) & 0x04 ) { p=kb_c[az]; pk=--kiki_c[az]+p; }
		else                      { p=kb_m[az]; pk=--kiki_m[az]+p; }
		if (k-*p) if (k-*++p) if (k-*++p) if (k-*++p) if (k-*++p) if (k-*++p) { PRT("hoge\n"); debug(); }
		*p=*pk;                                 
	} else {
		kikid(bz);								// ư�������������ä����̾��
	}
#else
	kikid(bz);								// ư�������������ä�
#endif

											// ����Ĺ��������ä�

	if ( (i=kiki_m[bz]) != 0 ) {			//  man �������� stoc ��
		for ( p = kb_m[bz] + i ; i ; i--) {
			k = *--p & 0x7f;				//  k ... ���ֹ�
			nnn = kn[k][0];
			k   = kn[k][1];
			switch ( nnn ) {
				case 0x06:
					if ( bz > k ) n = 2;	// ka down
					else          n = 0;
					if ((bz & 0x0f) > (k & 0x0f)) n++; /*  ka right */

					*(stoc + st++) = k;
					*(stoc + st++) = n;

					// ³���ƽ񤯤ȥ���ѥ��餬 on goto ... �Ǻ�Ŭ�����Ƥ���Ƥ�����
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
    
	// ��ư��������Ƥ���Ĺ��������ä������ѹ᤬ư�����ˤ����������ä�ɬ�פ�����ˡ�
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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x07][az&0x0f] = 1;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka -= n_kati[k];
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_man[k      ][az&0x0f] = 1;
#endif
		}
	}
#ifndef YSSFISH_NOHASH
	HashPass();		// ���Ƚ���ѤΥӥå�ȿž��24�� Ʊ�� Ʊ�� 23�� 28�����ζ��̤���̤��뤿��

	HashXor(k,bz);
	HashXor(k+nf,az);
#endif
	if (tk) {
		k=kn[tk][0];              // ��ä���μ��ࡣtk�ˤϼ�ä���ζ��ֹ档
		n = k & 0x07;
		if (k>0x80) {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x0f][az&0x0f] = 1;
#endif
			tume_hyouka -= totta_koma[ mo_m[n]++ ][k & 0x0f];	/* motigoma + */
#ifndef YSSFISH_NOHASH
			HashXor(k-0x70,az);
#endif
		} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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

#ifdef KI2_HIKAKU	// ���ѡ�ζ�ϡˤ�ư�������ν񤭴�����Ǿ��ˡ�
	if ( f_dir ) {
		if ( (f_dir - 1) & 0x04 ) kb_c[bz][ kiki_c[bz]++ ] = ban[az];	// ���������ˤ�ư���롣
		else                      kb_m[bz][ kiki_m[bz]++ ] = ban[az];
		ban[bz] = ban[az];
		if ( f_dir == 1 ) {		// ������ư�������岼������������ä��ƽ񤯡�
			yari_dm(bz);
			hid_dm(bz);
			ban[bz] = 0;
			yari_wm(az);
			hid_wm(az);
			if ( nf ) ryu4_wm(az);
		} else if ( f_dir == 2 ) {	// �岼��������ä���
			hil_dm(bz);
			hir_dm(bz);
			ban[bz] = 0;
			hil_wm(az);
			hir_wm(az);
			if ( nf ) ryu4_wm(az);
		} else if ( f_dir == 5 ) {		// ������ư�������岼������������ä��ƽ񤯡�
			yari_dc(bz);
			hid_dc(bz);
			ban[bz] = 0;
			yari_wc(az);
			hid_wc(az);
			if ( nf ) ryu4_wc(az);
		} else if ( f_dir == 6 ) {	// �岼��������ä���
			hil_dc(bz);
			hir_dc(bz);
			ban[bz] = 0;
			hil_wc(az);
			hir_wc(az);
			if ( nf ) ryu4_wc(az);
		} else if ( f_dir == 9 ) {		// �ѡ� 12...���塢10...������11...���塢9...����
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
		} else if ( f_dir == 13 ) {		// 12...���塢10...������11...���塢9...����
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

//			(*func_stoc_write_man[n])(k); // �ؿ�������ǸƤ� ---> ���ä��Τۤ����٤���������

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
//			(*func_stoc_write_com[n])(k); // �ؿ�������ǸƤ�

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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x07][az&0x0f] = 0;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_man[k & 0x07][az&0x0f] = 0;
#endif
		}
	}
// hash_code = ~hash_code;			/* ���Ƚ���ѤΥӥå�ȿž */
// hash_code ^= hash_ban[k   ][az];
// hash_code ^= hash_ban[k-nf][bz];

#ifndef YSSFISH_NOHASH
   HashPass();		// ���Ƚ���ѤΥӥå�ȿž��24�� Ʊ�� Ʊ�� 23�� 28�����ζ��̤���̤��뤿��

   HashXor(k,az);
   HashXor(k-nf,bz);
#endif

   if (tk) {              // move() �Ǥ� tk = ���ֹ桢rmove()�Ǥ϶�μ���
	 k = tk & 0x07;
	 if (tk>0x80) {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_com[tk & 0x0f][az&0x0f] = 0;
#endif
		tume_hyouka += totta_koma[ --mo_m[k] ][tk & 0x0f];	/* motigoma - */
#ifndef YSSFISH_NOHASH
		HashXor(tk-0x70,az);
#endif
	 }
	 else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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
//	if ( able_move( 0xff, az, uk, 0 ) == 0 ) {	PRT("�롼���ȿ�λؤ���! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);	debug(); }
	if ( check_move( 0xff, az, uk, 0 ) == 0 ) {PRT("�롼���ȿ�λؤ���! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);debug();}
	if ( init_ban[az] != 0 ) {PRT("���Ǿ�꤬����ǤϤʤ��� az=%x,uk=%x, init_ban[az]=%x\n",az,uk,init_ban[az]);debug();}
	if ( uk > 0x80 && mo_c[uk&0x07] <= 0 ) {PRT("���������äƤ��ʤ� COM\n");debug();}
	if ( uk < 0x80 && mo_m[uk&0x07] <= 0 ) {PRT("���������äƤ��ʤ� MAN\n");debug();}
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
   HashPass();		// ���Ƚ���ѤΥӥå�ȿž��24�� Ʊ�� Ʊ�� 23�� 28�����ζ��̤���̤��뤿��
#endif

	if (uk<0x80) {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_man[k][az&0x0f] = 0;
#endif
		tume_hyouka -= utta_koma[ mo_m[k]-- ][k];	/*    motigoma -    */
#ifndef YSSFISH_NOHASH
		HashXor(uk,az);
#endif
	}
	else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_man[nnn][az&0x0f] = 1;
#endif
		tume_hyouka += utta_koma[ ++mo_m[nnn] ][nnn];   /*    motigoma +    */
#ifndef YSSFISH_NOHASH
		HashXor(k,az);
#endif
	} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_com[nnn][az&0x0f] = 1;
#endif
		tume_hyouka -= utta_koma[ ++mo_c[nnn] ][nnn];
#ifndef YSSFISH_NOHASH
		HashXor(k-0x70,az);
#endif
		hash_motigoma += hash_mask[0][nnn];			/* value */
	}

#ifndef YSSFISH_NOHASH
   HashPass();		// ���Ƚ���ѤΥӥå�ȿž��24�� Ʊ�� Ʊ�� 23�� 28�����ζ��̤���̤��뤿��
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
2��ǡ��������ݻ�����ˤ�
�ơ��֥�˻��ġ�������Ǥʤ��������ζ���Ф��ƻ��ġ�
����Ǥä�����

nifu_com_table[k&0x07][az&0x0f] = 0;

����᤹����

nifu_com_table[k&0x07][az&0x0f] = 1;

�𤬰�ư������ϡ�
��Ũ��������硢
nifu_com_table[tk&0x0f][az&0x0f] = 1;

����ʬ���⤬�����硢

k==0x01 && nf=0x08;
nifu_com_table[k&0x0f][az&0x0f] = 1;

nifu_c() ... �⤬�ǤƤ��1���֤���

*/

// �ϥå���γ�ǧ�򤹤�����ʤΤ����������ʤ������̤����Ƥȥϥå��女���ɤ�����
void shogi::move_hash(int az,int bz,int nf)
{
	int k,n,tk;

#ifdef _DEBUG
	// hash_move�Ǥ����������Ƥ�ư�����Ƥʤ��Τǥ����å���̵��̣	
	tk = init_ban[az];
	if ( check_move_hash( bz, az, tk, nf ) == 0 ) {PRT("�롼���ȿ�λؤ���! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
//	if ( able_move( bz, az, tk, nf ) == 0 ) {PRT("\n\n�롼���ȿ�λؤ���! bz,az,tk,nf = %x,%x,%x,%x \n",bz,az,tk,nf);debug();}
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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x07][az&0x0f] = 1;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka -= n_kati[k];
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x0f][az&0x0f] = 1;
#endif
			tume_hyouka -= totta_koma[ mo_m[n]++ ][k & 0x0f];	/* motigoma + */
			HashXor(k - 0x70,az);
		} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_man[k       ][az&0x0f] = 1;
#endif
			tume_hyouka += totta_koma[ mo_c[n]++ ][k];
			HashXor(k,az);
			hash_motigoma += hash_mask[0][n];	/* value */
		}

		*kn_stn[n]++ = tk;      // ���ֹ��Ѥ�stoc�� ++
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
			tume_hyouka -= n_kati[k & 0x07];	// ���ʤ����ȡ��᤹���ǡ���μ��ब�㤦�Τ�&�����ͤ���ա�
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[k & 0x07][az&0x0f] = 0;
#endif
		}
	} else {
		if ( nf ) {
			tume_hyouka += n_kati[k & 0x07];
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_man[k & 0x07][az&0x0f] = 0;
#endif
		}
	}

	HashPass();		// ���Ƚ���ѤΥӥå�ȿž��
	HashXor(k,az);
	HashXor(k-nf,bz);

	if (tk) {					// move() �Ǥ� tk = ���ֹ档������Ǥϼ��ࡣ��ա���
		k = tk & 0x07;
		if (tk>0x80) {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
			nifu_table_com[tk & 0x0f][az&0x0f] = 0;
#endif
			tume_hyouka += totta_koma[ --mo_m[k] ][tk & 0x0f];	/* motigoma - */
			HashXor(tk - 0x70,az);
		} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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
	// ���Ǥ��Υ����å������������Ƥ˴ط��ʤ��Τ�OK
	if ( check_move( 0xff, az, uk, 0 ) == 0 ) {PRT("�롼���ȿ�λؤ���! hit() bz,az,tk,nf = %x,%x,%x,%x \n",0xff,az,uk,0);debug();}
#endif

	k = uk & 0x07;                          /*  koma wo utu     */
	n = *--kn_stn[k];
	kn[n][0] = uk;
	kn[n][1] = az;
	ban[az] = n;

	init_ban[az] = uk;

	HashPass();		// ���Ƚ���ѤΥӥå�ȿž��24�� Ʊ�� Ʊ�� 23�� 28�����ζ��̤���̤��뤿��

	if (uk<0x80) {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_man[k][az&0x0f] = 0;
#endif
		tume_hyouka -= utta_koma[ mo_m[k]-- ][k];	/*    motigoma -    */
		HashXor(uk,az);
	} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_com[k][az&0x0f] = 0;
#endif
		tume_hyouka += utta_koma[ mo_c[k]-- ][k];
		HashXor(uk-0x70,az);
		hash_motigoma -= hash_mask[0][k];			// COM�λ����������򼨤�
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
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
		nifu_table_man[nnn][az&0x0f] = 1;
#endif
		tume_hyouka += utta_koma[ ++mo_m[nnn] ][nnn];   /*    motigoma +    */
		HashXor(k,az);
	} else {
#ifdef NIFU_TABLE		// 2��ǡ������˥ơ��֥���ݻ�������
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

/*** �����������񤯡��ä� ***/
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
