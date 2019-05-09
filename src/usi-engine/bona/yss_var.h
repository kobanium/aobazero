// 2019 Team AobaZero
// This source code is in the public domain.
#ifndef INCLUDE_yss_var_h_GUARD	//[[
#define INCLUDE_yss_var_h_GUARD

#if defined(_MSC_VER)
#define PRIx64	"I64x"
#define PRIu64	"I64u"
#define PRIs64	"I64d"
typedef unsigned __int64 uint64;
typedef   signed __int64  int64;
#ifndef UINT64_MAX
  #define UINT64_MAX (0xffffffffffffffffui64)		// defined in gcc.
#endif
#else
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
//#include <inttypes.h>		// PRIx64 , should use PRIx64 rather than "llx". undef "*.cpp"?
#ifndef UINT64_MAX
  #define UINT64_MAX (18446744073709551615ULL)	// defined "*.c", undefined "*.cpp" in gcc
#endif
#ifndef PRIx64
  #define PRIx64	"llx"
  #define PRIu64	"llu"
  #define PRIs64	"lld"
#endif


//typedef uint64_t uint64;	// Linux
typedef long long unsigned int uint64;
typedef long long   signed int  int64;
#endif




#if (_MSC_VER <= 1310)	// .NET 2003
typedef long     FIND_FILE_HANDLE;
#else
typedef intptr_t FIND_FILE_HANDLE;
#endif

/*
VC6.0 : 1200
VC7.0 : 1300    .NET
VC7.1 : 1310	.NET 2003
VC8.0 : 1400	2005
*/

#if (defined(_WIN64) || defined(__x86_64__))
typedef uint64       HASH_ALLOC_SIZE;
#else
typedef unsigned int HASH_ALLOC_SIZE;
#endif

#endif	//]] INCLUDE__GUARD
