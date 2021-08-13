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
// Win98�Ǥ�threadID���������ѿ�����ꤷ�ʤ��ȥ��顼�ˤʤ롣
// beginthread()�ϥ���åɥϥ�ɥ���֤����ɤ������ꡣ�ȤäƤϤ����ʤ���
*/

#ifdef SMP_CRITICALSECTION	// lock()��critical section��Ȥ���硣
   typedef CRITICAL_SECTION lock_t;
#  define LockInit(v)      ( InitializeCriticalSection(&v) )
#  define UnLock(v)        ( LeaveCriticalSection(&v)      )
#else
   typedef volatile int lock_t[1];
#  define LockInit(v)      ((v)[0] = 0)
#  define UnLock(v)        ((v)[0] = 0)
#endif


#ifdef SMP_LOCKCHAR_OFF	// �ϥå���˥�å��򤫤��ʤ���硣������7%®���ʤ롣


#else

//#define INLINE __forceinline

//#define SMP_LOCK_COUNT	// ��å������ͤ���������������ʥǥХ��ѡ�
#ifdef SMP_LOCK_COUNT	// ��å������ͤ���������������ʥǥХ��ѡ�
extern volatile int lock_all;		// 10000���1�٤��餤���ͤ��Ƥ��롣
extern volatile int lock_counter;
#endif

#define SMP_LOCK_FREE	// lockfree hash��read �Ǥϥ�å����ʤ�

#ifdef SMP_LOCK_FREE
inline void LockChar(volatile char *hPtr)
{
#ifdef SMP_LOCK_INTERLOCK
	for (;;) {
		// ��¤�Τ� char �ʤΤ� ����ǥ������¸�� x86�� 0x12345678 �� 78 56 34 12 �Ȥʤ��ȥ륨��ǥ�����
		if ( _interlockedbittestandset((long*)hPtr, 0)==0 ) break;	// �ǲ��̥ӥåȤ�Ω�Ƥơ����ΥӥåȤ�����å����Ȥˤ���1�ˤ��롣0����1�˽��褿�������� 
		while ((*hPtr) & 1) {}	// �����ˤʤ�ޤ��Ԥ�
	}
#else
	__asm
	{
		mov     ecx, hPtr
   L1:  mov     al, BYTE PTR [ecx]			// �ޤ��������ɤ������ǧ��mov al, BYTE PTR [hPtr]	���������ʤ����ȸǤޤ롦������
		test    al, 1						// AND ���äƥե饰�������Ѳ�
		jnz     L1
		mov     ah, al
		inc     ah
		lock cmpxchg BYTE PTR [ecx], ah		// al �� [ecx] ����Ӥ������������� ah ��[ecx]�ˡ�ZF���åȡ�
											// �������ʤ����ϥ��åȤ��줺��ZF�򥯥ꥢ���� al ���ͤ����롣
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

// char1�Х��Ȥǥ�å��򤫤���ȡ��ʥϥå��奵������32byte�˼���뤿���
inline void LockChar(volatile char *hPtr)
{
#ifdef SMP_LOCK_COUNT		// ��å������ͤ���������������ʥǥХ��ѡ�
	lock_all++;
	if ( *hPtr != 0 ) { lock_counter++; }	// �ǥХ��ѡ�
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

#define UnLockChar(v)    ((v)[0] = 0)		// hash table���Ѥ�char1�Х��ȤΥ����å� YSS���ɲ�

#endif

#endif


inline void Pause()
{
#ifdef SMP_HT
    __asm {
       pause			// ����å��Ե���ˤ�pause��Ƥ�
     }
#else
  #ifdef SMP_PAUSE_SLEEP
	Sleep(0);	// Sleep(0)������2CPU��8CPUS�Ȥ���ư��������硢10%���餤ͥ���ʴ�����Sleep(0) ��롼�פ�����ȷ�ɥӥ����롼�פˤʤ롣
//	{ extern void PassWindowsSystemSub(); PassWindowsSystemSub(); }
//	_mm_pause();
  #endif
#endif
}


#ifdef SMP_CRITICALSECTION	// lock()��critical section��Ȥ���硣

//inline void LockY(lock_t v) { EnterCriticalSection(&v); }	// ���ν������ȹ�¤�ΤΥ��ԡ����Ϥ����Τǥ���
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
