// 2023 Team AobaZero
// This source code is in the public domain.
#include "../config.h"

#include <numeric>
#include <random>
#include <algorithm>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <chrono>
#include <exception>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#if !defined(_MSC_VER)
#include <sys/time.h>
#include <unistd.h>
#endif

#include "../Network.h"
#include "../GTP.h"
#include "../Random.h"

#include "shogi.h"

#include "lock.h"
#include "yss_var.h"
#include "yss_dcnn.h"
#include "process_batch.h"


/*
実現確率が高い局面を10分考えさせて定跡を作成。
初形で10分考えて着手回数を記憶。これを確率として延々作成。
初手▲26歩が0.65ならこれを選ぶ(▲76歩は0.20)。2手目△84歩が0.50、
3回目は
▲26歩(0.65) * △84歩(0.50) = 0.325  これを選ぶ
▲76歩(0.20)                = 0.200
▲78金(0.10)                = 0.100
*/


const char R_BOOK_FILE[] = "r_book.bin";
//const char R_BOOK_FILE[] = "r_book_lose.bin";
//const char R_BOOK_FILE[] = "r_book_kaku35.bin";

typedef struct R_BOOK : HASH_SHOGI {	// Realization Probability Book  構造体からも継承できるのか。怪しい使い方だけど。
	double prob;			// Realization Probability
	std::string str_seq;	// 現在局面までの手順
//	int weight;				// 作成したweight番号
} R_BOOK;

std::vector <R_BOOK> r_book;
const int R_BOOK_SIZE = 1024*64;
int R_BOOK_MASK;
int r_book_use = 0;
double total_prob;

const int R_INIT = 0;
const int R_DONE = 1;	// 展開済み
 

void r_book_reset()
{
	for (int i=0;i<R_BOOK_SIZE;i++) {
		R_BOOK *r = &r_book[i];
		r->deleted = 1;
		LockInit(r->entry_lock);
		std::vector<CHILD>().swap(r->child);	// memory free hack for vector. 
	}
	r_book_use = 0;
}

void r_book_clear()
{
	R_BOOK_MASK = R_BOOK_SIZE - 1;
	HASH_ALLOC_SIZE size = sizeof(R_BOOK) * R_BOOK_SIZE;
	r_book.resize(R_BOOK_SIZE);	// reserve()だと全要素のコンストラクタが走らないのでダメ
	PRT("R_BOOK_SIZE=%7d(%3dMB),sizeof(R_BOOK)=%d,sizeof(CHILD)=%d\n",R_BOOK_SIZE,(int)(size/(1024*1024)),sizeof(R_BOOK),sizeof(CHILD));
	r_book_reset();
}

void save_r_book()
{
	FILE *fp = fopen(R_BOOK_FILE,"wb");
	if ( !fp ) DEBUG_PRT("");
	fwrite( &r_book_use ,sizeof(int), 1, fp);
	for (int i=0;i<r_book_use;i++) {
		R_BOOK *r = &r_book[i];
		fwrite( &r->hashcode64  ,sizeof(uint64), 1, fp);
		fwrite( &r->hash64pos   ,sizeof(uint64), 1, fp);
		fwrite( &r->mate_bit    ,sizeof(int   ), 1, fp);
		fwrite( &r->win_sum     ,sizeof(float ), 1, fp);
		fwrite( &r->squared_eval,sizeof(float ), 1, fp);
		fwrite( &r->games_sum   ,sizeof(int   ), 1, fp);
		fwrite( &r->col	        ,sizeof(int   ), 1, fp);
		fwrite( &r->age	        ,sizeof(int   ), 1, fp);
		fwrite( &r->net_value   ,sizeof(float ), 1, fp);
		fwrite( &r->child_num   ,sizeof(int   ), 1, fp);
		for (int j=0; j<r->child_num; j++) {
			fwrite( &r->child[j],sizeof(CHILD), 1, fp);
		}
		fwrite( &r->prob       ,sizeof(double ), 1, fp);
		int size = r->str_seq.size();
		fwrite( &size          ,sizeof(int    ), 1, fp);
		fwrite(  r->str_seq.c_str() , size     , 1, fp);
	}
	fclose(fp);
}

void load_r_book()
{
	FILE *fp = fopen(R_BOOK_FILE,"rb");
	if ( !fp ) {
		PRT("no %s. start new book.\n",R_BOOK_FILE);
		return;
	}
	if ( fread( &r_book_use ,sizeof(int), 1, fp) != 1 ) DEBUG_PRT("");
	int cv[100] = {0};
	int cc[2] = {0};
	for (int i=0;i<r_book_use;i++) {
		R_BOOK *r = &r_book[i];
		if ( fread( &r->hashcode64  ,sizeof(uint64), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->hash64pos   ,sizeof(uint64), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->mate_bit    ,sizeof(int   ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->win_sum     ,sizeof(float ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->squared_eval,sizeof(float ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->games_sum   ,sizeof(int   ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->col         ,sizeof(int   ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->age         ,sizeof(int   ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->net_value   ,sizeof(float ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( fread( &r->child_num   ,sizeof(int   ), 1, fp) != 1 ) DEBUG_PRT("");

		int max_g = 0, max_i = -1, dones = 0;
		r->child.resize(r->child_num);
		for (int j=0; j<r->child_num; j++) {
			CHILD *pc = &r->child[j];
			if ( fread( pc,sizeof(CHILD), 1, fp) != 1 ) DEBUG_PRT("");
			if ( pc->mate_bit == R_DONE ) dones++;
			if ( pc->games > max_g ) { max_g = pc->games; max_i = j; }
//			PRT("%2d:%8x,%3d,(%6.3f),bias=%6.3f\n",j,pc->move,pc->games,pc->value,pc->bias);
		}
		if ( fread( &r->prob       ,sizeof(double ), 1, fp) != 1 ) DEBUG_PRT("");
		int size = 0;
		if ( fread( &size          ,sizeof(int    ), 1, fp) != 1 ) DEBUG_PRT("");
		if ( size==0 ) DEBUG_PRT("");
		char *buf = new char[size+1];
		if ( fread( buf, size     , 1, fp) != 1 ) DEBUG_PRT("");
		buf[size] = 0;
		r->str_seq = buf;
		delete buf;
		cc[r->col]++;
		CHILD *pc = &r->child[max_i];
		float v = pc->value;
		if ( r->col == 1 ) v = -v;
		v = (v + 1.0) * 50;	// -1 < v < +1   -->  0 < v < +100
		if ( v < 0 ) v = 0;
		if ( v > +99 ) v = +99;
		cv[(int)v]++;
		int book_ply = std::count(r->str_seq.begin(), r->str_seq.end(), ' ')-2;
		if ( i > r_book_use-10 || book_ply > 51 ) PRT("%4d:%10f,%016" PRIx64 ",%8d,%d,%6.3f,%2d,ply=%d,%s\n",i,r->prob,r->hash64pos,r->games_sum,r->col,pc->value,dones,book_ply,r->str_seq.c_str()+23);
	}

	int same = 0;
	for (int i=0;i<r_book_use-1;i++) {
		R_BOOK *r = &r_book[i];
		if ( r->hash64pos == 0 ) continue;
		for (int j=i+1;j<r_book_use;j++) {
			R_BOOK *q = &r_book[j];
			if ( q->hash64pos == 0 ) continue;
			if ( r->hash64pos == q->hash64pos ) { q->hash64pos = 0; same++; }
		}
	}
	for (int i=0;i<100;i++) if ( cv[i] ) PRT("%3d:%4d(%5.2f %%)\n",i,cv[i],100.0*cv[i]/r_book_use);
	PRT("r_book_use=%d,same=%d,cc[]=%d,%d\n",r_book_use,same,cc[0],cc[1]);
	if ( same ) DEBUG_PRT("");
	fclose(fp);
}

void find_next_extend(int *p_max_i, int *p_max_c)
{
	double max_b = -DBL_MAX;
	int max_i = -1;
	int max_c = 0;
	for (int i=0;i<r_book_use;i++) {
		R_BOOK *r = &r_book[i];
		if ( r->games_sum == 0 ) DEBUG_PRT("");

		for (int j=0; j<r->child_num; j++) {
			CHILD *pc = &r->child[j];
			if ( pc->mate_bit == R_DONE ) continue;	// 展開済み
			if ( pc->games == 0 ) continue;

			float v = pc->value;
			if ( r->col == 1 ) v = -v;
			v = (v + 1.0) * 50;	// -1 < v < +1   -->  0 < v < +100
			if ( v > +55+10 || v < +55-10 ) continue;	// 勝率が高すぎ、低すぎる手は展開しない
			
			double a = (double)pc->games / r->games_sum;
			double b = r->prob + log(a);
			if ( b > max_b ) {
				max_b = b;
				max_i = i;
				max_c = j;
			}
//			PRT("%3d:%2d:%8x,%3d,(%6.3f),bias=%6.3f,%d,%lf,%lf,%lf\n",i,j,pc->move,pc->games,pc->value,pc->bias,r->games_sum,a,b,max_b);
		}
	}
	if ( max_i < 0 ) DEBUG_PRT("");
	R_BOOK *r = &r_book[max_i];
	CHILD *pc = &r->child[max_c];
	*p_max_i = max_i;
	*p_max_c = max_c;
	PRT("max_i=%d,c=%d,%8x,%3d,(%6.3f),bias=%6.3f\n",max_i,max_c,pc->move,pc->games,pc->value,pc->bias);
}

int add_book(tree_t * restrict ptree, std::string pos_str, double prob)
{
	PRT("%lf:%d:ply=%d,%s\n",prob,r_book_use,std::count(pos_str.begin(), pos_str.end(), ' ')-2, pos_str.c_str());
	strcpy(str_cmdline, pos_str.c_str());
	char *lasts;
	strtok_r( str_cmdline, str_delimiters, &lasts );
	usi_posi( ptree, &lasts );

	uint64 hash64pos = get_marge_hash(ptree, root_turn);
	int i;
	for (i=0;i<r_book_use;i++) {
		R_BOOK *r = &r_book[i];
		if ( hash64pos == r->hash64pos ) break;
	}
	if ( i != r_book_use ) {
		static int count = 0;
		PRT("same pos=%d/%d, count=%d\n",i,r_book_use,++count);
		return 0;
	}
	
	const char *p = "go visit";
	strcpy(str_cmdline, p);
	strtok_r( str_cmdline, str_delimiters, &lasts );
	usi_go( ptree, &lasts );

	HASH_SHOGI *phg = HashShogiReadLock(ptree, root_turn);
	UnLock(phg->entry_lock);

	R_BOOK *r = &r_book[r_book_use];
	r->child.resize(phg->child_num);

	for (int i=0; i<phg->child_num; i++) {
		CHILD *pc = &phg->child[i];
		r->child[i] = *pc;
		r->child[i].mate_bit = R_INIT;
		char buf[7] = "resign";
//		pc->move = 0;
		if ( pc->move ) csa2usi( ptree, str_CSA_move(pc->move), buf );
//		if ( pc->games ) PRT("%2d:%s,%3d,(%6.3f),bias=%6.3f,%s\n",i,str_CSA_move(pc->move),pc->games,pc->value,pc->bias,buf);
	}

	r->hashcode64   = phg->hashcode64;
	r->hash64pos    = phg->hash64pos;
	r->mate_bit     = phg->mate_bit;
	r->win_sum      = phg->win_sum;
	r->squared_eval = phg->squared_eval;
	r->games_sum    = phg->games_sum;
	r->col	        = phg->col;
	r->age	        = phg->age;
	r->net_value    = phg->net_value;
	r->child_num    = phg->child_num;

	r->prob         = prob;
	r->str_seq      = pos_str;
	r_book_use++;
	if ( r->games_sum == 0 ) DEBUG_PRT("");

// 	copy(phg->child.begin(), phg->child.end(), back_inserter(r->child));
	PRT("r_book_use=%d,child_num=%d\n",r_book_use,r->child.size());
	if ( 0 ) for (int i=0; i<r->child_num; i++) {
		CHILD *pc = &r->child[i];
		char buf[7] = "resign";
		if ( pc->move ) csa2usi( ptree, str_CSA_move(pc->move), buf );
		if ( pc->games ) PRT("%2d:%s,%3d,(%6.3f),bias=%6.3f,%s\n",i,str_CSA_move(pc->move),pc->games,pc->value,pc->bias,buf);
	}
	return 1;
}


const char *lose_str[10] = {
// dlshogi - AobaZero
"position startpos moves 2g2f 8c8d 7g7f 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a4b 9g9f 7a6b 4g4f 4b3c 3i4h 7c7d 6i7h 1c1d 3g3f 4a3b 4h4g 9c9d 1g1f 8a7c 5i6h 5a4b 2i3g 6c6d 4i4h 6b6c 4g5f 6a6b 2h2i 8b8a 2f2e 4b4a 6g6f 4a5b 6h7i 5b4b 7i8h 6c5d 3g4e 3c2b 3f3e 3d3e 1f1e 1d1e 2e2d 2c2d 2i2d 8e8f 8g8f P*8g 7h8g P*8e 8f8e 2b2c 2d2i P*2h 2i2h 4c4d P*2d 2c3d P*1b 7c8e 7g8f 4d4e 1b1a+ N*3f P*8b 8a8b 2h1h 3f4h+ 1a2a B*6h N*4d 3b3c 1h1e 6h5g+ P*8c 8b8c P*8d 8c8b 4f4e 3d4e 5f4e 5d4e 2d2c+ 3c4d 1e1b+ 4b4c P*5e 4d3d B*1a P*4b S*5a P*2b 1b2b 5g6f 8h9h 6f5e 2c2d S*7h 2d3d 4c3d G*3c 3d4d 2b2d N*3d 3c3d 4d5d 3d3e P*3d 1a5e+ 5d5e 2d2f 3d3e L*5g G*5f B*3c G*4d 5g5f 4e5f 2f3e 5e6f G*6h L*9g 8i9g 7h8g 9h8g G*7g 8f7g 8e7g+ 6h7g 6f5g 3c4d+ P*8f 8g8f 5g5h 5a6b B*5g 6b7c 5g3e+ 4d3e R*6e 3e3f S*4g 3f4f 5f6g+ 7g8g 8b8a N*7g P*8e 9g8e 6e6f 4f5e 4g5f+ 5e6f 5f6f B*3f B*4g 3f4g 4h4g R*2i 6f7f 8g7f B*4i G*3i 4i2g+ L*5i 5h6h P*6i 6h5i 3i3h 5i5h S*4i 5h5g B*3e 5g5f 3h4g 5f4e 4g4f 4e3d G*4d 3d2e G*1e",
// てぃー〇の振り飛車 - AobaZero
"position startpos moves 7g7f 8c8d 6i7h 8d8e 8h7g 3c3d 7i6h 2b7g+ 6h7g 3a4b 3i3h 7a6b 5i6h 4b3c 4g4f 9c9d 9g9f 4a3b 1g1f 1c1d 3g3f 7c7d 2i3g 5a4b 3h4g 6c6d 4i4h 8a7c 4g5f 6b6c 6g6f 8b8a 2g2f 6a6b 2f2e 8a4a 6h7i 4a8a 7i6h 4b4a 2h2i 4a3a 6h7i 3a4b 7i8h 6c5d 5f4e 5d6c 4e5f 6c5d 5f4e 5d6c 2i6i 6b7b 4e5f 6c5d 4f4e 4b3a B*4f 8a6a 6i3i 3a2b 3i2i 6d6e 6f6e 9d9e 9f9e 7d7e 7f7e 5d6e P*6f P*7f 7g6h 6e6f P*6g 6f7e P*7d P*6d 7d7c+ 7b7c 8h7i 6a8a 7i6i 8e8f 8g8f 7e8f 6i5h 8f8g 7h8g 8a8g+ 2e2d 2c2d P*2e 2b3a 2e2d P*2h 2i3i B*8f P*7g 8g8h S*9g 8f9g+ 9i9g G*7h 6h5i 8h8i 5h4g 7h6h 5f6e 6h5i N*2c 3a4b 6e6d 7c6d 4f6d S*6b G*6a S*5a 2c1a+ P*6c 6d9a+ P*8b 2d2c+ 3b2c L*2f 2h2i+ 2f2c+ 2i3i 1a2a 8i8e 2c3c 4b5b 6a5a 6b5a 3c4c 5b6b 4c5c 6b5c S*4d 5c6d B*5c 6d7c L*7e 8e7e 5c7e+ P*4f 4g4f N*5d 4f5f R*4f 5f5e 4f4e 5e4e G*4f 4e5d L*5b 5d4c 5a4b 4c4b P*4a 4b5a S*4b 5a6a 3i3h S*7d",
// TMOQ - AobaZero
"position startpos moves 7g7f 8c8d 2g2f 8d8e 8h7g 3c3d 7i6h 2b7g+ 6h7g 3a4b 3i3h 7a6b 6i7h 4b3c 3g3f 4a3b 4g4f 9c9d 9g9f 1c1d 1g1f 6c6d 2i3g 5a4b 3h4g 7c7d 5i6h 8a7c 2h2i 6b6c 4i4h 6a6b 6g6f 8b8a 4g5f 4b4a 6h7i 4a5b 7i8h 5b4b 2f2e 6c5d 3g4e 3c2b 3f3e 3d3e 2e2d 4c4d 7f7e B*5b 2d2c+ 2b2c P*2d 2c3d B*2c 5d4c 7e7d 5b7d P*3c 2a3c 2c3d+ 4c3d S*2c P*2h 2i2h P*2g 2h2g P*2f 2g2f P*2e 2c3d+ 2e2f P*7e 7d6c 2d2c+ 3b2c 3d2c 3c4e 4f4e P*7f 7g6h N*8f 7h7i 9d9e N*7d 9e9f P*9h 6c7d 7e7d 9f9g+ 9h9g 9a9g+ 8h9g P*9f 9g8h 9f9g+ 8h9g R*9f 9g8h 9f9i+ 8h9i P*9h 9i8h B*9i 8h9g 8a9a R*9f 9a9f 9g9f R*9d R*9e N*8d 9f9g 9d9e B*9f 9e9f",
// Novice - AobaZero
"position startpos moves 2g2f 8c8d 2f2e 8d8e 7g7f 4a3b 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 1g1f 1c1d 6i7h 2b3c 3i4h 7a6b 4g4f 9c9d 9g9f 6c6d 3g3f 5a4b 4h4g 7c7d 5i6h 8a7c 2i3g 6b6c 4i4h 6a6b 2h2i 8b8a 4g5f 4b4a 6g6f 4a5b 6h7i 5b4b 7i8h 6c5d 3g4e 3c2b 3f3e 3d3e 1f1e 1d1e 2e2d 2c2d 2i2d 8e8f 8g8f 2b2c 4e5c+ 6b5c 2d5d 5c5d B*6c P*8g 8h8g 8a8c P*2d 2c2d 6c5d+ R*5i 5d6d 4b3c 6d7d 5i8i+ S*8h P*8e 7d8c 8e8f 8g8f B*4b 8f8g P*8f 7g8f 4b8f 8g8f 8i7h 8c7c 7h8h P*8g N*6a 7c5a S*4b 5a6a P*7c G*3d 3c3d B*4e 3d3c R*3d 3c2c 3d3b+ 2c3b N*4d 4c4d G*2c 3b4a 4e6c+ 4a3a N*4c 4b4c 6a4c S*7g 8f7e G*7d 6c7d 7c7d 7e6d B*8b 6d5c R*8c S*6c 8b7a 5c5d N*6b 6c6b+ G*5c 4c5c 8c5c 5d5c 7a6b 5c6b S*5c 6b7a 5c6b 7a6b P*6a 6b5a B*8d 5a5b 8d5a G*4a",
// koron - AobaZero
"position startpos moves 7g7f 8c8d 2g2f 8d8e 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a4b 6i7h 7a6b 3g3f 4b3c 3i3h 4a3b 4g4f 9c9d 9g9f 1c1d 2i3g 6c6d 1g1f 5a4b 3h4g 7c7d 4i4h 8a7c 2h2i 6b6c 5i6h 6a6b 4g5f 8b8a 6g6f 4b4a 2f2e 4a5b 6h7i 5b4b 7i8h 6c5d 3g4e 3c2b 3f3e 3d3e 1f1e 1d1e 2e2d 2c2d 2i2d 8e8f 8g8f 2b2c 4e5c+ 6b5c 2d5d 5c5d B*6c P*8g 8h8g 8a8c P*2d 2c2d 6c5d+ R*5i 5d6d 4b3c 6d7d 5i8i+ S*8h P*8e 7d8c 8e8f 8g8f B*4b 8f8g P*8f 7g8f 4b8f 8g8f 8i7h 8c7c 7h8h P*8g N*6a 7c5a S*4b 5a6a P*7c B*5e 3c2c 5e7c+ N*8a G*3d 2c3d R*8d 3d2c 8d8a+ S*9b 8a7a G*8a 7a6b 8h4h N*2f 4h5g N*4d 3b3a N*5d P*8d 7c8d 4b3c 6a4c P*4b 5d4b+ 3a4b 6b4b 5g8g 8f8g P*8f 8g8f N*7d 8d7d G*7e 7d7e G*3a 4b3a 2d2e 3a3b 2c2d G*2c",
NULL
};

int find_next_add_lose_game(tree_t * restrict ptree)
{
	static int lose_n = 0;
	static int m = 0;

	if ( m > 90 ) { m = 0; lose_n++; }

	const char *p = lose_str[lose_n];
	if ( p == NULL ) return 0;
	char c = 0;
	int i,count = 0;
	for (i=0; ;i++) {
		c = *(p+i);
		if ( c==0 ) break;
		if ( c!=' ' ) continue;
		count++;
		if ( count == m+3 ) break;
	}
	m++;
	if ( c==0 ) return 0;
	std::string str = p;
	str = str.substr(0,i);
	PRT("lose_n=%d,m=%d\n",lose_n,m);
	return add_book(ptree, str, log(1));
}


int find_next_add(tree_t * restrict ptree)
{
	std::string str;
	if ( r_book_use == 0 ) {
//		const char *p = "position startpos moves 7g7f 8b3b 2g2f 3a4b";
		const char *p = "position startpos moves";
//		const char *p = "position startpos moves 2g2f 8c8d 2f2e 8d8e 7g7f 4a3b 8h7g 3c3d 7i8h 2b7g+ 8h7g 3a2b 1g1f 1c1d 6i7h 2b3c 3i4h 7c7d 4g4f 7a6b 4h4g 5a4b 3g3f 6c6d 2i3g 8a7c 5i6h 6b6c 9g9f 9c9d 4i4h 6a6b 2h2i 8b8a 4g5f" // 角換わり35手目まで

		str = p;
		total_prob = log(1);
	} else {
		int max_i,max_c;
		find_next_extend(&max_i, &max_c);

		R_BOOK *r = &r_book[max_i];
		CHILD *pc = &r->child[max_c];

		strcpy(str_cmdline, r->str_seq.c_str());
		char *lasts;
		strtok_r( str_cmdline, str_delimiters, &lasts );
		usi_posi( ptree, &lasts );
		char buf[7] = "resign";
		if ( pc->move ) csa2usi( ptree, str_CSA_move(pc->move), buf );
		str = r->str_seq + " " + buf;
		pc->mate_bit = R_DONE;
		double a = (double)pc->games / r->games_sum;
		total_prob = r->prob + log(a);
	}
	return add_book(ptree, str, total_prob);
}

int book_node;
int start_book_ply;
// min-maxで最善手を探す(複数候補がある場合) 千日手のループに注意か。min-maxで評価値を求めても探索(登録)ノード数に差があるので信憑性の問題が。
float min_max_book(tree_t * restrict ptree, std::string pos_str, int ply, int *err)
{
	const int D_P = 0;
	*err = 0;
	int book_ply = std::count(pos_str.begin(), pos_str.end(), ' ')-2;
	if ( book_node ==0 ) start_book_ply = book_ply;
	book_node++;
	if ( ply<=D_P ) PRT("book ply=%d,%d,book_node=%4d:%s\n",book_ply, ply, book_node,pos_str.c_str());
	strcpy(str_cmdline, pos_str.c_str());
	char *lasts;
	strtok_r( str_cmdline, str_delimiters, &lasts );
	usi_posi( ptree, &lasts );

	uint64 hash64pos = get_marge_hash(ptree, root_turn);
	int n;
	for (n=0;n<r_book_use;n++) {
		R_BOOK *r = &r_book[n];
		if ( hash64pos == r->hash64pos ) break;
	}
	if ( n == r_book_use ) {
		PRT("not found Err.book ply=%d,%d,book_node=%4d:%s\n",book_ply, ply, book_node,pos_str.c_str());
		*err = 1;
		return 0;
	}

	int   i_max = -1;		// 展開済みの手の中での最善手
	float v_max = -999;
	int   max_games = 0;
	float max_value = -999;	// 全部の手が未展開の場合に返す値
	std::string str;
	R_BOOK *r = &r_book[n];

	char usi_buf[MAX_LEGAL_MOVES][7] = {0};
	for (int i=0; i<r->child_num; i++) {
		CHILD *pc = &r->child[i];
		csa2usi( ptree, str_CSA_move(pc->move), usi_buf[i]);	// 探索するとptreeの内容が変わってしまうので事前に
	}

	for (int i=0; i<r->child_num; i++) {
		CHILD *pc = &r->child[i];
//		if ( strcmp(usi_buf[i],"2b7g+")==0 ) continue;	// 後手からの角交換を禁止したら？
		if ( pc->mate_bit == R_INIT || ply > book_ply - start_book_ply ) {
			if ( pc->games > max_games  ) {
				max_games = pc->games;
				max_value = pc->value;
			}
			continue;
		}
		if ( pc->games == 0 ) continue;
		if ( pc->value == ILLEGAL_MOVE ) continue;
		str = r->str_seq + " " + usi_buf[i];
		int b = book_node;
//		if ( ply<=D_P ) PRT("%2d:%2d,%7s,ret_v=%8.5f,g=%8d,raw_v=%8.5f(%7d),n=%4d,before\n",ply,i,usi_buf[i],0.0f,pc->games,pc->value,book_node-b,n);
		float v = -min_max_book(ptree, str, ply+1,err);
		if ( *err ) continue;
		if ( ply<=D_P ) PRT("%2d:%2d,%7s,ret_v=%8.5f,g=%8d,raw_v=%8.5f(%7d),n=%4d\n",ply,i,usi_buf[i],v,pc->games,pc->value,book_node-b,n);
		if ( v > v_max ) {
			v_max = v;
			i_max = i;
		}
	}
	float ret_v;
	if ( i_max < 0 ) {
		ret_v = max_value;
	} else {
		ret_v = v_max;
	}
	if ( ply<=D_P ) PRT("%2d:%4d:ret_v=%8.5f,i_max=%d,book_node=%d\n",ply,n,ret_v,i_max,book_node);
	return ret_v;
}

void make_r_book(tree_t * restrict ptree)
{
	r_book_clear();
	load_r_book();
#if 1
	std::string s = "position " + (std::string)str_usi_position;
	const char *p = s.c_str();
//	const char *p = "position startpos moves 7g7f 4a3b 2g2f 8c8d 7i7h 3c3d";
	int err;
	book_node = 0;
	min_max_book(ptree, p, 0, &err);
	return;
#endif
	for (int i=0; i<R_BOOK_SIZE; i++) {
		if ( find_next_add(ptree)==0 ) { i--; continue; }
//		if ( find_next_add_lose_game(ptree)==0 ) { i--; continue; }
		save_r_book();
		if ( r_book_use >= R_BOOK_SIZE ) break;
	}
}

int get_force(tree_t * restrict ptree, HASH_SHOGI *phg)
{
	std::string s = "position " + (std::string)str_usi_position;
//	PRT("s='%s'\n",s.c_str());

	typedef struct MANE {
		int winner;		// 後手勝ちなら1
		const char *str;
	} MANE;
	MANE mane[100] = {
		// N+Lightweight, N-dlshogi with HEROZ, 先手を真似する
		{ 0, "position startpos moves 2g2f 8c8d 2f2e 8d8e 6i7h 4a3b 3i3h 9c9d 9g9f 7a7b 3g3f 8e8f 8g8f 8b8f 5i6h 1c1d 2i3g 7c7d 2e2d 2c2d 2h2d P*2c 2d7d 7b7c P*8g 8f8b 7d7e 7c6d 7e2e 3c3d 7g7f 4c4d 4g4f 3a4b 4i4h 4b4c 3h4g 6a5b 6h5h 2a3c 2e2i 8b7b 7h7g 5c5d 5h6h 2b3a 7i7h 7b8b 1g1f 5a4a 6g6f 8a7c 8g8f 8b8d 7g8g P*7e 6f6e 6d6e 4f4e 5d5e 8h5e 8d7d 7f7e 3a7e P*7f 7e5c 7h6g P*8e P*6d 6e5d 7f7e 7d8d 8f8e 8d8e P*8f 8e8d 5e8h 4d4e 4g5f P*5e 5f5e 5d5e 8h5e 5c6d 5e6d 8d6d S*7d P*7b 7d7c 7b7c N*5f 6d8d P*4d 4c5d B*7f S*6e 7f6e 5d6e S*4c S*4b 8i7g 6e5f 4c3b 4a3b 6g5f P*5h S*2a 3b4a 2i2c+ B*5d 5f6e B*5i 6h5h 5i4h+ 5h4h N*5e B*2i S*4g 2i4g 5e4g+ 4h4g B*6i 4g3h 4a5a P*6b G*4g 3h2i 5a6b N*7d 7c7d 6e5d 4g3g 7g6e 6i4g+ 2i1h N*6a B*4a P*5a P*6d N*7a 6d6c+ 5b6c 2c3b 4g3f 1h2i 3f4g 2i1h 3g2h 1h2h P*2g 2h3i 4g5g G*4h 5g7e 3b4b 7e4b 5d6c 7a6c S*7c 6a7c G*5c 4b5c 6e5c+ 6b5c 4d4c+" },
		{ 0, NULL }
	};

	for (int loop = 0; ;loop++) {
		if ( mane[loop].str == NULL ) break;
		std::string m = mane[loop].str;
		if ( root_turn != mane[loop].winner || m.find(s) == std::string::npos ) continue;
		size_t n = s.size();
		std::string m0 = m.substr(n).c_str();
		size_t i = m0.find(" ");
		if ( i == 0 ) {
			m0 = m0.substr(1);
			i = m0.find(" ");
		}
		if ( i == std::string::npos ) continue;
		const char *p = m0.substr(0,i).c_str();
		PRT("'%s'\n",p);

		for (int i=0; i<phg->child_num; i++) {
			CHILD *pc = &phg->child[i];
			char buf[7] = "resign";
			if ( pc->move ) csa2usi( ptree, str_CSA_move(pc->move), buf);
			if ( strcmp(buf, p)==0 ) {
				PRT("copy winner move[%d]=%s,winner=%d\n",loop,p,mane[loop].winner);
				return pc->move;
			}
		}
//		PRT("find %d,%s\n",n,m.substr(n).c_str());
	}

	// △44歩で角道を止めるよりAobaZeroの学習で多く出てくる変化に誘導すべきか
	if ( s == "position startpos moves 2g2f" ) return 0x00008c21; // △34歩
	if ( s == "position startpos moves 7g7f" ) return 0x00008c21; // △34歩
	if ( s == "position startpos moves 7g7f 3c3d 2g2f" )           return 0x00008ba0; // △44歩
	if ( s == "position startpos moves 2g2f 3c3d 7g7f" )           return 0x00008ba0; // △44歩
	if ( s == "position startpos moves 2g2f 3c3d 6i7h" )           return 0x00008ba0; // △44歩
	if ( s == "position startpos moves 2g2f 3c3d 2f2e 2b3c 7g7f" ) return 0x00008ba0; // △44歩

	if ( s == "position startpos moves 5i5h 5a5b 5h5i 5b5a 2g2f" ) return 0x00008c21; // △34歩 電竜戦4
	if ( s == "position startpos moves 5i5h 5a5b 5h5i 5b5a"      ) return 0x00009c2f; // ▲76歩 電竜戦4

	return 0;
}

int get_book_move(tree_t * restrict ptree, HASH_SHOGI *phg)
{
return 0;	// no book
	static int bDone = 0;
	if ( ! bDone ) {
		r_book_clear();
		load_r_book();
		bDone = 1;
	}
	int book_move = 0;

	int force_move = get_force(ptree, phg);
	if ( force_move ) {
		PRT("force!\n");
		return force_move;
	}

	uint64 hash64pos = get_marge_hash(ptree, root_turn);
	int n;
	for (n=0;n<r_book_use;n++) {
		R_BOOK *r = &r_book[n];
		if ( hash64pos == r->hash64pos ) break;
	}
	if ( n == r_book_use ) {
		PRT("no book move.\n");
		return book_move;
	}

	int   max_i = -1;	// 最善手
	int   max_games = 0;
	R_BOOK *r = &r_book[n];
	for (int i=0; i<r->child_num; i++) {
		CHILD *pc = &r->child[i];
		if ( pc->games ) PRT("%2d:%7s,%08x,%7d(%6.3f)%d,bias=%6.3f\n",i,str_CSA_move(pc->move),pc->move,pc->games,pc->value,pc->mate_bit,pc->bias);
		if ( pc->games < max_games  ) continue;
		max_games = pc->games;
		max_i = i;
	}
	if ( max_i < 0 ) {
		PRT("no best book move? Err\n");
		return book_move;
	}

	CHILD *pc = &r->child[max_i];
	book_move = pc->move;

	char buf[7] = "resign";
	if ( pc->move ) csa2usi( ptree, str_CSA_move(pc->move), buf);
	PRT("book_move=%s, n=%d,max_i=%d/%d,g=%d/%d,v=%7.4f(%6.2f%%),raw_v=%7.4f(%6.2f%%)\n",buf,n,max_i,r->child_num,pc->games,r->games_sum,pc->value,(pc->value+1.0)/2.0*100,r->net_value,(r->net_value+1.0)/2.0*100);
	return book_move;
}

