/* Chapter 9. statsMX_ST.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20
#define CACHE_LINE_SIZE 64

/* Usage: statsMX_ST nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

DWORD WINAPI Worker(void*);
int workerDelay = DELAY_COUNT;

__declspec(align(CACHE_LINE_SIZE))
typedef struct _THARG {
	HANDLE hMutex;
	int threadNumber;
	unsigned int tasksToComplete;
	unsigned int tasksComplete;
} THARG;

HANDLE hSem;	 // hSem: Throttling semaphore를 위한 핸들

int _tmain(int argc, LPTSTR argv[])
{
	INT nThread, iThread, maxthread; // maxthread: Semaphore가 지닐 수 있는 최대 카운트 값
	HANDLE* hWorkers, hMutex;
	unsigned int tasksPerThread, totalTasksComplete;
	THARG** pThreadArgsArray, * pThreadArg;
	BOOL traceFlag = FALSE;
	SYSTEM_INFO SysInfo; // SysInfo: 시스템 정보(주로 CPU와 메모리)에 관련된 정보를 담는 변수

	if (argc < 3) {
		_tprintf(_T("Usage: statsMX_ST nThread ntasks [trace]\n"));
		return 1;
	}
	_tprintf(_T("statsMX_ST %s %s %s\n"), argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));

	nThread = _ttoi(argv[1]);
	tasksPerThread = _ttoi(argv[2]);
	if (argc >= 4) workerDelay = _ttoi(argv[3]);
	traceFlag = (argc >= 5);

	/* Create the throttling semaphore. Initial and max counts are set to the
	   number of processors, except on a single-processor system, it's set to
	   the number threads, so there is no throttling
	 */
	 // GetSystemInfo : 컴퓨터 시스템의 시스템 정보를 알아내는 함수, 인자는 SYSTEM_INFO 구조체 주소, return value는 없다. (SysInfo에 값이 저장됨)
	GetSystemInfo(&SysInfo);
	maxthread = nThread; // 인자로 입력받은 스레드 개수를  maxthread에 할당
	// SysInfo 시스템에서 프로세서 갯수가 1개 보다 크면  maxthread를 시스템의 프로세서 개수로 할당한다. 
	if (SysInfo.dwNumberOfProcessors > 1) maxthread = SysInfo.dwNumberOfProcessors;

	// CreateSemaphore : 세마포어 생성
	// throttling semaphore를 사용하여 임계영역에 진입할 수 있는 스레드의 개수를 세마포어를 이용해 제한함
	// 1. lInitialCount:  Semaphore 의 생성 시 초기 값(count 값)을 설정하게 된다. 이 값은 0 이상이고 lMaximumCount 에 전달되는 값보다 같거나 작아야 한다.
	// 2. lMaximumCount : Semaphore가 지닐 수 있는 최대 카운트 값을 결정한다. 따라서 Semaphore가 지닐수 있는 값의 범위는 0이상 lMaximumCount 이하가 된다. 1을 최대 값으로 설정하게 되면 바이너리 Semaphore(Semaphore 가 0 혹은  1인 Semaphore)가 된다.
	// 3. lpName: 생성되는 SEmaphore에 이름을 줄 경우 사용되는 인자이다. NULL 포인터를 전달할 경우 이름이 없는 Semaphore를 생성하게 된다.
	// return value : 성공 시 생성된 Semaphore 오브젝트의 핸들, 실패 시 NULL 리턴
	hSem = CreateSemaphore(NULL, maxthread, maxthread, NULL);
	if (NULL == hSem) // 세마포어 생성 실패시 에러 출력, exitCode = 1
		ReportError(_T("Failed creating semaphore."), 1, TRUE);
	/* Create the mutex */

	// 위의 세마포어는 throttling용이므로 따로 mutex를 생성해주어야함
	// 자세한 설명은 statsmx 코드에서 했습니다!
	if ((hMutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
		ReportError(_T("Failed creating mutex."), 1, TRUE);

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
		pThreadArg->hMutex = hMutex;
		hWorkers[iThread] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Worker, pThreadArg, 0, NULL);
		if (hWorkers[iThread] == NULL)
			ReportError(_T("Cannot create consumer thread"), 4, TRUE);
	}

	/* Wait for the threads to complete */
	for (iThread = 0; iThread < nThread; iThread++)
		WaitForSingleObject(hWorkers[iThread], INFINITE);
	CloseHandle(hMutex);
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


	// WaitForSingleObject:  
	// 세마포어를 하나 얻어서 들어감, 이 때 WaitForSingleObject에서 세마포어 생성시 지정했던 lInitialCount을 1 감소시킨다.
	// dwMilliseconds는 INFINITE로 설정하여 접근 가능할 때 까지 무한대기한다.
	// throttle 세마포어: 임계영역이나 mutex에서 경쟁하는 스레드가 줄어들면 동기화 오버헤드가 커지는 것을 방지하기 위해 사용.
	// 0 보다 큰 경우에는 signaled state
	// 0 인 경우 non-signaled state (일반적으로 1이상으로 값을 설정, 0일 경우 Lock)
	// 즉, Semaphore 카운트가 1 이상인 경우 Semaphore를 얻을 수 있음
	WaitForSingleObject(hSem, INFINITE);

	// 완료한 태스크가 완료해야할 태스크보다 적으면 계속 루프 돌기
	while (threadArgs->tasksComplete < threadArgs->tasksToComplete) {
		delay_cpu(workerDelay);
		WaitForSingleObject(threadArgs->hMutex, INFINITE);
		(threadArgs->tasksComplete)++;
		ReleaseMutex(threadArgs->hMutex);
	}

	// ReleaseSemaphore: Semaphore 오브젝트의 반환, 사용이 끝난 세마포어의 Count 값을 증가시킨다.
	// 1. hSemaphore:  반환하고자 하는 Semaphore의핸들을 전달한다.
	// 2. lReleaseCount: 반환한다는 것은 Semaphore의 카운트를 증가시킨다는 것을 의미한다.  lReeaseCount 는 증가시킬 크기를 전달한다. 일반적으로 1로 전달한다.
			//	 만약에 Semaphore가 지닐 수 있는 최대 카운트(Semaphore 생성 시 결정)를 넘겨서 증가시킬 것을 요구하는 경우 카운트는 변경되지 않고 FALSE 만 리턴된다.
	// 3. lpPreviousCount: 함수 호출로 변경되기 전의 카운트 값을 저장할 변수의 포인터를 전달한다. 필요없다면 NULL 포인터를 전달하면 된다.
	// return value : 성공 시 TRUE, 실패 시 FALSE 리턴
	ReleaseSemaphore(hSem, 1, NULL);

	return 0;
}

