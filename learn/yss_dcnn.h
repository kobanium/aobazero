// 2019 Team AobaZero
// This source code is in the public domain.
// yss_dcnn.h
#ifndef INCLUDE_YSS_DCNN_H_GUARD	//[
#define INCLUDE_YSS_DCNN_H_GUARD

#include <vector>
#include "lock.h"
using namespace std;

const int SHOGI_MOVES_MAX = 593;
const float ILLEGAL_MOVE = -1000;
typedef struct child {
	int   move;			// ��ξ��
	int   games;		// ���μ��õ���������
	float value;		// ��Ψ(+1 �Ǿ�����0 ���餱)
	float bias;			// policy
} CHILD;

typedef struct hash_shogi {
	lock_t entry_lock;			// SMP�ǤΥ�å���(���Υ�å��Ǥ�entry���ȤǤ⺹�Ϥʤ� mt=2��)
	uint64 hashcode64;			// �ϥå��女����
	int deleted;	// ����Ѥ�
	int games_sum;	// ���ζ��̤��褿���(�Ҷ��̤β���ι��)
	int sort_done;	// ʣ�����������ǥ����ȺѤߤ���
	int used;		// 
	Color col;		// ����
	int age;		// �����ܤλ׹ͻ�����Ͽ����pondering�ǤɤΥǡ�����ä������Υ����å��˻Ȥ�
	float net_value;		// value network�Ǥξ�Ψ
	int   has_net_value;

	int child_num;
	CHILD child[SHOGI_MOVES_MAX];
} HASH_SHOGI;


#define F11259  1	// ��ʸ�ɤ����������0��¸�ߤ��ʤ������
#if (F11259==0 ) 
const int MOVE_C_Y_X_ID_MAX = 3781;
#else
const int MOVE_C_Y_X_ID_MAX = 11259;	// 3781;
#endif

#define GCT_SELF 0	// GCT�δ��� selfplay_gct-???.hcpe3.xz ��Ȥ���硣 https://tadaoyamaoka.hatenablog.com/entry/2021/05/06/223701

typedef struct ZERO_DB {
	uint64 hash;	// ����򼨤��ϥå���
	uint64 date;	// ���������(��������Τ�����)
	int weight_n;	// weight�ֹ�
	int index;		// �̤��ֹ�
	int result;		// ��̡���꾡������꾡��������ʬ��
	int result_type;// ��λ�������ꡢ����(513��)�����������Ϣ³����β�ƨ���ˤ��ȿ§����
	int moves;		// ���(����Υ�������Ʊ��)
	int handicap;	// �����
#if ( GCT_SELF==1)
	vector <unsigned char> v_init_pos;		// ���϶��̡ܼ��� 81+7*2+1���ϥեޥ��256bit(32byte)��ɽ���Ǥ���Τǰ��̤ϲ�ǽ
#endif
	vector <unsigned short> v_kif;			// ����
	vector <unsigned short> v_playouts_sum;	// Root��õ�������̾��800����
	vector < vector<unsigned int> > vv_move_visit;	// (��+������)�Υڥ������16bit���ꡢ����16bit�����
	vector <unsigned short> v_score_x10k;	// Root��ɾ���͡���ʬ���鸫����Ψ��1.0�Ǿ�����0.0���餱��1���ܤ���Ƥ��롣
	vector <unsigned short> v_rawscore_x10k;// Network�Τ��ζ��̤ξ�Ψ
	vector < vector<char> > vv_raw_policy;
} ZERO_DB;

extern ZERO_DB zdb_one;	// �����ɤ߹��ߤǻ���

enum { ZD_DRAW, ZD_S_WIN, ZD_G_WIN };
enum { RT_NONE, RT_TORYO, RT_KACHI, RT_SENNICHITE, RT_G_ILLEGAL_ACTION, RT_S_ILLEGAL_ACTION, RT_CHUDAN };
const int RT_MAX = 7;	// 7����

const unsigned short NO_ROOT_SCORE = 10001;

void free_zero_db_struct(ZERO_DB *p);
void start_zero_train(int *, char ***);

#endif	//]] INCLUDE__GUARD
