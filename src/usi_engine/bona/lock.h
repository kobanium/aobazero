// 2019 Team AobaZero
// This source code is in the public domain.
// lock.h
#ifndef INCLUDE_LOCK_H_GUARD	//[
#define INCLUDE_LOCK_H_GUARD

#if (defined(_WIN64))
#define SMP_LOCK_INTERLOCK	// use Interlock func for Lock().
#endif


#ifdef SMP_LOCK_INTERLOCK
#include <Windows.h>
#endif

// __ANDROID__ is always predefined by the toolchain when targetting Android.
// __arm__   -> for ARM 32-bit devices (but not 64-bit ARMv8 a.k.a AARCH64)
// __i386__  -> for x86 ones.
#ifdef __ANDROID__
/*
typedef CRITICAL_SECTION lock_yss_t;
//inline void LockInit(lock_yss_t hPtr) { InitializeCriticalSection(&hPtr); }	// fail inline
//inline void Lock(lock_yss_t hPtr)     { EnterCriticalSection(&hPtr); }		// CRITICAL_SECTION is struct. passed copy.
//inline void UnLock(lock_yss_t hPtr)   { LeaveCriticalSection(&hPtr); }
#  define LockInit(v)   ( InitializeCriticalSection(&v) )
#  define UnLock(v)     ( LeaveCriticalSection(&v)      )
#  define Lock(v)       ( EnterCriticalSection(&v)      )
*/
#include <pthread.h>

typedef pthread_mutex_t lock_yss_t;			// linux 24 bytes, ARM 4 bytes. In Android's Bionic libc, a simple atomic compare-and-exchange
#define LockInit(v)     ( pthread_mutex_init(&v,NULL) )	
#define Lock(v)	        ( pthread_mutex_lock(&v)      )
#define UnLock(v)       ( pthread_mutex_unlock(&v)    )
#define LockDestroy(v)  ( pthread_mutex_destroy(&v)   )
//#define TryLock(v)      ( pthread_mutex_trylock(&v)   )

#else

typedef volatile int lock_yss_t[1];



inline void LockInit(lock_yss_t hPtr) { *hPtr = 0; }
inline void UnLock(lock_yss_t hPtr)   { *hPtr = 0; }

//inline void Lock(lock_yss_t hPtr) {}
inline void Lock(lock_yss_t hPtr)
{
#ifdef SMP_LOCK_INTERLOCK
	for (;;) {
		if ( _interlockedbittestandset((long*)hPtr, 0)==0 ) return;
		while (*hPtr) {
			_mm_pause();	// tell CPU here is spin loop
		}
/*		int i;
		for (i=0;i<100;i++) {
			_mm_pause();
			if ( *hPtr==0 ) break;
		}
		if ( i==100 ) Sleep(0);	//
*/
	}
#else

#  if defined(_MSC_VER)
#    if 1
    __asm
      {
        mov     ecx, hPtr
   la:  mov     eax, 1
        xchg    eax, [ecx]	// atomic. very slow.
        test    eax, eax
        jz      end
   lb:
		pause				// nice in Hyper-Thireading,  and normal multi-thread
        mov     eax, [ecx]
        test    eax, eax
        jz      la
        jmp     lb
   end:
      }
#    else
    __asm
      {						// TTAS (test and test-and-set) is better?
        mov     ecx, hPtr
   la:  mov     eax, [ecx]
        test    eax, eax
        jz      lb
		pause
        jmp     la
   lb:  mov     eax, 1
        xchg    eax, [ecx]
        test    eax, eax
        jnz     la
      }
#    endif
#  else
//		if ( __sync_lock_test_and_set(hPtr, 1) ) 
/* 
 	int itemp;
	for (;;) {
		asm ( "1:   movl     $1,  %1 \n\t"
			  "     xchgl   (%0), %1 \n\t"
	        : "=g" (hPtr), "=r" (itemp) : "0" (hPtr) );		//
		if ( ! itemp ) { return; }
		while ( *hPtr );
	}
*/
/*
	int dummy;
	asm __volatile__(
		"1:    movl    $1, %0"   "\n\t"
		"      xchgl   (%1), %0" "\n\t"
		"      testl   %0, %0"   "\n\t"
		"      jz      3f"       "\n\t"
		"2:    pause"            "\n\t"
		"      movl    (%1), %0" "\n\t"
		"      testl   %0, %0"   "\n\t"
		"      jnz     2b"       "\n\t"
		"      jmp     1b"       "\n\t"
		"3:"                     "\n\t"
		:"=&q"(dummy) :"q"(hPtr) :"cc", "memory");
*/
	int dummy;				// test first
	asm __volatile__(
		"1:    movl    (%1), %0" "\n\t"				//
		"      testl   %0, %0"   "\n\t"
		"      jz      3f"       "\n\t"
		"2:    pause"            "\n\t"
		"      jmp     1b"       "\n\t"

		"3:    movl    $1, %0"   "\n\t"
		"      xchgl   (%1), %0" "\n\t"
		"      testl   %0, %0"   "\n\t"
		"      jnz     2b"       "\n\t"				//
		:"=&q"(dummy) :"q"(hPtr) :"cc", "memory");	//
													//
#  endif

#endif
}

#endif
#endif	//] INCLUDE_LOCK_H_GUARD
