/********************************************************************/
/*   Chapter 9
	 Program to test the various Windows synchronization techniques for
	 mutual exclusion performance impact. Critical Sections, mutexes,
	 Slim Read/Write Locks are compared, in terms of performance, in a very simple fashion.
	 Nearly all the factors that can impact threaded performance
	 are represented here in a simplified way so that you can
	 study the plausibility of various designs.
		Number of threads
		Use of mutexes or critical sections
		Nested resource acquisition
		CPU and I/O intensity
		Control of the number of active threads

	 Usage:
	 TimeMutualExclusion Which [Depth [Delay [NumThreads [SleepPoints [NumThActive [SharedPercent]]]]]]

		Which:	1 - Test Critical Sections
				2 - Test nested Critical Sections
				3 - Test Mutexes
				4 - Test Slim Reader/Writer locks
				CONCEPT: We will determine the relative peformance
						charateristics of CSs, mutexes, and SRW locks, especially the
						amount of time they spend in the kernel. Testing
						two distinct nested CSs will determine if there is
						a linear (doubling) or nonlinear performance impact.

		Depth:	1 (default) or more for the number of recursive enter calls followed
						by the same number of leave calls
						Can be 0 to show effect of no synchronization calls
						Assumed 1 when Which == 4
				CONCEPT: is the performance impact linear with depth?

		Delay:	0 (default) or more for a delay factor to waste CPU time
					after all nested enters and before the nested leaves.
				CONCEPT: A large delay is indicative of CPU-intensive threads.
					A CPU-Intensive thread may have its priority increased but
					it might also be prempted for another thread.

		NumThreads:	Number of concurrent threads contending for the
					lock. Must be between 1 and
					64 (the max for WaitForMultipleObjects).
					4 is the default value.
				CONCEPT: Determine if total elapsed time is linear with
					the number of threads. A large number of threads
					contending for the same resources might degrade
					performance non-linearly.

		SleepPoints:	0	[Default] Never sleep, hence do not yield CPU
						1	Sleep(0) after obtaining all resources, yielding
							the CPU while holding the resources
						2	Sleep(0) after releasing all resources, yielding
							the CPU without holding any resources
						3	Sleep(0) at both points
				CONCEPT: Yielding the CPU shows the best-case effect on a
					thread that performs I/O or blocks for other reasons.

		NumThActive: Number of activated threads. NumThreads is the default.
					All other threads are waiting on a semaphore. 0 means
					no limit.
				CONCEPT: Performance may improve if we limit the number of
					concurrently active threads contending for the same
					resources. This also has the effect of serializing
					thread execution, esp with low values.

		SharedPercent: Ignored if Which != 4 (Slim Reader/Writer locks)
					Default 0 (all locks are exclusive). Lock type is
					determined randomly.

	  ADDITIONAL TESTING CONCEPT: Determine if performance varies depending
	  upon operation in the foreground or background.

  Author: Johnson (John) M. Hart. April 23, 1998. Extended May 10, 1998,
		Updated May 31, 2009.
		Based on an idea from David Poulton, with reuse of a small
		amount of code
	Build as a MULTITHREADED CONSOLE APPLICATION.
*/
/********************************************************************/

#include "Everything.h"
#include <VersionHelpers.h>


#define ITERATIONS 10000000 // mutual exclusion test에서 반복시키는 횟수를 10000000으로 정의

#define THREADS 4 // 스레드를 4개로 정의 (최대 64로 지정할 수 있음)

DWORD WINAPI CrSecProc(LPVOID); // Critical Section을 위한 위한 함수 CrSecProc 선언
DWORD WINAPI CrSecProcA(LPVOID); // Nested Critical Section 을 위한 함수 CrSecProcA 선언
DWORD WINAPI MutexProc(LPVOID); // mutex를 위한 함수 MutexProc 선언
DWORD WINAPI SRWLockProc(LPVOID); // Slim Reader Writer locks 를 위한 함수 SRWLockProc 선언

int DelayFactor = 0, Which = 1, Depth = 1, NumThreads = THREADS, SleepPoints = 0, SharedPercent = 0; 
int NumThActive; // active 스레드 수

HANDLE hMutex; // mutex 핸들 선언
HANDLE hSem;	 // active 스레드를 제어하는 세마포어 핸들 선언
CRITICAL_SECTION hCritical, hCriticalA;	 // 임계영역을 위한 변수 선언
SRWLOCK hSRWL; // 슬림 리더 라이터 락 변수 선언

/********************************************************************/
/*  Time wasting function. Waste a random amount of CPU time.       */
/********************************************************************/

// WasteTime 함수 정의
void WasteTime(DWORD DelayFactor)
{
	// cpu의 랜덤한 양을 사용함
	int Delay, k, i; 
	if (DelayFactor == 0) return; // DelayFactor가 0이면 delay안한 것으로 생각하여 바로 리턴함
	Delay = (DWORD)(DelayFactor * (float)rand() / RAND_MAX); // 랜덤값을 Delay 변수에 할당한다.
	for (i = 0; i < Delay; i++) k = rand()*rand(); /* Waste time */ // cpu 시간을 사용함

	return;
}

/************************************************************************/
/*  For each mutual exclusion algorithm create the threads and wait for */
/*	them to finish														*/
/************************************************************************/
int _tmain(int argc, LPTSTR argv[])
{
	HANDLE *hThreadHandles; // 스레드 핸들에 대한 array
	DWORD ThreadID;	// 스레드 id
	int iTh; // 몇번째 스레드인지 나타내는 변수

	FILETIME FileTime;	 // 파일 시간을 나타내는 변수
	SYSTEMTIME SysTi; // 시스템 시간을 나타내는 변수

	/*	TimeMutex Which [Depth [Delay [NumThreads [SleepPoints]]]] */

		/*  Randomly initialize the random number seed */
	GetSystemTime(&SysTi); // 시스템 시간을 얻어서 SysTi변수에 넣음
	SystemTimeToFileTime(&SysTi, &FileTime); // 시스템 시간을 파일 시간으로 변환
	srand(FileTime.dwLowDateTime); // FileTime의 dwLowDateTime을 바탕으로 random 값 생성

	if (argc >= 4) DelayFactor = _ttoi(argv[3]);  // 인자 수가 4개 이상일 경우 4번째 값인 argv[3]을 DelayFactor로 사용 (0부터 시작)
	if (argc >= 3) Depth = _ttoi(argv[2]); // 인자 수가 3개 이상일 경우 argv[2]를 Depth로 사용
	if (argc >= 2) Which = _ttoi(argv[1]); // 인자 수가 2개 이상일 경우 argv[1]을 Which로 사용
	if (argc >= 5) NumThreads = _ttoi(argv[4]); // 인자 수가 5개 이상일 경우 argv[4]를 스레드 개수로 사용
	if (argc >= 6) SleepPoints = _ttoi(argv[5]); // 인자 수가 6개 이상일 경우 argv[5]를 SleepPoints로 사용
	if (argc >= 7) NumThActive = max(_ttoi(argv[6]), 0); // 인자 수가 7개 이상일 경우 argv[6]를 NumThActive로 사용 (단, 0보다 작으면 0으로 할당)
	else NumThActive = NumThreads; // 단, 인자 수가 7개 미만이면 NumThActive를 스레드 수로 할당함

	if (4 == Which && argc >= 8) { // 인자 수가 8개 이상이고 Which가 4이면 
		SharedPercent = min(_ttoi(argv[7]), 100); // SharedPercent를 argv[7]로 할당하나 이 값이 100 보다 작으면 100으로 할당한다.
		if (!IsWindows8OrGreater()) // 윈도우 운영체제 버전이 8이상이 아닐경우 에러 출력, exit code : 1 
			ReportError(_T("This program requires Windows NT 6.0 or greater"), 1, FALSE);
	}

	// Which가 1, 2, 3, 4가 아닐경우 에러 출력
	if (Which != 1 && Which != 2 && Which != 3 && Which != 4) {
		_tprintf(_T("The first command line argument must be 1, 2, 3, 4.\n"));
		return 1;
	}

	// 스레드 수가 1보다 작거나 MAXIMUM_WAIT_OBJECTS보다 크면 에러 출력
	if (!(NumThreads >= 1 && NumThreads <= MAXIMUM_WAIT_OBJECTS)) {
		_tprintf(_T("Number of threads must be between 1 and %d\n"), MAXIMUM_WAIT_OBJECTS);
		return 2;
	}

	// active 스레드 수 계산 (NumThActive과 NumThreads 중에 작은 것으로 할당)
	NumThActive = min(NumThActive, NumThreads);

	// SleepPoints가 0보다 작거나 3보다 큰 경우 SleepPoints를 0으로 할당
	if (!(SleepPoints >= 0 && SleepPoints <= 3)) SleepPoints = 0;

	// 예외처리한 인자들을 출력한다.
	_tprintf(_T("Which = %d, Depth = %d, Delay = %d, NumThreads = %d, SleepPoints = %d, NumThActive = %d\n"),
		Which, Depth, DelayFactor, NumThreads, SleepPoints, NumThActive);

	// Which가 4일 경우 SharedPercent를 출력
	if (4 == Which)
		_tprintf(_T("Percent of acquistions which are shared = %d.\n"), SharedPercent);

	if (3 == Which) hMutex = CreateMutex(NULL, FALSE, NULL); 
	// Which==3일 경우 mutex 생성 
	// <파라미터>
	// 1. lpMutexAttributes : NULL 로 설정했음으로 child process로 상속될 수 없음
	// 2. bInitialOwner : mutex를 생성한 스레드가 소유권을 갖는지 여부 (False: 스레드 호출하는 순간 소유권을 획득하지 못한다.(TRUE: 스레드 호출하는 순간 소유권획득))
	// 3. lpName: mutex 이름을 지정 (NULL임으로 이름 없이 생성) 
	// return : 새롭게 생성된 mutex object의 handle

	if (1 == Which || 2 == Which) InitializeCriticalSection(&hCritical); // which가 1또는 2면 hCritical 임계영역을 초기화

	if (2 == Which) InitializeCriticalSection(&hCriticalA); // which가 2면 hCriticalA 임계영역을 초기화
	if (4 == Which) InitializeSRWLock(&hSRWL); // Which가 4면 SRWLock 초기화

	hSem = CreateSemaphore(NULL, NumThActive, max(NumThActive, 1), NULL);
	// CreateSemaphore :  세마포어 생성
	// throttling semaphore를 사용하여 임계영역에 진입할 수 있는 스레드의 개수를 세마포어를 이용해 제한함
	// 1. lInitialCount:  Semaphore 의 생성 시 초기 값(count 값)을 설정하게 된다. 이 값은 0 이상이고 NumThActive 에 전달되는 값보다 같거나 작아야 한다.
	// 2. lMaximumCount : Semaphore가 지닐 수 있는 최대 카운트 값을 결정한다. 따라서 Semaphore가 지닐수 있는 값의 범위는 0이상 lMaximumCount 이하가 된다. 1을 최대 값으로 설정하게 되면 바이너리 Semaphore(Semaphore 가 0 혹은  1인 Semaphore)가 된다.
	// 3. lpName: 생성되는 Semaphore에 이름을 줄 경우 사용되는 인자이다. NULL 포인터를 전달할 경우 이름이 없는 Semaphore를 생성하게 된다.
	// return value : 성공 시 생성된 Semaphore 오브젝트의 핸들, 실패 시 NULL 리턴

	// throttling 세마포어 생성에 실패할 경우 에러 출력
	if (hSem == NULL) {
		_tprintf(_T("Cannot create throttle semaphore. %d\n"), GetLastError());
		return 3;
	}

	/*	Create all the threads, then wait for them to complete */


	hThreadHandles = (HANDLE *)malloc(NumThreads * sizeof(HANDLE)); // 스레드 수만큼 스레드 핸들에 메모리 할당
	
	if (hThreadHandles == NULL) { // 메모리 할당 실패시 에러 출력
		_tprintf(_T("Can not allocate memory for thread handles.\n"));
		return 4;
	}

	// 스레드 개수만큼 반복
	for (iTh = 0; iTh < NumThreads; iTh++)
	{

		// _beginthreadex: 스레드 생성 (여기서는 hWorkers 스레드를 생성한다.)
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달 (변수 Which 값 에 따라서 다르게 지정했음)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정 (NULL로 지정)
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 ( 0: 즉시 실행 )
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (ThreadID: 식별자 아이디, NULL: 스레드 식별자 사용 하지 않음 )
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		if ((hThreadHandles[iTh] =
			(HANDLE)_beginthreadex
			(NULL, 0, Which == 1 ? (_beginthreadex_proc_type)CrSecProc : Which == 2 ? (_beginthreadex_proc_type)CrSecProcA :
				Which == 3 ? (_beginthreadex_proc_type)MutexProc : Which == 4 ? (_beginthreadex_proc_type)SRWLockProc : NULL,
				NULL, 0, (unsigned int *)&ThreadID)) == NULL)
		{

			// 스레드 생성 실패시 에러 출력
			_tprintf(_T("Error Creating Thread %d\n"), iTh);
			return 1;
		}
	}
	// WaitForMultipleObjects
		// 여러개의 스레드가 종료되길 대기한다.
		//  -> 기다리기로 한 모든 객체가 신호 상태에 놓일 때까지 기다린다. 대기 시간을 정할 수도 있다.
		// <파라미터>
		//  1. DWORD nCount: 배열에 저장되어 있는 핸들 개수를 전달한다. (NumThreads)
		//  2. const HANDLE *lpHandles: 핸들을 저장하고 있는 배열의 주소 정보를 전달한다. (hThreadHandles)
		//  3. BOOL bWaitAll: 관찰 대상이 모두 Signaled 상태가 되기를 기다리고자하는지 아니면 하나라도 Signaled 상태가 되면 반환할것인지 결정한다. (TRUE로 지정 -  lpHandles의 모든 handle의 signal state가 될때까지 대기)
		//  4. DWORD dwMilliseconds: 커널오브젝트가 Singaled상태가 될때까지 기다릴 수 있는 최대 시간. 만약 상수 INFINITE를 인자로 전달하면 커널 오브젝트가 Signaled 상태가 될 때까지 반환하지 않고 무한정 대기함
		// <return>
		// - WAIT_FAILED : 대기 동작이 실패했음 ,GetLastError을 통해 자세한 정보를 얻음
		// - WAIT_OBJECT_0 : 커널 객체가 주어진 시간 간격 안에 시그널된 상태로 변함
		// - WAIT_TIMEOUT: 커널 객체가 시그널되기 전에 주어진 시간 초과 간격이 모두 지났음
		// - WAIT_ABANDONED_0 : 커널 객체가 뮤텍스를 사용했고, 뮤텍스를 소유한 스레드가 자발적으로 뮤텍스의 소유권을 하제하지 않고 종료
	WaitForMultipleObjects(NumThreads, hThreadHandles, TRUE, INFINITE);

	for (iTh = 0; iTh < NumThreads; iTh++)	CloseHandle(hThreadHandles[iTh]); // 스레드 개수만큼 반복하여 모든 스레드 핸들을 닫는다
	free(hThreadHandles); // hThreadHandles 자원 해제

	if (Which == 1 || Which == 2) DeleteCriticalSection(&hCritical); // Which가 1 또는 2면 임계영역  hCritical 삭제 
	if (Which == 2) DeleteCriticalSection(&hCriticalA); // Which가 2면 임계영역 hCriticalA 삭제
	if (Which == 3) CloseHandle(hMutex); // Which가 3이면 mutex 핸들 닫기 
	return 0;
}

/********************************************************************/
/*	Simple thread functions to enter and leave lock (CS or mutex or SRW)
	Each thread must wait on the gating semaphore before starting
	and releases the semaphore upon completion				*/
	/********************************************************************/

// mutex를 위한 함수
DWORD WINAPI MutexProc(LPVOID Nothing)
{
	int i, k;

	if (NumThActive > 0) WaitForSingleObject(hSem, INFINITE); 
	// NumThActive가 0 보다 클 경우 세마포어를 하나 얻어서 들어가며,
	// 이 때 WaitForSingleObject에서 세마포어 생성시 지정했던 lInitialCount을 1 감소시킨다.
	// dwMilliseconds는 INFINITE로 설정하여 접근 가능할 때 까지 무한대기한다.
	// throttle 세마포어: 임계영역이나 mutex에서 경쟁하는 스레드가 줄어들면 동기화 오버헤드가 커지는 것을 방지하기 위해 사용.
	// 0 보다 큰 경우에는 signaled state
	// 0 인 경우 non-signaled state (일반적으로 1이상으로 값을 설정, 0일 경우 Lock)
	// 즉, Semaphore 카운트가 1 이상인 경우 Semaphore를 얻을 수 있음

	// 앞에서 지정한 반복 횟수 ITERATIONS만큼 반복함
	for (i = 0; i < ITERATIONS; i++)
	{
		for (k = 0; k < Depth; k++) /* Depth may be 0 */
			//  hMutex가 signaled state가 될때까지 기다림 
			// 인자 dwMilisecondes를 INFINITE로 설정했음으로 무한정 대기 
			// mutext는 WaitFoSingleObject 함수가 반환될 때 자동으로 non-signaled 상태가 된다. (auto-reset 이라서) 
			// 이 WaitForSingleObject 함수를 지나면 mutex는 non-signaled state가 된다.
			if (WaitForSingleObject(hMutex, INFINITE) == WAIT_FAILED) {
				_tprintf(_T("Wait for Mutex failed: %d\n"), GetLastError());
				break;
			}
		if (SleepPoints % 2 == 1) Sleep(0); // SleepPoints가 홀수면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
		WasteTime(DelayFactor); // Critical Section 임계영역 (mutex를 잡음)
		for (k = 0; k < Depth; k++) ReleaseMutex(hMutex); // 호출 스레드가 점유하던 mutext 객체를 release (non-signaled state에서 signaled state로 바뀜)
		if (SleepPoints >= 2) Sleep(0); //  SleepPoints가 2 이상이면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
	}
	if (NumThActive > 0) ReleaseSemaphore(hSem, 1, (LPLONG)&i);
	// NumThActive가 0보다 크면,  ReleaseSemaphore: Semaphore 오브젝트의 반환, 사용이 끝난 세마포어의 Count 값을 증가시킨다.
	// 1. hSemaphore:  반환하고자 하는 Semaphore의핸들을 전달한다.
	// 2. lReleaseCount: 반환한다는 것은 Semaphore의 카운트를 증가시킨다는 것을 의미한다.  lReeaseCount 는 증가시킬 크기를 전달한다. 일반적으로 1로 전달한다.
			//	 만약에 Semaphore가 지닐 수 있는 최대 카운트(Semaphore 생성 시 결정)를 넘겨서 증가시킬 것을 요구하는 경우 카운트는 변경되지 않고 FALSE 만 리턴된다.
	// 3. lpPreviousCount: 함수 호출로 변경되기 전의 카운트 값을 저장할 변수의 포인터를 전달한다. 필요없다면 NULL 포인터를 전달하면 된다.
	// return value : 성공 시 TRUE, 실패 시 FALSE 리턴
	return 0;
}

// Critical Section을 위한 함수
DWORD WINAPI CrSecProc(LPVOID Nothing)
{
	int i, k;

	if (NumThActive > 0) WaitForSingleObject(hSem, INFINITE);
	// NumThActive가 0 보다 클 경우 세마포어를 하나 얻어서 들어가며,
	// 이 때 WaitForSingleObject에서 세마포어 생성시 지정했던 lInitialCount을 1 감소시킨다.
	// dwMilliseconds는 INFINITE로 설정하여 접근 가능할 때 까지 무한대기한다.
	// throttle 세마포어: 임계영역이나 mutex에서 경쟁하는 스레드가 줄어들면 동기화 오버헤드가 커지는 것을 방지하기 위해 사용.
	// 0 보다 큰 경우에는 signaled state
	// 0 인 경우 non-signaled state (일반적으로 1이상으로 값을 설정, 0일 경우 Lock)
	// 즉, Semaphore 카운트가 1 이상인 경우 Semaphore를 얻을 수 있음

	for (i = 0; i < ITERATIONS; i++)
	{
		for (k = 0; k < Depth; k++)
			EnterCriticalSection(&hCritical); // 임계영역으로 들어감
		if (SleepPoints % 2 == 1) Sleep(0); // SleepPoints가 홀수면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
		WasteTime(DelayFactor);  // Critical Section 임계영역
		for (k = 0; k < Depth; k++)
			LeaveCriticalSection(&hCritical); // 임계영역에서 빠져나온다.
		if (SleepPoints >= 2) Sleep(0);  //  SleepPoints가 2 이상이면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
	}
	if (NumThActive > 0) ReleaseSemaphore(hSem, 1, (LPLONG)&i);
	// NumThActive가 0보다 크면,  ReleaseSemaphore: Semaphore 오브젝트의 반환, 사용이 끝난 세마포어의 Count 값을 증가시킨다.
	// 1. hSemaphore:  반환하고자 하는 Semaphore의핸들을 전달한다.
	// 2. lReleaseCount: 반환한다는 것은 Semaphore의 카운트를 증가시킨다는 것을 의미한다.  lReeaseCount 는 증가시킬 크기를 전달한다. 일반적으로 1로 전달한다.
			//	 만약에 Semaphore가 지닐 수 있는 최대 카운트(Semaphore 생성 시 결정)를 넘겨서 증가시킬 것을 요구하는 경우 카운트는 변경되지 않고 FALSE 만 리턴된다.
	// 3. lpPreviousCount: 함수 호출로 변경되기 전의 카운트 값을 저장할 변수의 포인터를 전달한다. 필요없다면 NULL 포인터를 전달하면 된다.
	// return value : 성공 시 TRUE, 실패 시 FALSE 리턴
	return 0;
}

// Nested Critical Sections 를 위한 함수
DWORD WINAPI CrSecProcA(LPVOID Nothing)
{
	int i, k;

	if (NumThActive > 0) WaitForSingleObject(hSem, INFINITE);
	// NumThActive가 0 보다 클 경우 세마포어를 하나 얻어서 들어가며,
	// 이 때 WaitForSingleObject에서 세마포어 생성시 지정했던 lInitialCount을 1 감소시킨다.
	// dwMilliseconds는 INFINITE로 설정하여 접근 가능할 때 까지 무한대기한다.
	// throttle 세마포어: 임계영역이나 mutex에서 경쟁하는 스레드가 줄어들면 동기화 오버헤드가 커지는 것을 방지하기 위해 사용.
	// 0 보다 큰 경우에는 signaled state
	// 0 인 경우 non-signaled state (일반적으로 1이상으로 값을 설정, 0일 경우 Lock)
	// 즉, Semaphore 카운트가 1 이상인 경우 Semaphore를 얻을 수 있음


	for (i = 0; i < ITERATIONS; i++)
	{
		for (k = 0; k < Depth; k++) {
			EnterCriticalSection(&hCritical); // 임계영역 hCritical으로 들어감 
			EnterCriticalSection(&hCriticalA); // 임계영역 hCriticalA으로 들어감
		}
		if (SleepPoints % 2 == 1) Sleep(0);// SleepPoints가 홀수면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
		WasteTime(DelayFactor); // 임계영역
		for (k = 0; k < Depth; k++) {
			LeaveCriticalSection(&hCriticalA); // hCriticalA 임계영역에서 빠져나옴
			LeaveCriticalSection(&hCritical); // hCritical 임계영역에서 빠져나옴
		}
		if (SleepPoints >= 2) Sleep(0); //  SleepPoints가 2 이상이면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
	}
	if (NumThActive > 0) ReleaseSemaphore(hSem, 1, (LPLONG)&i);
	// NumThActive가 0보다 크면,  ReleaseSemaphore: Semaphore 오브젝트의 반환, 사용이 끝난 세마포어의 Count 값을 증가시킨다.
	// 1. hSemaphore:  반환하고자 하는 Semaphore의핸들을 전달한다.
	// 2. lReleaseCount: 반환한다는 것은 Semaphore의 카운트를 증가시킨다는 것을 의미한다.  lReeaseCount 는 증가시킬 크기를 전달한다. 일반적으로 1로 전달한다.
			//	 만약에 Semaphore가 지닐 수 있는 최대 카운트(Semaphore 생성 시 결정)를 넘겨서 증가시킬 것을 요구하는 경우 카운트는 변경되지 않고 FALSE 만 리턴된다.
	// 3. lpPreviousCount: 함수 호출로 변경되기 전의 카운트 값을 저장할 변수의 포인터를 전달한다. 필요없다면 NULL 포인터를 전달하면 된다.
	// return value : 성공 시 TRUE, 실패 시 FALSE 리턴
	return 0;
}

// SRWLock를 위한 함수
DWORD WINAPI SRWLockProc(LPVOID Nothing)
{
	int i;

	if (NumThActive > 0) WaitForSingleObject(hSem, INFINITE);
	// NumThActive가 0 보다 클 경우 세마포어를 하나 얻어서 들어가며,
	// 이 때 WaitForSingleObject에서 세마포어 생성시 지정했던 lInitialCount을 1 감소시킨다.
	// dwMilliseconds는 INFINITE로 설정하여 접근 가능할 때 까지 무한대기한다.
	// throttle 세마포어: 임계영역이나 mutex에서 경쟁하는 스레드가 줄어들면 동기화 오버헤드가 커지는 것을 방지하기 위해 사용.
	// 0 보다 큰 경우에는 signaled state
	// 0 인 경우 non-signaled state (일반적으로 1이상으로 값을 설정, 0일 경우 Lock)
	// 즉, Semaphore 카운트가 1 이상인 경우 Semaphore를 얻을 수 있음

	for (i = 0; i < ITERATIONS; i++)
	{
		// 참이거나 SharedPercent > 0 이고 100.0 * (float)rand()) / RAND_MAX 값이 SharedPercent큰 경우 Exclusive lock(베타 모드), 아니면 shared lock모드
		if (1 || SharedPercent > 0 && (100.0 * (float)rand()) / RAND_MAX > SharedPercent)
		{
			// Exclusive lock
			AcquireSRWLockExclusive(&hSRWL);  // 쓰기용 lock을 얻으면 lock을 해제할때까지 다른 스레드가 공유자원에 접근할 수 없다.
			if (SleepPoints % 2 == 1) Sleep(0); // SleepPoints가 홀수면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
			WasteTime(DelayFactor); // 임계영역
			ReleaseSRWLockExclusive(&hSRWL);  // 쓰기용 lock 해제
			if (SleepPoints >= 2) Sleep(0);  //  SleepPoints가 2 이상이면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
		}
		else {
			// Shared Lock
			AcquireSRWLockShared(&hSRWL);  // 읽기용  lock을 얻으면 lock을 해제할때까지 다른 스레드가 공유자원에 접근할 수 없다.
			if (SleepPoints % 2 == 1) Sleep(0); // SleepPoints가 홀수면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
			WasteTime(DelayFactor); // 임계영역
			ReleaseSRWLockShared(&hSRWL); // 읽기용 lock 해제
			if (SleepPoints >= 2) Sleep(0);  //  SleepPoints가 2 이상이면 Sleep (0) (자원을 보유하고 있는 CPU를 생산)
		}
	}
	if (NumThActive > 0) ReleaseSemaphore(hSem, 1, (LPLONG)&i);
	// NumThActive가 0보다 크면,  ReleaseSemaphore: Semaphore 오브젝트의 반환, 사용이 끝난 세마포어의 Count 값을 증가시킨다.
	// 1. hSemaphore:  반환하고자 하는 Semaphore의핸들을 전달한다.
	// 2. lReleaseCount: 반환한다는 것은 Semaphore의 카운트를 증가시킨다는 것을 의미한다.  lReeaseCount 는 증가시킬 크기를 전달한다. 일반적으로 1로 전달한다.
			//	 만약에 Semaphore가 지닐 수 있는 최대 카운트(Semaphore 생성 시 결정)를 넘겨서 증가시킬 것을 요구하는 경우 카운트는 변경되지 않고 FALSE 만 리턴된다.
	// 3. lpPreviousCount: 함수 호출로 변경되기 전의 카운트 값을 저장할 변수의 포인터를 전달한다. 필요없다면 NULL 포인터를 전달하면 된다.
	// return value : 성공 시 TRUE, 실패 시 FALSE 리턴
	return 0;
}
