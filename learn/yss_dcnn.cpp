// 2019 Team AobaZero
// This source code is in the public domain.
// yss_dcnn.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <random>

#include "yss_ext.h"     /**  load extern **/
#include "yss_prot.h"    /**  load prototype function **/

#include "yss.h"

#if !defined(_MSC_VER)
#include <dirent.h>
#include <sys/stat.h>
#endif

using namespace std;

#ifdef F2187	// yss.h
const int f2187 = 1;
#else
const int f2187 = 0;
#endif

const int HANDICAP_TYPE = 7;

#define USE_CAFFE 1

#define YSS_TRAIN 0		// 0でtest, 1でtrain DB作成
const int DCNN_CHANNELS = 362;
//const int DCNN_CHANNELS = 85;	// 45+2 + 14*2 + 10

#define FILE_HEADER "i361_11259_1600self_leveldb"

const int fMAKE_LEVELDB = 0;

#if (YSS_TRAIN==0)
const char CONVERT_DB_FILE[] = "test_"  FILE_HEADER;
#else
const char CONVERT_DB_FILE[] = "train_" FILE_HEADER;
#endif

const int B_SIZE = 9;

const int NET_128 = 0;
const int NET_361 = 1;
const int NET_362 = 2;
const int LABEL_CHANNELS = 139;
const int MOVES_MAX = 593;

const int fSelectZeroDB = 1;	// ZERO_DB から選択中。千日手のチェックなどが違う


/*
将棋のディープラーニング
Policy、Value

入力
先手の歩、香、桂、銀、金、角、飛、と、車、圭、全、馬、竜
駒ごとの 01 のmap

歩が打てる場所
利きのmap 0,1,2,3以上

直前の手。移動元、移動先、どの駒を取ったか、成ったか

大駒の動けるマス


持ち駒
歩0,1,2,3,4, 香、桂、銀、金、角、飛 0,1,2
5 + 3*6 = 21
4 + 2*6 = 16


Ponanzaは出力を可能手で固定
取った駒は無視。移動元から移動先に成ったか、成らずか

盤上の歩 90通り
9-5段目 1通り         45
4-3段目 2通り(成)     18*2=36
2段目   1通り         9

香 441通り
9段目 10通り  9*10 = 90
8段目  9通り  9*9  = 81
7段目  8      9*8  = 72
6段目  7      9*7  = 63
5段目  6      9*6  = 54
4段目  5      9*5  = 45
3段目  3      9*3  = 27
2段目  1      9*1  =  9

桂 128通り   
9-6段目   1+2*7+1  16    16*4
5段目     2+4*7+2  32    32 
4-3段目   1+2*7+1  16    16*2

銀
9段目    2+3*7+2 25   25
8-5段目  3+5*7+3 41   41*4
4段目

いや、これだとうまく表現できないか。移動元 -> 移動先(成) の全組み合わせ？

(99)から移動可能な場所は 11+8+11 = 31
上に8+3
右 8
斜め右 8+3
桂馬 1
(89)             11+8+9+1 = 29
上に8+3
左右 8    
斜め右 7+2
斜め左 1
(88)             

0-3873 までの分類するsoftmaxになる。

移動元(81)-移動先(81) の2つを使う利点は同〜、というのを理解しやすいか？
実際に盤上に駒があるし。
移動先(81)を元に並べると多少理解しやすい？
3813通り？

50億局面=5000000000,1棋譜から100としても5000万棋譜。1ヶ月、4台の12CPUで作るとすると、1台で1200万/月。100万/1CPU月
3万棋譜/1CPU日。1250棋譜/1CPU時間。1分20棋譜、3秒で1局。

どの局面を学習させるか。平手の初期局面を何度も学習させても仕方がない。
初手76歩、初手26歩、これを指したときの勝率が分かる程度で十分。
全体の学習棋譜数を10000とすると、


AlphaZeroの構造を真似してみる。
歩、香、桂、銀、金、角、飛、王、と、車、圭、全、馬、竜
14種類。1回目の同一局面、2、3。持ち駒の数、1,2,3...
手番、手数(1,2,...)

出力
歩、1
香、7
桂、2
銀、5
金、6
角、8 x 4
飛、8 x 4
馬、8 x 8
駒打 7
4枚
81マスからどの方向へ、何マス動いたか？8方向。最大8マス。よって
81 x 8x8 = 64
桂は2つ

76歩なら。        22角成(88)は
上方向に1つ。    右斜めに6つ 
000000000        000000000
000000000		 000000000
000000000		 000000000
000000000		 000000000
000000000		 000000000
000000000		 000000000
001000000		 000000000
000000000		 010000000
000000000		 000000000


*/

#if (USE_CAFFE==1)

#ifndef CPU_ONLY
#include <cuda_runtime.h>
#endif
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>

#include "caffe/caffe.hpp"
#include "caffe/util/io.hpp"
#include "caffe/blob.hpp"

// convert to leveldb
#include <algorithm>
#include <fstream>  // NOLINT(readability/streams)
#include <utility>

#include "boost/scoped_ptr.hpp"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "caffe/proto/caffe.pb.h"
#include "caffe/util/db.hpp"
#include "caffe/util/rng.hpp"

using namespace caffe;  // NOLINT(build/namespaces)
using namespace std;
using std::pair;
using boost::scoped_ptr;

void setOneToDatum(Datum* datum, int num_inputs, int height, int width, unsigned char *pData ) {
  datum->set_channels(num_inputs);
  datum->set_height(height);
  datum->set_width(width);
//datum->set_label(label);
  datum->clear_data();
  datum->clear_float_data();
  std::string* datum_string = datum->mutable_data(); // ->mutable_float_data()
  
//  static char data[num_inputs][height][width];
  for (int c = 0; c < num_inputs; c++) {
    for (int h = 0; h < height; h++) {
      for (int w = 0; w < width; w++) {
        unsigned char v = pData[c*height*width + h*width + w];
        datum_string->push_back( v );
//      datum_string->push_back( data[c][h][w] );
      }
    }
  }
}

const int CommitNum = 1000;

static int count_convert_db;
static Datum datum_convert_db;
static scoped_ptr<db::DB> *p_db = NULL;
static scoped_ptr<db::Transaction> *p_txn = NULL;

void create_convert_db()
{
  ::google::InitGoogleLogging("yssfish");
  // Print output to stderr (while still logging)
  FLAGS_alsologtostderr = 1;

#ifndef GFLAGS_GFLAGS_H_
  namespace gflags = google;
#endif
  // Create new DB
  static scoped_ptr<db::DB> db(db::GetDB("leveldb"));	// "lmdb" or "leveldb"
  db->Open(CONVERT_DB_FILE, db::NEW);
  static scoped_ptr<db::Transaction> txn(db->NewTransaction());

  p_db = &db;
  p_txn = &txn;
  count_convert_db = 0;
}
void add_one_data_datum(unsigned char *pData)
{
  setOneToDatum(&datum_convert_db, DCNN_CHANNELS, B_SIZE, B_SIZE, pData);
  // sequential
  const int kMaxKeyLength = 256;
  char key_cstr[kMaxKeyLength];
  int length = snprintf(key_cstr, kMaxKeyLength, "%010d", count_convert_db);

  // Put in db
  std::string out;
  CHECK(datum_convert_db.SerializeToString(&out));
  (*p_txn)->Put(std::string(key_cstr, length), out);

  if (++count_convert_db % CommitNum == 0) {
    // Commit db
    (*p_txn)->Commit();
    (*p_txn).reset((*p_db)->NewTransaction());
    LOG(INFO) << "Processed " << count_convert_db << " files.";
  }
}
void finish_data_datum()
{
  // write the last batch
  if (count_convert_db % CommitNum != 0) {
    (*p_txn)->Commit();
    LOG(INFO) << "Processed " << count_convert_db << " files.";
  }
}
#else
void create_convert_db() {}
void add_one_data_datum(unsigned char *pData) {}
void finish_data_datum() {}
#endif




const int unique_max = 3781;	// 以前は 3813;
short unique_from_to[81][81][2];
short unique_hit_to[81][7];
int te_unique[unique_max];

void make_unique_from_to()
{
	int sum=0;
	int bz,az,k;
	for (az=0x11;az<=0x99;az++) for (bz=0x11;bz<=0x99;bz++) {
		int bx = bz & 0x0f;
		int ax = az & 0x0f;
		int by =(bz & 0xf0) >> 4;
		int ay =(az & 0xf0) >> 4;
		if ( bx < 1 || bx > 9 || ax < 1 || ax > 9 ) continue;
		if ( by < 1 || by > 9 || ay < 1 || ay > 9 ) continue;
		int bz81 = get81(bz);
		int az81 = get81(az);
		unique_from_to[bz81][az81][0] = -1;
		unique_from_to[bz81][az81][1] = -1;
		if ( az==bz ) continue;
		int nari = 0;
		if ( bz < 0x40 || az < 0x40 ) nari = 1;
		int ok = 0;
		if ( ax==bx || ay==by ) ok = 1;			// 飛
		if ( abs(ax-bx) == abs(ay-by) ) ok = 1;	// 角
		int must_nari = 0;
		if ( az == bz - 0x21 || az == bz - 0x1f ) {
			ok = 1;	// 桂
			if ( az < 0x30 ) must_nari = 1;
		}
		if ( ok == 0 ) continue;
		if ( must_nari == 0 ) {
			te_unique[sum] = pack_te(bz,az,0,0);
			unique_from_to[bz81][az81][0] = sum++;
		}
		if ( nari ) {
			te_unique[sum] = pack_te(bz,az,0,0x08);
			unique_from_to[bz81][az81][1] = sum++;
		}
//		PRT("(%02x->%02x)nari=%d,%d\n",bz,az,nari,sum);
	}
	for (az=0x11;az<=0x99;az++) for (k=1;k<=7;k++) {
		int ax = az & 0x0f;
		int ay =(az & 0xf0) >> 4;
		if ( ax < 1 || ax > 9 ) continue;
		if ( ay < 1 || ay > 9 ) continue;
		int az81 = get81(az);
		unique_hit_to[az81][k-1] = -1;
		if ( az < 0x20 && k <= 3 ) continue;
		if ( az < 0x30 && k == 3 ) continue;
		te_unique[sum] = pack_te(0xff,az,k,0);
		unique_hit_to[az81][k-1] = sum++;
//		PRT("(%02x->%02x)k=%d,%d\n",0xff,az,k,sum);
	}
	if ( sum != unique_max ) DEBUG_PRT("sum=%d,unique_max=%d\n",sum,unique_max);
	PRT("make_unique_from_to=%d\n",sum);
}

int get_unique_from_te(int bz,int az, int tk, int nf)
{
	// 先手が基本。後手番では反転させる。
	if ( bz==0xff ) {
		return unique_hit_to[get81(az)][(tk & 0x0f)-1];
	}
	return unique_from_to[get81(bz)][get81(az)][(nf != 0)];
}
int get_te_from_unique(int u)
{
	return te_unique[u];
}


const int STOCK_MAX = 2400*5;	// 1棋譜から100手、左右で200個取れる。*400(9GB), 362  *200(20GB)

unsigned char (*dcnn_data)[DCNN_CHANNELS][B_SIZE][B_SIZE];
const uint64 DCNN_DATA_SIZE = (uint64)STOCK_MAX * DCNN_CHANNELS * B_SIZE * B_SIZE;

//float *dcnn_label_data;
int (*dcnn_labels)[2];	// [0]...手, [1]...勝敗
const int DCNN_LABELS_SIZE = STOCK_MAX * sizeof(int) * 2;

void clear_dcnn_data()
{
	memset(dcnn_data, 0, DCNN_DATA_SIZE);
}


#if !defined(_MSC_VER)
// sDir is full path.  "/home/yss/aya/kgs4d/001".
char *find_next_file(char *sDir, const char *sExt)
{
	static DIR *dir = NULL;
	static char dir_name[TMP_BUF_LEN];
	struct dirent *dp = NULL;

	if ( dir == NULL ) {
		dir = opendir(sDir);
		if ( dir == NULL ) { DEBUG_PRT("fail opendir() %s\n",sDir); }
		strcpy(dir_name, sDir);
	}

	for (;;) {
		dp = readdir(dir);
		if ( dp == NULL ) {
			closedir(dir);
			dir = NULL;
			return NULL;
		}
//		PRT("%s\n", dp->d_name);
//		if ( strstr(dp->d_name, "csa")==NULL && strstr(dp->d_name, "CSA")==NULL ) continue;
		if ( strstr(dp->d_name, sExt )==NULL                                    ) continue;
		break;	// found
	}
	static char filename[TMP_BUF_LEN*2];
	sprintf(filename, "%s/%s",dir_name, dp->d_name);
	return filename;
}
#endif
/*
int get_next_sgf_file(char *sFullPathDir, char *filename)
{
#if defined(_MSC_VER)
	if ( get_next_file(filename,"*.csa")==0 ) return 0;
#else
	char *p = find_next_file(sFullPathDir, "csa");
	if ( p==NULL ) return 0;
	strcpy(filename, p);
#endif
	return 1;
}
*/
#if defined(_MSC_VER)
int get_one_v_file(char *filename, const char *dir_list[]) { return 0; }
#else

const char **dir_v_sgf_list = NULL;
int dir_v_sgf_n = 0;

int get_one_v_file(char *filename, const char *dir_list[])
{
	static struct dirent **namelist = NULL;
	static int dir_num = 0;
	static int dir_i   = 0;
	static char *p_current_dir = NULL;
	static char str_current_dir[TMP_BUF_LEN*2];
	static int sum_dir = 0;
	static int all_files = 0;

	if ( dir_v_sgf_list == NULL ) dir_v_sgf_list = dir_list;

	for (;;) {
		if ( p_current_dir ) {
			const char *sKifExt[2] = { "csa","kif" };
			int nExt = 0;	// 拡張子の種類(csaが基本)
			if ( strstr(p_current_dir, "uuun"  ) ) nExt = 1;
			if ( strstr(p_current_dir, "prokif") ) nExt = 1;
			const char *pExt = sKifExt[nExt];

			char *p = find_next_file(p_current_dir, pExt);
			if ( p==NULL ) {
				p_current_dir = NULL;
			} else {
//				char filename[TMP_BUF_LEN];
				sprintf(filename,"%s",p);
//				PRT("%s\n", filename);
				all_files++;
				return 1;
			}
		}

		if ( namelist == NULL ) {
			const char *sDir = dir_v_sgf_list[dir_v_sgf_n];
			if ( sDir == NULL ) { PRT("done...sum_dir=%d,all_files=%d\n",sum_dir,all_files); return -1; }
			dir_num = scandir(sDir, &namelist, NULL, NULL);
			if ( dir_num < 0 || namelist==NULL ) { DEBUG_PRT("Err scandir\n"); }
		}
		if ( dir_i >= dir_num ) {
			dir_num = 0;
			dir_i   = 0;
			free(namelist);
			namelist = NULL;
			dir_v_sgf_n++;
			continue;
		}

		struct dirent *p = namelist[dir_i];
		if ( p->d_type == DT_DIR && p->d_name[0] != '.' ) {
//		if ( p->d_type == DT_DIR && p->d_name[0] != '.' && atoi(p->d_name) >= 2013 ) {
			PRT("%4d:%10d,%s\n",sum_dir, all_files, p->d_name);
			sprintf(str_current_dir,"%s/%s",dir_v_sgf_list[dir_v_sgf_n],p->d_name);
			p_current_dir = str_current_dir;
			sum_dir++;
		}
		free(p);
		dir_i++;
	}

	return 0;
}
#endif

void shuffle_prt(int *p_stock_num)
{
	int stock_num = *p_stock_num;
	if ( stock_num == 0 ) return;
	int i;
	char used[STOCK_MAX];
	for (i=0; i<stock_num; i++) used[i] = 0;
	for (i=0; i<stock_num; i++) {
		int r = -1;
		for (;;) {
			r = rand_yss() % stock_num;
			if ( used[r]==0 ) break;
		}
		used[r] = 1;
//		PRT("[%2d]=%2d\n",i,r);
//		PRT("%s",str_stock[r]);
#if (USE_CAFFE==1)
		const char sLabel[] = "/home/yss/yssfish/value/label_tmp.txt";
		static int fFirst = 1;
		if ( fFirst ) {
			if ( fopen(sLabel,"r")!=NULL ) { DEBUG_PRT("Err %s exist!!!\n",sLabel); }
			fFirst = 0;
		}
		FILE *fp = fopen(sLabel,"a");
		if ( fp==NULL ) { DEBUG_PRT("fail fopen()\n"); }
//		fprintf(fp,"%.0f\n", dcnn_label_data[r]);
		fprintf(fp,"%u\n",(unsigned int)(dcnn_labels[r][0]));
		fprintf(fp,"%d\n",dcnn_labels[r][1]);
		fclose(fp);

		add_one_data_datum((unsigned char *)dcnn_data[r]);
#endif
	}
	clear_dcnn_data();
	*p_stock_num = 0;
}

vector<string> flood_players;

void load_flood_players()
{
#if defined(_MSC_VER)
	FILE *fp = fopen("C:\\Yss_smp\\r3000.txt","r");
#else
	FILE *fp = fopen("/home/yss/yssfish/r3000.txt","r");
#endif
	if ( fp==NULL ) DEBUG_PRT("");
	for (;;) {
		char str[TMP_BUF_LEN];
		if ( fgets( str, TMP_BUF_LEN-1, fp ) == NULL ) break;
		char name[TMP_BUF_LEN];
		sscanf(str,"%s",name);
		unsigned int i;
		for (i=0; i<flood_players.size(); i++) {
			if ( strcmp(flood_players[i].c_str(),name)==0 ) break;
		}
        if ( i!=flood_players.size() ) continue;
        flood_players.push_back(name);
//		PRT("%s\n",name);
	}
	PRT("load_flood_players=%d\n",flood_players.size());
}

void shogi::make_policy_leveldb()
{			
//	make_move_id_c_y_x(); return;

	load_flood_players();
	int ct1 = get_clock();

	const char *dir_list[] = {
#if (YSS_TRAIN==0) 
		"/home/yss/uuun_kif/uuun_kifu2",
//		"/home/yss/yssfish/csa",
#else
		"/home/yss/uuun_kif/uuun_ivy",
		"/home/yss/prokif",
		"/home/yss/floodkif",
//		"/home/yss/yssfish",
#endif
		NULL
	}; 

//	if ( FV_LEARNING==0 ) { PRT("\n\nset FV_LEARNING=1 for ignoring make_mm3_10_pattern_root()\n\n\n"); debug(); }
	if ( fMAKE_LEVELDB == 0 ) { DEBUG_PRT("set fMAKE_LEVELDB=1\n"); }


	create_convert_db();
	dcnn_data = (unsigned char(*)[DCNN_CHANNELS][B_SIZE][B_SIZE])malloc( DCNN_DATA_SIZE );
	if ( dcnn_data == NULL ) { DEBUG_PRT("fail malloc()\n"); }
	clear_dcnn_data();

//	dcnn_label_data = (float *)malloc( STOCK_MAX * sizeof(float) );
//	if ( dcnn_label_data == NULL ) { PRT("fail malloc()\n"); debug(); }
//	memset(dcnn_label_data, 0, STOCK_MAX * sizeof(float) );
	dcnn_labels = (int(*)[2])malloc( DCNN_LABELS_SIZE );
	if ( dcnn_labels == NULL ) { DEBUG_PRT("fail malloc()\n"); }
	memset(dcnn_labels, 0, DCNN_LABELS_SIZE );

	// i362_pro_flood, 6254 棋譜(Test), 448365 棋譜 (20279263局面) 217545-716-230104 先手勝率0.514

//	init_rand_yss();	// 常に同じ順番にする
#if defined(_MSC_VER)
	if ( change_dir("C:\\Yss_smp\\20170515")==0 ) return;
//	if ( change_dir("W:\\prokif\\20140703_79942")==0 ) return;
//	if ( change_dir("C:\\Yss_smp\\learnkif")==0 ) return;
#endif
	
	int all=0,sum=0,both=0,either=0,loop;
	int stock_num = 0;
	int result_sum[3];
	result_sum[0] = result_sum[1] = result_sum[2] = 0;
	
	for (loop=0 ;; loop++) {
		char filename[TMP_BUF_LEN];
#if defined(_MSC_VER)
		if ( get_next_file(filename, "*.csa")==0 ) break;
//		if ( get_next_file(filename, "*.kif")==0 ) break;
#else
		if ( get_one_v_file(filename, dir_list) < 0 ) break;
#endif
		if ( open_one_file(filename)==0 ) break;
//		PRT("%s,%3d\n",filename,all_tesuu);
		if ( all_tesuu < 30 ) continue;


		int fBot = 0;
		if ( strstr(filename,"uuun_kif") || strstr(filename,"floodkif") ) fBot = 1;
		int fFlood = 0;
		if ( strstr(filename,"floodkif") ) fFlood = 1;
		 
		int ignore_muda[2];
		ignore_muda[0] = ignore_muda[1] = 0;

		int i,j,rn[2];	// rating 3000以上のplayer
		rn[0] = rn[1] = 0;
		for (i=0; i<(int)flood_players.size(); i++) {
			for (j=0;j<2;j++) {
				if ( strcmp(flood_players[i].c_str(),PlayerName[j])==0 ) rn[j] = 1;
			}
		}
		if ( fFlood==0 ) rn[0] = rn[1] = 1;
		// 王手の水平線を削除する
		int type = 0;
		if ( strstr(KifBuf,"%TORYO")     ) type = 1;
		if ( strstr(KifBuf,"%KACHI")     ) type = 1;
		if ( strstr(KifBuf,"sennichite") ) type = 2;	// 'summary:sennichite:ye_Cortex-A17_4c draw:Gikou_7700K draw
		if ( strstr(KifBuf,"time up")    ) type = 1;	// 'summary:time up:sonic win:Gikou_2_6950XEE lose   ... 時間切れは、勝ったほうの手で終わっている
		if ( strstr(KifBuf,"max_moves:") ) type = 2;
		if ( strstr(KifBuf,"先手の勝ち") || "後手の勝ち" ) type = 1;
		if ( strstr(KifBuf,"手で千日手") ) type = 2;

		if ( type == 0 ) continue;
		int result = 0;
		if ( type == 1 ) {
			if ( (all_tesuu & 1)==0 ) {
				result = -1;
			} else {
				result = +1;
			}
		}	
		if ( 1 || type == 0 ) PRT("%s,%4d,%3d,type=%d,result=%2d\n",filename,nKifBufSize,all_tesuu,type,result);

		Color win_c = WHITE;
		if ( (all_tesuu & 1)==0 ) win_c = BLACK;

		for (i=0; i<all_tesuu; i++) back_move();
		for (i=0; i<all_tesuu; i++) {
			int *p = &kifu[tesuu+1][0];
			Move  m = (Move)pack_te( *(p+0),*(p+1),*(p+2),*(p+3) );
			Color c = (Color)(i&1);
			// 同〜と取られている王手。負けてる側。28手前でも。36手前。40手前、一回発生したらそれ以降の負けた側の手は全部無視。
			if ( is_move_gives_check(m, c) && c != win_c && i < all_tesuu-1 && kifu[tesuu+1][1] == kifu[tesuu+2][1] ) {
				int bz = get_bz(m);
				int az = get_az(m);
				int tk = get_tk(m);
				int muda = 0;
				if ( c==WHITE && bz==0xff && kiki_m[az]==0 && kiki_c[az] ) muda=1;
				if ( c==BLACK && bz==0xff && kiki_c[az]==0 && kiki_m[az] ) muda=1;
				if ( c==WHITE && bz!=0xff && kiki_m[az]==1 && kiki_c[az] && tk==0 ) muda=1;
				if ( c==BLACK && bz!=0xff && kiki_c[az]==1 && kiki_m[az] && tk==0 ) muda=1;
				if ( c==WHITE && bz!=0xff && kiki_m[az]==1 && kiki_c[az] && (tk&0x07)==1 && (init_ban[bz]&0x07)>=0x02 ) muda=1;
				if ( c==BLACK && bz!=0xff && kiki_c[az]==1 && kiki_m[az] && (tk&0x07)==1 && (init_ban[bz]&0x07)>=0x02 ) muda=1;
				if ( muda ) {
					if ( all_tesuu-i <= 40 ) ignore_muda[i&1] = 1;
//					PRT("無駄王手%3d(%3d):",i+1,all_tesuu-i); print_te(m); PRT("%s\n",filename);
				}
			}
			int fOK = 1;
			if ( rn[i&1] == 0 ) fOK = 0;
			if ( fBot && ignore_muda[i&1]==1 ) fOK = 0;
			if ( i>=256 ) fOK = 0;
//			if ( rn[i&1] && (fBot && ignore_muda[i&1]==0) ) {	// i128はプロの棋譜は無視してた
			if ( fOK ) {
//				if ( (i&1)==1 ) hanten_with_hash_kifu();

				int bz = *(p+0), az = *(p+1), tk = *(p+2), nf = *(p+3);
				int j;
				for (j=0;j<1;j++) {	// 2で左右反転も登録
					if ( j==1 ) {
						az = (az & 0xf0) + (10 - (az & 0x0f));
						if ( bz != 0xff ) bz = (bz & 0xf0) + (10 - (bz & 0x0f));
					}
//					int u = get_unique_from_te(bz,az,tk,nf);	// 盤面反転で *p (kifu) も反転してる
//					if ( u < 0 ) DEBUG_PRT("i=%d,u=%d,%02x,%02x,%02x,%02x\n",i,u,bz,az,tk,nf);
					int win_r = result;
					if ( (i&1)==1 ) {
						hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
						win_r = -win_r;	// 勝敗まで反転はしなくてよい？ 反転した方が学習が簡単なはず。出てきた結果を反転
					}
//					dcnn_labels[stock_num][0] = u;
					dcnn_labels[stock_num][0] = get_move_id_c_y_x(pack_te(bz,az,tk,nf));	//pack_te(bz,az,tk,nf);
					dcnn_labels[stock_num][1] = win_r;
					set_dcnn_channels(c, i, NULL, stock_num, 0);
					if ( j==1 ) flip_horizontal_channels(stock_num);
//					PRT("%3d,%02x,%02x,%02x,%02x,%5d,%08x\n",i,bz,az,tk,nf,dcnn_labels[stock_num][0], get_move_from_c_y_x_id(dcnn_labels[stock_num][0]));
//					PRT("%3d,%02x,%02x,%02x,%02x,%5d,%08x\n",i,bz,az,tk,nf,dcnn_labels[stock_num][0], get_te_from_unique(dcnn_labels[stock_num][0]));
					stock_num++;
					all++;
					if ( stock_num == STOCK_MAX ) { DEBUG_PRT("STOCK_MAX\n"); }
				}
				
//				if ( (i&1)==1 ) hanten_with_hash_kifu();
			}
			forth_move();
		}

//		PRT("%d %d:%s %s,",rn[0],rn[1],PlayerName[0],PlayerName[1]);
		if ( rn[0] + rn[1] == 2 ) both++;
		if ( rn[0] + rn[1] == 1 ) either++;
		result_sum[result+1]++;
		sum++;

		if ( sum >= 30 ) break;

		// shuffle
		if ( stock_num < STOCK_MAX * 29 / 30 ) continue;
		shuffle_prt(&stock_num);
	}

	shuffle_prt(&stock_num);
	finish_data_datum();
	PRT("\nloop=%d,sum=%d,both=%d,either=%d, result[]=%d,%d,%d\n",loop,sum,both,either,result_sum[0],result_sum[1],result_sum[2]);
	PRT("pos=%d/%d,%d, %.3f sec\n",all,sum,loop,get_spend_time(ct1));
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

void make_label_leveldb()
{			
	int ct1 = get_clock();
	if ( DCNN_CHANNELS != LABEL_CHANNELS ) { DEBUG_PRT("DCNN_CHANNELS err.\n"); }

	create_convert_db();
	dcnn_data = (unsigned char(*)[DCNN_CHANNELS][B_SIZE][B_SIZE])malloc( DCNN_DATA_SIZE );
	if ( dcnn_data == NULL ) { DEBUG_PRT("fail malloc()\n"); }
	clear_dcnn_data();

	FILE *fp = fopen("test_i362_pro_flood_leveldb.txt","r");
	if ( fp==NULL ) { DEBUG_PRT("fail fopen()\n"); }
	
	char str[TMP_BUF_LEN];
	if ( fgets( str, TMP_BUF_LEN-1, fp ) == NULL ) DEBUG_PRT("");

	int num_row = atoi(str);
	PRT("num_row=%d\n",num_row);
	int i,sum=0;
	for (i=0;i<num_row;i++) {
		if ( fgets( str, TMP_BUF_LEN-1, fp ) == NULL ) DEBUG_PRT("too short");
		char str2[TMP_BUF_LEN];
		if ( fgets( str2, TMP_BUF_LEN-1, fp ) == NULL ) DEBUG_PRT("too short");
#if defined(_MSC_VER)
		unsigned long pack = (unsigned long)_atoi64(str);
#else
		unsigned long pack = atoll(str);
#endif

		int c,y,x;
		for (c=0;c<LABEL_CHANNELS;c++) {
			for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				dcnn_data[0][c][y][x] = 0;
			}
		}
		get_c_y_x_from_move(&c, &y, &x, pack);
		dcnn_data[0][c][y][x] = 1;
		add_one_data_datum((unsigned char *)dcnn_data[0]);
		sum++;
	}

	finish_data_datum();
	PRT("sum=%d, %.3f sec\n",sum, get_spend_time(ct1));
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
		if ( F11259 ) fOK = 1;
		if ( fOK ) {
			move_id_c_y_x[c][y][x] = sum_ok;
			move_from_id_c_y_x_id[sum_ok] = m;
		}
		sum_ok += fOK;
		if ( fView ) if ( x==8 ) PRT("\n");
	}
	if ( sum_ok != MOVE_C_Y_X_ID_MAX && F11259==0 ) DEBUG_PRT("");
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

int get_move_id_c_y_x(int pack)
{
	int c,y,x;
	get_c_y_x_from_move(&c, &y, &x, pack);
	int id = get_move_id_from_c_y_x(c,y,x);
	if ( id < 0 ) DEBUG_PRT("id err,c=%d,y=%d,x=%d,%08x\n",c,y,x,pack);
	return id;
}

const int MOVE_2187_MAX = 2187;

int get_dlshogi_policy(int bz, int az, int tk, int nf) {
	int fDrop = 0;
	int fNari = (nf!=0);
	int dir = -1;
	if ( bz==0xff ) {
		fDrop = 1;
		dir = (tk & 0x0f) - 1;
	} else for (;;) {
		// bz と az から方向を決定
		if ( az == bz - 0x21 ) { dir = 8; break; }	// 桂馬
		if ( az == bz - 0x1f ) { dir = 9; break; }
		int dx =  (bz & 0x0f)     -  (az & 0x0f);
		int dy = ((bz & 0xf0)>>4) - ((az & 0xf0)>>4);
		if ( dx==0 ) {
			if ( dy > 0 ) dir = 2;
			if ( dy < 0 ) dir = 6;
		} else if ( dy==0 ) {
			if ( dx > 0 ) dir = 0;
			if ( dx < 0 ) dir = 4;
		} else {
			if ( abs(dx) != abs(dy) ) DEBUG_PRT("");
			if ( dx > 0 && dy > 0 ) dir = 1;
			if ( dx < 0 && dy > 0 ) dir = 3;
			if ( dx < 0 && dy < 0 ) dir = 5;
			if ( dx > 0 && dy < 0 ) dir = 7;
		}
		break;
	}
	int ax = (az & 0x0f);
	int ay = (az & 0xf0) >> 4;
	int z81 = (ay-1)*9 + (ax-1);
	int index = z81 + 81*dir + fNari*(81*10) + fDrop*(81*20);
	if ( index < 0 || index >= 2187 || dir < 0 ) DEBUG_PRT("");
	return index;
}

void prt_dcnn_data(int stock_num,int c,int turn_n)
{
	int x,y;
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		PRT("%d",dcnn_data[stock_num][c][y][x] );
		if ( y==B_SIZE-3 && x==B_SIZE-1 ) PRT(" %3d",stock_num);
		if ( y==B_SIZE-2 && x==B_SIZE-1 ) PRT(" %3d",c);
		if ( y==B_SIZE-1 && x==B_SIZE-1 ) PRT(" %3d\n",turn_n);//PRT(" stock_num=%d,ch=%d,r=%d\n",stock_num,c,turn_n);
		if ( x==B_SIZE-1 ) PRT("\n");
	}
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
		PRT("i=%2d,",i);
		if ( i<28 ) {
			int n = i;
			if ( n >= 14 ) n -= 14;
			if ( n>=12 ) n++;
			PRT("%s\n",koma[n+1]);
		} else if ( i<42 ) {
			int n = i - 28;
			if ( n >= 7 ) n -= 7;
			PRT("mo %s *10\n",koma[n+1]);
		} else {
			PRT("REP=%d\n",i-42);
		}
		for (y=0;y<B_SIZE;y++) {
			for (t=0;t<8;t++) {
				for (x=0;x<B_SIZE;x++) {
					if ( i>=28 && i<42 ) {
						PRT("%.0f",data[t*45+i][y][x]*10.0 );
					} else {
						PRT("%.0f",data[t*45+i][y][x] );
					}
				}
				if ( t!=7 ) PRT(" ");
			}
			PRT("\n");
		}
		PRT("\n");
	}
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		PRT("%.0f",data[360][y][x] );
		if ( x==B_SIZE-1 ) PRT("\n");
	}
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		PRT("%6.3f",data[361][y][x] );
		if ( x==B_SIZE-1 ) PRT("\n");
	}
	PRT("---------------------------\n");
}

void flip_horizontal_channels(int stock_num)
{
	int c;
	for (c=0;c<DCNN_CHANNELS;c++) {
		unsigned char r_data[B_SIZE][B_SIZE];
		int x,y;
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			unsigned char v = dcnn_data[stock_num][c][y][x];
			int xx = (B_SIZE-1) - x;
			r_data[y][xx] = v;
		}
//		prt_dcnn_data(stock_num, c, -1);
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			dcnn_data[stock_num][c][y][x] = r_data[y][x];
		}
//		prt_dcnn_data(stock_num, c, -1);
	}
}

void shogi::setYssChannels(Color sideToMove, int moves, float *p_data, int net_type, int input_num)
{
	float (*data)[B_SIZE][B_SIZE] = (float(*)[B_SIZE][B_SIZE])p_data;

	for (int i=0;i<input_num;i++) for (int y=0;y<B_SIZE;y++) for (int x=0;x<B_SIZE;x++) {
		data[i][y][x]=0;
	}
	set_dcnn_channels(sideToMove, moves, p_data, -1, 0);
//	{ for (int i=0;i<input_num;i++) for (int y=0;y<B_SIZE;y++) for (int x=0;x<B_SIZE;x++) PRT("%.0f,",data[i][y][x]); } PRT("\n");
}

void set_one_dcnn(int stock_num, int c, int y, int x, int v)
{
	if ( c < 0 || c >= DCNN_CHANNELS || y < 0 || y >= B_SIZE || x < 0 || x >= B_SIZE || v < 0 || v > 255 ) { DEBUG_PRT("Err set_one_dcnn %d,%d,%d,%d = %d\n",stock_num,c,y,x,v); }
	dcnn_data[stock_num][c][y][x] = v;
}
inline void set_dcnn_data(int stock_num, float data[][B_SIZE][B_SIZE], int n, int y, int x, float v=1.0f)
{
	if ( fMAKE_LEVELDB ) {
		set_one_dcnn(stock_num, n,y,x, (int)v);
	} else {
		data[n][y][x] = v;
	}
}
void shogi::set_dcnn_channels(Color sideToMove, const int ply, float *p_data, int stock_num, int nHandicap)
{
	float (*data)[B_SIZE][B_SIZE] = (float(*)[B_SIZE][B_SIZE])p_data;
	int base = 0;
	int add_base = 0;
	int x,y;
	int flip = (ply + (nHandicap!=0)) & 1;	// 後手の時は全部ひっくり返す
	int current_t = ply;	// 現局面の手数。棋譜＋探索深さ
	if ( GCT_SELF ) flip = (ply + fGotekara) & 1;
	if ( GCT_SELF && ply != 0 ) DEBUG_PRT("");	// for AI_book2, hcpe does not have ply.
	if ( flip && sideToMove != BLACK ) DEBUG_PRT("");
	// move_hit_kif[], move_hit_hashcode[] に棋譜+探索深さの棋譜とハッシュ値を入れること 

	int loop,back_num=0;
	const int T_STEP = 6;
//	const int T_STEP = 1;
	for (loop=0; loop<T_STEP; loop++) {
		add_base = 28;
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			int z = make_z(x+1,y+1);
			int k = init_ban[z];
			if ( k==0 ) continue;
			int m = k & 0x0f;
			if ( m>=0x0e ) m--;	// m = 1...14
			m--;
			// 先手の歩、香、桂、銀、金、角、飛、王、と、杏、圭、全、馬、竜 ... 14種類、+先手の駒が全部1、で15種類
			if ( k > 0x80 ) m += 14;
			int yy = y, xx = x;
			if ( flip ) {
				yy = B_SIZE - y -1;
				xx = B_SIZE - x -1;
				m -= 14;
				if ( m < 0 ) m += 28;	// 0..13 -> 14..27
			} 
			set_dcnn_data(stock_num, data, base+m, yy,xx);
		}
		base += add_base;

		add_base = 14;
		int i;
		for (i=1;i<8;i++) {
			int n0 = mo_m[i];
			int n1 = mo_c[i];
			if  ( flip ) {
				int tmp = n0;
				n0 = n1;
				n1 = tmp;
			} 
			// 持ち駒の最大数
			float mo_div[8] = { 0, 18, 4, 4, 4, 4, 2, 2 }; 
			float f0 = (float)n0 / mo_div[i];
			float f1 = (float)n1 / mo_div[i];
			for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				set_dcnn_data(stock_num, data, base+0+i-1, y,x, f0);
				set_dcnn_data(stock_num, data, base+7+i-1, y,x, f1);
			}
		}
		base += add_base;

		int j,sum = 0;
		if ( fSelectZeroDB == 0 ) {
			for (j=0;j<tesuu;j++) { 	
				HASH *psen = &sennitite[j];
				if ( hash_code1 == psen->hash_code1 && hash_code2 == psen->hash_code2 ) {
					if ( hash_motigoma == psen->motigoma ) sum++;
				}
			}
		} else {
			for (j=0;j<current_t-1;j++) { 	
				if ( hash_code1 == move_hit_hashcode[j][0] && hash_code2 == move_hit_hashcode[j][1] ) {
					if ( hash_motigoma == move_hit_hashcode[j][2] ) sum++;
				}
			}
		}
//		if ( 1 || sum > 0 ) PRT("repeat=%d,t=%3d,loop=%d,sideToMove=%d,ply=%d\n",sum,tesuu,loop,sideToMove,ply);
		if ( sum > 3 ) sum = 3;	// 同一局面5回以上。4回と同じで

		add_base = 3;
		for (i=0;i<3;i++) {
			if ( sum>=i+1 ) for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				set_dcnn_data(stock_num, data, base+i, y,x);
			}
		}
		base += add_base;

		if ( (fSelectZeroDB && current_t == 0) || (fSelectZeroDB==0 && tesuu == 0)) {
			base += (28 + 14 + 3) * (T_STEP - (loop+1));	// 最後なら何もしない
			break;
		}
		
		if ( fSelectZeroDB == 0 ) {
			back_move();
		} else {
			int m = move_hit_kif[current_t-1];
			int bz,az,tk,nf;
			unpack_te(&bz,&az,&tk,&nf, m);
			remove_hit_hash(bz,az,tk,nf);
			current_t--;
		}

		back_num++;
	}

	if ( fSelectZeroDB == 0 ) {
		int i;
		for (i=0;i<back_num;i++) forth_move();
		if ( ply != tesuu ) DEBUG_PRT("ply=%d,tesuu=%d\n",ply,tesuu);
	} else {
		int i;
		for (i=0;i<back_num;i++) {
			int m = move_hit_kif[current_t+i];
			int bz,az,tk,nf;
			unpack_te(&bz,&az,&tk,&nf, m);
			move_hit_hash(bz,az,tk,nf);
		}
	}

	if ( T_STEP == 1 ) base += (28 + 14 + 3) * (6 - 1);

#if 1
//	int prev_base = base;
/*
	add_base = 28*2;
	int i;
	for (i=1;i<8;i++) {
		int n0 = mo_m[i];
		int n1 = mo_c[i];
		if  ( flip ) {
			int tmp = n0;
			n0 = n1;
			n1 = tmp;
		}
		// 持ち駒の最大数
		int mo_max[8] = { 0, 8, 4, 4, 4, 4, 2, 2 }; 	// 9+5*4+3*2 = 35
//		int mo_add[8] = { 0, 0, 9,14,19,24,29,32 }; 	// 9+5*4+3*2 = 35
		int mo_add[8] = { 0, 0, 8,12,16,20,24,26 }; 	// 8+4*4+2*2 = 28
		if ( n0 > mo_max[i] ) n0 = mo_max[i];
		if ( n1 > mo_max[i] ) n1 = mo_max[i];
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			for (int j=0;j<n0;j++) set_dcnn_data(stock_num, data, base+mo_add[i]+j   , y,x);
			for (int j=0;j<n1;j++) set_dcnn_data(stock_num, data, base+mo_add[i]+j+28, y,x);
		}
	}
	base += add_base;
*/

/*
	add_base = 81*2;
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		int z = make_z(x+1,y+1);
		int n = y*9 + x;
		if ( flip ) n = (B_SIZE - y -1)*9 + (B_SIZE - x -1);
		for (loop=0; loop<8; loop+=2) {
			int dz = z8[loop];
			for ( int az = z+dz; ; az += dz ) {
				int tk = init_ban[az];
				if ( tk == 0xff ) break;
				int yy = (az & 0xf0) >> 4;
				int xx = (az & 0x0f);
				yy -= 1;
				xx -= 1;
				if ( xx < 0 || yy < 0 ) DEBUG_PRT("");
				if  ( flip ) {
					yy = B_SIZE - y -1;
					xx = B_SIZE - x -1;
				}
				set_dcnn_data(stock_num, data, base+ n+ (loop&1)*81, yy,xx);
				if ( tk ) break;
			}
		}
	}
	base += add_base;
*/

	for (int loop_k=1; loop_k<=15; loop_k++) {
		if ( loop_k == 13 ) continue;	// 14枚
		add_base = 2;
		clear_kb_kiki_kn();
		init();
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			int z = make_z(x+1,y+1);
			if ( (init_ban[z] & 0x0f) == loop_k ) kiki_write(z);
		}
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
			int z = make_z(x+1,y+1);
			int n0 = kiki_m[z];	// sente
			int n1 = kiki_c[z];

			int kage;
			kage = 0;
			for (int i=0; i<kiki_m[z]; i++) if ( kb_m[z][i] & 0x80 ) kage++;
//			if ( n0 && n0==kage ) { print_kiki(); print_kb(); DEBUG_PRT("m:z=%02x,kage=%d\n",z,kage); }
			n0 -= kage;
			kage = 0;
			for (int i=0; i<kiki_c[z]; i++) if ( kb_c[z][i] & 0x80 ) kage++;
//			if ( n1 && n1==kage ) { print_kiki(); print_kb(); DEBUG_PRT("c:z=%02x,kage=%d\n",z,kage); }
			n1 -= kage;

			int yy = y, xx = x;
			if  ( flip ) {
				int tmp = n0;
				n0 = n1;
				n1 = tmp;
				yy = B_SIZE - y -1;
				xx = B_SIZE - x -1;
			}
//			const int ZERO_ON = 0;
//			const int M = 2;
//			if ( n0>M ) n0=M;
//			if ( n1>M ) n1=M;
//			if ( ZERO_ON==0 ) { n0--; n1--; }
//			if ( n0 >= 0 ) set_dcnn_data(stock_num, data, base+          n0, yy,xx);
//			if ( n1 >= 0 ) set_dcnn_data(stock_num, data, base+M+ZERO_ON+n1, yy,xx);
			if ( n0 ) set_dcnn_data(stock_num, data, base+0, yy,xx);	// 利きの数無視
			if ( n1 ) set_dcnn_data(stock_num, data, base+1, yy,xx);
		}
		base += add_base;
	}

	add_base = 1;
	clear_kb_kiki_kn();
	init();
	allkaku();
	int check = kiki_c[kn[1][1]];
	if  ( flip ) {
		if ( check ) DEBUG_PRT("");
		check = kiki_m[kn[2][1]];
	} else {
		if ( kiki_m[kn[2][1]] ) DEBUG_PRT("");
	}
	if ( check ) for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data(stock_num, data, base, y,x);
	}
	base += add_base;

	// 手合い割
	add_base = 7;
	if ( nHandicap < 0 || nHandicap >= HANDICAP_TYPE ) DEBUG_PRT("");
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data(stock_num, data, base + nHandicap, y,x);
	}
//	if ( nHandicap!=0 ) DEBUG_PRT("");
	base += add_base;

/*
	add_base = 6;
	// 宣言勝ちの点数を計算
	int in_num[2]   = { 0 };	// 敵陣内にある王以外の枚数
	int decl_in[2]  = { 0 };	// 点数(敵陣内の駒＋持駒) ... 宣言に有効
	int decl_sum[2] = { 0 };	// 点数(盤上の駒  ＋持駒)

	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		int z = make_z(x+1,y+1);
		int k = init_ban[z];
		if ( k==0 || (k&0x0f)==0x08 ) continue;	// 空白と王
		int a = 1;
		if ( (k&0x07) >= 0x06 ) a = 5;	// 大駒は5点
		int c = (k&0x80) >> 7;
		decl_sum[c] += a;
		if ( c==0 && y <= 2 ) { decl_in[c] += a; in_num[c]++; }
		if ( c==1 && y >= 6 ) { decl_in[c] += a; in_num[c]++; }
	}

	int s0 = mo_m[1] + mo_m[2] + mo_m[3] + mo_m[4] + mo_m[5] + mo_m[6]*5 + mo_m[7]*5;
	int s1 = mo_c[1] + mo_c[2] + mo_c[3] + mo_c[4] + mo_c[5] + mo_c[6]*5 + mo_c[7]*5;
	decl_in[0] += s0;
	decl_in[1] += s1;
	decl_sum[0] += s0;
	decl_sum[1] += s1;
	if  ( flip ) {
		int tmp;
		tmp = in_num[0];   in_num[0]   = in_num[1];   in_num[1]   = tmp;
		tmp = decl_in[0];  decl_in[0]  = decl_in[1];  decl_in[1]  = tmp;
		tmp = decl_sum[0]; decl_sum[0] = decl_sum[1]; decl_sum[1] = tmp;
	}
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data(stock_num, data, base+0, y,x, (float)in_num[0]  /10.0f);
		set_dcnn_data(stock_num, data, base+1, y,x, (float)in_num[1]  /10.0f);
		set_dcnn_data(stock_num, data, base+2, y,x, (float)decl_in[0] /27.0f);
		set_dcnn_data(stock_num, data, base+3, y,x, (float)decl_in[1] /27.0f);
		set_dcnn_data(stock_num, data, base+4, y,x, (float)decl_sum[0]/27.0f);
		set_dcnn_data(stock_num, data, base+5, y,x, (float)decl_sum[1]/27.0f);
	}
	base += add_base;
*/

	add_base = 9;	// 17 - (1+7+0);
	base += add_base;

	add_base = 10;
//	clear_kb_kiki_kn();	// 王手チェックで生成済み
//	init();
//	allkaku();

	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		int z = make_z(x+1,y+1);
		int n0 = kiki_m[z];	// sente
		int n1 = kiki_c[z];
/*
		int kage;
		kage = 0;
		for (int i=0; i<kiki_m[z]; i++) if ( kb_m[z][i] & 0x80 ) kage++;
		if ( n0 && n0==kage ) DEBUG_PRT("m:z=%02x,kage=%d\n",z,kage);
		n0 -= kage;
		kage = 0;
		for (int i=0; i<kiki_c[z]; i++) if ( kb_c[z][i] & 0x80 ) kage++;
		if ( n1 && n1==kage ) DEBUG_PRT("c:z=%02x,kage=%d\n",z,kage);
		n1 -= kage;
*/
		int yy = y, xx = x;
		if  ( flip ) {
			int tmp = n0;
			n0 = n1;
			n1 = tmp;
			yy = B_SIZE - y -1;
			xx = B_SIZE - x -1;
		}
/*
		float f0 = (float)n0 / 18;
		float f1 = (float)n1 / 18;
		set_dcnn_data(stock_num, data, base+0, yy,xx, f0);
		set_dcnn_data(stock_num, data, base+1, yy,xx, f1);
*/
//		const int ZERO_ON = 1;
		const int M = 4;
		if ( n0>M ) n0=M;
		if ( n1>M ) n1=M;
		int i;
		if ( n0==0 )       set_dcnn_data(stock_num, data, base+0      , yy,xx);
		for (i=0;i<n0;i++) set_dcnn_data(stock_num, data, base+i+1    , yy,xx);
		if ( n1==0 )       set_dcnn_data(stock_num, data, base+0  +M+1, yy,xx);
		for (i=0;i<n1;i++) set_dcnn_data(stock_num, data, base+i+1+M+1, yy,xx);
//		if ( ZERO_ON==0 ) { n0--; n1--; }
//		if ( n0 >= 0 ) set_dcnn_data(stock_num, data, base+          n0, yy,xx);
//		if ( n1 >= 0 ) set_dcnn_data(stock_num, data, base+M+ZERO_ON+n1, yy,xx);
	}
	base += add_base;


	add_base = 0+18 + 17;
	base += add_base;


//	int left = (28 + 14 + 3)*(8-T_STEP) - (base - prev_base);	// 45*X - add
//	base += left;
//	if ( left < 0 ) DEBUG_PRT("");
#endif


	add_base = 2;
	if ( sideToMove == BLACK ) for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data(stock_num, data, base, y,x);
	}
	for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
		set_dcnn_data(stock_num, data, base+1, y,x, (float)ply/512.0f);
	}
	base += add_base;

//	if ( 1 || t==8 ) { hyouji(); int i; for (i=0;i<base;i++) prt_dcnn_data(data,i,-1); DEBUG_PRT("ply=%d,tesuu=%d\n",ply,tesuu); }
	if ( 0 && ply==152 ) {
		hyouji();
		int i;
		for (i=0;i<base;i++) prt_dcnn_data(data,i,-1);

		for (i=0; i<ply; i++) {
			if ( ply-1-i < 0 ) DEBUG_PRT("");
			int m = move_hit_kif[ply-1-i];
			int bz,az,tk,nf;
			unpack_te(&bz,&az,&tk,&nf, m);
			remove_hit_hash(bz,az,tk,nf);
		}
		hyouji();
		PRT("PI\n+\n");
		for (i=0;i<ply;i++) {
			int m = move_hit_kif[i];
			int bz,az,tk,nf;
			unpack_te(&bz,&az,&tk,&nf, m);

			if ( (i)&1 ) PRT("-");
			else         PRT("+");
			int k = 0,b=0,a=0;
			if ( bz == 0xff ) {
				b = 0x00;
				k = tk & 0x07;
			} else {
				k = init_ban[bz] & 0x0f;
				x = 10 - (bz & 0x0f);
				b = (bz>>4) + (x<<4);
				if ( nf ) k += 8;
			}
			int x = 10 - (az & 0x0f);
			a = (az>>4) + (x<<4);
			if ( 0 ) {
				if ( b ) b = 0xaa - b;
				a = 0xaa - a;
			}
			PRT("%02X%02X%s\n",b,a,koma[k]);

			move_hit_hash(bz,az,tk,nf);
		}
		DEBUG_PRT("ply=%d,tesuu=%d,flip=%d\n",ply,tesuu,flip);
	}

	if ( DCNN_CHANNELS != base ) { DEBUG_PRT("Err. DCNN_CHANNELS != base %d\n",base); }
}

// 左右反転
void symmetry_dcnn_channels(float *p_data)
{
	float (*data)[B_SIZE][B_SIZE] = (float(*)[B_SIZE][B_SIZE])p_data;
	int n,x,y;
	for (n=0; n<DCNN_CHANNELS; n++) {
		for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE/2;x++) {
			int tmp                  = data[n][y][B_SIZE-1 - x];
			data[n][y][B_SIZE-1 - x] = data[n][y][           x];
			data[n][y][           x] = tmp;
		}
	}
}

void setModeDevice(int gpu_id)
{
#ifdef CPU_ONLY
	Caffe::set_mode(Caffe::CPU);
	(void)gpu_id;
#else
	Caffe::set_mode(Caffe::GPU);
	Caffe::SetDevice(gpu_id);
#endif
}

#if 0
#if (USE_CAFFE==1)

const int CNN_NUM = 8;

Net<float> *p_caffe_net[CNN_NUM];

pthread_mutex_t cnn_lock[CNN_NUM];


void initCNN(int gpu_id)
{
	static int fDone[CNN_NUM];
	if ( fDone[gpu_id] ) return;
	fDone[gpu_id] = 1;

	setModeDevice(gpu_id);
	const char *p_prot = NULL, *p_trained = NULL;
//	p_prot    = "/home/yss/yssfish/cnn/i128_F128L12p4.prototxt";
//	p_trained = "/home/yss/yssfish/cnn/i128_F128L12p4_s950.caffemodel";
//	p_prot    = "/home/yss/w3680/test/20180122_i362_pro_flood_F64L29_b64_1_half/yss_F64L29.prototxt";
//	p_trained = "/home/yss/w3680/test/20180122_i362_pro_flood_F64L29_b64_1_half/_iter_1880000.caffemodel";

//	p_prot    = "/home/yss/test/20180122_i362_pro_flood_F64L29_b64_1_half/yss_F64L29.prototxt";
//	p_trained = "/home/yss/test/20180129_i362_pro_flood_F64L29_b64_ft5/_iter_2100000.caffemodel";
//	p_prot    = "/home/yss/test/20180122_i362_pro_flood_F64L29_b64_1_half/yss_F64L29.prototxt";
//	p_trained = "/home/yss/test/20180122_i362_pro_flood_F64L29_b64_1_half/_iter_1880000.caffemodel";
	p_prot    = "/home/yss/test/20190306_64L29_policy_160_139_bn_relu_cut_visit_x4_47000_1/aoba_zero_predict.prototxt";
	p_trained = "/home/yss/test/20190306_64L29_policy_160_139_bn_relu_cut_visit_x4_47000_1/_iter_910000.caffemodel";
//	p_prot    = "/home/yss/prg/yssfish/aoba_zero_256x20b_predict.prototxt";
//	p_trained = "/home/yss/prg/yssfish/20190417replay_lr001_wd00002/_iter_964000.caffemodel ";

	p_caffe_net[gpu_id] = new Net<float>(p_prot, caffe::TEST);
	p_caffe_net[gpu_id]->CopyTrainedLayersFrom(p_trained);

	pthread_mutex_init(&cnn_lock[gpu_id],NULL);

//	if ( fSelectZeroDB ) DEBUG_PRT("Err! fSelectZeroDB=%d\n",fSelectZeroDB);
}

int LockCNN(int n)
{
	return pthread_mutex_lock(&cnn_lock[n]);
}
int TryLockCNN(int n)
{
	return pthread_mutex_trylock(&cnn_lock[n]);	// return 0, when lock successes
}
void UnLockCNN(int n)
{
	pthread_mutex_unlock(&cnn_lock[n]);
}

int getNetChannels(int net_type)
{
	if ( net_type == NET_128 ) return 128;
	if ( net_type == NET_361 ) return 361;
	if ( net_type == NET_362 ) return 362;
	DEBUG_PRT("unknown nModel\n");
	return 0;
}

float getResultCNN(Color sideToMove, int moves, shogi *pso, float *result,int net_type, int gpu_id, HASH_SHOGI *phg)
{
	float ret_v = 0;
	const int size = B_SIZE;
	int mini_batch = 1;	// 1 <= mini_batch <= 2

	int input_dim = getNetChannels(net_type);

	float *data = new float[mini_batch*input_dim*size*size];

	pso->setYssChannels(sideToMove, moves, data, net_type, input_dim);
	int b;
	for (b=1;b<mini_batch;b++) {
		int c;
		for (c=0;c<input_dim;c++) {
			int x,y;
			for (y=0;y<B_SIZE;y++) for (x=0;x<B_SIZE;x++) {
				float v = *(data + c*B_SIZE*B_SIZE + y*B_SIZE + x);	// [c][y][x];
				int xx = B_SIZE-1 - x;
				*(data + b*input_dim*B_SIZE*B_SIZE + c*B_SIZE*B_SIZE + y*B_SIZE + xx) = v; // [b][c][y][xx]
			}
//			prt_dcnn_data(stock_num, c, b);
		}
	}

	setModeDevice(gpu_id);

	Net<float> *net = p_caffe_net[gpu_id];
	Blob<float> *input_layer = net->input_blobs()[0];
	input_layer->Reshape(mini_batch, input_dim, size, size);
//	net->Reshape();	// これは必要？なくても正常動作
	input_layer->set_cpu_data(data);	// ポインタをセットしてるだけ
	const vector<Blob<float>*>& rr = net->Forward();

	if ( 0 ) {
		int i;
		for (i=0;i<mini_batch*input_dim*size*size;i++) {
			PRT("xyz %d:%.5f\n",i,data[i]);
		}
		exit(1);
	}
if ( 1 ) {
	int m_a[MOVES_MAX];
	float p_a[MOVES_MAX];
	int m_n = 0;
	int c,y,x;
	float sum = 0,sum_legal = 0;
	int fGoteTurn = ( sideToMove == BLACK );
 	int dep = pso->fukasa;
	for (c=0;c<139;c++) for (y=0;y<9;y++) for (x=0;x<9;x++) {
		float a = rr[0]->cpu_data()[c*81+y*9+x];

		int m = get_move_from_c_y_x(c, y, x);
//		PRT("%08x ",m);
		if ( m==0 ) a = 0;	// こんなことしてなくても最初からa=0。DL優秀

		sum += a;
//		PRT("%5.3f ",a);
		if ( m!=0 && a > 0.0000 ) {
			int bz,az,tk,nf;
			unpack_te(&bz,&az,&tk,&nf, m);
//			if ( bz==0xff && fGoteTurn ) tk |= 0x80;
			if ( fGoteTurn ) {
				hanten_sasite(&bz,&az,&tk,&nf);
			}
			if ( bz != 0xff ) tk = pso->init_ban[az];
			int mm = pack_te(bz,az,tk,nf);
			int fOK = 0;
			if ( pso->is_pseudo_legalYSS((Move)mm, sideToMove) ) fOK = 1;
			if ( fOK ) {
				if ( m_n >= MOVES_MAX ) DEBUG_PRT("MOVES_MAX!\n");
				p_a[m_n] = a;
				m_a[m_n++] = mm;
				
				
				int move_num = phg->child_num;
				int i;
				for ( i = 0; i < move_num; i++ ) {
					CHILD *pc = &phg->child[i];
//					PRT("%3d:pc->move=%08x\n",i,pc->move);
					if ( pc->move == mm ) {
						pc->bias = a;
						sum_legal += a;
						break;
					}
				}
//				if ( i==move_num ) DEBUG_PRT("move not found! mm=%08x,a=%f\n",mm,a);	// 王手無視がある

			}
		}
//		if ( x==8 ) PRT("\n");
//		if ( x==8 && y==8 ) PRT("\n");
	}
	if ( dep==0 ) PRT("sum=%.3f,sum_legal=%.3f\n",sum,sum_legal);
	// sort
 	int i,j;
	for (i=0;i<m_n-1;i++) {
		float max_a = p_a[i];
		int max_i = i;
		for (j=i+1;j<m_n;j++) {
			if ( p_a[j] <= max_a ) continue;
			max_a = p_a[j];
			max_i = j;
		}
		if ( max_i == i ) continue;
		float tmp_a = p_a[i];
		int   tmp_m = m_a[i];
		p_a[i] = p_a[max_i];
		m_a[i] = m_a[max_i];
		p_a[max_i] = tmp_a;
		m_a[max_i] = tmp_m;
	}

	float mul = 1.0f;
	if ( sum > sum_legal && sum_legal > 0 ) mul = sum / sum_legal;
	for ( i = 0; i < phg->child_num; i++ ) {
		CHILD *pc = &phg->child[i];
		pc->bias *= mul;
	}

	float sum_adj = 0;
	for (c=0;c<m_n;c++) {
		if ( dep==0 && c < 30 ) PRT("%3d:%08x,%6.4f(-> %6.4f)\n",c,m_a[c],p_a[c],p_a[c]*mul);
		p_a[c] *= mul;
		sum_adj += p_a[c];
	}
	float row_v = rr[1]->cpu_data()[0];	// -1 〜 +1 まで。+1 で勝ちの局面。常に先手番として評価(後手はひっくり返してる)
	float v_fix = row_v;
	if ( sideToMove==BLACK ) v_fix = -v_fix;

	ret_v = v_fix;
	
	PRT("%.3f(%6.3f)",v_fix,row_v); PS->print_path();
	if ( dep==0 ) PRT("moves=%d,sum_adj=%.3f,sum_legal=%.3f,row_v=%6.3f\n",m_n,sum_adj,sum_legal,row_v);
//	PRT("value:%6.3f\n",a);
//	for (i=0;i<100;i++) PRT("%4d, %f\n",i,rr[0]->cpu_data()[i]);
}

/*
	int i;
	for (i=0;i<unique_max;i++) {
		result[i] = 0;
	}
	for (b=0;b<mini_batch;b++) {
		for (i=0;i<unique_max;i++) {
			float v = rr[0]->cpu_data()[b*unique_max + i];
			int r_i = i;
			if ( b==1 ) {
				int m = get_te_from_unique(i);
				int bz = get_bz(m), az = get_az(m), tk = get_tk(m), nf = get_nf(m);
				az = (az & 0xf0) + (10 - (az & 0x0f));
				if ( bz != 0xff ) bz = (bz & 0xf0) + (10 - (bz & 0x0f));
				r_i = get_unique_from_te(bz, az, tk, nf);
			}
			
			result[r_i] += v / (float)mini_batch;
		}
	}
*/
	if ( 0 ) {
		float sum = 0;
		int i;
		for (i=0;i<unique_max;i++) {
			float v = rr[0]->cpu_data()[i];
//			int te = get_te_from_unique(i);
//			sort_te[i] = te;
//			sort_v[i]  = v;
			sum += v;
//			PRT("%5.3f, %08x ",v,te);
//			if ( ((i+1)%4)==0 ) PRT("\n");
		}
//		PRT("\nsum=%.3f\n",sum);
	}

	delete[] data;
	return ret_v;
}

int shogi::get_cnn_next_move()
{
	initCNN(0);

	int ret_m = 0;
	float ret_v = 0;
	float result[unique_max];
//	double ct1 = clock();
	Color sideToMove = WHITE;
	if ( (tesuu & 1) == 1 ) sideToMove = BLACK;

//	if ( sideToMove == BLACK ) hanten_with_hash_kifu();

//	getResultCNN(sideToMove, tesuu, this, result, NET_362, 0);
//	getResultCNN(sideToMove, tesuu, this, result, NET_361, 0);
	return 0;

	int i;
	float sum = 0;
	int   sort_te[unique_max];
	float sort_v[unique_max];
	for (i=0;i<unique_max;i++) {
		float v = result[i];
		int te = get_te_from_unique(i);
		sort_te[i] = te;
		sort_v[i]  = v;
		sum += v;
//		PRT("%5.3f, %08x ",v,te);
//		if ( ((i+1)%4)==0 ) PRT("\n");
	}
//	PRT("\nsum=%.3f\n",sum);

	for (i=0;i<unique_max-1;i++) {
		float max_v = sort_v[i];
		int   max_i = i;
		for (int j=i+1;j<unique_max;j++) {
			if ( sort_v[j] <= max_v ) continue;
			max_v = sort_v[j];
			max_i = j;
		}
		if ( max_i == i ) continue;
		float tmp_v  = sort_v[i];
		int   tmp_te = sort_te[i];
		sort_v[i]  = sort_v[max_i];
		sort_te[i] = sort_te[max_i];
		sort_v[max_i] = tmp_v;
		sort_te[max_i] = tmp_te;
	}	
	sum = 0;
	for (i=0;i<unique_max;i++) {
		float v = sort_v[i];
		int   m = sort_te[i];
		int bz = get_bz(m), az = get_az(m), tk = get_tk(m), nf = get_nf(m);
		if ( bz != 0xff ) tk = init_ban[az];
		int mm = pack_te(bz,az,tk,nf);
		
		bool ok = is_pseudo_legalYSS((Move)mm, WHITE);
		if ( ok==true ) {
			do_moveYSS((Move)mm);
			int oute = kiki_c[kn[1][1]];
			undo_moveYSS((Move)mm);
			if ( oute ) ok = false;
		}
		if ( ok==true && ret_m == 0 ) {
			ret_m = mm;
			ret_v = v;
		}
		sum += v;
		if ( i<20 ) PRT("%3d:%08x %8.5f %8.5f %d\n",i,mm,v,sum, (ok==true));
	}

//	if ( sideToMove == BLACK ) hanten_with_hash_kifu();
	if ( sideToMove == BLACK && ret_m != 0 ) {
		int bz = get_bz(ret_m), az = get_az(ret_m), tk = get_tk(ret_m), nf = get_nf(ret_m);
		hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
		ret_m = pack_te(bz,az,tk,nf);
	}

//	int u = get_unique_from_te(bz,az,tk,nf);
//	if ( u < 0 ) DEBUG_PRT("i=%d\n",i);

	print_usi_best_update((int)(ret_v * 2000));

//	double t = (clock() - ct1)/CLOCKS_PER_SEC;
//	fprintf(stderr, "%d:i=%d,t=%.1f, %f\n",kind,i,t, t/i);
	return ret_m;
}
#endif


													  
HASH_SHOGI *hash_shogi_table = NULL;
int Hash_Shogi_Table_Size = 1024*4*1;	// 9路 16*16で754MB, 19路 16*4で793MB, 16*16で3.2GB, 16*16*4で12.7GB
int Hash_Shogi_Mask;
int hash_shogi_use = 0;		
int hash_shogi_sort_num = 0;
int thinking_age = 0;

const int UCT_LOOP_FIX = 50;

#define REHASH_SHOGI (REHASH_MAX-1)

void set_Hash_Shogi_Table_Size(int size)
{
	Hash_Shogi_Table_Size = size;
}

void hash_shogi_table_reset()
{
	HASH_ALLOC_SIZE size = sizeof(HASH_SHOGI) * Hash_Shogi_Table_Size;
	memset(hash_shogi_table,0,size);
	for (int i=0;i<Hash_Shogi_Table_Size;i++) {
		hash_shogi_table[i].deleted = 1;
		LockInit(hash_shogi_table[i].entry_lock);
	}
	hash_shogi_use = 0;
}

void hash_shogi_table_clear()
{
	Hash_Shogi_Mask       = Hash_Shogi_Table_Size - 1;
	HASH_ALLOC_SIZE size = sizeof(HASH_SHOGI) * Hash_Shogi_Table_Size;
	if ( hash_shogi_table == NULL ) hash_shogi_table = (HASH_SHOGI*)malloc( size );
	if ( hash_shogi_table == NULL ) { DEBUG_PRT("Fail malloc hash_shogi\n"); }
	PRT("HashShogi=%7d(%3dMB),sizeof(HASH_SHOGI)=%d,Hash_SHOGI_Mask=%d\n",Hash_Shogi_Table_Size,(int)(size/(1024*1024)),sizeof(HASH_SHOGI),Hash_Shogi_Mask);
	hash_shogi_table_reset();
}

int IsHashFull()
{
	if ( hash_shogi_use >= Hash_Shogi_Table_Size*90/100 ) {
		PRT("hash full! hash_shogi_use=%d,Hash_Shogi_Table_Size=%d\n",hash_shogi_use,Hash_Shogi_Table_Size);
		return 1;
	}
	return 0; 
}
void all_hash_go_unlock()
{
	for (int i=0;i<Hash_Shogi_Table_Size;i++) UnLock(hash_shogi_table[i].entry_lock);
}
void hash_half_del()
{
	int i,sum = 0;
	for (i=0;i<Hash_Shogi_Table_Size;i++) if ( hash_shogi_table[i].deleted==0 ) sum++;
	if ( sum != hash_shogi_use ) PRT("warning! sum=%d,hash_shogi_use=%d\n",sum,hash_shogi_use);
	hash_shogi_use = sum;	// hash_shogi_useはロックしてないので12スレッドだと頻繁にずれる

	int max_sum = 0;
	int del_games = max_sum * 5 / 10000;	// 0.05%以上。5%程度残る。メモリを最大限まで使い切ってる場合のみ。age_minus = 2 に。

	const double limit_occupy = 50;		// 50%以上空くまで削除
	const int    limit_use    = (int)(limit_occupy*Hash_Shogi_Table_Size / 100);
	int del=0,age_minus = 4;
	for (;age_minus>=0;age_minus--) {
		for (i=0;i<Hash_Shogi_Table_Size;i++) {
			HASH_SHOGI *pt = &hash_shogi_table[i];
			if ( pt->deleted == 0 && pt->used && (pt->age <= thinking_age - age_minus || pt->games_sum < del_games) ) {
				memset(pt,0,sizeof(HASH_SHOGI));
				pt->deleted = 1;
//				pt->used = 0;	// memsetで既に0
				hash_shogi_use--;
				del++;
			}
//			if ( hash_go_use < limit_use ) break;	// いきなり10分予測読みして埋めてしまっても全部消さないように --> 前半ばっかり消して再ハッシュでエラーになる。
		}
		double occupy = hash_shogi_use*100.0/Hash_Shogi_Table_Size;
		PRT("hash del=%d,age=%d,minus=%d, %.0f%%(%d/%d)\n",del,thinking_age,age_minus,occupy,hash_shogi_use,Hash_Shogi_Table_Size);
		if ( hash_shogi_use < limit_use ) break;
		if ( age_minus==0 ) { DEBUG_PRT("age_minus=0\n"); }
	}
}

void free_hash_shogi_table()
{
	if ( hash_shogi_table != NULL ) {
		free(hash_shogi_table);
		hash_shogi_table = NULL;
	}
}

HASH_SHOGI* shogi::HashShogiReadLock()
{
research_empty_block:
	int n,first_n,loop = 0;
	
	uint64 hashcode64 = get_hashcode64();
//	PRT("phg->hash=%" PRIx64 ",%08x,%08x,%08x\n",hashcode64,hash_code1,hash_code2,hash_motigoma);

	n = (int)hashcode64 & Hash_Shogi_Mask;
	first_n = n;

	HASH_SHOGI *pt_base = hash_shogi_table;
	HASH_SHOGI *pt_first = NULL;

	for (;;) {
		HASH_SHOGI *pt = &pt_base[n];
		Lock(pt->entry_lock);		// Lockをかけっぱなしにするように
		if ( pt->deleted == 0 ) {
			if ( hashcode64 == pt->hashcode64 ) {
				return pt;
			}
		} else {
			if ( pt_first == NULL ) pt_first = pt;
		}

		UnLock(pt->entry_lock);
		// 違う局面だった
		if ( loop == REHASH_SHOGI ) break;	// 見つからず
		if ( loop >= 2 && pt_first ) break;	// 妥協。2回探してなければ未登録扱い。
		n = (rehash[loop++] + first_n ) & Hash_Shogi_Mask;
	}
	if ( pt_first ) {
		// 検索中に既にpt_firstが使われてしまっていることもありうる。もしくは同時に同じ場所を選んでしまうケースも。
		Lock(pt_first->entry_lock);
		if ( pt_first->deleted == 0 ) {	// 先に使われてしまった！
			UnLock(pt_first->entry_lock);
			goto research_empty_block;
		}
		return pt_first;	// 最初にみつけた削除済みの場所を利用
	}
	int sum = 0;
	for (int i=0;i<Hash_Shogi_Table_Size;i++) { sum = hash_shogi_table[i].deleted; PRT("%d",hash_shogi_table[i].deleted); }
	DEBUG_PRT("\nno child hash Err loop=%d,hash_shogi_use=%d,first_n=%d,del_sum=%d(%.1f%%)\n",loop,hash_shogi_use,first_n,sum, 100.0*sum/Hash_Shogi_Table_Size); return NULL;
}

float f_rnd()
{
	double rnd();	// 内部でrnd521()を呼んでいる
	return (float)rnd();
}

/*
void UCTNode::dirichlet_noise(float epsilon, float alpha) {
    auto dirichlet_vector = std::vector<float>{};
    std::gamma_distribution<float> gamma(alpha, 1.0f);
*/

char *shogi::prt_pv_from_hash(int ply)
{
	static char str[TMP_BUF_LEN];
	if ( ply==0 ) str[0] = 0;
	HASH_SHOGI *phg = HashShogiReadLock();
	UnLock(phg->entry_lock);
	if ( phg->used == 0 ) return str;
	if ( phg->hashcode64 !=get_hashcode64() ) return str;
	if ( ply > 30 ) return str;

	int max_i = -1;
	int max_games = 0;
	int i;
	for (i=0;i<phg->child_num;i++) {
		CHILD *pc = &phg->child[i];
		if ( pc->games > max_games ) {
			max_games = pc->games;
			max_i = i;
		}
	}
	if ( max_i >= 0 ) {
		CHILD *pc = &phg->child[max_i];
		do_moveYSS((Move)pc->move);
		int bz,az,tk,nf;
		unpack_te(&bz,&az,&tk,&nf, pc->move);
		if ( ply > 0 ) strcat(str," ");
		strcat(str,str_usi_move(bz,az,tk,nf));
		
		print_teban(ply+tesuu+1);
		print_te_no_space(pc->move);
		prt_pv_from_hash(ply+1);
		undo_moveYSS((Move)pc->move);
	}
	return str;
}

int shogi::uct_search_start(Color sideToMove)
{
	static int fDone = 0;
	if ( fDone==0 ) {
		init_rnd521( (int)time(NULL)+getpid_YSS() );		// 起動ごとに異なる乱数列を生成
		fDone = 1;
	}

/*
	thinking_age = (thinking_age + 1) & 0x7ffffff;
	if ( thinking_age == 0 ) thinking_age = 1;
	if ( thinking_age == 1 ) {
		hash_shogi_table_clear();
	} else {
		hash_half_del();
	}
*/
	hash_shogi_table_clear();
	HASH_SHOGI *phg = HashShogiReadLock();
	create_node(sideToMove, phg);
	UnLock(phg->entry_lock);


	int ct1 = get_clock();
	int uct_count = UCT_LOOP_FIX;
	int loop;
	for (loop=0; loop<uct_count; loop++) {
		uct_tree(sideToMove);

//		if ( IsNegaMaxTimeOver() ) break;
//		if ( is_main_thread() ) PassWindowsSystem();	// GUIスレッド以外に渡すと中断が利かない場合あり
		if ( IsHashFull() ) break;
	}
	double ct = get_spend_time(ct1);

	// select best
	int best_move = 0;
	int max_i = -1;
	int max_games = 0;
	int sum_games = 0;
	int i;
	for (i=0;i<phg->child_num;i++) {
		CHILD *pc = &phg->child[i];
		if ( pc->games > max_games ) {
			max_games = pc->games;
			max_i = i;
		}
		sum_games += pc->games;
		if ( pc->games ) PRT("%3d:%08x,%3d,%6.3f,bias=%6.3f\n",i,pc->move,pc->games,pc->value,pc->bias);
	}
	if ( max_i >= 0 ) {
		CHILD *pc = &phg->child[max_i];
		best_move = pc->move;
		PRT("best:%08x,%3d,%6.3f,bias=%6.3f\n",pc->move,pc->games,pc->value,pc->bias);

		char *pv_str = prt_pv_from_hash(0); PRT("\n");
		PRT("%.2f sec, child_num=%d,create=%d,loop=%d,%.0f/s\n",ct,phg->child_num,hash_shogi_use,loop,(double)loop/ct);

		char str[TMP_BUF_LEN*4];
		float v = pc->value;
		if ( sideToMove == BLACK ) v = v + 1;
		int vi = (int)((v - 0.5)*4000.0f);
		sprintf(str,"info depth %d score cp %d nodes %d pv %s\n",0,vi,phg->games_sum,pv_str);
		send_usi(str);	// floodgate only
	}

//	const int SORT_MAX = MAX_LEGAL_MOVES;	// 593
	const int SORT_MAX = 8;
	int sort[SORT_MAX][2];
	int sort_n = 0;
	
	for (i=0;i<phg->child_num;i++) {
		CHILD *pc = &phg->child[i];
		if ( pc->games > max_games ) {
			max_games = pc->games;
			max_i = i;
		}
		sum_games += pc->games;
		if ( pc->games ) {
			PRT("%3d:%8x,%3d,%6.3f,bias=%6.3f\n",i,pc->move,pc->games,pc->value,pc->bias);
			if ( sort_n < SORT_MAX ) {
				sort[sort_n][0] = pc->games;
				sort[sort_n][1] = pc->move;
				sort_n++;
			}
		}
	}
	if ( max_i >= 0 ) {
		CHILD *pc = &phg->child[max_i];
		best_move = pc->move;
		PRT("best:%08x,%3d,%6.3f,bias=%6.3f\n",pc->move,pc->games,pc->value,pc->bias);

//		char *pv_str = prt_pv_from_hash(ptree, ply, sideToMove); PRT("%s\n",pv_str);
	}

	for (i=0; i<sort_n-1; i++) {
		int max_i = i;
		int max_g = sort[i][0];
		int j;
		for (j=i+1; j<sort_n; j++) {
			if ( sort[j][0] <= max_g ) continue;
			max_g = sort[j][0];
			max_i = j;
		}
		if ( max_i == i ) continue;
		int tmp_i = sort[i][0];
		int tmp_m = sort[i][1];
		sort[i][0] = sort[max_i][0];
		sort[i][1] = sort[max_i][1];
		sort[max_i][0] = tmp_i;
		sort[max_i][1] = tmp_m;
	}
/*	
	char buf_move_count[256];
	buf_move_count[0] = 0;
	sprintf(buf_move_count,"%d",sum_games);
	for (i=0;i<sort_n;i++) {
		char buf[7];
//		csa2usi( ptree, str_CSA_move(sort[i][1]), buf );
//		if ( 0 ) strcpy(buf,str_CSA_move(sort[i][1]));
//		PRT("%s,%d,",str_CSA_move(sort[i][1]),sort[i][0]);
		char str[TMP_BUF_LEN];
		getCsaStr(buf, bz,az,tk,nf, tSenteTurn)

		sprintf(str,",%s,%d",buf,sort[i][0]);
		strcat(buf_move_count,str);
		strcpy(kifu_comment[tesuu], buf_move_count);
//		PRT("%s",str);
	}
//	PRT("\n");
*/
/*
	// selects moves proportionally to their visit count
	if ( ptree->nrep < nVisitCount && sum_games > 0 ) {
		int r = irnd() % sum_games;
		int s = 0;
		CHILD *pc = NULL;
		for (i=0; i<phg->child_num; i++) {
			pc = &phg->child[i];
			s += pc->games;
			if ( s > r ) break;
		}
		if ( i==phg->child_num ) DEBUG_PRT("not found\n");
		best_move = pc->move;
		PRT("rand select:%s,%3d,%6.3f,bias=%6.3f,r=%d\n",str_CSA_move(pc->move),pc->games,pc->value,pc->bias,r);
	}
	PRT("%.2f sec, child_num=%d,create=%d,loop=%d,%.0f/s, (%d/%d),fAddNoise=%d\n",ct,phg->child_num,hash_shogi_use,loop,(double)loop/ct,ptree->nrep,nVisitCount,fAddNoise );
*/	

	return best_move;
}

void shogi::create_node(Color sideToMove, HASH_SHOGI *phg)
{
	if ( phg->used ) {
		PRT("別経路で作成？深=%d,games_sum=%d,子=%d, ",fukasa,phg->games_sum,phg->child_num); print_tejun();
		return;
	}

	int root_learning_sasite[MOVES_MAX][8];

	int moves_num = generate_all_move_root((int)sideToMove, root_learning_sasite[0]);
	int loop;
	for (loop=0; loop<moves_num; loop++) {
		int *p = root_learning_sasite[loop];
		int m = pack_te(*(p+0),*(p+1),*(p+2),*(p+3));
		CHILD *pc = &phg->child[loop];
		pc->move = m;
		pc->bias = f_rnd() * 0.1f;
		pc->games = 0;
		pc->value = 0;
	}

	phg->child_num      = moves_num;
	phg->col            = sideToMove;

	float v = get_network_policy_value( sideToMove, PS->tesuu + fukasa, phg );
//	float v = f_rnd();
	if ( sideToMove==BLACK ) v = -v;

	phg->hashcode64     = get_hashcode64();
	phg->games_sum      = 0;	// この局面に来た回数(子局面の回数の合計)
	phg->used           = 1;
	phg->age            = thinking_age;
	phg->deleted        = 0;
	phg->net_value      = v;
	phg->has_net_value  = 1;

//	PRT("create_node(),"); prt64(phg->hashcode64); PRT("\n"); print_path(); 
	hash_shogi_use++;
}

double shogi::uct_tree(Color sideToMove)
{
	int create_new_node_limit = 1;

//	uct_nodes++;
	HASH_SHOGI *phg = HashShogiReadLock();	// phgに触る場合は必ずロック！

	if ( phg->used == 0 ) {	// 1手目に対して2手目がすべての同じ(絶対手)の場合
		DEBUG_PRT("not created? 深=%d,col=%d,c_num=%d\n",fukasa,phg->col,phg->child_num);
//		create_node(sideToMove, phg);
	}

	if ( phg->col != sideToMove ) { DEBUG_PRT("hash col Err. phg->col=%d,col=%d,age=%d(%d),child_num=%d,games_sum=%d,sort=%d,phg->hash=%" PRIx64 "\n",phg->col,sideToMove,phg->age,thinking_age,phg->child_num,phg->games_sum,phg->sort_done,phg->hashcode64); }

	int child_num = phg->child_num;

	int select = -1;
	int loop;
	double max_value = -10000;

select_again:
 	for (loop=0; loop<child_num; loop++) {
		CHILD *pc = &phg->child[loop];
		if ( pc->value == ILLEGAL_MOVE ) continue;

		const double cBASE = 19652.0;
		const double cINIT = 1.25;
		double c = (log((1.0 + phg->games_sum + cBASE) / cBASE) + cINIT);
		double puct = c * pc->bias * sqrt((double)phg->games_sum + 1.0) / (pc->games + 1.0);
		double mean_action_value = (pc->games == 0) ? -1.0 : pc->value;
		double uct_value = mean_action_value + 2.0 * puct;
//		if ( fukasa==0 ) PRT("%3d:v=%5.3f,bias=%.3f,p=%5.3f,u=%5.3f,g=%4d,s=%5d\n",loop,pc->value,pc->bias,puct,uct_value,pc->games,phg->games_sum);
		if ( uct_value > max_value ) {
			max_value = uct_value;
			select = loop;
		}
	}
	if ( select < 0 ) {
		float v = -1;
		if ( sideToMove==BLACK ) v = -1;
//		PRT("no legal move. mate? depth=%d,child_num=%d,v=%.0f\n",fukasa,child_num,v);
		UnLock(phg->entry_lock);
		return v;
	}

	// 実際に着手
	CHILD *pc = &phg->child[select];

	do_moveYSS((Move)pc->move);
//	hyouji();	
//	PRT("%2d:%08x(%3d/%5d):select=%3d,v=%.3f\n",fukasa,pc->move,pc->games,phg->games_sum,select,max_value);
	if ( Check(sideToMove) ) {
//		hyouji();	
		PRT("王手回避ミス%2d:%08x(%2d/%3d):selt=%3d,v=%.3f\n",fukasa,pc->move,pc->games,phg->games_sum,select,max_value);
		undo_moveYSS((Move)pc->move);
		pc->value = ILLEGAL_MOVE;
		select = -1;
		max_value = -10000;
		goto select_again;
	}
	if ( Check(flip_color(sideToMove) ) && (pc->move & 0xff000fff)==0xff000100 ) {
		// 打歩詰？
		int k;
		if ( sideToMove==WHITE ) {
			k = tunderu_m(0);
		} else {
			k = tunderu(0);
		}
		if ( k==TUMI ) {
			PRT("打歩詰%2d:%08x(%2d/%3d):selt=%3d,v=%.3f\n",fukasa,pc->move,pc->games,phg->games_sum,select,max_value);
			undo_moveYSS((Move)pc->move);
			pc->value = ILLEGAL_MOVE;
			select = -1;
			max_value = -10000;
			goto select_again;
		}
	}

	path[fukasa] = pc->move;	// 手順を（位置を）記憶
	hash_path[fukasa] = get_hashcode64();
	fukasa++;
	if ( fukasa >= D_MAX ) { DEBUG_PRT("depth over=%d\n",fukasa); }

	double win = 0;

	int do_playout = 0;
	if ( pc->games < create_new_node_limit || fukasa == (D_MAX-1) ) {
		do_playout = 1;
	}
	if ( do_playout ) {	// evaluate this position
		UnLock(phg->entry_lock);

		HASH_SHOGI *phg2 = HashShogiReadLock();	// 1手進めた局面のデータ
		if ( phg2->used ) {
//			PRT("has come already?\n"); //debug();	// 手順前後?
		} else {
			create_node(flip_color(sideToMove), phg2);
		}
		win = -phg2->net_value;
		
		UnLock(phg2->entry_lock);
		Lock(phg->entry_lock);

	} else {
		// down tree
		const int fVirtualLoss = 1;
		const int VL_N = 1;
		int one_win = -1;
		if ( fVirtualLoss ) {	// この手が負けた、とする。複数スレッドの時に、なるべく別の手を探索するように
			pc->value = (float)(((double)pc->games * pc->value + one_win*VL_N) / (pc->games + VL_N));	// games==0 の時はpc->value は無視されるので問題なし
			pc->games      += VL_N;
			phg->games_sum += VL_N;	// 末端のノードで減らしても意味がない、のでUCTの木だけで減らす
		}

		UnLock(phg->entry_lock);
		win = -uct_tree(flip_color(sideToMove));
		Lock(phg->entry_lock);

		if ( fVirtualLoss ) {
			phg->games_sum -= VL_N;
			pc->games      -= VL_N;		// gamesを減らすのは非常に危険！ あちこちで games==0 で判定してるので
			if ( pc->games < 0 ) { DEBUG_PRT("Err pc->games=%d\n",pc->games); }
			if ( pc->games == 0 ) pc->value = 0;
			else                  pc->value = (float)((((double)pc->games+VL_N) * pc->value - one_win*VL_N) / pc->games);
		}
	}

	fukasa--;
	undo_moveYSS((Move)pc->move);

	double win_prob = ((double)pc->games * pc->value + win) / (pc->games + 1);	// 単純平均

	pc->value = (float)win_prob;
	pc->games++;			// この手を探索した回数
	phg->games_sum++;
	phg->age = thinking_age;

	UnLock(phg->entry_lock);
	return win;
}


int shogi::think_kifuset()
{
	int ct1 = get_clock();

	if ( IsSenteTurn() ) hanten_with_hash();
	hash_calc(0,1);	// ハッシュ値を作りなおす
	
	int bz,az,tk,nf;
	int k = thinking_with_level( &bz,&az,&tk,&nf );

	if ( IsSenteTurn() ) hanten_with_hash();

	int t = (int)get_spend_time(ct1);

	if ( k == -TUMI ) return k;

	if ( IsSenteTurn() ) hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
	kifu_set_move_sennitite(bz,az,tk,nf,t);

	return k;
}


float shogi::get_network_policy_value(Color sideToMove, int ply, HASH_SHOGI *phg)
{
#if (USE_CAFFE==0)
	return 0;
#else

	initCNN(0);

	int i;
	for (i=0;i<tesuu;i++) {
		move_hit_kif[i] = pack_te( kifu[i+1][0],kifu[i+1][1],kifu[i+1][2],kifu[i+1][3] );
	}
	for (i=0;i<fukasa;i++) {
		move_hit_kif[tesuu+i] = path[i];
	}

	for (i=tesuu+fukasa-1;i>=0;i--) {
		int bz,az,tk,nf;
		unpack_te(&bz,&az,&tk,&nf, move_hit_kif[i]);
//		PRT("fukasa=%d,ply=%d,tesuu=%d,%3d:%08x\n",fukasa,ply,tesuu,i,move_hit_kif[i]);
		remove_hit_hash(bz,az,tk,nf);
	}
	for (i=0;i<tesuu+fukasa;i++) {
		int bz,az,tk,nf;
		unpack_te(&bz,&az,&tk,&nf, move_hit_kif[i]);
		move_hit_hash(bz,az,tk,nf);
		move_hit_hashcode[i][0] = hash_code1;
		move_hit_hashcode[i][1] = hash_code2;
		move_hit_hashcode[i][2] = hash_motigoma;
	}
/*
	for (i=0;i<fukasa;i++) {
		move_hit_kif[tesuu+i] = path[i];
		move_hit_hashcode[tesuu+i][0] = hash_path[i];	// 手抜き
		move_hit_hashcode[tesuu+i][1] = hash_path[i]; 
		move_hit_hashcode[tesuu+i][2] = hash_path[i];
	}
*/
	if ( tesuu + fukasa != ply ) DEBUG_PRT("Err\n");

	if ( 0 ) {
		int size = 1*DCNN_CHANNELS*B_SIZE*B_SIZE;
		float *data = new float[size];
		memset(data, 0, sizeof(float)*size);

		set_dcnn_channels( sideToMove, ply, data, 0, 0);
//		if ( ply==1 ) { prt_dcnn_data_table((float(*)[B_SIZE][B_SIZE])data); debug(); }
		{ int sum=0; int i; for (i=0;i<size;i++) sum = 37*sum + (int)(data[i]+0.1f); PRT("mul sum=%d\n",sum); }
		delete[] data;
	}
	
//	const auto result = Network::get_scored_moves_yss_zero((float(*)[B_SIZE][B_SIZE])data);
//  float net_eval = result.second;
	float result[unique_max];
	float net_v = getResultCNN( sideToMove, ply, this, result, NET_362, 0, phg);
	if ( fukasa==0 ) PRT("root v=%f\n",net_v);
//	float v = (net_v + 1.0f) / 2.0f;	// 0 <= v <= 1
//    if ( sideToMove ) {	// DCNNは自分が勝ちなら+1、負けなら-1を返す。探索中は先手は+1〜0, 後手は 0〜-1を返す。
//        v = v - 1;
//    }
//	float v = f_rnd();
//	if ( sideToMove==BLACK ) v = -v;
//	phg->net_value      = net_v;

//  std::vector<Network::scored_node> nodelist;

//	PRT("result.first.size()=%d\n",result.first.size());
//	auto &node = result.first;
//	PRT("node.size()=%d\n",node.size());
//	{ int i; for (i=0;i<60;i++) PRT("%f,%d\n",node[i].first,node[i].second); }
/*
    for (const auto& node : result.first) {
        auto id = node.second;
        legal_sum += node.first;
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
		
	    if ( id < 100 ) PRT("%4d,%08x(%08x),%f\n",id,yss_m,bona_m, node.first);
    }
*/
/*
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
		int cap  = (int)UToCap(move);
		int piece_m	= (int)I2PieceMove(move);

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
		PRT("%3d:%s(%d)(%08x)id=%5d, bias=%8f,from=%2d,to=%2d,cap=%2d,drop=%3d,pr=%d,peice=%d\n",i,str_CSA_move(move),sideToMove,yss_m,id,bias, from,to,cap,drop,is_promote,piece_m);

		pc->bias = bias;
		legal_sum += bias;
	}
	PRT("legal_sum=%9f, raw_v=%10f,set_v=%10f\n",legal_sum, result.second,phg->net_value );
*/
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

	return net_v;
#endif
}


void self_mcts_loop()
{
#ifdef SELFPLAY_COM	// YSS同士のシリアル対戦の場合、すぐに終わる。
	DEBUG_PRT("SELFPLAY_COM Err\n");
#endif
    { extern int fNowPlaying; fNowPlaying = 0; }	// 定跡無視
	int save_games = 0;
	int draw = 0, swin = 0;
	for (;;) {
		int loop;
		const int max_loop = 400;
		PS->hirate_ban_init(0);
		PRT_OFF();
		for (loop=0;loop<max_loop;loop++) {
			int k = PS->think_kifuset();
			if ( k == -TUMI ) break;	// 投了の時はループを抜ける。
		}
		PRT_ON();
		
		++save_games;
		
		char sr[TMP_BUF_LEN];
		sprintf(sr,"%%TORYO");
		if ( loop==max_loop ) sprintf(sr,"%%HIKIWAKE");

		int year,month,day,week,hour,minute,second;
		get_localtime(&year, &month, &day, &week, &hour, &minute, &second);
		char st[TMP_BUF_LEN];
		sprintf(st,"$START_TIME:%4d/%02d/%02d %02d:%02d:%02d",year,month,day,hour,minute,second);

		PS->SaveCSA(sr, st);
		char fs[TMP_BUF_LEN];
		sprintf(fs,"%04d%02d%02d_%02d%02d%02d_%05d.csa",year,month,day,hour,minute,second,save_games);
		FILE *fp = fopen(fs, "wb");
		if ( !fp ) DEBUG_PRT("");
		fprintf(fp,"%s",KifBuf);
		fclose(fp);
		
		fp = fopen("abc.txt","a");
		if ( fp == NULL ) { DEBUG_PRT("fail fopen\n"); }
		if ( loop==max_loop ) draw++;
		if ( loop!=max_loop && (loop&1) ) swin++;
		fprintf(fp,"%5d,uct_loop=%4d, %5d/%5d/%5d(%.4f), %3d:%s\n",save_games,UCT_LOOP_FIX,swin,draw,save_games,(double)(swin+draw/2)/save_games,loop,sr);
		fclose(fp);
		PRT("%5d,uct_loop=%4d, %5d/%5d/%5d(%.4f), %3d:%s\n",save_games,UCT_LOOP_FIX,swin,draw,save_games,(double)(swin+draw/2)/save_games,loop,sr);

		if ( save_games >= 1 ) break;
	}
}
#endif

/*
50万棋譜から1万棋譜をランダムに取り出して、プールに入れて
棋譜を学習用データに変換。

361
 (28 + 14 + 3) * (T_STEP)
 45 * 8 + 1 = 361
361* 9*9 = 29241
1個のデータにつき 29241 必要
50万棋譜、平均100手として 500000 * 100 * 29241 = 1462050000000
= 1394319 MB = 1361 GB / 8 = 170GB

35783413189  ... Train LevelDBのサイズ, = 34125 MB = 33GB, snappyとやらで圧縮されてるらしい。
棋譜をそのまま持って、その都度変換する。時間かかりそうだけど。
50万棋譜なら軽い。平均100手として、1手2byte。500000*100*2=100000000 = 95MB
1棋譜、400手まで。ハッシュ値、日付も。


archive/ の下に10000棋譜単位で圧縮されて入る。1個の棋譜は '/' で区切られて10000個続く
arch000000000000.csa.xz
arch000000000100.csa.xz

pool/ の下に 圧縮されて入っている。棋譜は1個づつ。
no000000000000.csa.xz
no000000000001.csa.xz

棋譜だけでなく、1手ごとにRootの選択回数が入る。(手,選択回数)、最大593手。
棋譜番号も必要か。あれば便利。固定サイズだと使いにくいか。可変DB
1手につき30手の候補。平均120手として
1棋譜 120 * 31 = 3720手。1手に2byte、探索回数に2byte、3720*4 = 14880
50万棋譜だと 14880 * 500000 = 7440000000 byte = 7095 MB = 7GB
u8700(32GB), i7(14GB), メモリに載ることは乗るか。i7だと厳しい。


*/


// from tcp/src/common
#include "common/err.hpp"
#include "common/iobase.hpp"
#include "common/xzi.hpp"
#include <iostream>
#include <memory>
#include <set>
#include <cstdint>
using std::ifstream;
using std::ios;
using std::unique_ptr;
using namespace Err;
static size_t read_xz_if_exist(const char *fname, unique_ptr<char []> &ptr) noexcept {
  ifstream ifs(fname, ios::binary);
  if (!ifs) {
    ptr.reset(nullptr);
    return 0;
  }

  DevNul devnul;
  XZDecode<ifstream, DevNul> xzd_nul;
  if (!xzd_nul.decode(&ifs, &devnul, SIZE_MAX))
    die(ERR_INT("XZDecode::decode()"));
  size_t size = xzd_nul.get_len_out();

  ptr.reset(new char [size]);
  ifs.clear();
  ifs.seekg(0, ios::beg);
  PtrLen<char> pl_out(ptr.get(), 0);
  XZDecode<ifstream, PtrLen<char>> xzd;
  if (!xzd.decode(&ifs, &pl_out, size)) die(ERR_INT("XZDecode::decode()"));
  assert(pl_out.len == size);
  ifs.close();

  return size;
}

int tmp_main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " finename" << std::endl;
    return 1;
  }

  unique_ptr<char []> ptr;
  size_t size = read_xz_if_exist(argv[1], ptr);

  if (!ptr) {
    std::cout << argv[1] << " doesn't exist" << std::endl;
  } else {
	std::cout << size << std::endl;
    char *p = ptr.get();
    size_t i;
    for (i=0; i<size; i++) printf("%c",*(p+i));
//    std::cout.write(ptr.get(), size);
  }
  return 0;
}





vector< vector<int> > v_db;

void v_db_test()
{
	int i;
	for (i=0;i<10;i++) {
		vector <int> v(rand()+100);
		v_db.push_back(v);
	}
	for (i=0;i<10;i++) {
		PRT("sizeof(%2d)=%d\n",i,v_db[i].size());
	}
	PRT("sizeof()=%d,cap=%d\n",v_db.size(),v_db.capacity());
//	vector<vector<int>>().swap(v_db); 
	for (i=0;i<(int)v_db.size();i++) {
		v_db[i].clear();
		std::vector<int>().swap(v_db[i]);	// memory free hack for vector. 
		PRT("sizeof(%2d)=%d,cap=%d\n",i,v_db[i].size(),v_db[i].capacity());
	}
	v_db.clear();
	PRT("sizeof()=%d,cap=%d\n",v_db.size(),v_db.capacity());
	PRT("sizeof()=%d\n",sizeof(v_db));
}

void free_zero_db_struct(ZERO_DB *p)
{
	p->hash = 0;
	p->date = 0;
	p->weight_n = 0;
	p->index = 0;
	p->result = 0;
	p->result_type = 0;
	p->moves = 0;
	p->handicap = 0;
	std::vector<unsigned short>().swap(p->v_kif);			// memory free hack for vector. 
	std::vector<unsigned short>().swap(p->v_playouts_sum);
	vector< vector<unsigned int> >().swap(p->vv_move_visit); 
	std::vector<unsigned short>().swap(p->v_score_x10k);
#if ( GCT_SELF==1)
	std::vector<unsigned char>().swap(p->v_init_pos);
#endif

}

//const int ZERO_DB_SIZE = 267000000;	// AI_book2
//const int ZERO_DB_SIZE = 20000000;	// gct001-075
//const int ZERO_DB_SIZE = 1000000;	// 100000,  500000
//const int ZERO_DB_SIZE = 2190000;	// 100000,  500000
const int ZERO_DB_SIZE = 500000;	// 100000,  500000
const int MAX_ZERO_MOVES = 513;	// 512手目を後手が指して詰んでなければ。513手目を先手が指せば無条件で引き分け。
ZERO_DB zdb_one;

ZERO_DB *zdb;
//ZERO_DB zdb[ZERO_DB_SIZE];
int *pZDBsum = NULL;
unsigned char *pZDBmove;
unsigned char *pZDBmaxmove;
unsigned short *pZDBplayouts_sum;
unsigned short *pZDBscore_x10k;
const int ZDB_POS_MAX = ZERO_DB_SIZE * 128;	// 128 = average moves. 64 = gct001-075
//const int ZDB_POS_MAX = ZERO_DB_SIZE * 1;	// AI book2

int zdb_count = 0;
int zdb_count_start = 61300000; //59020000;//58410000;//51000000;//1000000; //53920000; 52320000;48000000;1000000;48300000;18000000; 10000000;11600000; 10300000; 9500000;8500000; 7400000; //5220000; //3200000; //2100000; //390000;//130000;//460000;//29700000; //18200000;//23400000; //20300000; //18800000; //16400000;	//10300000; //5200000;	// 400万棋譜から読み込む場合は4000000
uint64_t zero_kif_pos_num = 0;
int zero_kif_games = 0;
int zero_pos_over250;
const int MINI_BATCH = 128;	// aoba_zero.prototxt の cross_entroy_scale も同時に変更すること！layerのnameも要変更
//const int MINI_BATCH = 32;	// aoba_zero.prototxt の cross_entroy_scale も同時に変更すること！layerのnameも要変更
const int ONE_SIZE = DCNN_CHANNELS*B_SIZE*B_SIZE;	// 362*9*9; *4= 117288 *64 = 7506432,  7MBにもなる mini_batch=64
int nGCT_files;	// 1つの selfplay_gct-00*.csa に入ってる棋譜数
int gct_csa = 1;	// ファイル番号
int sum_gct_loads = 0; // 1ファイルの全棋譜を読み込んだ後に加算される

const int fReplayLearning = 1;	// すでに作られた棋譜からWindowをずらせて学習させる
const int fWwwSample = 0;		// fReplayLearning も同時に1


// Sum-Tree用
// Prioritized Experience Replayのsum-treeの実装
// https://tadaoyamaoka.hatenablog.com/entry/2019/08/18/154610
// https://github.com/jaromiru/AI-blog/blob/master/SumTree.py

const bool fSumTree = true;

const int LEAVES = 41352794;	// 棋譜の数ではない。局面数。ZERO_DB_SIZE;
const int SUMTREE_SIZE = 2*LEAVES - 1;
int sumtree[SUMTREE_SIZE];
int add_n = 0;
int capacity = LEAVES;
std::mt19937 get_mt_rand;

void stree_propagate(int idx, int change) {
    int parent = (idx - 1) / 2;
    sumtree[parent] += change;
    if ( sumtree[parent] > INT_MAX/2 ) DEBUG_PRT("sumtree:too big =%d\n",sumtree[parent]);
    if ( parent != 0 ) stree_propagate(parent, change);
}
int stree_retrieve(int idx, int s) {
    int left = 2 * idx + 1;
    int right = left + 1;
//	if ( left >= len(st) ) return idx;
    if ( left >= SUMTREE_SIZE ) return idx;
    if ( s <= sumtree[left] ) {
        return stree_retrieve(left, s);
    } else {
        return stree_retrieve(right, s - sumtree[left]);
	}
}
void stree_update(int idx, int p) {
    int change = p - sumtree[idx];
	if ( p <= 0 ) DEBUG_PRT("idx=%d,change=%d,p=%d\n",idx,change,p);
    sumtree[idx] = p;
    stree_propagate(idx, change);
}
int stree_total() {
	return sumtree[0];
}
void stree_add(int p) {
	if ( p <= 0 ) DEBUG_PRT("");
    int idx = add_n + capacity - 1;
    stree_update(idx, p);

    add_n += 1;
	if ( add_n >= capacity ) add_n = 0;
}
// 要素が10個なら idx = 9...18 となる。idx - (LEAVES-1) が求めたい値
int stree_get(int s) {
	if ( s <= 0 ) DEBUG_PRT("");
    int idx = stree_retrieve(0, s);
    return idx; // sumtree[idx]
}
int stree_get_i(int s) {	// 0 <= i <= LEAVES-1
    int i = stree_get(s) - (LEAVES-1);
    if ( i < 0 || i >= LEAVES ) DEBUG_PRT("");
    return i;
}
void init_stree() {
	std::random_device rd;
	get_mt_rand.seed(rd());
	for (int i=0; i<LEAVES; i++) stree_add(1);	// 全部1で初期化する。実際の値はreplace()を使って更新
}
void stree_replace(int n, int p) {
	if ( n < 0 || n >= LEAVES ) DEBUG_PRT("n=%d,p=%d",n,p);
	int idx = n + (LEAVES-1);
	if ( idx >= SUMTREE_SIZE ) DEBUG_PRT("");
	stree_update(idx, p);
}


#if 0
void stree_test()
{
	std::random_device rd;
	std::mt19937 get_mt_rand(rd());
//	get_mt_rand.seed(time(NULL));

	PRT("total=%d\n",stree_total());

//	int P[LEAVES] = { 6, 48, 31, 26, 49, 43, 93, 74, 79, 13 };
//	int P[LEAVES] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	int P[LEAVES] = { 2, 1, 1, 1, 1, 3, 1, 1, 1, 2 };
	int i;
	for (i=0; i<LEAVES; i++) stree_add(P[i]);
//	stree_update((LEAVES-1)+4, 8);	// 1個のデータのみを更新テスト
	int t = stree_total();
	PRT("total=%d,add_n=%d\n",t,add_n);
	std::uniform_int_distribution<> dist(0, t-1);	// 0からt-1までの一様乱数
	int p_sum[SUMTREE_SIZE] = {0};
	for (i=0;i<10000;i++) {
//	    int s = (get_mt_rand() % t)+1;	// +1 が必要。% はやや不正確
		int s = dist(get_mt_rand) + 1;	// +1 が必要。一様乱数
		int idx = stree_get(s);
		p_sum[idx]++;
    }
	for (i=0;i<SUMTREE_SIZE;i++) {
		PRT("%2d:%3d:%5d,%6.3f(%6.3f)\n",i,sumtree[i],p_sum[i], (float)p_sum[i]/10000,(float)sumtree[i]/t);
	}
}
#endif



void init_zero_kif_db()
{
	if ( pZDBsum == NULL ) pZDBsum = (int*)malloc( ZERO_DB_SIZE * sizeof(int) );
	if ( pZDBsum == NULL ) { DEBUG_PRT("Fail malloc\n"); }
	pZDBmove         = (unsigned char*) malloc( ZDB_POS_MAX * sizeof(char) );
	pZDBmaxmove      = (unsigned char*) malloc( ZDB_POS_MAX * sizeof(char) );
	pZDBplayouts_sum = (unsigned short*)malloc( ZDB_POS_MAX * sizeof(short) );
	pZDBscore_x10k   = (unsigned short*)malloc( ZDB_POS_MAX * sizeof(short) );
	zdb              = (ZERO_DB*)       malloc(ZERO_DB_SIZE * sizeof(ZERO_DB));
	if ( zdb == NULL ) DEBUG_PRT("");

	memset(pZDBsum,0,ZERO_DB_SIZE * sizeof(int));
	int i;
	for (i=0;i<ZERO_DB_SIZE;i++) {
		free_zero_db_struct(&zdb[i]);
	}
	PRT("pDB=%7d,sizeof(ZERO_DB)=%d\n",ZERO_DB_SIZE,sizeof(ZERO_DB));
}

// 最後に調べたarchiveのみキャッシュする
#if ( GCT_SELF==1)
//const int ONE_FILE_KIF_NUM = 350000;
const int ONE_FILE_KIF_NUM = 9375100;
#else
const int ONE_FILE_KIF_NUM = 10000;
#endif
static char recent_arch_file[TMP_BUF_LEN];
static uint64_t recent_arch_table[ONE_FILE_KIF_NUM];	// n番目の棋譜の開始位置
static char *p_recent_arch = NULL;
static uint64_t recent_arch_size;

const int USE_XZ_NONE      = 0;
const int USE_XZ_POOL_ONLY = 1;
const int USE_XZ_BOTH      = 2;

int USE_XZ = USE_XZ_BOTH;	//  1...poolのみ xz で。2...poolもarchiveも xz で

// archiveから棋譜番号=n の棋譜を取り出す。KifBuf[] に入る。速度無視。fpでは100番目以降が遅すぎて無理。
int find_kif_from_archive(int search_n)
{
//search_n = search_n % 190000;
//	char dir_arch[] = "./archive/";
//	char dir_arch[] = "/home/aobaz/kifu/archive/";
//	char dir_arch[] = "/home/yss/tcp_backup/archive20190304/unpack/";
//	char dir_arch[] = "/home/yss/tcp_backup/archive20190325/unpack/";
//	char dir_arch[] = "/home/yss/tcp_backup/archive20190421/unpack/";
//	char dir_arch[] = "/home/yss/tcp_backup/archive20201207/";
//	char dir_arch[] = "/home/yss/prg/komaochi/archive/";
//	char dir_arch[] = "/home/yss/koma_syn/archive/";
	char dir_arch[] = "/home/yss/tcp_backup/archive20201207/";
	int arch_n = (search_n/10000) * 10000;	// 20001 -> 20000

	if ( GCT_SELF ) {
		USE_XZ = USE_XZ_NONE;
		if ( search_n - sum_gct_loads == nGCT_files && nGCT_files != 0 ) {
			sum_gct_loads = zdb_count;
			gct_csa++;
			nGCT_files = 0;
			PRT("next gct_csa=%d,zdb_count=%d\n",gct_csa,zdb_count);
		}
		if ( gct_csa == 24*3+1 ) { PRT("stop memory.\n"); return 0; }
//		if ( gct_csa >= 2 ) { PRT("stop memory.\n"); return 0; }
	}

	char filename[TMP_BUF_LEN];
	if ( USE_XZ==USE_XZ_BOTH ) {
		sprintf(filename,"%sarch%012d.csa.xz",dir_arch,arch_n);
	} else {
		sprintf(filename,"%sarch%012d.csa",dir_arch,arch_n);
		if ( GCT_SELF ) {
//			sprintf(filename,"/home/yss/prg/learn/selfplay_gct-%03d.csa",gct_csa);
//			sprintf(filename,"/home/yss/shogi/dlshogi_hcpe/20210506/selfplay_gct-%03d.csa",gct_csa);
			if ( gct_csa < 25 ) {
				sprintf(filename,"/home/yss/shogi/dlshogi_hcpe/dlshogi_with_gct-%03d.csa",gct_csa);
			} else if ( gct_csa < 49 ) {
				sprintf(filename,"/home/yss/shogi/dlshogi_hcpe/suisho3kai-%03d.csa",gct_csa - 24);
			} else {
				sprintf(filename,"/home/yss/shogi/dlshogi_hcpe/floodgate_2019-2021_r3500-%03d.csa",gct_csa - 48);
			}
		}
	}
//	PRT("try open %s\n",filename);
	if ( strcmp(filename, recent_arch_file) != 0 ) {
		int ct1 = get_clock();
		size_t size = 0;
		if ( USE_XZ==USE_XZ_BOTH ) {
			unique_ptr<char []> ptr;
			size = read_xz_if_exist(filename, ptr);
			if (!ptr) {
				PRT("not found. %s\n",filename);
				return 0;
			}
			PRT("%s, size=%lu\n",filename,size);
			if ( p_recent_arch ) free(p_recent_arch);
			p_recent_arch = (char *)malloc(size);
			if ( p_recent_arch==NULL ) { PRT("fail malloc size=%d\n",size); exit(0); }
//			size_t ret = fread(p_recent_arch, 1, size, fp_arch);
			memcpy(p_recent_arch, ptr.get(), size);
		} else {
			FILE *fp_arch = fopen(filename,"r");
			if ( fp_arch==NULL ) {
				PRT("not found. %s\n",filename);
				return 0;
			}
			struct stat st;
			stat(filename, &st);
			size = st.st_size;

			PRT("%s, size=%lu\n",filename,size);
			if ( p_recent_arch ) free(p_recent_arch);
			p_recent_arch = (char *)malloc(size);
			if ( p_recent_arch==NULL ) { PRT("fail malloc size=%d\n",size); exit(0); }
			size_t ret = fread(p_recent_arch, 1, size, fp_arch);
			if ( ret != size ) { PRT("ret=%d Err\n",ret); exit(0); }
			fclose(fp_arch);
		}

		uint64_t i;
		recent_arch_table[0] = 0;
		int g=0,lines=0;
		for (i=0;i<size;i++) {
			char c = p_recent_arch[i];
			if ( c!='\n' ) continue;
			lines++;
			if ( i<size-2 && p_recent_arch[i+1]=='/' ) {
				if ( g >= ONE_FILE_KIF_NUM ) DEBUG_PRT("");
				g++;
				recent_arch_table[g] = i+2;
			}
		}
		strcpy(recent_arch_file, filename);
		recent_arch_size = size;
		nGCT_files = g;
		if ( GCT_SELF == 0 && g != ONE_FILE_KIF_NUM - 1 ) PRT("Err g=%d\n",g);
		PRT("lines=%d,g=%d,%.3f sec\n",lines,g,get_spend_time(ct1));	// 100で36秒。1回0.36秒
//		for (i=0;i<10;i++) PRT("%d:%d\n",i,recent_arch_table[i]); exit(0);
	}

	int n = search_n - arch_n;
	if ( GCT_SELF==0 && (n < 0 || n >= 10000) ) DEBUG_PRT("err\n");
	if ( GCT_SELF ) n = search_n - sum_gct_loads;

	KifBuf[0] = 0;
	nKifBufNum = 0;
	nKifBufSize = 0;

	uint64_t start_i = recent_arch_table[n  ];
	uint64_t next_i  = recent_arch_table[n+1] - 1;	// '/' を入れない
	if ( GCT_SELF==0 && n >= 9999 ) next_i = recent_arch_size;
	uint64_t one_size = next_i - start_i;

	if ( one_size > KIF_BUF_MAX-256 || one_size <= 0 ) DEBUG_PRT("Err one csa kif is too big, n=%d,%lu\n",n,one_size);
	strncpy(KifBuf, p_recent_arch+start_i, one_size);
	KifBuf[one_size] = 0;
	nKifBufSize = strlen(KifBuf);
	if ( nKifBufSize == 0 ) DEBUG_PRT("Err size=0, search_n=%d\n",search_n);
//	for (int i=0;KifBuf[i]!=0;i++) PRT("%c",KifBuf[i]); PRT("\n");
//	PRT("size=%d,n=%d,one_size=%d,search_n=%d,nGCT_files=%d\n",nKifBufSize,n,one_size,search_n,nGCT_files);
	return 1;
}

int find_kif_from_pool(int search_n)
{
	char dir_pool[] = "/home/yss/koma_syn/pool";
	char filename[TMP_BUF_LEN];
	if ( USE_XZ ) {
		sprintf(filename,"%s/no%012d.csa.xz",dir_pool,search_n);
	} else {
		sprintf(filename,"%s/no%012d.csa",dir_pool,search_n);	// no000000047916.csa
	}

	KifBuf[0] = 0;
	nKifBufNum = 0;
	nKifBufSize = 0;

	if ( USE_XZ ) {
		unique_ptr<char []> ptr;
		size_t size = read_xz_if_exist(filename, ptr);
		if (!ptr) {
//			PRT("not found. %s\n",filename);
			return 0;
		}
		if ( size > KIF_BUF_MAX-256 ) {
			DEBUG_PRT("Err one csa kif is too big\n");	
		} 
		memcpy(KifBuf, ptr.get(), size);
		KifBuf[size] = 0;
		nKifBufSize = strlen(KifBuf);
	} else {
//		PRT("try open %s\n",filename);
		FILE *fp = fopen(filename,"r");
		if ( fp==NULL ) {
//			PRT("not found. %s\n",filename);
			return 0;
		}

		for (;;) {
			const int size = TMP_BUF_LEN * 32;
			char str[size];
			if ( fgets( str, size-1, fp ) == NULL ) {
				break;
			}
//			str[80]=0; PRT("%s\n",str);
//			if ( str[0] == '/' ) break;
			if ( strlen(KifBuf) + strlen(str) > KIF_BUF_MAX-256 ) {
				DEBUG_PRT("Err one csa kif is too big\n");	
			}
			strcat(KifBuf,str);
			nKifBufSize = strlen(KifBuf);
		}
		fclose(fp);
	}
	if ( nKifBufSize == 0 ) { PRT("Err size=0, search_n=%d\n",search_n); exit(0); }
	return 1;
}

int get_guess_resign_moves(int moves)
{
	if ( moves == 0 ) return 0;
//	double y = 3.187 * exp(0.201*((double)moves/20.0 - 0));
	double y = 28.76 * log(moves / 20.0) - 37.35;	// 80手で2手、100手で8手、150手で20手、300手で40手削る
	if ( y < 0 ) y = 0;
	return (int)y;
}

void shogi::add_one_kif_to_db()
{
	ZERO_DB *pdb = &zdb[zdb_count % ZERO_DB_SIZE];	// 古いのは上書き
	ZERO_DB *p   = &zdb_one;
	free_zero_db_struct(pdb);
#if (GCT_SELF==1)
        if ( zdb_count >= ZERO_DB_SIZE ) DEBUG_PRT("ZERO_DB_SIZE is not enoght!\n");
        if ( p->moves <= 0 ) DEBUG_PRT("zdb_count=%d,p->moves=%d",zdb_count,p->moves);
#endif
	if ( 0 ) {	// 投了を実装した場合のテスト。投了手数を予想して最後の数手を削る
		int t = get_guess_resign_moves(p->moves);
		int t2 = t & 0xfffe;	// 偶数に
		if ( p->result == ZD_DRAW ) t2 = 0;
		if ( (rand_m521() % 10)==0 ) t2 = 0;	// 10%は投了なし
		p->moves -= t2;
		if ( p->moves <= 0 ) DEBUG_PRT("Err get_guess_resign_moves\n");
		p->v_kif.resize(p->moves);
		p->v_playouts_sum.resize(p->moves);
		p->vv_move_visit.resize(p->moves);
	}
//	pdb->date           = st.st_ctime;
	pdb->hash           = get_hashcode64();
	pdb->index          = zdb_count;
	pdb->weight_n       = p->weight_n;
	pdb->moves          = p->moves;
	pdb->result         = p->result;
	pdb->result_type    = p->result_type;
	pdb->handicap       = p->handicap;
//	copy(p->v_kif.begin(), p->v_kif.end(), back_inserter(pdb->v_kif));
//	copy(p->v_playouts_sum.begin(), p->v_playouts_sum.end(), back_inserter(pdb->v_playouts_sum));
#if ( GCT_SELF==1)
	pdb->v_init_pos     = p->v_init_pos;
#endif
	pdb->v_kif          = p->v_kif;
	pdb->v_playouts_sum = p->v_playouts_sum;
	pdb->vv_move_visit  = p->vv_move_visit;
	pdb->v_score_x10k   = p->v_score_x10k;
//	PRT("%6d:index=%d,res=%d,%3d:%08x %08x %08x\n",zdb_count,pdb->index,p->result,p->moves,hash_code1,hash_code2,hash_motigoma);
	zdb_count++;
}

// 棋譜が存在するか。すれば同時に読み込む
int is_exist_kif_file(int search_n)
{
	if ( find_kif_from_pool(search_n) == 0 ) {
		if ( find_kif_from_archive(search_n) == 0 ) {
			return 0;
		}
	}
	return 1;
}

/*
前回の調整から
過去1000棋譜の勝率が0.60を超えてたら、調整(+70の半分、+35を引く)
過去2000棋譜の勝率が0.58を超えてたら、調整
過去4000棋譜の勝率が0.56を超えてたら、調整
過去8000棋譜の勝率で無条件で        、調整

保存するのは
最後に調整した棋譜番号、現在の調整レート
サーバに渡すのは現在の調整レート、7種類のみ。
*/
int nHandicapLastID[HANDICAP_TYPE];
int nHandicapRate[HANDICAP_TYPE];
const char HANDICAP_ID_FILE[] = "handicap_rate.txt";
const char HANDICAP_SYN[] = "/home/yss/koma_syn/handicap/handicap.txt";

void load_handicap_rate()
{
	static bool bDone = false;
	if ( bDone ) return;
	bDone = true;
	FILE *fp = fopen(HANDICAP_ID_FILE,"r");
	if ( fp==NULL ) DEBUG_PRT("fail open.\n");
	char str[TMP_BUF_LEN];
	if ( fgets( str, TMP_BUF_LEN, fp ) == NULL ) DEBUG_PRT("");
	int *p = nHandicapLastID;
	int ret = sscanf(str,"%d %d %d %d %d %d %d",&p[0],&p[1],&p[2],&p[3],&p[4],&p[5],&p[6]);
	if ( ret != HANDICAP_TYPE ) DEBUG_PRT("ret=%d\n",ret);
	if ( fgets( str, TMP_BUF_LEN, fp ) == NULL ) DEBUG_PRT("");
	int *q = nHandicapRate;
	ret = sscanf(str,"%d %d %d %d %d %d %d",&q[0],&q[1],&q[2],&q[3],&q[4],&q[5],&q[6]);
	if ( ret != HANDICAP_TYPE ) DEBUG_PRT("ret=%d\n",ret);
	fclose(fp);
	int i;
	PRT("nHandicapLastID=");
	for (i=0;i<HANDICAP_TYPE;i++) PRT("%d ",nHandicapLastID[i]);
	PRT("\nnHandicapRate  =");
	for (i=0;i<HANDICAP_TYPE;i++) PRT("%d ",nHandicapRate[i]);
	PRT("\n");
}

void save_handicap_rate()
{
	FILE *fp = fopen(HANDICAP_ID_FILE,"w");
	if ( fp==NULL ) DEBUG_PRT("fail open.\n");
	char str[TMP_BUF_LEN] = { 0 };
	int i;
	for (i=0;i<HANDICAP_TYPE;i++) {
		if ( i>0 ) strcat(str," ");
		char buf[TMP_BUF_LEN];
		sprintf(buf,"%d",nHandicapLastID[i]);
		strcat(str,buf);
	}
	strcat(str,"\n");
	fprintf(fp,"%s",str);

	str[0]  = 0;
	for (i=0;i<HANDICAP_TYPE;i++) {
		if ( i>0 ) strcat(str," ");
		char buf[TMP_BUF_LEN];
		sprintf(buf,"%d",nHandicapRate[i]);
		strcat(str,buf);
	}
	strcat(str,"\n");
	fprintf(fp,"%s",str);
	fclose(fp);

	fp = fopen(HANDICAP_SYN,"w");
	if ( fp==NULL ) {
		PRT("fail open %s\n",HANDICAP_SYN);
	} else {
		fprintf(fp,"%s",str);
		fclose(fp);
	}

	fp = fopen("handicap_history.txt","a");
	if ( fp==NULL ) DEBUG_PRT("fail open.\n");
	for (i=0;i<HANDICAP_TYPE;i++) {
		if ( i>0 ) fprintf(fp,",");
		fprintf(fp,"%9d(%4d)",nHandicapLastID[i],nHandicapRate[i]);
	}
	fprintf(fp,"\n");
	fclose(fp);
}

const char AVERAGE_WINRATE_SYN[] = "/home/yss/tcp_backup/handicap/average_winrate.txt";

void save_average_winrate(float ave_wr) {
	if ( ave_wr <= 0 || ave_wr >= 1.0 ) DEBUG_PRT("");
	FILE *fp = fopen(AVERAGE_WINRATE_SYN,"w");
	if ( fp==NULL ) DEBUG_PRT("fail open.\n");
	fprintf(fp,"%d\n",(int)(ave_wr*1000.0));
	fclose(fp);

	fp = fopen("average_winrate_history.txt","a");
	if ( fp==NULL ) DEBUG_PRT("fail open.\n");
	fprintf(fp,"%9d(%.3f)\n",zdb_count,ave_wr);
	fclose(fp);
}

void count_recent_handicap_result(int h, int width, int result[])
{
	result[0] = result[1] = result[2] = 0;
	int w = width * HANDICAP_TYPE;
	int loop = zdb_count;
	if ( loop > ZERO_DB_SIZE ) loop = ZERO_DB_SIZE;
	for (int i=0;i<loop;i++) {
		ZERO_DB *p = &zdb[i];
		if ( h != p->handicap ) continue;
		if ( p->index > zdb_count - w ) {
			result[p->result]++;
		}
	}
}

void update_pZDBsum()
{
	const int H = HANDICAP_TYPE;
	int res_total_sum[H][4]    = { 0 };	// [3] は合計で利用
	int res_recent_sum[H][4]   = { 0 };
	uint64 moves_total_sum[H]  = { 0 };
	int    moves_recent_sum[H] = { 0 };
	uint64 mv_total_sum[H]  = { 0 };
	int    mv_recent_sum[H] = { 0 };
	int    mv_total_inc[H]  = { 0 };
	int    mv_recent_inc[H] = { 0 };
	int res_kind[H][4]      = { 0 };
	int res_type[H][RT_MAX] = { 0 };
	int kachi[H][4]         = { 0 };
	int result_hadicap_sum[H][3] = { 0 };
	int games_sum[H] = {0};
	const int G1000 = 1000*H;
	const int WR_OK_GAMES = 8000;	// 直近のこの対局数の勝率でレートを変動

	if ( GCT_SELF==0 ) load_handicap_rate();
	const int MAX_GAMES = (WR_OK_GAMES*12/10)*H;
	int i;
	for (i=0;i<H;i++) {
		if ( zdb_count - nHandicapLastID[i] > MAX_GAMES ) nHandicapLastID[i] = zdb_count - MAX_GAMES;
	}

	static int s_sum[101];
	zero_kif_pos_num = 0;
	zero_kif_games   = 0;
	zero_pos_over250 = 0;
	int loop = zdb_count;
	if ( loop > ZERO_DB_SIZE ) loop = ZERO_DB_SIZE;
	for (i=0;i<loop;i++) {
		ZERO_DB *p = &zdb[i];
		int h = p->handicap;
		if ( h < 0 || h >= H ) DEBUG_PRT("");
		if ( p->result_type < 0 || p->result_type >= RT_MAX ) DEBUG_PRT("");
		if ( p->moves == 0 ) { PRT("Err. p->moves=0\n"); exit(0); }
		if ( p->result < 0 || p->result > 2 ) DEBUG_PRT("");

		if ( zero_kif_pos_num + p->moves >= ZDB_POS_MAX ) DEBUG_PRT("ZDB_POS_MAX! %d/%d,%lu,%d",i,loop,zero_kif_pos_num,p->moves);
		for (int j=0;j<p->moves;j++) {
			int n = zero_kif_pos_num + j;
			int m0 = j;
			if ( m0 > 255 ) m0 = 255;
			int m1 = p->moves;
			if ( m1 > 255 ) m1 = 255;
			pZDBmove[n]    = m0;
			pZDBmaxmove[n] = m1;
			if ( GCT_SELF==0 ) pZDBplayouts_sum[n] = p->v_playouts_sum[j];
			if ( GCT_SELF==1 ) pZDBplayouts_sum[n] = 0;
			pZDBscore_x10k[n]   = p->v_score_x10k[j];
			if ( m0 >= 250 ) zero_pos_over250++;

			if ( fSumTree ) {
//				int s = pZDBplayouts_sum[n];
				int s = pZDBscore_x10k[n];
				int k = 10;			// 20 20 20 20 10 10 20 20 20 20
				if ( s < 3900 || s > 6100 ) k = 20;
				if ( s < 3000 || s > 7000 ) k = 30;
//				if ( s < 2500 || s > 7500 ) k = 20;
//				if ( s <  100 || s >  990 ) k = 5;
				stree_replace(n, k);
//				s_sum[s/100]++;
				int prev_s = 0;
				if ( j > 0 ) prev_s = pZDBscore_x10k[n-1];
				if (j&1) s = 10000 - s;
				else prev_s = 10000 - prev_s;
				if ( j==0 ) prev_s = s;
				s_sum[abs(s - prev_s)/100]++;
			}
		}
//		PRT("%d/%d,%lu,",i,loop,zero_kif_pos_num);
		zero_kif_pos_num += p->moves;
		pZDBsum[i] = zero_kif_pos_num;
		zero_kif_games += (p->moves != 0);

		if ( p->index >= zdb_count - G1000) {
			res_recent_sum[h][p->result]++;
			moves_recent_sum[h] += p->moves;
		}
		if ( p->index > nHandicapLastID[h] ) {
			result_hadicap_sum[h][p->result]++;
		}

		games_sum[h]++;
		res_total_sum[h][p->result]++;
		moves_total_sum[h] += p->moves;

		if ( p->index >= zdb_count - G1000) {
			if ( p->result_type == RT_KACHI ) {
				res_kind[h][p->result]++;
				res_kind[h][3]++;
			}
	    	res_type[h][p->result_type]++;
		}
		if ( p->result_type == RT_KACHI ) {
			kachi[h][p->result]++;
		}

		int j;
		for (j=0;j<p->moves;j++) {
			int n = p->vv_move_visit[j].size();
			if ( p->index >= zdb_count - G1000) {
				mv_recent_sum[h] += n;
				mv_recent_inc[h]++;
			}
			mv_total_sum[h] += n;
			mv_total_inc[h]++;
		}
	}
	if ( loop > 0 ) for (i=0;i<H;i++) {
		if ( mv_recent_inc[i] == 0 ) mv_recent_inc[i] = 1;
		if ( mv_total_inc[i]  == 0 ) mv_total_inc[i]  = 1;
		int s = 0;
		for (int j=0;j<4;j++) s += res_recent_sum[i][j];
		if ( s==0 ) s = 1;
//		if ( games_sum[i] == 0 ) games_sum[i] = 1;
//		PRT("%7d:move_ave=%5.1f(%5.1f) mv_ave=%.1f(%.1f), recent D=%3d,S=%3d,G=%3d(%5.3f) total %3d,%3d,%3d(%5.3f), %d,%d,%d,%d,rt=%d,%d,%d,%d,%d,%d,%d\n",
		//      手合 対局数  平均手数(最近) 候補数(最近)                                   先宣 後宣(最近)    投 千 GI SI 中断
		if ( games_sum[i] ) PRT("%7d:%d:%4d,%5.1f(%5.1f) %.1f(%.1f)D=%3d,S=%3d,G=%3d(%5.3f)t=%4d,%5d,%5d(%5.3f)%4d,%4d(%3d,%3d)rt=%3d,%2d,%d,%d,%d\n",
			zdb_count,i,games_sum[i],(float)moves_total_sum[i]/games_sum[i], (float)moves_recent_sum[i]/s,
			(float)mv_total_sum[i]/mv_total_inc[i], (float)mv_recent_sum[i]/mv_recent_inc[i],
			res_recent_sum[i][0],res_recent_sum[i][1],res_recent_sum[i][2], (float)(res_recent_sum[i][1] + res_recent_sum[i][0]/2.0)/(res_recent_sum[i][0]+res_recent_sum[i][1]+res_recent_sum[i][2]),
 			res_total_sum[i][0],res_total_sum[i][1],res_total_sum[i][2], (float)(res_total_sum[i][1] + res_total_sum[i][0]/2.0)/(res_total_sum[i][0]+res_total_sum[i][1]+res_total_sum[i][2]),
 			kachi[i][1],kachi[i][2],res_kind[i][1],res_kind[i][2],
			res_type[i][1],res_type[i][3],res_type[i][4],res_type[i][5],res_type[i][6] );
	}

	bool fUpdate = false;
	(void)fUpdate;
	int hand_diff[H] = { 0 };
	for (i=0;i<H;i++) {
		hand_diff[i] = 0;
		bool fOK = false;
		double wr = 0;
		double sum = 0;

		int k_width[4] = { 1000,2000,4000,WR_OK_GAMES };
		for (int k=0; k<4; k++) {
			int w = k_width[k];
			if ( zdb_count - w*H < nHandicapLastID[i] ) continue;
			int h_res[3];
			count_recent_handicap_result(i, w, h_res);
			sum = 0;
			for (int j=0;j<3;j++) sum += h_res[j];
			if ( sum == 0 ) sum = 1;
			double wins = (double)h_res[1] + (double)h_res[0]/2.0f;
			wr = wins / sum;	// 先手(下手)勝率
			if ( k==0 && (wr > 0.60 || wr < 0.40) ) fOK = true;
			if ( k==1 && (wr > 0.58 || wr < 0.42) ) fOK = true;
			if ( k==2 && (wr > 0.56 || wr < 0.44) ) fOK = true;
			if ( k==3                             ) fOK = true;
			if ( fOK ) break;
		}

		if ( wr <= 0.01 ) wr = 0.01;
		if ( wr >= 0.99 ) wr = 0.99;
		double d = -400.0 * log10(1.0 / wr - 1.0);
		int d2 = (int)(d / 2.0f);	// 半分の値で動かす

//		PRT("%d:%.0f,%.3f(%d),",i,sum,wr,d2); if ( i==H-1 ) PRT("\n");
		if ( fOK == false ) continue;

		int prev = nHandicapRate[i];
		nHandicapRate[i] += d2;
		const int RMAX = 1400+1157 - 100;
		if ( nHandicapRate[i] <    0 ) nHandicapRate[i] = 0;
		if ( nHandicapRate[i] > RMAX ) nHandicapRate[i] = RMAX;
		hand_diff[i] = nHandicapRate[i] - prev;
		if ( hand_diff[i] ) {
			fUpdate = true;
			nHandicapLastID[i] = zdb_count;
		}
	}
//	if ( fUpdate ) save_handicap_rate();

//	float ave_winrate = (float)(res_total_sum[0][1] + res_total_sum[0][0]/2.0)/(res_total_sum[0][0]+res_total_sum[0][1]+res_total_sum[0][2]);
//	save_average_winrate(ave_winrate);

//	PRT("H:"); for (i=0;i<H;i++) { PRT("%8d %4d(%3d)",nHandicapLastID[i], nHandicapRate[i], hand_diff[i]); } PRT("\n");
	if ( fSumTree ) { for (i=0;i<101;i++) PRT("%3d:%7d(%.3f)\n",i,s_sum[i],(float)s_sum[i]/zero_kif_pos_num); }

}

void shogi::load_exist_all_kif()
{
	int ct1 = get_clock();
	fSkipLoadKifBuf = 1;	// 常にメモリから棋譜を読む
	zdb_count = zdb_count_start;
	int count = 0;
	for (;;) {
		if ( fReplayLearning && count == ZERO_DB_SIZE ) break;
		if ( is_exist_kif_file(zdb_count)==0 ) {
			PRT("read all exist kif\n");
			break;
		}
		char filename[] = "dummy.csa";
		if ( open_one_file(filename)==0 ) { PRT("open_one_file() Err\n"); exit(0); }
		if ( all_tesuu > MAX_ZERO_MOVES ) { PRT("Err MAX_ZERO_MOVES=%d,zdb_count=%d\n",all_tesuu,zdb_count); exit(0); } 

		add_one_kif_to_db();
		count++;
		if ( 0 && (zdb_count % 10000)==0 ) {
			update_pZDBsum();
			PRT("zdb_count=%d,games=%d,pos_num=%d, %.3f sec\n",zdb_count,zero_kif_games,zero_kif_pos_num,get_spend_time(ct1));
			for (int i=0;i<3;i++) PRT("%d:index=%d,moves=%d\n",i,zdb[i].index,zdb[i].moves);
		}
	}

	update_pZDBsum();

	PRT("zdb_count=%d(count=%d),games=%d,pos_num=%lu, %.3f sec\n",zdb_count,count,zero_kif_games,zero_kif_pos_num,get_spend_time(ct1));
	if ( zdb_count_start != 0 && count < ZERO_DB_SIZE ) { DEBUG_PRT("Err. replay buf is not filled.\n"); }
}

int shogi::wait_and_get_new_kif(int next_weight_n)
{
	int new_kif_n = zdb_count;
	int add_kif_sum = 0;
	for (;;) {
		// call rsync
		int sleep_sec = 1200;		// wait some sec
		FILE *fp = fopen("sleep.txt","r");
		if ( fp==NULL ) {
			PRT("fail open sleep.\n");
		} else {
			char str[TMP_BUF_LEN];
			if ( fgets( str, TMP_BUF_LEN, fp ) ) {
				sleep_sec = atoi(str);
			}
			if ( sleep_sec < 0 ) DEBUG_PRT("");
			fclose(fp);
		}

		char str[256];
		sprintf(str,"sleep %d",sleep_sec);
		PRT("%s\n",str);
//		int ret = system(str);	// fail all system() 20190817?
		sleep(sleep_sec);
		int ret = system("/home/yss/koma_syn/rsync_one.sh");
		PRT("ret=%d\n",ret);
		// 新規に追加されたファイルを調べる
		for (;;) {
			if ( is_exist_kif_file(new_kif_n)==0 ) {
				break;
			}
			char filename[] = "dummy.csa";
			if ( open_one_file(filename)==0 ) { PRT("open_one_file() Err\n"); exit(0); }
			if ( all_tesuu > MAX_ZERO_MOVES ) { PRT("Err MAX_ZERO_MOVES=%d,new_kif_n=%d\n",all_tesuu,new_kif_n); exit(0); } 

			add_one_kif_to_db();
			add_kif_sum++;
			new_kif_n++;
		}
		update_pZDBsum();
		PRT("add_kif_sum=%d,next_weight_n=%d,",add_kif_sum,next_weight_n);
		PRT("zdb_count=%d,games=%d,pos_num=%d\n",zdb_count,zero_kif_games,zero_kif_pos_num);
		if ( add_kif_sum ) break;
	}
	return add_kif_sum;
}

int shogi::add_a_little_from_archive()
{
	if ( fSumTree ) return 0;

    int new_kif_n = zdb_count;
    int add_kif_sum = 0;
    int add = 2000;
    int i;
	for (i=0;i<add;i++) {
	    if ( is_exist_kif_file(new_kif_n)==0 ) {
			return -1;
        }
        char filename[] = "dummy.csa";
        if ( open_one_file(filename)==0 ) { PRT("open_one_file() Err\n"); exit(0); }
        if ( all_tesuu > MAX_ZERO_MOVES ) { PRT("Err MAX_ZERO_MOVES=%d,new_kif_n=%d\n",all_tesuu,new_kif_n); exit(0); }
        add_one_kif_to_db();
        add_kif_sum++;
        new_kif_n++;
    }
    update_pZDBsum();
    PRT("add_kif_sum=%d,",add_kif_sum);
    PRT("zdb_count=%d,games=%d,pos_num=%d(over250=%d)\n",zdb_count,zero_kif_games,zero_kif_pos_num,zero_pos_over250);
    return add_kif_sum;
}

int shogi::make_www_samples()
{
    int new_kif_n = zdb_count;
    int max_w = -1;
	const int WWW_KIF_MAX = 1000;
	int www_kif_num = 0;
	int www_kif[WWW_KIF_MAX];
	for (;;) {
	    if ( is_exist_kif_file(new_kif_n)==0 ) {
			break;
        }
        char filename[] = "dummy.csa";
        if ( open_one_file(filename)==0 ) { PRT("open_one_file() Err\n"); exit(0); }
        if ( all_tesuu > MAX_ZERO_MOVES ) { PRT("Err MAX_ZERO_MOVES=%d,new_kif_n=%d\n",all_tesuu,new_kif_n); exit(0); }
        add_one_kif_to_db();
        new_kif_n++;


		ZERO_DB *pdb = &zdb[(zdb_count-1) % ZERO_DB_SIZE];
		if ( pdb->weight_n <= max_w || pdb->moves < 80 ) continue;
		max_w = pdb->weight_n;
		PRT("%6d:index=%d,max_w=%d,moves=%d\n",zdb_count,pdb->index,max_w,pdb->moves);
		if ( (max_w%5) != 0 && max_w != 1 && max_w < 449 ) continue;
		if ( www_kif_num >= WWW_KIF_MAX ) DEBUG_PRT("");
		www_kif[www_kif_num++] = pdb->index;
		
		char name[256];
		if ( 0 ) {
			sprintf(name,"csa/no%012d_dum.csa",pdb->index);
			FILE *fp = fopen(name,"w");
			if ( fp == NULL ) DEBUG_PRT("");
			fprintf(fp,"%s",KifBuf);
			fclose(fp);
		}
		sprintf(name,"csa/no%012d.csa",pdb->index);
		FILE *fp = fopen(name,"w");
		if ( fp == NULL ) DEBUG_PRT("");
		fprintf(fp,"PI\n+\n");
		char *top = KifBuf;
		if ( *top == '\n' ) top += 1;
		char *buf = top;
		char *p[5];
		int i;
		for (i=0;i<5;i++) {
			p[i] = strchr(buf,'\n');
			if ( p[i]==NULL ) DEBUG_PRT("");
			buf = p[i] + 1;
		}
		*(p[2]) = 0;
		fprintf(fp,"%s\n",top);
		fprintf(fp,"%s",p[4]+1);
		fclose(fp);
//		if ( www_kif_num >= 3 ) break;
    }

	char name[256];
	sprintf(name,"csa/sample.html");
	FILE *fp = fopen(name,"w");
	if ( fp == NULL ) DEBUG_PRT("");

	fprintf(fp,"<html><head>\n");
	fprintf(fp,"<title>AobaZero, game samples. process of stduy of shogi from reinforcement learning.</title>\n");
	fprintf(fp,"<meta charset=\"utf-8\">\n");
	fprintf(fp,"<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n");
	fprintf(fp,"<script src=\"./kifjs/kifuforjs.js\" charset=\"utf-8\"></script>\n");
	fprintf(fp,"<script>Kifu.settings={ImageDirectoryPath: \"./kifjs\"};</script>\n");
	fprintf(fp,"<link rel=\"stylesheet\" type=\"text/css\" href=\"./kifjs/kifuforjs.css\">\n");
	fprintf(fp,"</head><body>AobaZero, self-play game samples.\n<br>");
	int i;
	for (i=www_kif_num-1; i>=0; i--) {
		fprintf(fp,"<a href=\"#%d\">%d</a>\n",i,i);
		if ( ((i+1)%25)==0 ) fprintf(fp,"<br>");
	}
	for (i=www_kif_num-1; i>=0; i--) {
		int n = www_kif[i];
		fprintf(fp,"<hr>\n");
		fprintf(fp,"<a name=\"%d\">no%012d.csa</a>\n",i,n);
		fprintf(fp,"<script>var kifu = Kifu.load(\"./no%012d.csa\");</script>\n",n);
	}
	fprintf(fp,"</body></html>\n");
	fclose(fp);

    return 0;
}

// move_hash() で変更されるのは ban, init_ban, mo_c, kn, kn_stn, tume_hyouka, nifu_table_com, hash_code1
typedef struct INIT_BOARD_DATA {
	int cp_init_ban[BAN_SIZE];
	int cp_mo_m[8];
	int cp_mo_c[8];
	int cp_ban[BAN_SIZE];
	int cp_kn[41][2];
	int  cp_kn_stoc[8][32];
	int *cp_kn_stn[8];

	char cp_nifu_table_com[16][10];
	char cp_nifu_table_man[16][10];

	int cp_tume_hyouka;
	unsigned int cp_hash_code1;
	unsigned int cp_hash_code2;
	unsigned int cp_hash_motigoma;
} INIT_BOARD_DATA;

INIT_BOARD_DATA init_board_data[HANDICAP_TYPE];

void shogi::copy_restore_dccn_init_board(int handicap, bool fCopy)
{
	if ( handicap < 0 || handicap >= HANDICAP_TYPE ) DEBUG_PRT("");
	INIT_BOARD_DATA *p = &init_board_data[handicap];

	if ( fCopy ) {
		memcpy(p->cp_init_ban,       init_ban,       sizeof(init_ban));
		memcpy(p->cp_mo_m,           mo_m,           sizeof(mo_m));
		memcpy(p->cp_mo_c,           mo_c,           sizeof(mo_c));
		memcpy(p->cp_ban,            ban,            sizeof(ban));
		memcpy(p->cp_kn,             kn,             sizeof(kn));
		memcpy(p->cp_kn_stoc,        kn_stoc,        sizeof(kn_stoc));
		memcpy(p->cp_kn_stn,         kn_stn,         sizeof(kn_stn));
		memcpy(p->cp_nifu_table_com, nifu_table_com, sizeof(nifu_table_com));
		memcpy(p->cp_nifu_table_man, nifu_table_man, sizeof(nifu_table_man));

		p->cp_tume_hyouka   = tume_hyouka;
		p->cp_hash_code1    = hash_code1;
		p->cp_hash_code2    = hash_code2;
		p->cp_hash_motigoma = hash_motigoma;
	} else {	// restore
		memcpy(init_ban,       p->cp_init_ban,       sizeof(init_ban));
		memcpy(mo_m,           p->cp_mo_m,           sizeof(mo_m));
		memcpy(mo_c,           p->cp_mo_c,           sizeof(mo_c));
		memcpy(ban,            p->cp_ban,            sizeof(ban));
		memcpy(kn,             p->cp_kn,             sizeof(kn));
		memcpy(kn_stoc,        p->cp_kn_stoc,        sizeof(kn_stoc));
		memcpy(kn_stn,         p->cp_kn_stn,         sizeof(kn_stn));
		memcpy(nifu_table_com, p->cp_nifu_table_com, sizeof(nifu_table_com));
		memcpy(nifu_table_man, p->cp_nifu_table_man, sizeof(nifu_table_man));

		tume_hyouka   = p->cp_tume_hyouka;
		hash_code1    = p->cp_hash_code1;
		hash_code2    = p->cp_hash_code2;
		hash_motigoma = p->cp_hash_motigoma;
	}
}

void shogi::init_prepare_kif_db()
{
//	fSelectZeroDB = 1;
	if ( fMAKE_LEVELDB ) { DEBUG_PRT("fMAKE_LEVELDB=1 ! Err\n"); }
	init_rnd521( (int)time(NULL)+getpid_YSS() );		// 起動ごとに異なる乱数列を生成

	init_zero_kif_db();
	load_exist_all_kif();

	for (int i=0; i<HANDICAP_TYPE; i++) {
		hirate_ban_init(i);
		copy_restore_dccn_init_board(i, true);
	}

	if ( fSumTree ) init_stree();
}

int binary_search_kif_db(int r)
{
	int min = 0;				//最小の添字
	int max = zero_kif_games-1;	//最大の添字
	int mid = -1;
	int prev_mid = -1;

	//最大値と最小値が一致するまでループ
	while ( min <= max ) {
		mid = (min + max) / 2;

		if ( pZDBsum[mid] == r ) {
			return mid + 1;
		}
		if ( pZDBsum[mid] < r ) {
			//目的値が中央値よりも上なら、最小値を中央値の一つ上に設定
			min = mid + 1;
		} else if ( pZDBsum[mid] > r ) {
			//目的値が中央値よりも下なら、最大値を中央値の一つ下に設定
			max = mid - 1;
		}
		if ( prev_mid == mid ) break;
		prev_mid = mid;
	}
	if ( mid < 0 ) { DEBUG_PRT("mid < 0 Err\n"); }
    if ( pZDBsum[min] > r ) return min;
    if ( pZDBsum[mid] > r ) return mid;
    return max;
}

const int POLICY_VISIT = 1;
/*
static int mb_count[ZERO_DB_SIZE];

void save_mb_count()
{
	FILE *fp=fopen("mb_count.bin","wb");
	if ( fp == NULL ) { PRT("fOpen fail\n"); debug(); }
	fwrite( mb_count, sizeof(mb_count), 1, fp);
	fclose(fp);
}

void rand_check_test()
{
	PRT("rand_m521()=%d\n",rand_m521());
	int i;
	const int zero_kif_pos_num = 58850000;
//	const int zero_kif_pos_num = 58850000 / 5;	//  / 5,  / 8.33
	static unsigned short mb_count[zero_kif_pos_num];
	//       2067961630
	//         58850000
	int loop = zero_kif_pos_num;
	for (i=0;i<loop;i++) {
		int r = rand_m521() % zero_kif_pos_num;	// 0 <= r < zero_kif_pos_num
		if ( i< 100 ) PRT("rand_m521()=%12d,r=%9d\n",rand_m521(),r);
		mb_count[r]++;
	}
	double dsum = 0;
	for (i=0;i<loop;i++) {
		int r = mb_count[i];
		if ( i<10000 ) { PRT("%1d,",r); if ( (i+1)%32==0 ) PRT("\n"); }
		double d = 1.0 - r;
		dsum += d*d;
	}
	dsum = dsum / loop;
	PRT("\nloop=%10d,dsum=%f,sqrt()=%f\n",loop,dsum,sqrt(dsum));
	// loop=     10000,dsum=0.992200,sqrt()=0.996092
	// loop=   3678125,dsum=1.000018,sqrt()=1.000009
	// loop=  11770000,dsum=1.000936,sqrt()=1.000468
	// loop=  58850000,dsum=1.000224,sqrt()=1.000112
	// loop=  58850000,dsum=0.999956,sqrt()=0.999978
}
*/


// 駒得だけの評価関数を学習
// Assessing Game Balance with AlphaZero: Exploring Alternative Rule Sets in Chess
// https://arxiv.org/abs/2009.04374
// 盤上の駒の数の差(自分 - 相手)を d1,d2, ... ,d20 とする。
// (歩、香、桂、銀、金、角、飛、と、成香、成桂、成銀、龍、馬、 持駒：歩、香、桂、銀、金、角、飛)
// d0 = 1 は固定。w0,w1,w2, ... w20 が求めたい重み。d0=1 は先手勝率が0.55など偏りを吸収するためか。
// 評価関数は g(s) = tanh(w T d)
// 対局結果を z (-1,0,+1)  とすると
// [z - g(s)]^2
// がすべての局面 s に対して最小になるように最適化。
// tanh(x) の微分は 4/(e^x+e^-x)^2
// x = w T d = w0*d0 + w1*d1 + ... w20*d20 とすると
// w0 に対する偏微分は 2(z-tanh(x))*(-1)*d0*( 4/(e^x+e^-x)^2 )

const int PIECE_LEARN = 0;
const int MAX_PVW = 21;
double piece_w[MAX_PVW] = {0};
double piece_d_sum[MAX_PVW] = {0};

void shogi::get_piece_num_diff(bool bGoteTurn, int d[])
{
	int i;
	for (i=0;i<MAX_PVW;i++) d[i] = 0;
	d[0] = 1;
	for (int y=0;y<B_SIZE;y++) for (int x=0;x<B_SIZE;x++) {
		int z = make_z(x+1,y+1);
		int k = init_ban[z];
		if ( k==0 ) continue;
		int m = k & 0x0f;
		if ( m==0x08 ) continue;
		if ( m>=0x0e ) m--;	// m = 1...14
		if ( m>=0x08 ) m--;	// m = 1...13
		if ( k < 0x80 ) {
			d[m]++;
		} else {
			d[m]--;
		}
	}
	for (i=1;i<8;i++) {
		d[13+i] = mo_m[i] - mo_c[i];
	}
	if ( bGoteTurn ) for (i=1;i<MAX_PVW;i++) d[i] = -d[i];
}

void shogi::sum_pwv(double z, bool bGoteTurn, double sumd[])
{
	int d[MAX_PVW];
	get_piece_num_diff(bGoteTurn, d);

	double x = 0;
	int i;
	for (i=0;i<MAX_PVW;i++) x += piece_w[i] * d[i];
	for (i=0;i<MAX_PVW;i++) {
		double e = exp(x) + exp(-x);
		sumd[i] += 2.0*(z-tanh(x))*(-1.0)*d[i]*( 4/(e*e) );
	}
}
void update_piece_w()
{
	static double add = 0.01;
	static int count;
	if ( (++count % 40)==0 ) {
		PRT("ADD=%.10f\n",add);
		double fu = piece_w[1];
		if ( fu <=0 ) fu = 1.0;
		for (int i=0;i<MAX_PVW;i++) {
			if ( piece_d_sum[i] > 0 ) piece_w[i] -= add;
			if ( piece_d_sum[i] < 0 ) piece_w[i] += add;
//			piece_w[i] += -piece_d_sum[i]*add;	// unstable
			piece_d_sum[i] = 0;
			PRT("%f(%f),",piece_w[i],piece_w[i]/fu);
		}
		PRT("\n");
	}
	if ( (count%800)==0 ) { add /= 1.5; PRT("DIV:ADD=%.10f\n",add); }
}

static uint64_t rand_try = 0;
static uint64_t rand_batch = 0;

#ifdef F2187
void shogi::prepare_kif_db(int fPW, int mini_batch, float *data, float *label_policy, float *label_value, float label_policy_visit[][MOVE_2187_MAX])
#else
void shogi::prepare_kif_db(int fPW, int mini_batch, float *data, float *label_policy, float *label_value, float label_policy_visit[][MOVE_C_Y_X_ID_MAX])
#endif
{
//	int ct1 = get_clock();
	int sum_handicap[HANDICAP_TYPE] = { 0 };
	int sum_result[3] = { 0 };
	int sum_turn[2] = { 0 };
	double sum_t = 0;
	double sum_diff_win_r = 0;

	std::uniform_int_distribution<> dist(0, stree_total()-1);

	// pos_sum の中から64個ランダムで選ぶ
	int *ri = new int[mini_batch];
	int *ri_moves = new int[mini_batch];
	int i;
	for (i=0;i<mini_batch;i++) {
		int r = rand_m521() % zero_kif_pos_num;	// 0 <= r < zero_kif_pos_num, 最初に右辺がunsigned longで評価されるようで、マイナスにはならない。gccでも

		if ( fSumTree ) {
			int s = dist(get_mt_rand) + 1;	// +1 が必要。一様乱数
			r = stree_get_i(s);
		}

		rand_try++;
		// 40手までで投了してる棋譜は10分の1
		// 0手目から30手目は 0手目は10分の1、30手目で1,  n * 9/300 + 30/300
		ri_moves[i] = pZDBmove[r];
		if ( 0 && GCT_SELF ) {
			int n = pZDBmove[r];
			if ( n < 4 ) { i--; continue; }	// 最初の4手には乱数？で分布なし
			if ( pZDBscore_x10k[r] == NO_ROOT_SCORE ) { i--; continue; }
		}
		if ( 1 ) {
			int m = pZDBmaxmove[r];
			int n = pZDBmove[r];
//			if ( m <= 40 && (rand_m521() % 10) != 0 ) { i--; continue; }
			if ( m <= 40 && (rand_m521() % 4) != 0 ) { i--; continue; }
//			if ( n < 30 && (int)(rand_m521() % 300) > (n*9+30) ) { i--; continue; }
			if ( n < 30 && (float)(rand_m521() % 3000) > exp(8.0*(30-n)/30) ) { i--; continue; }
//			if ( n < 30 && (float)(rand_m521() % 1000) > pow(2.0,n) ) { i--; continue; }
//			if ( n < 24 ) { i--; continue; }
//			double x = 1.0 + abs(n-60) / 10.0;// from policy weight surprize. around 60 moves is most difficult.
//			if ( x > 6 ) x = 6;
//			double y = 0.003*x*x*x - 0.058*x*x + 0.141*x + 0.896;
//			if ( (double)(rand_m521() % 1000) > y*1000 )  { i--; continue; }
//			if ( n < 250 ) { i--; continue; } 
		}
		if ( 1 ) {
			int s = pZDBplayouts_sum[r];
			int x = pZDBscore_x10k[r];
			if ( s < 50 && (x==0 || x==10000) ) { i--; continue; }
//			if ( x < 1000 ) { i--; continue; }
//			if ( x < 2000 && (rand_m521() % 10) != 0  ) { i--; continue; }
//			if ( x < 2000 && (int)(rand_m521() % 200) > (x/10)  ) { i--; continue; }
//			if ( x < 1000 && (int)(rand_m521() % 100) > (x/10)  ) { i--; continue; }
		}

		int j;
		for (j=0;j<i;j++) {
			if ( ri[j] == r ) break;
		}
		if ( j != i ) { i--; continue; }
		ri[i] = r;
	}
	rand_batch += mini_batch;

	for (i=0;i<mini_batch;i++) {
		int r = ri[i];
		int j = 0;
		int sum = 0, t = -1;
		if ( 0 ) for (j=0;j<ZERO_DB_SIZE;j++) {
			ZERO_DB *p = &zdb[j];
			sum += p->moves;
			if ( sum > r ) {
				t = r - (sum - p->moves);
				if ( t >= p->moves || t < 0 ) { DEBUG_PRT("t over %d err.i=%d,j=%d,r=%d,sum=%d\n",t,i,j,r,sum); }
				break;
			}
		}
		int bi = binary_search_kif_db(r);	// 16秒が1秒に
		ZERO_DB *p = &zdb[bi];
		t = r - (pZDBsum[bi] - zdb[bi].moves);
		if ( t < 0 || t >= MAX_ZERO_MOVES ) { DEBUG_PRT("t=%d(%d) err.j=%d,r=%d,bi=%d\n",t,p->moves,j,r,bi); }
//		PRT("%3d:%7d,j=%4d:bi=%3d,t=%3d,moves=%3d,res=%d\n",i,r,j,bi,t,p->moves,p->result);
//		if ( bi != j ) debug();
//		if ( r - (pZDBsum[bi] - pZDB[bi].moves) != t ) debug();
//		mb_count[bi]++;

		copy_restore_dccn_init_board(p->handicap, false);
		fGotekara = (p->handicap!=0);
#if ( GCT_SELF==1)
		if ( GCT_SELF ) {
			for (int y=0;y<9;y++) for (int x=0;x<9;x++) {
				init_ban[(y+1)*16+(x+1)] = p->v_init_pos[y*9+x];
			}
			for (int i=0;i<7;i++) mo_m[i+1] = p->v_init_pos[81+0+i];
			for (int i=0;i<7;i++) mo_c[i+1] = p->v_init_pos[81+7+i];
			fGotekara = p->v_init_pos[81+7*2];
			ban_saikousei_without_kiki();
		}
#endif
		int fSymmetry = rand_m521() % 2;	// 乱数で左右反転
		fSymmetry = 0;	// 左右反転なし
//		if ( ri_moves[i] < 240 ) fSymmetry = 0;

		for (j=0;j<t;j++) {
			int bz,az,tk,nf;
			// 棋泉形式の2バイトを4バイトに変換。numは現在の手数。num=0から始まる。
			trans_4_to_2_KDB( p->v_kif[j]>>8, p->v_kif[j]&0xff, (j+fGotekara)&1, &bz, &az, &tk, &nf);

			move_hit_hash(bz,az,tk,nf);				// 利きは駒の種類ごとに作り直してるので不要
			move_hit_kif[j] = pack_te( bz,az,tk,nf );
			move_hit_hashcode[j][0] = hash_code1;	// 指した後の局面のハッシュ値を入れる
			move_hit_hashcode[j][1] = hash_code2;
			move_hit_hashcode[j][2] = hash_motigoma;
		}
		if ( j >= p->moves || j!=t ) { DEBUG_PRT("no next move? t=%d(%d) err.j=%d,r=%d\n",t,p->moves,j,r); }
		if ( 1 ) {
			int s = p->v_playouts_sum[t];
			int x = p->v_score_x10k[t];
			if ( s < 50 && (x==0 || x==10000) ) { static int count; PRT("r=%8d,t=%3d,s=%5d,x=%5d,count=%d\n",r,t,s,x,count++); }
//			if ( s < 50 ) PRT("r=%8d,t=%3d,s=%5d,x=%d\n",r,t,s,x);
		}

		int bz,az,tk,nf;
		bool bGoteTurn = (t+fGotekara) & 1;
		trans_4_to_2_KDB( p->v_kif[j]>>8, p->v_kif[j]&0xff, bGoteTurn, &bz, &az, &tk, &nf);

		int win_r = 0;
		if ( p->result == ZD_S_WIN ) win_r = +1;
		if ( p->result == ZD_G_WIN ) win_r = -1;
		if ( p->result == ZD_DRAW  ) win_r = 0;

		unsigned int score_x10k = p->v_score_x10k[j];
		float score_div = (float)score_x10k / 10000.0f;
		float score     = score_div * 2.0f - 1.0f;	// +1.0 >= x >= -1.0,   自分から見た勝率

		if ( bGoteTurn ) {
			hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
			win_r = -win_r;	// 勝敗まで反転はしなくてよい？ 反転した方が学習が簡単なはず。出てきた結果を反転
		}

		if ( fSymmetry ) {
			hanten_sasite_sayuu(&bz,&az,&tk,&nf);
		}

		// 実際の勝敗と探索値の平均を学習。https://tadaoyamaoka.hatenablog.com/entry/2018/07/01/121411
		float ave_r = ((float)win_r + score) / 2.0;
//		float ave_r = ((float)win_r*0 + score*10) / 10.0;

//		float m = pZDBmaxmove[r];
//		float n = pZDBmove[r];
//		float b = n/m;
//		float ave_r = (float)win_r*b + score*(1-b);
		if ( score_x10k == NO_ROOT_SCORE ) ave_r = win_r;
//		ave_r = win_r;	// not use average
//		PRT("(%6.3f,%6.3f,%6.3f,%3.0f/%3.0f,%.3f)",(float)win_r,score,ave_r,n,m,b);

		int playmove_id = 0;
		if ( f2187 ) {
			playmove_id = get_dlshogi_policy(bz,az,tk,nf);
		} else {
			playmove_id = get_move_id_c_y_x(pack_te(bz,az,tk,nf));
		}
		label_policy[i] = (float)playmove_id;
		label_value[i]  = (float)ave_r;

		if ( POLICY_VISIT ) {
			int k;
			int move_max = 0;
			if ( f2187 ) {
				move_max = MOVE_2187_MAX;
			} else {
				move_max = MOVE_C_Y_X_ID_MAX;
			}

			for (k=0; k<move_max; k++) {
				label_policy_visit[i][k] = 0.0f;
			}
			int playout_sum = 1;
			if ( p->v_playouts_sum.size() > 0 ) playout_sum = p->v_playouts_sum[j];
			if ( playout_sum <= 0 ) DEBUG_PRT("");
			label_policy_visit[i][playmove_id] = 1.0f / (float)playout_sum;		// 存在しない場合は1回探索した、とする

			int found = 0;
			int n = p->vv_move_visit[j].size();
			for (k=0;k<n;k++) {
				unsigned int x = p->vv_move_visit[j][k];
				int b0 = x>>24;
				int b1 =(x>>16)&0xff;
				int visit = x&0xffff;
				int bz,az,tk,nf;
				trans_4_to_2_KDB( b0, b1, bGoteTurn, &bz, &az, &tk, &nf);
				if ( bGoteTurn ) hanten_sasite(&bz,&az,&tk,&nf);	// 指し手を先後反転
//				PRT("r=%5d:b0=%3d,b1=%3d,[%3d][%3d] (%02x,%02x,%02x,%02x)v=%3d\n",r,b0,b1,j,k,bz,az,tk,nf,visit);
				if ( fSymmetry ) {
					hanten_sasite_sayuu(&bz,&az,&tk,&nf);
				}
#ifdef F2187
				int id = get_dlshogi_policy(bz,az,tk,nf);
#else
				int id = get_move_id_c_y_x(pack_te(bz,az,tk,nf));
#endif
				label_policy_visit[i][id] = (float)visit / playout_sum;
//				PRT("r=%5d:b0=%3d,b1=%3d,[%3d][%3d] (%02x,%02x,%02x,%02x)id=%5d,v=%3d,%6.4f\n",r,b0,b1,j,k,bz,az,tk,nf,id,visit,label_policy_visit[i][id]);
				if ( id==playmove_id ) found = 1;
			}
			if ( found==0 ) { static int count=0; if ( count++<=10 ) { PRT("\nno best move visit. i=%d,id=%d,bi=%d,t=%d/%d,visit_size=%d\n",i,playmove_id,bi,t,p->moves,p->vv_move_visit[j].size()); hyouji(); } }
		}

		float *pd = (float *)data + ONE_SIZE * i;
		memset(pd, 0, sizeof(float)*ONE_SIZE);
//		PRT("%2d:t=%d,win_r=%d,policy=%.0f\n",i,t,win_r,label_policy[i]); hyouji();
		set_dcnn_channels((Color)bGoteTurn, t, pd, -1, p->handicap);
//		prt_dcnn_data_table((float(*)[B_SIZE][B_SIZE])pd);
		if ( fSymmetry ) {
			symmetry_dcnn_channels(pd);
		}

		sum_handicap[p->handicap]++;
		sum_result[p->result]++;
		sum_turn[bGoteTurn]++;
		sum_t +=t;
		sum_diff_win_r +=fabs(win_r - ave_r);

		if ( PIECE_LEARN ) sum_pwv(win_r, bGoteTurn, piece_d_sum);

		if ( fPW ) PRT("%3d ",p->weight_n);
//		if ( fPW ) PRT("%d(%3d) ",bi,p->weight_n);
		static int tc[MAX_ZERO_MOVES],tc_all;
		int tt = t;
		if ( tt>199 ) tt=199;
		tc[tt]++;
		tc_all++;
//		if ( (tc_all % 1000000)==0 ) { PRT("tc="); for (int k=0;k<200;k++) PRT("%f,",(float)tc[k]/tc_all); PRT("\n"); }
	}
	if ( fPW ) PRT("\nhandicap=%d,%d,%d,%d,%d,%d,%d,result=%d,%d,%d,turn=%d,%d,ave_t=%.2f,ave_diff_win_r=%.5f\n"
		,sum_handicap[0],sum_handicap[1],sum_handicap[2],sum_handicap[3],sum_handicap[4],sum_handicap[5],sum_handicap[6]
		,sum_result[0],sum_result[1],sum_result[2], sum_turn[0],sum_turn[1], sum_t/mini_batch, sum_diff_win_r/mini_batch	);
//	PRT("%.2f sec, mini_batch=%d,%8d,%8.1f,%6.3f\n",get_spend_time(ct1), mini_batch, ri[0], label_policy[0], label_value[0]);
	if ( PIECE_LEARN ) update_piece_w();
	delete [] ri;
	delete [] ri_moves;
}

void convert_caffemodel(int iteration, int weight_number)
{
	print_time();
	PRT(",convert_caffemodel. weight_number=%4d,zdb_count=%10d,iteration=%d\n",weight_number,zdb_count,iteration);
	FILE *fp = fopen("/home/yss/test/koma_ext/aoba.sh","w");
	if ( fp==NULL ) DEBUG_PRT("");
	fprintf(fp,"#!/bin/bash\n");
	fprintf(fp,"cd /home/yss/test/koma_ext/\n");
	fprintf(fp,"export LD_LIBRARY_PATH=/home/yss/caffe_cpu/build/lib:/home/yss/cuda/cuda-11.2/lib64:\n");
	fprintf(fp,"python3 ep_short_auto_py3.py /home/yss/shogi/learn/snapshots/_iter_%d.caffemodel\n",iteration);
	fprintf(fp,"mv binary.txt w%012d.txt\n",weight_number);
	fprintf(fp,"xz -9 -z -k w%012d.txt\n",weight_number);
	fprintf(fp,"mv w%012d.txt.xz  ../../koma_syn/weight/\n",weight_number);
	fclose(fp);
	int ret = system("bash /home/yss/test/koma_ext/aoba.sh");

	ret = system("sleep 10");
	ret = system("/home/yss/koma_syn/rsync_weight_only.sh");
	(void)ret;
}


#include <boost/utility.hpp>
#include <boost/format.hpp>
#include <boost/spirit/home/x3.hpp>

namespace x3 = boost::spirit::x3;
const int WT_NUM = 23425624;	// 256x20b
std::vector<float> wt_keep(WT_NUM);

int load_network(std::string filename)
{
	ifstream wtfile;
	wtfile.open(filename);
		 
    if (wtfile.fail()) {
        PRT("Could not open weights file: %s\n", filename.c_str());
        return 0;
    }

    // Count size of the network
    PRT("Detecting residual layers...");

    // First line was the version number
    auto linecount = size_t{1};
    auto channels = 0;
    auto line = std::string{};
	std::getline(wtfile, line);
    while (std::getline(wtfile, line)) {
        auto iss = std::stringstream{line};
        // Third line of parameters are the convolution layer biases,
        // so this tells us the amount of channels in the residual layers.
        // We are assuming all layers have the same amount of filters.
        if (linecount == 2) {
            auto count = std::distance(std::istream_iterator<std::string>(iss),
                                       std::istream_iterator<std::string>());
            channels = count;
            PRT("%d channels...", channels);
        }
        linecount++;
    }
    // 1 format id, 1 input layer (4 x weights), 14 ending weights,
    // the rest are residuals, every residual has 8 x weight lines
    auto residual_blocks = linecount - (1 + 4 + 14);
    if (residual_blocks % 8 != 0) {
        PRT("\nInconsistent number of weights in the file.\n");
        return 0;
    }
	PRT("linecount=%d...",linecount);
    residual_blocks /= 8;
    PRT("%d blocks.\n", residual_blocks);

    // Re-read file and process
    wtfile.clear();
    wtfile.seekg(0, std::ios::beg);

    // Get the file format id out of the way
    std::getline(wtfile, line);

    const auto plain_conv_layers = 1 + (residual_blocks * 2);
    const auto plain_conv_wts = plain_conv_layers * 4;
    linecount = 0;
    int num = 0;
    while (std::getline(wtfile, line)) {
        std::vector<float> weights;
        auto it_line = line.cbegin();
        const auto ok = phrase_parse(it_line, line.cend(),  *x3::float_, x3::space, weights);
		int n = weights.size();
      	for (int i=0; i<n; i++) wt_keep[num+i] = weights[i];
        num += n;
//	    PRT("%3d:n=%8d:num=%10d\n",linecount,n,num);
        
        if (!ok || it_line != line.cend()) {
            PRT("\nFailed to parse weight file. Error on line %d.\n",
                    linecount + 2); //+1 from version line, +1 from 0-indexing
            return 0;
        }
        if (linecount < plain_conv_wts) {
            if (linecount % 4 == 0) {
//                m_fwd_weights->m_conv_weights.emplace_back(weights);
            } else if (linecount % 4 == 1) {
                // Redundant in our model, but they encode the
                // number of outputs so we have to read them in.
//                m_fwd_weights->m_conv_biases.emplace_back(weights);
            } else if (linecount % 4 == 2) {
//				modify_bn_scale_factor(weights);
//               m_fwd_weights->m_batchnorm_means.emplace_back(weights);
            } else if (linecount % 4 == 3) {
//				modify_bn_scale_factor(weights);
//                process_bn_var(weights);
//                m_fwd_weights->m_batchnorm_stddevs.emplace_back(weights);
            }
        } else {
            switch (linecount - plain_conv_wts) {
//              case  0: m_fwd_weights->m_conv_pol_w = std::move(weights); break;
            }
        }
        linecount++;
    }
	PRT("num=%d...linecount=%d\n",num,linecount);
	if ( num != WT_NUM ) DEBUG_PRT("");
//	for (int i=0; i<100; i++) PRT("%f,",wt_keep[i]);

    return num;
}



#if (USE_CAFFE==1)
#include <iostream>
#include <memory>
#include <random>
#include <caffe/caffe.hpp>
#include <glog/logging.h>

#include "caffe/caffe.hpp"
#include "caffe/util/io.hpp"
#include "caffe/blob.hpp"
#include "caffe/layers/memory_data_layer.hpp"

using namespace caffe;
using namespace std;

// Accumulate gradients across batches through the iter_size solver field.
// With this setting batch_size: 16 with iter_size: 1 and batch_size: 4 with iter_size: 4 are equivalent.
const int ITER_SIZE = 1;
//const int ITER_SIZE = 32;

constexpr auto kDataSize = MINI_BATCH * ITER_SIZE;	// ITER_SIZE=1 で 7MB

#ifdef F2187
float policy_visit[kDataSize][MOVE_2187_MAX];
array<float, kDataSize * MOVE_2187_MAX> dummy_policy_visit;
#else
float policy_visit[kDataSize][MOVE_C_Y_X_ID_MAX];
array<float, kDataSize * MOVE_C_Y_X_ID_MAX> dummy_policy_visit;
#endif

string get_short_float(float f)
{
//	PRT("%f:%.3g:",f,f);
	char str[TMP_BUF_LEN];
	sprintf(str,"%.3g",f);	// %.6gが安全か。LZ は %.3g
	string s = str;
	if ( strncmp(str,"-0.",3)==0 ) s = "-."+(string)(str+3);
	if ( strncmp(str,"0." ,2)==0 ) s = (str+1);
	return s;
}

void load_aoba_txt_weight( const caffe::shared_ptr<caffe::Net<float>> &net, std::string filename )
{
	int param_num = load_network(filename);
	int set_num = 0;

//	{ auto p = net->layer_by_name("conv1_3x3_192")->blobs()[0]->cpu_data(); for (int i=0;i<10;i++) PRT("%.6f,",*(p+i)); PRT("\n");	}
	auto layers = net->layers();
	auto nLayersSize = layers.size();
//	auto names = net->blob_names();		// include "slice", "silence"
	auto names = net->layer_names();

	PRT("Load aoba txt weight. %s, nLayersSize=%d\n",filename.c_str(), nLayersSize);
	int sum = 0;
	for (size_t i = 0; i < nLayersSize; i++) {
		auto layer = layers[i];
		auto weights = layer->blobs();
		auto n = weights.size();
		if ( n==0 ) continue;
		PRT("%3d,%d,%-23s",i,n,names[i].c_str());
		int sum_layer = 0;
		for (size_t j=0; j<n; j++) {
			auto b = weights[j];
			auto v = b->shape();
			int add = 0;
			PRT("(");
			for (size_t k=0; k<v.size(); k++) {
				int m = v[k];
				if ( k>0 ) PRT(",");
				PRT("%d",m);
				if ( add==0 ) add = 1;
				add *= m;
			}
			PRT(") ");
			if ( strstr(names[i].c_str(),"bn") && j==2 ) continue; // BN scale factor, constant (999.982361)
			sum_layer += add;
			auto p = b->mutable_cpu_data();
			for (int i=0;i<add;i++) {
//				if ( fabs(*(p+i) - wt_keep[set_num]) > 0.01 ) PRT(".");
				*(p+i) = wt_keep[set_num++];
//				PRT("%.6f,",*(p+i));
//				if ( i==10 ) { PRT("%s",get_short_float(*(p+i)).c_str()); PRT(","); }
			}
//			PRT("\n");
		}
		sum += sum_layer;
		PRT(", %d\n",sum_layer);
	}
	PRT("sum=%d,param_num=%d,set_num=%d\n",sum,param_num,set_num);
	if ( sum != set_num || sum != param_num ) { DEBUG_PRT("weight param Err.\n"); }
}

void start_zero_train(int *p_argc, char ***p_argv )
{
	FLAGS_alsologtostderr = 1;		// コンソールへのログ出力ON
	GlobalInit(p_argc, p_argv);

	PS->init_prepare_kif_db();
	if ( fWwwSample ) { PS->make_www_samples(); return; }

	// MemoryDataLayerはメモリ上の値を出力できるDataLayer．
	// 各MemoryDataLayerには入力データとラベルデータ（1次元の実数）の2つを与える必要があるが，
	// ラベル1つは使わないのでダミーの値を与えておく．
	static array<float, kDataSize> dummy_data;
	fill(dummy_data.begin(), dummy_data.end(), 0.0);
	fill(dummy_policy_visit.begin(), dummy_policy_visit.end(), 0.0);

	setModeDevice(0);

	// Solverの設定をテキストファイルから読み込む
	SolverParameter solver_param;
	if ( ITER_SIZE==1 ) {
		ReadProtoFromTextFileOrDie("aoba_solver.prototxt", &solver_param);
	} else if ( ITER_SIZE==64 ) {
		if ( MINI_BATCH!=64 ) DEBUG_PRT("MINI_BATCH err\n");
		ReadProtoFromTextFileOrDie("aoba_zero_solver_mb64_is64.prototxt", &solver_param);
	} else if ( ITER_SIZE==32 ) {
		if ( MINI_BATCH!=128 ) DEBUG_PRT("MINI_BATCH err\n");
		ReadProtoFromTextFileOrDie("aoba_zero_solver_mb128_is32.prototxt", &solver_param);
	} else {
		DEBUG_PRT("ITER_SIZE err\n");
	}
	std::shared_ptr<Solver<float>> solver;
	solver.reset(SolverRegistry<float>::CreateSolver(solver_param));

	//評価用のデータを取得
	const auto net      = solver->net();
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210604/_iter_10000.caffemodel";	// w0001
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210607/_iter_60000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210610/_iter_90000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210615_lr0001/_iter_400000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210722_lr0001/_iter_1730000.caffemodel";	// w213
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210805_lr0001/_iter_1070000.caffemodel";	// w320
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210828_lr00001/_iter_2030000.caffemodel";// w523
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210922_kldgain/_iter_2380000.caffemodel";// w761
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20210922_kldgain/_iter_2210000.caffemodel";// w744 巻き戻し
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20211002/_iter_1150000.caffemodel";	// w859 雷で停電
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20211014/_iter_950000.caffemodel";		// w954
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20211023/_iter_760000.caffemodel";		// w1030
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/20211118/_iter_1390000.caffemodel";	// w1169
//	const char sNet[] = "/home/yss/shogi/learn/20220112_191203_256x20b_swish/_iter_1976875.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220120_161939_256x20b_swish_from_20220112_191203/_iter_620000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220124_121016_256x20b_swish_from_20220120_161939/_iter_630000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220209_014645_256x20b_swish_no_ave_no_30_cos/_iter_1899620.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220216_205552_256x20b_swish_no_ave_no_30_cos_from_20220209_014645/_iter_230000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220218_071436_256x20b_swish_no_ave_no_30_from_20220216_205552/_iter_810000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220328_iter_1248000.caffemodel";	// w3922
//	const char sNet[] = "/home/yss/shogi/learn/snapshots/_iter_90000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220505_234024_256x20b_swish_ave_no_30_from_5392k_games_x3/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220509_125939_256x20b_ave_no_30_from_1000k_games_from_20220505_234024/_iter_782130.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220515_105749_256x20b_ave_no_30_from_53930k_games_from_20220509_125939/_iter_367135.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/w3969_iter_2752000.caffemodel";	// w3969
//	const char sNet[] = "/home/yss/shogi/learn/20220601_005103_256x20b_ave_exp_8_30_30_x_m40_4_loop_div4_from_51000k/_iter_600000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220810_014249_256x20b_ave_gct_001_025/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220813_134309_256x20b_ave_gct_026_050_from_20220810_014249/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220819_225132_256x20b_ave_gct_001_025_T1/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220823_120232_256x20b_ave_gct_001_025_T1_from_20220819_225132/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220831_152345_256x20b_ave_book_softmax/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220904_061307_256x20b_ave_book_softmax_from_20220831_152345/_iter_1600000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20220712_111426_256x20b_ave_exp_8_30_30_x_m40_4_loop_div4_from_51000k_20220601_005103/_iter_800000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20221105_181114_256x20b_ave_exp_8_30_30_x_m40_4_cos_from_58410k_20220712_111426/_iter_480000.caffemodel";
//	const char sNet[] = "/home/yss/shogi/learn/20221107_170923_ave_exp_8_30_30_x_m40_4_cos_from_58410k_20221105_181114/_iter_400000.caffemodel";

	int next_weight_number = 1170;	// 現在の最新の番号 +1

//	net->CopyTrainedLayersFrom(sNet);	// caffemodelを読み込んで学習を再開する場合
//	load_aoba_txt_weight( net, "/home/yss/w000000000689.txt" );	// 既存のw*.txtを読み込む。*.caffemodelを何か読み込んだ後に
	LOG(INFO) << "Solving ";
	PRT("fReplayLearning=%d\n",fReplayLearning);

	int iteration = 0;	// 学習回数
	int add = 0;		// 追加された棋譜数
	int remainder = 0;
	int iter_weight = 0;

wait_again:
	if ( GCT_SELF ) {
	} else if ( fReplayLearning ) {
		add = PS->add_a_little_from_archive();
		if ( add < 0 ) { PRT("done..\n"); solver->Snapshot(); return; }
		if ( zdb_count <=  ZERO_DB_SIZE ) goto wait_again;
//		if ( zdb_count <= 18850000 ) goto wait_again;
//		if ( zdb_count >  13000000 ) { PRT("done...\n"); solver->Snapshot(); return; }
//		if ( iteration >= 100000*1 ) { PRT("done...\n"); solver->Snapshot(); return; }
//		if ( iteration > 1000 ) solver_param.set_base_lr(0.01);
	} else {
		if ( 0 && iteration==0 && next_weight_number==1170 ) {
			add = 200;	// 初回のみダミーで10000棋譜追加したことにする
		} else {
			add = PS->wait_and_get_new_kif(next_weight_number);
		}
	}

	const int AVE_MOVES = 128;	// 1局の平均手数
	float add_mul = (float)AVE_MOVES / MINI_BATCH;
	int nLoop = (int)((float)add*add_mul); // MB=64でadd*2, MB=128でadd*1, MB=180でadd*0.711
	// (ITER_SIZE*MINI_BATCH)=4096 なら最低でも32棋譜必要(32*128=4096)
	int min_n = ITER_SIZE*MINI_BATCH / AVE_MOVES;
	if ( min_n > 1 ) {
		add += remainder;
		nLoop = add / min_n;
		remainder = add - nLoop * min_n;
	}

	const int ITER_WEIGHT_BASE = 10000*AVE_MOVES / (ITER_SIZE*MINI_BATCH);	// 10000棋譜(平均128手)ごとにweightを作成
	int iter_weight_limit = ITER_WEIGHT_BASE;
	float reduce = 1.0;	// weightは10000棋譜ごとで学習回数を10000から8000などに減らす。棋譜生成速度が速すぎるため
	FILE *fp = fopen("reduce.txt","r");
	if ( fp==NULL ) {
		PRT("fail open reduce.\n");
	} else {
		char str[TMP_BUF_LEN];
		if ( fgets( str, TMP_BUF_LEN, fp ) ) {
			reduce = atof(str);
		}
		nLoop             = (int)(reduce * nLoop);
		iter_weight_limit = (int)(reduce * ITER_WEIGHT_BASE);
		PRT("reduce=%7.4f, add=%d,nLoop=%d,iter_weight_limit=%d/%d\n",reduce,add,nLoop,iter_weight_limit,ITER_WEIGHT_BASE);
		if ( reduce <= 0 || reduce > 1.0 || iter_weight_limit <= 0 ) DEBUG_PRT("");
		fclose(fp);
	}

//nLoop /= 4;
//nLoop *= 0.602;	// *= 2.66 ... 800000 iteration / ((600000 kifu/ 2000) * 1000 Loop) = 2.66
//nLoop = (int)((float)nLoop * 0.029755f);
//nLoop = (int)((float)nLoop * 0.72727f);
//nLoop = (int)((float)nLoop * 0.701);
	if ( GCT_SELF ) nLoop = 800000*1;
nLoop = 100000*1;

	PRT("nLoop=%d,add=%d,add_mul=%.3f,MINI_BATCH=%d,kDataSize=%d,remainder=%d,iteration=%d(%d/%d),rand=%lu/%lu(%lf)\n",nLoop,add,add_mul,MINI_BATCH,kDataSize,remainder,iteration,iter_weight,iter_weight_limit, rand_try,rand_batch,(double)rand_try/(double)rand_batch);
	int loop;
	for (loop=0;loop<nLoop;loop++) {
		static array<float, kDataSize * ONE_SIZE> input_data;	// 大きいのでstaticで
		static array<float, kDataSize>            policy_data;
		static array<float, kDataSize>            value_data;

		int fPW = 0;
		if ( ITER_SIZE== 1 && loop==0 && (iteration % 16)==0 ) fPW = 1;
		if ( ITER_SIZE>=32 && loop==0 && (iteration %  8)==0 ) fPW = 1;
		if ( fPW ) PRT("%d:",next_weight_number);
		PS->prepare_kif_db(fPW, kDataSize, input_data.data(), policy_data.data(), value_data.data(), policy_visit);

		// 入力データをMemoryDataLayer"data"にセット
		const auto input_layer  = boost::dynamic_pointer_cast<MemoryDataLayer<float>>(net->layer_by_name("data"));
		assert(input_layer);
		input_layer->Reset(input_data.data(), dummy_data.data(), kDataSize);
		// 教師データをMemoryDataLayer"label_policy"にセット
		if ( POLICY_VISIT ) {
			const auto policy_visit_layer = boost::dynamic_pointer_cast<MemoryDataLayer<float>>(net->layer_by_name("label_policy"));
			assert(policy_visit_layer);
			policy_visit_layer->Reset((float*)policy_visit, dummy_policy_visit.data(), kDataSize);
		} else {
			const auto policy_layer = boost::dynamic_pointer_cast<MemoryDataLayer<float>>(net->layer_by_name("label_policy"));
			assert(policy_layer);
			policy_layer->Reset(policy_data.data(), dummy_data.data(), kDataSize);
		}

		const auto value_layer = boost::dynamic_pointer_cast<MemoryDataLayer<float>>(net->layer_by_name("label_value"));
		assert(value_layer);
		value_layer->Reset(value_data.data(), dummy_data.data(), kDataSize);

		// Solverの設定通りに学習を行う
		solver->Step(1);
//		solver->Solve();
//		solver->Snapshot();	// prototxt の設定で保存される
		iteration++;
		iter_weight++;

		if ( fReplayLearning==0 && iter_weight >= iter_weight_limit ) {
			iter_weight = 0;
			solver->Snapshot();
			convert_caffemodel(iteration, next_weight_number);
			next_weight_number++;
		}

	}
return;
	if ( GCT_SELF ) return;
	goto wait_again;
}
#endif

