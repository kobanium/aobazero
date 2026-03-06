// 2022 Team AobaZero
// This source code is in the public domain.
#include "../config.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>
#include <iostream>
#include "shogi.h"

#include "yss_var.h"
#include "yss_dcnn.h"
#include "param.hpp"


#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <sysexits.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#endif

const int CHILD_MAX = 7;

int pid_child[CHILD_MAX];
int pfd_a[CHILD_MAX][2];
int pfd_b[CHILD_MAX][2];
FILE *to_engine_stream[CHILD_MAX], *from_engine_stream[CHILD_MAX];

const int USI_MAX_LINES = 100;
const int USI_BUF_SIZE = 128*16;
char usi_return_latest_line[CHILD_MAX][USI_BUF_SIZE];
char usi_return_line[CHILD_MAX][USI_MAX_LINES][USI_BUF_SIZE];
int  usi_return_line_num[CHILD_MAX];

void kill_usi_child() {
	int sig = SIGTERM;	//SIGKILLでAobaZeroを殺すと共有メモリが解放されない
	int i;
	for (i=0; i<CHILD_MAX; i++)	{
		if ( pid_child[i] == 0 ) continue;
		std::cerr << "kill pid " << pid_child[i] << std::endl;
#ifdef _WIN32
#else
		kill(pid_child[i], sig);
#endif
		pid_child[i] = 0;
	}
}
void error(const char *msg) {
	PRT("error: %s\n", msg);
	kill_usi_child();
 	abort();
}

void send_wait(int n, const char *usi_commnad_line, const char *wait)
{
	int nLen = (int)strlen(usi_commnad_line);
	if ( nLen <= 0 ) error("nLen err");
	char sSend[USI_BUF_SIZE];
	strcpy(sSend,usi_commnad_line);
	sSend[nLen] = 0;
//	sprintf(sTmp, ",A%d\n", total_time[moves&1]);
//	strcat(sSend, sTmp);
	PRT("%d->%s", n,sSend);

	fprintf(to_engine_stream[n], "%s", sSend);
	fflush(to_engine_stream[n]);
	usi_return_line_num[n] = 0;
	usi_return_latest_line[n][0] = 0;
	if ( strlen(wait)==0 ) return;

	for (;;) {
		char usi_line[USI_BUF_SIZE];
		if ( fgets(usi_line, USI_BUF_SIZE, from_engine_stream[n]) == NULL ) break;
		PRT("<-%s",usi_line);
		strcpy(usi_return_line[n][usi_return_line_num[n]], usi_line);
		usi_return_line_num[n]++;
		if ( usi_return_line_num[n] >= USI_MAX_LINES ) error("usi lines over");
		strcpy(usi_return_latest_line[n], usi_line);
		if ( strstr(usi_line, wait) ) break;
	}
}

//#define ENGINE_DIR "/home/yss/prg/Kristallweizen"
//#define ENGINE_DIR "/home/yss/shogi/suisho5"
#define ENGINE_DIR "/home/yss/shogi/nnue_dr4_learner/exe"

void run_usi_engine()
{
#ifdef _WIN32
#else
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	const char *sRun[5] = 
//	   { ENGINE_DIR "/yane750zen2","","","","" };
//	   { ENGINE_DIR "/yane483_nnue_avx2","","","","" };
	   { ENGINE_DIR "/yane_evallearn","","","","" };

	int i;
	for (i=0; i<CHILD_MAX; i++) {
		if ( pipe(pfd_a[i]) == -1) error("can't open pipe a");
		if ( pipe(pfd_b[i]) == -1) error("can't open pipe b");
		int child = fork();
		pid_child[i] = child;
		if ( child < 0 ) { PRT("Err fork()\n"); abort(); }
		if ( child == 0 ) {
			if ( strlen(ENGINE_DIR) && chdir(ENGINE_DIR) < 0 ) error("chdir fail");
			// attach pipe a to stdin
			if (dup2(pfd_a[i][0], 0) == -1) error("dup pfd_a[0] failed");
			// attach pipe b to stdout
			if (dup2(pfd_b[i][1], 1) == -1) error("dup pfd_b[1] failed");
			execlp(sRun[0], sRun[0], sRun[1], sRun[2], sRun[3], sRun[4], NULL);
			error("execlp failed");
		}
		to_engine_stream[i]   = fdopen(pfd_a[i][1], "w");	// Attach pipe a to to_gnugo_stream
		from_engine_stream[i] = fdopen(pfd_b[i][0], "r");	// Attach pipe b to from_gnugo_stream
	}
#endif
}
/*
void CsaLoop()
{
	send_wait(0, "usi\n",    "usiok");
	send_wait(0, "isready\n","readyok");
	send_wait(0, "setoption name BookMoves value 0\n","");
	send_wait(0, "setoption Threads value 1\n","");
	send_wait(0, "setoption NodesLimit value 500000\n","");

	PRT("Game starts...\n");

	send_wait(0, "go\n","bestmove");
	PRT("%s",usi_return_latest_line[0]);
	send_wait(0, "position startpos moves 7g7f 8b3b 2g2f 3a4b 5i6h\n","");
	send_wait(0, "go\n","bestmove");
	send_wait(0, "position sfen l2g1R3/2ss1+P3/2ng3p1/1+Pp1p4/k3P4/NLPG1P3/g1B3P2/2KS4r/1N7 b 4P2LNSB5p 1\n","");
	send_wait(0, "go\n","bestmove");

	send_wait(0, "quit\n","");
	PRT("End CsaLoop()\n");
}

int hogemain()
{
	run_usi_engine();
	CsaLoop();
	kill_usi_child();
	return 0;
}
*/
//                        012345678
static char usi_koma[] = " PLNSGBRK";

std::string get_sfen_string(tree_t * restrict ptree, int sideToMove, int ply)
{
	std::string s = "position sfen ";
	char buf[256];
	int x,y;
	for (y=0;y<9;y++) {
		for (x=0;x<9;x++) {
			int z = y*9+x;
			int k = BOARD[z];
			if ( k==empty ) {
				int i,spaces=0;
				for (i=x;i<9;i++) {
					int z = y*9+i;
					int k = BOARD[z];
					if ( k!=empty ) break;
					spaces++;
				}
				if ( spaces == 0 ) DEBUG_PRT("");
				x += spaces-1;
				sprintf(buf,"%d",spaces);
				s += buf;
				continue;
			}
			int nf = 0;
			if ( abs(k) >= 0x09 ) nf = 0x08;
			if ( nf ) s += "+";
			int kk = abs(k) & 0x07;
			if ( kk == 0  ) kk = abs(k);
			sprintf(buf,"%c",usi_koma[kk&0x0f]+32*(k<0));
			s += buf;
		}
		if ( y!=8 ) s += "/";
	}

	int moves = ptree->nrep + ply - 1;

	// The pieces are always listed in the order rook, bishop, gold, silver, knight, lance, pawn;
	// https://yaneuraou.yaneu.com/2016/07/15/sfen%e6%96%87%e5%ad%97%e5%88%97%e3%81%af%e6%9c%ac%e6%9d%a5%e3%81%af%e4%b8%80%e6%84%8f%e3%81%ab%e5%ae%9a%e3%81%be%e3%82%8b%e4%bb%b6/
	const char ct[2] = { 'b','w' };
	sprintf(buf, " %c ",ct[sideToMove]);
//	sprintf(buf, " %c ",ct[moves&1]);
	s += buf;
	int i,sum = 0;
	for (i=7;i>=1;i--) {
		int n = get_motigoma(i, HAND_B);
		sum += n;
		if ( n==0 ) continue;
		if ( n > 1 ) { sprintf(buf,"%d",n); s += buf; }
		s += usi_koma[i];
	}
	for (i=7;i>=1;i--) {
		int n = get_motigoma(i, HAND_W);
		sum += n;
		if ( n==0 ) continue;
		if ( n > 1 ) { sprintf(buf,"%d",n); s += buf; }
		s += (char)(usi_koma[i]+32);
	}
	if ( sum==0 ) s += "-";
	sprintf(buf," %d\n",moves+1);	// 次に指す手数
	s += buf;
	return s;
}
/*
int usi_str_z(char *str)
{
	int x = str[0] - '1' + 1;
	int y = str[1] - 'a' + 1;
	int z = (y<<4) + (10 - x);
	return z;
}

char *str_usi_move(int bz,int az,int tk,int nf)
{
	static char str[10];
	memset(str,0,sizeof(str));
	if ( bz==0xff ) {
		sprintf(str,"%c*",usi_koma[tk&0x0f]);
	} else {
		sprintf(str,"%s", usi_z_str(bz));
	}
	strcat(str,usi_z_str(az));
	if ( nf ) strcat(str,"+");
	return str;
}

// 移動前か移動後、のみ正しく動作
void shogi::getCsaStr(char *str, int bz,int az,int tk,int nf, int SenteTurn)
{
	char turn[2] = { '-','+' };
	int k,x,bb,aa;
	if ( bz == 0xff ) {
		bb = 0x00;
		k = tk & 0x07;
	} else {
		k = init_ban[bz] & 0x0f;
		if ( k==0 ) {
			k = init_ban[az] & 0x0f;
		} else {
			if ( nf ) k += 8;
		}
		x = 10 - (bz & 0x0f);
		bb = (bz>>4) + (x<<4);
	}
	x = 10 - (az & 0x0f);
	aa = (az>>4) + (x<<4);
	sprintf(str,"%c%02X%02X%s",turn[SenteTurn],bb,aa,koma[k]);
}

void usistr_to_move(tree_t * restrict ptree, char *str, int *p_bz, int *p_az, int *p_tk, int *p_nf, int fSenteTurn)
{
	// position startpos moves 7g7f 3c3d 2g2f+ G*6i
	int nLen = strlen_int(str);
	if ( nLen < 4 || nLen > 5 ) { DEBUG_PRT("position move Err nLen=%d,%s\n",nLen,str); }
	int bz,az,tk=0,nf=0;
	if ( str[1] == '*' ) {
		bz = 0xff;
		for (tk=0;;tk++) {
			if ( usi_koma[tk] == str[0] ) break;
			if ( usi_koma[tk] == 0 ) { DEBUG_PRT("not found drop piece\n"); }
		}
		if ( ! fSenteTurn ) tk |= 0x80;
	} else {
		bz = usi_str_z(&str[0]);
	}
	az = usi_str_z(&str[2]);
	if ( str[4] == '+' ) nf = 0x08;
	if ( bz != 0xff ) tk = init_ban[az];

	if ( 0 ) {	// CSA file output
		char tmp[TMP_BUF_LEN];
		getCsaStr(tmp, bz,az,tk,nf, fSenteTurn);
		PRT("%s",tmp); 
		if ( ((tesuu+1)%8)==0 ) PRT("\n"); else PRT(",");
	}
//	PRT("%x,%x,%x,%x\n",bz,az,tk,nf);
	*p_bz = bz; *p_az = az; *p_tk = tk; *p_nf = nf;
}
*/

unsigned int usi2move(tree_t * restrict ptree, int sideToMove, char *str)
{
	char str_buf[7];
	unsigned int move;
    if ( usi2csa( ptree, str, str_buf ) < 0 ) DEBUG_PRT("");
	PRT("csa:%s\n",str_buf);
    if ( interpret_CSA_move_turn( ptree, &move, str_buf, sideToMove ) < 0 ) DEBUG_PRT("");
	return move;
}

unsigned int get_best_move_alphabeta_usi(tree_t * restrict ptree, int sideToMove, int ply)
{
	// sfenで
	std::string s = get_sfen_string(ptree, sideToMove, ply);
	PRT("%s",s.c_str());

	static int fDone = 0;
	if ( fDone==0 ) {
		fDone = 1;
		run_usi_engine();
		send_wait(0, "usi\n",    "usiok");
		send_wait(0, "isready\n","readyok");
		send_wait(0, "setoption name BookMoves value 0\n","");
		send_wait(0, "setoption Threads value 1\n","");
		send_wait(0, "setoption NodesLimit value 500000\n","");
	}
	send_wait(0, s.c_str(),"");
	send_wait(0, "go\n","bestmove");
	PRT("%s",usi_return_latest_line[0]);
	char *p = strstr(usi_return_latest_line[0],"bestmove");
	if ( p==NULL ) DEBUG_PRT("");
	char *str = p+9;
	char *q = strchr(str,' ');
	char *r = strchr(str,'\n');
	if ( r==NULL ) DEBUG_PRT("");
	if ( q == NULL ) *r = 0;
	else *q = 0;
	if ( strstr(str,"resign") ) {
		return 0;
	}
//	int bz,az,tk,nf;
//	usistr_to_move(ptree, str, &bz,&az,&tk,&nf, !sideToMove);

	unsigned int move = usi2move(ptree, sideToMove, str);
	PRT("=%s=(%08x)\n",str,move);
	return move;
}



// MCTSの末端で静止探索を行って、その末端局面でノードを評価。Policyがいるので2回呼ぶことになるか・・・。
// Policyの最善を1手深く読む、だと +75
// 常に1手先を評価、で同一playout回数で+75 ELO強い
// http://www.yss-aya.com/bbs_log/bbs2023.html#bbs99
bool get_pv_qsearch_usi(tree_t * restrict ptree, int sideToMove, int ply, float *ret_v)
{
	*ret_v = 0;
	bool ok = false;
	// sfenで
	std::string s = get_sfen_string(ptree, sideToMove, ply);
//	PRT("%s",s.c_str());

	int tid = get_thread_id(ptree);
	if ( tid >= CHILD_MAX ) DEBUG_PRT("tid=%d > CHILD_MAX!\n");
	static int done = 0;
	static lock_yss_t pipe_lock;
	Lock(pipe_lock);
	if ( done==0 ) {
		run_usi_engine();
		for (int i=0; i<CHILD_MAX; i++) {
			send_wait(i, "usi\n",    "usiok");
			send_wait(i, "setoption name EvalDir value /home/yss/shogi/nnue_dr4_learner/exe/eval/\n", "");
 			send_wait(i, "isready\n","readyok");
		}
		done = 1;
	}
	UnLock(pipe_lock);
	send_wait(tid, s.c_str(),"");
	send_wait(tid, "qsearch\n","qsearch :");
	// qsearch : Value = 2185 , PV = 6i5h 4g5h+ 6e7d
//	PRT("%s",usi_return_latest_line[tid]);
	char *p = strstr(usi_return_latest_line[tid],"PV = ");
	if ( p==NULL ) DEBUG_PRT("");
	char *str = p+5;
	if ( strstr(str,"resign") ) return ok;

//	PRT("hash=%" PRIx64 ",%" PRIx64 "\n",ptree->rand2_hash,get_marge_hash(ptree, sideToMove));
	// 1手ずつ取り出す
	int turn = sideToMove;
	p = str;
	const int LOOP_MAX = 10;
	unsigned int ma[LOOP_MAX];
	int loop;
	for (loop=0; loop<LOOP_MAX; loop++) {
		char *q = strchr(p,' ');
		char *r = strchr(p,'\n');
		if ( r==NULL ) DEBUG_PRT("");
		if ( q == NULL ) *r = 0;
		else *q = 0;
		if ( *p == 0 ) break;

		unsigned int move = usi2move(ptree, turn, p);
		ma[loop] = move;
		MakeMove( turn, move, ply );
		ptree->path[ply] = move;
		MOVE_CURR = move;
		copy_min_posi(ptree, Flip(turn), ply);

		turn = Flip(turn); ply++;

		if ( q == NULL ) break;
		p = q+1;
	}

	if ( loop ) {
		HASH_SHOGI *phg = HashShogiReadLock(ptree, turn);	// 空きを探してる時にロックで固まる
//		HASH_SHOGI hs;
//		HASH_SHOGI *phg = &hs;
//		phg->deleted = 1;
		if ( phg->deleted ) create_node(ptree, turn, ply, phg, false, false);
		PRT("loop=%d:v=%.5f\n",loop,phg->net_value);
		float v = phg->net_value;
		if ( loop & 1 ) v = -v;
		*ret_v = v;
		ok = true;

		UnLock(phg->entry_lock);
	}

	for (int i=loop; i>0; i--) {
		turn = Flip(turn); ply--;
		UnMakeMove( turn, ma[i-1], ply );
	}

//	PRT("hash=%" PRIx64 ",%" PRIx64 "\n",ptree->sequence_hash,get_marge_hash(ptree, sideToMove));
	if ( turn != sideToMove ) DEBUG_PRT("");
//	if ( loop ) print_board(ptree);
	PRT("%s,loop=%d,turn=%d,sideToMove=%d\n",str,loop,turn,sideToMove);
	return ok;
}
