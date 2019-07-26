// 2019 Team AobaZero
// This source code is in the public domain.
#include "../config.h"

#include <numeric>
#include <random>

#include <cstdint>
#include <algorithm>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>


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

//using namespace Utils;
std::string default_weights;
std::vector<int> default_gpus;
void init_global_objects();	// Leela.cpp

void init_network()
{
//	Random::get_Rng().seedrandom(cfg_rng_seed);

//	cfg_weightsfile = "networks/20180122_i362_pro_flood_F64L29_b64_1_half_version_2.txt";
//	cfg_weightsfile = "networks/20180620_i362_64x29_iter_1_version.txt";
//	cfg_weightsfile = "/home/yss/aobazero/networks/20190306_64L29_policy_160_139_bn_relu_cut_visit_x4_iter_910000.txt";
	if ( !default_weights.empty() ) cfg_weightsfile = default_weights;

#ifdef USE_OPENCL
	if ( !default_gpus.empty() ) {
		std::copy(default_gpus.begin(), default_gpus.end(), back_inserter(cfg_gpus) );
	}
//	PRT("cfg_rowtiles    =%d\n",cfg_rowtiles);
#endif

	init_global_objects();

	PRT("cfg_softmax_temp=%.3f,cfg_random_temp=%.3f,cfg_num_threads=%d,cfg_batch_size=%d\n",cfg_softmax_temp,cfg_random_temp,cfg_num_threads,cfg_batch_size);

	// Initialize network
//	Network::initialize();

/*
static  auto network = std::make_unique<Network>();
    auto playouts = 800;
	std::string weightsfile("hogehoge");
    network->initialize(playouts, weightsfile);

//	Network::initialize(800 , weightsfile);
*/
}

inline void set_dcnn_data(float data[][B_SIZE][B_SIZE], int n, int y, int x, float v=1.0f)
{
//	PRT("%.5f\n",v);
	data[n][y][x] = v;
}

int get_motigoma(int m, int hand)
{
	if ( m==1 ) return (int)I2HandPawn(hand);
	if ( m==2 ) return (int)I2HandLance(hand);
	if ( m==3 ) return (int)I2HandKnight(hand);
	if ( m==4 ) return (int)I2HandSilver(hand);
	if ( m==5 ) return (int)I2HandGold(hand);
	if ( m==6 ) return (int)I2HandBishop(hand);
	if ( m==7 ) return (int)I2HandRook(hand);
	PRT("motigoma Err m=%d\n",m); debug();
	return 0;
}

void set_dcnn_channels(tree_t * restrict ptree, int sideToMove, int ply, float *p_data)
{
	float (*data)[B_SIZE][B_SIZE] = (float(*)[B_SIZE][B_SIZE])p_data;
	int base = 0;
	int add_base = 0;
	int x,y;
 	const int t = ptree->nrep + ply - 1;	// 手数。棋譜の手数+探索深さ。ply は1から始まるので1引く。
	int flip = (t&1);	// 後手の時は全部ひっくり返す
	int loop;
	const int T_STEP = 8;
	const int STANDARDIZATION = 1;

	if ( sideToMove != (t&1) ) { PRT("sideToMove Err\n"); debug(); }
	if ( ply < 1 ) DEBUG_PRT("ply=%d Err.\n",ply);
	
	for (loop=0; loop<T_STEP; loop++) {
		add_base = 28;

		int np = ptree->nrep + ply - loop - 1;
		if ( np < 0 ) { PRT("np Err\n"); debug(); }
		min_posi_t *p = &ptree->record_plus_ply_min_posi[np];	// [0] には平手局面 [1] は1手目を指した後の局面

		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			int z = y*B_SIZE + x;
			int k = p->asquare[z];
			if ( k==0 ) continue;
			int m = abs(k);
			if ( m>=0x0e ) m--;	// m = 1...14
			m--;
			// 先手の歩、香、桂、銀、金、角、飛、王、と、杏、圭、全、馬、竜 ... 14種類、+先手の駒が全部1、で15種類
			if ( k < 0 ) m += 14;
			int yy = y, xx = x;
			if ( flip ) {
				yy = B_SIZE - y -1;
				xx = B_SIZE - x -1;
				m -= 14;
				if ( m < 0 ) m += 28;	// 0..13 -> 14..27
			} 
			set_dcnn_data(data, base+m, yy,xx);
		}
		base += add_base;

		add_base = 14;
		int i;
		for (i=1;i<8;i++) {
			int n0 = get_motigoma(i, p->hand_black);
			int n1 = get_motigoma(i, p->hand_white);	// mo_c[i];
			if  ( flip ) {
				int tmp = n0;
				n0 = n1;
				n1 = tmp;
			} 
			// 持ち駒の最大数
			const float mo_div[8] = { 0, 18, 4, 4, 4, 4, 2, 2 };
			float div = 1.0f;
			if ( STANDARDIZATION ) div = mo_div[i];
			float f0 = (float)n0 / div;
			float f1 = (float)n1 / div;
			for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				set_dcnn_data( data, base+0+i-1, y,x, f0);
				set_dcnn_data( data, base+7+i-1, y,x, f1);
			}
		}
		base += add_base;

		int sum = 0;
		uint64 key  = HASH_KEY;
//		uint64 hand = (flip==0) ? HAND_B : HAND_W;
		uint64 hand = HAND_B;
		if ( loop > 0 ) {
			key  = ptree->rep_board_list[np];
			hand = ptree->rep_hand_list[ np];
		}
		for (i=np-2; i>=0; i-=2) {
			if ( ptree->rep_board_list[i] == key && ptree->rep_hand_list[i] == hand ) {
//				PRT("same:sum=%d,i=%3d,t=%3d,loop=%d\n",sum,i,t,loop);
				sum++;
			}
		}
//		if ( 1 || sum > 0 ) PRT("repeat=%d,t=%3d,loop=%d\n",sum,t,loop);
		if ( sum > 3 ) sum = 3;	// 同一局面5回以上。4回と同じで

		add_base = 3;
		for (i=0;i<3;i++) {	// 000, 100, 110, 111          論文だけでは実装不明。000,100,010,001 かも
			if ( sum>=i+1 ) for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				set_dcnn_data( data, base+i, y,x);
			}
		}
		base += add_base;

		if ( np == 0 ) {
			base += (28 + 14 + 3) * (T_STEP - (loop+1));	// 最後なら何もしない
			break;
		}
	}
	
	add_base = 1;
	if ( sideToMove == 1 ) for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data( data, base, y,x);
	}
	if ( DCNN_CHANNELS == 362 ) {
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
//			set_dcnn_data( data, base+1, y,x, t);
			float div = 1.0f;
			if ( STANDARDIZATION ) div = 512.0f;
			set_dcnn_data( data, base+1, y,x, (float)t/div);
//			set_dcnn_data( data, base+1, y,x, 0);
		}
		add_base = 2;
	}
	base += add_base;

//	PRT("base=%d,net_type=%d\n",base,net_type);
//	if ( t==8 ) { hyouji(); int i; for (i=0;i<base;i++) prt_dcnn_data(data,i,-1); DEBUG_PRT("t=%d,tesuu=%d\n",t,tesuu); }
	if ( DCNN_CHANNELS != base ) { PRT("Err. DCNN_CHANNELS != base %d\n",base); debug(); }
}

void prt_dcnn_data(float (*data)[B_SIZE][B_SIZE],int c,int turn_n)
{
	int x,y;
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		PRT("%.0f ",data[c][y][x] );
		if ( y==B_SIZE-2 && x==B_SIZE-1 ) PRT(" %3d",c);
		if ( y==B_SIZE-1 && x==B_SIZE-1 ) PRT(" %3d\n",turn_n);
		if ( x==B_SIZE-1 ) PRT("\n");
	}
}
void prt_dcnn_data_table(float (*data)[B_SIZE][B_SIZE])
{
	int i,t,x,y;
	for (i=0;i<45;i++) {
		PRT("i=%2d\n",i);
		for (y=0;y<B_SIZE;y++) {
			for (t=0;t<8;t++) {
				for (x=0;x<B_SIZE;x++) {
					PRT("%.0f",data[t*45+i][y][x] );
				}
				if ( t!=7 ) PRT(" ");
			}
			PRT("\n");
		}
	}
	PRT("\n");
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		PRT("%.0f",data[360][y][x] );
		if ( x==B_SIZE-1 ) PRT("\n");
	}
	PRT("---------------------------\n");
}

void PRT_path(tree_t * restrict ptree, int /*sideToMove*/, int ply)
{
	int i;
	for (i=1;i<ply;i++) {
		int m = ptree->current_move[i];
		PRT("%s:",str_CSA_move(m));
	}
	PRT("\n");
}

bool is_nan_inf(float x)
{
	return ( std::isnan(x) || std::isinf(x) );
}

float get_network_policy_value(tree_t * restrict ptree, int sideToMove, int ply, HASH_SHOGI *phg)
{
	if ( ptree->nrep < 0 || ptree->nrep >= REP_HIST_LEN ) { PRT("nrep Err=%d\n",ptree->nrep); debug(); }

	int size = 1*DCNN_CHANNELS*B_SIZE*B_SIZE;
	float *data = new float[size];
	memset(data, 0, sizeof(float)*size);

	set_dcnn_channels(ptree, sideToMove, ply, data);
//	if ( 1 || ply==1 ) { prt_dcnn_data_table((float(*)[B_SIZE][B_SIZE])data);  }
//	if ( 1 && ptree->nrep+ply==101+3 ) { int sum=0; int i; for (i=0;i<size;i++) sum = sum*37 + (int)(data[i]*1000.0f); PRT("sum=%08x,ply=%d,nrep=%d\n",sum,ply,ptree->nrep); }

//	const auto result = Network::get_scored_moves_yss_zero((float(*)[B_SIZE][B_SIZE])data);
	const auto result = GTP::s_network->get_scored_moves_yss_zero((float(*)[B_SIZE][B_SIZE])data);


//	float xxx = NAN;
//	if ( std::isnan(xxx) || std::isinf(xxx) ) PRT("xxx is nan!\n"); else PRT("xxx is not nan...\n");
    float raw_v = result.second;
	if ( is_nan_inf(raw_v) ) raw_v = 0;
	float v_fix = raw_v;	// (raw_v + 1) / 2;
	if ( sideToMove==BLACK ) v_fix = -v_fix;	// 手番関係なく先手勝ちが+1
//	float v = (net_eval + 1.0f) / 2.0;	// 0 <= v <= 1
//    if ( sideToMove ) {	// DCNNは自分が勝ちなら+1、負けなら-1を返す。探索中は先手は+1～0, 後手は 0～-1を返す。
//        v = v - 1;
//    }
//	float v = f_rnd();
//	if ( sideToMove==BLACK ) v = -v;
//	phg->net_value      = v_fix;

//  std::vector<Network::scored_node> nodelist;

//	PRT("result.first.size()=%d\n",result.first.size());
	auto &node = result.first;
//	PRT("node.size()=%d\n",node.size());
//	{ int i; for (i=0;i<60;i++) PRT("%f,%d\n",node[i].first,node[i].second); }

    float all_sum = 0.0f;
    for (const auto& node : result.first) {
        auto id = node.second;
        all_sum += node.first;
		int yss_m = get_move_from_c_y_x_id(id);
		int bz,az,tk,nf;
		unpack_te(&bz,&az,&tk,&nf, yss_m);
		int bz_x = bz & 0x0f;
		int bz_y =(bz & 0xf0) >> 4;
		int az_x = az & 0x0f;
		int az_y =(az & 0xf0) >> 4;
		int from = 0,to = 0;
		if ( bz==0xff ) {
			from = tk + 81 - 1;
		} else {
			from = (bz_y-1) * 9 + (bz_x-1);
		}
		to = (az_y-1) * 9 + (az_x-1);
		int promotion = nf >> 3;
		int bona_m = (promotion << 14) | (from <<7) | to;
		if ( yss_m == 0 ) bona_m = 0;
		// "piece to move" は moveのみ。dropでは 0
		
	    if ( 0 && ply==1 && id < 100 ) PRT("%4d,%08x(%08x),%f\n",id,yss_m,bona_m, node.first);
	}


	float legal_sum = 0.0f;

	int move_num = phg->child_num;
	unsigned int * restrict pmove = ptree->move_last[0];
	int i;
	for ( i = 0; i < move_num; i++ ) {
		int move = pmove[i];
		CHILD *pc = &phg->child[i];

		int from = (int)I2From(move);
		int to   = (int)I2To(move);
		int drop = (int)From2Drop(from);
		int is_promote = (int)I2IsPromote(move);
//		int cap  = (int)UToCap(move);
//		int piece_m	= (int)I2PieceMove(move);

		int bz = get_yss_z_from_bona_z(from);
		int az = get_yss_z_from_bona_z(to);
		int tk = 0;
		if ( from >= nsquare ) {
			bz = 0xff;
			tk = drop;
		}
		int nf = is_promote ? 0x08 : 0x00;
		if ( sideToMove ) {
			flip_dccn_move(&bz,&az,&tk,&nf);
		}
		int yss_m = pack_te(bz,az,tk,nf);
		int id = get_id_from_move(yss_m);

		// node[i].second ... id
		float bias = node[id].first;
		if ( id != node[id].second ) { PRT("id=%d,%d err\n",id,node[id].second); debug(); }
//		if ( ply==1 ) PRT("%3d:%s(%d)(%08x)id=%5d, bias=%8f,from=%2d,to=%2d,cap=%2d,drop=%3d,pr=%d,peice=%d\n",i,str_CSA_move(move),sideToMove,yss_m,id,bias, from,to,cap,drop,is_promote,piece_m);

		if ( is_nan_inf(bias) ) bias = 0;
		pc->bias = bias;
		legal_sum += bias;
	}
//	PRT("legal_sum=%9f,all_sum=%f, raw_v=%10f,v_fix=%10f\n",legal_sum,all_sum, raw_v,v_fix );

	if ( fPrtNetworkRawPath ) {
		PRT("%9.6f(%9.6f)",v_fix,raw_v);
		PRT_path(ptree, sideToMove, ply);
	}

	// sort
	int j;
	for ( i = 0; i < move_num-1; i++ ) {
		CHILD *pc = &phg->child[i];
		float max_b = pc->bias;
		int   max_i = i;
		for ( j = i+1; j < move_num; j++ ) {
			CHILD *pc = &phg->child[j];
			if ( max_b > pc->bias ) continue;
			max_b = pc->bias;
			max_i = j;
		}
		if ( max_i==i ) continue;
		CHILD c_tmp = phg->child[i];
		phg->child[i] = phg->child[max_i];
		phg->child[max_i] = c_tmp;
	}

	float mul = 1.0f;
//	PRT("all_sum=%f,legal=%f\n",all_sum,legal_sum);
	if ( all_sum > legal_sum && legal_sum > 0 ) mul = 1.0f / legal_sum;
	for ( i = 0; i < phg->child_num; i++ ) {
		CHILD *pc = &phg->child[i];
		if ( 0 && ply==1 && i < 30 ) {
			PRT("%3d:%s(%08x), bias=%8f->(%8f)\n",i,str_CSA_move(pc->move), get_yss_packmove_from_bona_move(pc->move), pc->bias, pc->bias*mul);
		}
		pc->bias *= mul;
	}


/*
    // If the sum is 0 or a denormal, then don't try to normalize.
    if (legal_sum > std::numeric_limits<float>::min()) {
        // re-normalize after removing illegal moves.
        for (auto& node : nodelist) {
            node.first /= legal_sum;
        }
    }

    link_nodelist(nodecount, nodelist, net_eval);
*/
//	if ( ply==1 ) DEBUG_PRT("stop\n");

	delete[] data;
	return v_fix;
}

void get_c_y_x_from_move(int *pc, int *py, int *px, int pack_move)
{
	unsigned int n = (unsigned int)pack_move;

	int bz = (n & 0xff000000) >> 24;
	int az = (n & 0x00ff0000) >> 16;
	int tk = (n & 0x0000ff00) >> 8;
	int nf = (n & 0x000000ff);
//	PRT("%08x: %02x,%02x,%02x,%02x\n",n,bz,az,tk,nf);

	int az_x = (az & 0x0f);
	int az_y = (az & 0xf0) >> 4;
	tk = tk & 0x07;
	int bz_x = (bz & 0x0f);
	int bz_y = (bz & 0xf0) >> 4;
	int dx = bz_x - az_x;
	int dy = bz_y - az_y;
	int dir = -1;
	if ( dx == 0 && dy >  0 ) dir = 0;
	if ( dx >  0 && dy >  0 ) dir = 1;
	if ( dx >  0 && dy == 0 ) dir = 2;
	if ( dx >  0 && dy <  0 ) dir = 3;
	if ( dx == 0 && dy <  0 ) dir = 4;
	if ( dx <  0 && dy <  0 ) dir = 5;
	if ( dx <  0 && dy == 0 ) dir = 6;
	if ( dx <  0 && dy >  0 ) dir = 7;

	if ( dx == +1 && dy == 2 ) dir = 8;
	if ( dx == -1 && dy == 2 ) dir = 9;

	if (dx > 0) dx = +1;
	if (dx < 0) dx = -1;
	if (dy > 0) dy = +1;
	if (dy < 0) dy = -1;
	int distance = -1;
	int dist;
	for (dist=0;dist<8;dist++) {
		if ( az_x + dx*(dist+1) == bz_x && az_y + dy*(dist+1) == bz_y ) distance = dist;
	}

	int c = -1, y = -1, x = -1;
	if ( bz == 0xff ) {
		if ( tk == 0 || tk > 7 ) DEBUG_PRT("tk err");
		c = (64+2)*2 + tk-1;
		y = az_y-1;
		x = az_x-1;
//		PRT("drop: %2d,distance=%2d,c=%3d\n",dir,distance,c);
	} else {
		y = bz_y-1;
		x = bz_x-1;
		if ( dir == -1 ) DEBUG_PRT("dir err");
		if ( nf == 8 ) {
			c = (64+2);
		} else {
			c = 0;
		}
		if ( dir >= 0 && dir <= 7 ) {
			c += dir*8 + distance;
			if ( distance == -1 || distance >= 8 ) DEBUG_PRT("distance err");
		}
		if ( dir == 8 || dir == 9 ) {
			if ( dir == 8 ) c += 64;
			if ( dir == 9 ) c += 64 + 1;
		}
//		PRT("dir : %2d,distance=%2d,c=%3d\n",dir,distance,c);
	}
	if ( c == -1 || c >= LABEL_CHANNELS || x==-1 || y==-1 ) DEBUG_PRT("c err");
	*pc = c;
	*py = y;
	*px = x;
}

int get_move_from_c_y_x(int c, int y, int x)
{
	int bz=0,az=0,tk=0,nf=0;
	if ( c >= (64+2)*2 ) {
		tk = (c - (64+2)*2) + 1;
		bz = 0xff;
		az = (y+1)*16 + (x+1);
	} else {
		if ( c >= (64+2) ) {
			nf = 0x08;
			c -= (64+2);
		}
		if ( c >= 64 ) {	// knight
			bz = (y+1)*16 + (x+1);
			if ( c == 64 ) {
				az = bz - 0x20 - 1;
			} else {
				az = bz - 0x20 + 1;
			}
		} else {
			//c += dir*8 + distance;
			int dir = c / 8;	// 0...7
			// dir  = 0,1,2,3,4,5,6,7
			// z8[] = 6,5,4,3,2,1,0,7
			int d = (8 + 6-dir) & 0x07;
			// z8[8] = { +0x01, +0x11, +0x10, +0x0f, -0x01, -0x11, -0x10, -0x0f };
			const int z8[8] = { +0x01, +0x11, +0x10, +0x0f, -0x01, -0x11, -0x10, -0x0f };
			int dz = z8[d];
			int dist = c - dir*8;	// 0...7
			bz = (y+1)*16 + (x+1);
			az = bz + dz * (dist+1);
			int i;
			for (i=0;i<dist;i++) {
				int z = bz + dz * (i+1);
				if ( (z & 0x0f) < 1 || (z & 0x0f) > 9 || (z & 0xf0) < 0x10 || (z & 0xf0) > 0x90 ) az = 0;  
			}
		}
	}
	if ( bz < 0 || bz > 0xff ) { bz = 0; DEBUG_PRT("bz=%d\n",bz); }
	if ( az < 0 || az > 0xff ) { az = 0; }
	if ( tk < 0 || tk > 0x07 ) { tk = 0; DEBUG_PRT("tk=%d\n",tk); } 
	if ( nf < 0 || nf > 0xff ) { nf = 0; DEBUG_PRT("nf=%d\n",nf); }
	return pack_te(bz,az,tk,nf);
}

#define f11259  1	// 論文どおりに生成。0で存在しない手を削除
#if (f11259==0 ) 
const int MOVE_C_Y_X_ID_MAX = 3781;
#else
const int MOVE_C_Y_X_ID_MAX = 11259;	// 3781;
#endif

int move_id_c_y_x[LABEL_CHANNELS][B_SIZE][B_SIZE];	// idが入る。-1で着手不可
int move_from_id_c_y_x_id[MOVE_C_Y_X_ID_MAX];		// idからmoveを

void make_move_id_c_y_x()
{
	const int fView = 0;
	static int fDone = 0;
	if ( fDone ) return;
	fDone = 1;
	int c,y,x,sum_ok=0;
	for (c=0;c<LABEL_CHANNELS;c++) for (y=0;y<9;y++) for (x=0;x<9;x++) {
		if ( fView ) if ( x==0 && y==0 ) PRT("c=%d\n",c);
		int m = get_move_from_c_y_x(c, y, x);
		if ( fView ) PRT("%08x",m);
		int bz,az,tk,nf;
		unpack_te(&bz,&az,&tk,&nf, m);
		int bz_x = bz & 0x0f;
		int bz_y =(bz & 0xf0) >> 4;
		int az_x = az & 0x0f;
		int az_y =(az & 0xf0) >> 4;
		int fOK = 1;
		if ( bz != 0xff ) {
			if ( az_x < 1 || az_x > 9 ) fOK = 0;
			if ( az_y < 1 || az_y > 9 ) fOK = 0;
			if ( abs(bz_x - az_x)==1 && bz_y - az_y==2 && az_y<=2 && nf==0 ) fOK = 0;
			if ( nf==0x08 && az > 0x40 && bz > 0x40 ) fOK = 0;
		} else {
			if ( tk <= 0x03 && az < 0x20 ) fOK = 0;
			if ( tk == 0x03 && az < 0x30 ) fOK = 0;
		}
		if ( fOK ) {
			if ( fView ) PRT(" ");
		} else {
			if ( fView ) PRT("*");
			m = 0;
		}
		move_id_c_y_x[c][y][x] = -1;
		if ( f11259 ) fOK = 1;
		if ( fOK ) {
			move_id_c_y_x[c][y][x] = sum_ok;
			move_from_id_c_y_x_id[sum_ok] = m;
		}
		sum_ok += fOK;
		if ( fView ) if ( x==8 ) PRT("\n");
	}
	if ( sum_ok != MOVE_C_Y_X_ID_MAX && f11259==0 ) DEBUG_PRT("");
	PRT("sum_ok=%d/%d, MOVE_C_Y_X_ID_MAX=%d\n",sum_ok,LABEL_CHANNELS*9*9,MOVE_C_Y_X_ID_MAX);	// sum_ok=3781/11259
}

int get_move_id_from_c_y_x(int c, int y, int x)
{
	return move_id_c_y_x[c][y][x];
}
int get_move_from_c_y_x_id(int id)
{
	return move_from_id_c_y_x_id[id];
}
int get_id_from_move(int pack_move)
{
	int c,x,y;
	get_c_y_x_from_move(&c, &y, &x, pack_move);
	return get_move_id_from_c_y_x(c, y, x);
}
void flip_dccn_move( int *bz, int *az, int * /* tk */, int *nf )
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
//	if ( *tk ) {
//		if ( *tk > 0x80 ) *tk -= 0x80;
//		else *tk += 0x80;
//	}
	*nf = *nf;
}
int get_yss_z_from_bona_z(int to)
{
	int to_y = to / 9;				// 0..8
	int to_x = to - to_y * 9;		// 0..8
	int az = ((to_y + 1)<<4) + (to_x + 1);
	return az;
}

int get_yss_packmove_from_bona_move(int move)
{
	int from = (int)I2From(move);
	int to   = (int)I2To(move);
	int drop = (int)From2Drop(from);
	int is_promote = (int)I2IsPromote(move);
//	int cap  = (int)UToCap(move);
//	int piece_m	= (int)I2PieceMove(move);

	int bz = get_yss_z_from_bona_z(from);
	int az = get_yss_z_from_bona_z(to);
	int tk = 0;
	if ( from >= nsquare ) {
		bz = 0xff;
		tk = drop;
	}
	int nf = is_promote ? 0x08 : 0x00;
	return pack_te(bz,az,tk,nf);
}

// alpha ... Chess = 0.3, Shogi = 0.15, Go = 0.03
// epsilon = 0.25
void add_dirichlet_noise(float epsilon, float alpha, HASH_SHOGI *phg)
{
    auto child_cnt = phg->child_num;

    auto dirichlet_vector = std::vector<float>{};
    std::gamma_distribution<float> gamma(alpha, 1.0f);
    for (int i = 0; i < child_cnt; i++) {
        dirichlet_vector.emplace_back(gamma(Random::get_Rng()));
//      PRT("%3d:%f\n",i,gamma(Random::get_Rng()));
    }
    auto sample_sum = std::accumulate(begin(dirichlet_vector),
                                      end(dirichlet_vector), 0.0f);

    // If the noise vector sums to 0 or a denormal, then don't try to
    // normalize.
    if (sample_sum < std::numeric_limits<float>::min()) {
        return;
    }

    for (auto& v: dirichlet_vector) {
        v /= sample_sum;
    }

    int i;
    for (i=0; i<child_cnt;i++) {
        float eta_a = dirichlet_vector[i];
		CHILD *pc = &phg->child[i];
        float score = pc->bias;
        score = score * (1 - epsilon) + epsilon * eta_a;
        PRT("%3d:%8s,noise=%10f, bias=%f -> %f\n",i,str_CSA_move(pc->move),eta_a,pc->bias,score);
        pc->bias = score;
    }
}

