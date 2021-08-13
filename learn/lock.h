// 2019 Team AobaZero
// This source code is in the public domain.
// lock.h
#ifndef INCLUDE_LOCK_H_GUARD	//[
#define INCLUDE_LOCK_H_GUARD

#if defined(SMP)


#if !defined(_MSC_VER)

#ifdef SMP_CRITICALSECTION
	typedef pthread_mutex_t lock_t;
	#define LockInit(v)      ( pthread_mutex_init(&v,NULL) )
	#define LockY(v)	     ( pthread_mutex_lock(&v)      )
	#define UnLock(v)        ( pthread_mutex_unlock(&v)    )
#else
#endif

inline void Pause() { sched_yield(); }


#else							// for Windows


#define pthread_attr_t  HANDLE
#define pthread_t       HANDLE
#define thread_t        HANDLE

/*
#define tfork(t,f,p)    do {                                                           \
                          unsigned threadID;                                           \
                          *t = (pthread_t)_beginthreadex(NULL,0,(f),(p),0,&threadID);  \
                        } while (0)
// Win98ではthreadIDを受け取る変数を指定しないとエラーになる。
// beginthread()はスレッドハンドルを返すかどうか不定。使ってはいけない。
*/

#ifdef SMP_CRITICALSECTION	// lock()にcritical sectionを使う場合。
   typedef CRITICAL_SECTION lock_t;
#  define LockInit(v)      ( InitializeCriticalSection(&v) )
#  define UnLock(v)        ( LeaveCriticalSection(&v)      )
#else
   typedef volatile int lock_t[1];
#  define LockInit(v)      ((v)[0] = 0)
#  define UnLock(v)        ((v)[0] = 0)
#endif


#ifdef SMP_LOCKCHAR_OFF	// ハッシュにロックをかけない場合。危険だが7%速くなる。


#else

//#define INLINE __forceinline

//#define SMP_LOCK_COUNT	// ロックが衝突した回数を数える場合（デバグ用）
#ifdef SMP_LOCK_COUNT	// ロックが衝突した回数を数える場合（デバグ用）
extern volatile int lock_all;		// 10000回に1度くらい衝突している。
extern volatile int lock_counter;
#endif

#define SMP_LOCK_FREE	// lockfree hash。read ではロックしない

#ifdef SMP_LOCK_FREE
inline void LockChar(volatile char *hPtr)
{
#ifdef SMP_LOCK_INTERLOCK
	for (;;) {
		// 構造体の char なので エンディアン依存！ x86は 0x12345678 が 78 56 34 12 となるリトルエンディアン
		if ( _interlockedbittestandset((long*)hPtr, 0)==0 ) break;	// 最下位ビットを立てて、元のビットをチェック。とにかく1にする。0から1に出来たら成功。 
		while ((*hPtr) & 1) {}	// 偶数になるまで待つ
	}
#else
	__asm
	{
		mov     ecx, hPtr
   L1:  mov     al, BYTE PTR [ecx]			// まず偶数かどうかを確認。mov al, BYTE PTR [hPtr]	この代入なしだと固まる・・・。
		test    al, 1						// AND を取ってフラグだけを変化
		jnz     L1
		mov     ah, al
		inc     ah
		lock cmpxchg BYTE PTR [ecx], ah		// al と [ecx] を比較して等しい場合は ah を[ecx]に。ZFセット。
											// 等しくない場合はセットされずにZFをクリアして al に値が入る。
		jnz     L1
	}	
#endif
}

inline void UnLockChar(volatile char *hPtr)
{
//	if ( ((*hPtr) & 1) == 0 ) { PRT("lockfree Err=%d\n",(*hPtr)); debug(); }
	(*hPtr)++;
/*	(*hPtr) = 0;
	__asm
	{
//		mov     ecx, hPtr
//      mov  BYTE PTR [ecx], 0	
//		inc  BYTE PTR [ecx]	
	}
*/
}
#else 

// char1バイトでロックをかけると？（ハッシュサイズを32byteに収めるため）
inline void LockChar(volatile char *hPtr)
{
#ifdef SMP_LOCK_COUNT		// ロックが衝突した回数を数える場合（デバグ用）
	lock_all++;
	if ( *hPtr != 0 ) { lock_counter++; }	// デバグ用。
#endif		

#ifdef SMP_LOCK_INTERLOCK
	for (;;) {
		if ( _interlockedbittestandset((long*)hPtr, 0)==0 ) break;
		while (*hPtr);
	}
#else

	__asm
	{
        xor     eax, eax
		mov     ecx, hPtr
   la:  mov     al, 1
        xchg    al, BYTE PTR [ecx]
        test    eax, eax
        jz      end
   lb:
#ifdef SMP_HT
		pause
#endif
	    mov     al, BYTE PTR [ecx]
        test    eax, eax
        jz      la
        jmp     lb
   end:
	}
#endif
}

#define UnLockChar(v)    ((v)[0] = 0)		// hash table専用のchar1バイトのアンロック YSSで追加

#endif

#endif


inline void Pause()
{
#ifdef SMP_HT
    __asm {
       pause			// スレッド待機中にもpauseを呼ぶ
     }
#else
  #ifdef SMP_PAUSE_SLEEP
	Sleep(0);	// Sleep(0)の方が2CPUで8CPUSとかを動かした場合、10%ぐらい優秀な感じ。Sleep(0) をループさせると結局ビジーループになる。
//	{ extern void PassWindowsSystemSub(); PassWindowsSystemSub(); }
//	_mm_pause();
  #endif
#endif
}


#ifdef SMP_CRITICALSECTION	// lock()にcritical sectionを使う場合。

//inline void LockY(lock_t v) { EnterCriticalSection(&v); }	// この書き方だと構造体のコピーが渡されるのでダメ
#  define LockY(v)      ( EnterCriticalSection(&v) )

#else

inline void LockY(volatile int *hPtr)
{
#ifdef SMP_LOCK_INTERLOCK
	for (;;) {
		if ( _interlockedbittestandset((long*)hPtr, 0)==0 ) return;
		while (*hPtr);
	}
#else

    __asm
      {
        mov     ecx, hPtr
   la:  mov     eax, 1
        xchg    eax, [ecx]
        test    eax, eax
        jz      end
   lb:
#ifdef SMP_HT
		pause
#endif
        mov     eax, [ecx]
        test    eax, eax
        jz      la
        jmp     lb
   end:
      }
#endif
}

#endif


#endif	// windows
#endif	// SMP code
#endif	//]] INCLUDE__GUARD
