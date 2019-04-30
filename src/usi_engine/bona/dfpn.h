// 2019 Team AobaZero
// This source code is in the public domain.
#if defined(DFPN)
#ifndef DFPN_H
#define DFPN_H

/* #define DFPN_DBG */

#define DFPN_AGE_MASK           0xffU
#define INF                     0x7fffffU
#define INF_1                   0x7ffffeU
#define MAX_NMOVE               256

#define DFPN_ERRNO_MAXNODE      -101
#define DFPN_ERRNO_MAXPLY       -102
#define DFPN_ERRNO_SUMPHI       -103
#define DFPN_ERRNO_DELTA2P      -104
#define DFPN_ERRNO_SIGNAL       -105

/* #define DFPN_HASH_MASK     0x00fffffU */ /*   24MByte */
/* #define DFPN_HASH_MASK     0x01fffffU */ /*   48MByte */
/* #define DFPN_HASH_MASK     0x03fffffU */ /*   96MByte */
/* #define DFPN_HASH_MASK     0x07fffffU */ /*  192MByte */
/* #define DFPN_HASH_MASK     0x0ffffffU */ /*  384MByte */
/* #define DFPN_HASH_MASK     0x1ffffffU */ /*  768MByte */
/* #define DFPN_HASH_MASK     0x3ffffffU */ /* 1536MByte */
#define DFPN_NODES_MASK    UINT64_C(0x1fffffffffff)
#define DFPN_NUM_REHASH    32


#if defined(DFPN_DBG)
extern int dbg_flag;
#  define DOut(ply,fmt,...) if ( dbg_flag && ply <= 64 ) out( "%-*d" fmt "\n", 2*ply, ply, __VA_ARGS__ )
#else
#  define DOut( ... )
#endif

/*#define DFPNSignKey(word1, word2, word3) word1 = ( word1 ^ word2 ^ word3 )*/
#define DFPNSignKey(word1, word2, word3)

typedef struct { uint64_t word1, word2, word3; } dfpn_hash_entry_t;

typedef struct {
  uint64_t hash_key;
  uint64_t nodes;
  uint64_t expanded;
  unsigned int hand_b;
  unsigned int min_hand_b;
  unsigned int move;
  unsigned int phi;
  unsigned int delta;
  unsigned int priority;
  int is_delta_loop;
  int is_phi_loop;
  int is_loop;
  int is_weak;
} child_t;


typedef struct {
  child_t * restrict children;
  uint64_t hash_key;
  uint64_t nodes;
  uint64_t new_expansion;
  unsigned int min_hand_b;
  unsigned int hand_b;
  unsigned int phi;
  unsigned int delta;
  unsigned int sum_phi;
  unsigned int min_delta;
  int is_delta_loop;
  int is_phi_loop;
  int nmove, turn, icurr_c;
} node_t;


typedef struct {
  uint64_t node_limit;
  int turn_or;
  int root_ply;
  unsigned int sum_phi_max;
  child_t child_tbl[ MAX_NMOVE * PLY_MAX ];
  node_t anode[ PLY_MAX ];
} dfpn_tree_t;


enum { weak_chuai = 1, weak_drop  = 100 };


void CONV dfpn_hash_probe( const dfpn_tree_t * restrict pdfpn_tree,
			   child_t * restrict pchild, int ply, int turn );
void CONV dfpn_hash_store( const tree_t * restrict ptree,
			   dfpn_tree_t * restrict pdfpn_tree,
			   int ply );
int CONV dfpn_detect_rep( const dfpn_tree_t * restrict pdfpn_tree,
			  uint64_t hash_key, unsigned int hand_b,
			  int ply, int * restrict ip );
unsigned int CONV dfpn_max_hand_b( unsigned int hand_b, unsigned hand_w );
unsigned int CONV dfpn_max_hand_w( unsigned int hand_b, unsigned hand_w );
float CONV dfpn_hash_sat( void );

extern dfpn_hash_entry_t * restrict dfpn_hash_tbl;
extern unsigned int dfpn_hash_mask;
extern unsigned int dfpn_hash_age;
extern const unsigned int hand_tbl[16];

#endif /* DFPN_H */
#endif /* DFPN */
