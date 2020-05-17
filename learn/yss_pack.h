// 2019 Team AobaZero
// This source code is in the public domain.
// yss_pack.h
#ifndef INCLUDE_YSS_PACK_H_GUARD	//[
#define INCLUDE_YSS_PACK_H_GUARD


#if 1
inline int get81(int z) {	
	int y = z >> 4;		// 1 <= y <= 9
	int x = z & 0x0f;	// 1 <= x <= 9
	return (y-1)*9 + (x-1);		// 0...80
}
#else
inline int get81(int z) {
	// 16倍してある数値を9倍に直すには？ 16x -> 9x = (8x+1x)
	int x  = z & 0x0f;
	int y  = z   >> 4;
	int y8 = y   << 3;
	return (y8+y)-9 + (x-1);
// 18.048 秒, 16倍してある数値を9倍
// 18.095 秒, 通常。差はなし。
}
/*
inline int get81(int z) {
	z -= 0x11;	// 0x00 - 0x88;
	int y = z >> 4;
	int x = z & 0x0f;
	return y*9 + x;		// 0xff --> 14*9 + 14 = 140
}
*/
#endif

inline int get256(int z81) {	// 0 <= z81 <= 80
	int y = z81 / 9;
	int x = z81 - y*9;
	return ((y+1)<<4) + (x+1);
}

inline int get81x(int z) { return z - (z/9)*9; }	// z=10 で x=1, z=17 で x=8
inline int get81y(int z) { return z/9; }			// z=10 で y=1, z=80 で y=8

inline int flip(int z) { return 80 - z; }

inline int flip256(int z) {	return 0xa0 - (z & 0xf0) + 0x0a - (z & 0x0f); }

int x_flip(int z);


// 指し手をintに圧縮
inline int pack_te(int bz,int az,int tk,int nf) {
	return (bz << 24) | (az << 16) | (tk << 8) | nf;
}
inline void unpack_te(int *bz,int *az,int *tk,int *nf, int te) {
	*bz = (te >> 24) & 0xff;
	*az = (te >> 16) & 0xff;
	*tk = (te >>  8) & 0xff;
	*nf = (te      ) & 0xff;
}
inline int get_bz(int te) { return (te >> 24) & 0xff; }
inline int get_az(int te) { return (te >> 16) & 0xff; }
inline int get_tk(int te) { return (te >>  8) & 0xff; }
inline int get_nf(int te) { return (te      ) & 0xff; }

inline void set_te(int *p, int bz,int az,int tk,int nf)
{
	*(p+0) = bz;
	*(p+1) = az;
	*(p+2) = tk;
	*(p+3) = nf;
}
inline void set_te(int *p, int *pp)
{
	*(p+0) = *(pp+0);
	*(p+1) = *(pp+1);
	*(p+2) = *(pp+2);
	*(p+3) = *(pp+3);
}
inline void swap_te(int *p, int *pp)
{
	int bz = *(p+0);
	int az = *(p+1);
	int tk = *(p+2);
	int nf = *(p+3);
	*(p+0) = *(pp+0);
	*(p+1) = *(pp+1);
	*(p+2) = *(pp+2);
	*(p+3) = *(pp+3);
	*(pp+0) = bz;
	*(pp+1) = az;
	*(pp+2) = tk;
	*(pp+3) = nf;
}
inline bool equal_te(int *p, HASH_RET *phr)
{
	if ( *(p+0)!=phr->bz ) return false;
	if ( *(p+1)!=phr->az ) return false;
	if ( *(p+2)!=phr->tk ) return false;
	if ( *(p+3)!=phr->nf ) return false;
	return true;
}
inline bool equal_te(int *p, int bz,int az,int tk,int nf)
{
	if ( *(p+0)!=bz ) return false;
	if ( *(p+1)!=az ) return false;
	if ( *(p+2)!=tk ) return false;
	if ( *(p+3)!=nf ) return false;
	return true;
}
inline bool equal_te(int *p, int *pp)
{
	if ( *(p+0)!=*(pp+0) ) return false;
	if ( *(p+1)!=*(pp+1) ) return false;
	if ( *(p+2)!=*(pp+2) ) return false;
	if ( *(p+3)!=*(pp+3) ) return false;
	return true;
}

inline void set_te_kari(int *p, int bz,int az,int tk,int nf, int kari)
{
	*(p+0) = bz;
	*(p+1) = az;
	*(p+2) = tk;
	*(p+3) = nf;
	*(p+5) = kari;
}

inline bool is_out_of_board(int z)
{
	int x =  z & 0x0f;
	int y = (z & 0xf0) >> 4;
	if ( x < 1 || x > 9 || y < 1 || y > 9 ) return true;
	return false;
}

inline int make_z(int x,int y)	
{
	return (y)*16+x;	// 1 <= x <= 9, 1 <= y <= 9
}

#endif	//] INCLUDE_YSS_PACK_H_GUARD
