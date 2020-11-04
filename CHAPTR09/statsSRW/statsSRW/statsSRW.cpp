/* Chapter 9. statsSRW.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20
#define CACHE_LINE_SIZE 64

/* Usage: statsSRW nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

DWORD WINAPI Worker(void*);
int workerDelay = DELAY_COUNT;

__declspec(align(CACHE_LINE_SIZE))
typedef struct _THARG {
	/*
	* 슬림 리더 라이터 락 - Slim Reader Writer Lock (SRW Lock)
	- 기존 CriticalSection 비해 조금 더 적은 메모리 사용과 빠른 수행 속도를 가진다. (CRITICAL_SECTION와 달리 SRWLOCK은 공유 자원을 읽기만 하는 스레드와 값을 수정하려고 하는 스레드를 구분해서 관리한다)
	- Reader 쓰레드와 Writer 쓰레드가 완전히 분리될 수 있을 경우 사용될 수 있다.
	- Reader 쓰레드와 Writer 쓰레드가 분리되어 있는 경우 아래와 같이 두 가지 모드로 SRW 락을 사용할 수 있다.
		- Shared mode : 복수의 쓰레드가 read-only로 접근시 훨씬 더 강력한 성능 발휘
		- Exclusive mode : 복수의 쓰레드가 읽고 쓸 때 사용하는 모드

	- 해당 statsSRW 코드에서는 Exclusive mode를 사용한다.

	<진입할때 다음 함수 사용>
	void AcquireSRWLockExclusive(PSRWLOCK SRWLock);

	<해제시 다음 함수 사용>
	void ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
	*/
	SRWLOCK SRWL;
	int threadNumber;
	unsigned int tasksToComplete;
	unsigned int tasksComplete;
} THARG;

int _tmain(int argc, LPTSTR argv[])
{
	INT nThread, iThread;
	HANDLE* hWorkers;
	SRWLOCK srwl; // SRWLOCK 변수 선언
	unsigned int tasksPerThread, totalTasksComplete;
	THARG** pThreadArgsArray, * pThreadArg;
	BOOL traceFlag = FALSE;

	if (argc < 3) {
		_tprintf(_T("Usage: statsSRW nThread ntasks [trace]\n"));
		return 1;
	}
	_tprintf(_T("statsSRW %s %s %s\n"), argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));

	nThread = _ttoi(argv[1]);
	tasksPerThread = _ttoi(argv[2]);
	if (argc >= 4) workerDelay = _ttoi(argv[3]);
	traceFlag = (argc >= 5);

	/* Initialize the SRW lock */
	InitializeSRWLock(&srwl); // SRWLock  초기화 (인자는 SRWLock 에 대한 포인터)

	hWorkers = (HANDLE*)malloc(nThread * sizeof(HANDLE));
	pThreadArgsArray = (THARG**)malloc(nThread * sizeof(THARG*));

	if (hWorkers == NULL || pThreadArgsArray == NULL)
		ReportError(_T("Cannot allocate working memory for worker handles or argument array."), 2, TRUE);

	for (iThread = 0; iThread < nThread; iThread++) {
		/* Fill in the thread arg */
		pThreadArg = (pThreadArgsArray[iThread] = (THARG*)_aligned_malloc(sizeof(THARG), CACHE_LINE_SIZE));
		if (NULL == pThreadArg)
			ReportError(_T("Cannot allocate memory for a thread argument structure."), 3, TRUE);
		pThreadArg->threadNumber = iThread;
		pThreadArg->tasksToComplete = tasksPerThread;
		pThreadArg->tasksComplete = 0;
		pThreadArg->SRWL = srwl; // SRWLock 구조체 srwl를 pThreadArg->SRWL에 할당
		hWorkers[iThread] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Worker, pThreadArg, 0, NULL);
		if (hWorkers[iThread] == NULL)
			ReportError(_T("Cannot create consumer thread"), 4, TRUE);
	}

	/* Wait for the threads to complete */
	for (iThread = 0; iThread < nThread; iThread++)
		WaitForSingleObject(hWorkers[iThread], INFINITE);

	free(hWorkers);
	_tprintf(_T("Worker threads have terminated\n"));
	totalTasksComplete = 0;
	for (iThread = 0; iThread < nThread; iThread++) {
		pThreadArg = pThreadArgsArray[iThread];
		if (traceFlag) _tprintf(_T("Tasks completed by thread %5d: %6d\n"), iThread, pThreadArg->tasksComplete);
		totalTasksComplete += pThreadArg->tasksComplete;
		_aligned_free(pThreadArg);
	}
	free(pThreadArgsArray);

	if (traceFlag) _tprintf(_T("Total work performed: %d.\n"), totalTasksComplete);

	return 0;
}

DWORD WINAPI Worker(void* arg)
{
	THARG* threadArgs;
	int iThread;

	threadArgs = (THARG*)arg;
	iThread = threadArgs->threadNumber;

	while (threadArgs->tasksComplete < threadArgs->tasksToComplete) {
		delay_cpu(workerDelay);
		AcquireSRWLockExclusive(&(threadArgs->SRWL)); // 쓰기용 lock을 얻으면 lock을 해제할때까지 다른 스레드가 공유자원에 접근할 수 없다.
		(threadArgs->tasksComplete)++; // lock 안에서 완료 태스크를 1 증가한다.
		ReleaseSRWLockExclusive(&(threadArgs->SRWL)); // 쓰기용 lock을 해제한다.
	}

	return 0;
}

