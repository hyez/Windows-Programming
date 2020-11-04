/* Chapter 9. statsMX.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20  // DELAY_COUNT 정의
#define CACHE_LINE_SIZE 64 // CACHE_LINE_SIZE 정의

/* Usage: statsMX nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

DWORD WINAPI Worker(void*); // Worker 스레드 함수
int workerDelay = DELAY_COUNT; // 전역변수 workerDelay

__declspec(align(CACHE_LINE_SIZE))
/* __declspec은 저장소 클래스 정보를 지정 하기 위한 MS 확장 문법이다.
   __declspec(align(#))를 사용하여 사용자 정의 데이터(예: 함수의 정적 할당 또는 자동 데이터)를 정확하게 제어가능
 */

// Worker가 인자를 넘기기 위한 변수, 즉, 각 스레드에게 넘겨줄 구조체 변수
typedef struct _THARG {
	HANDLE hMutex; // 한번에 한개만 들어가도록 mutex handle 지정
	int threadNumber; // 스레드 번호 (몇번째 스레드 인지)
	unsigned int tasksToComplete; // 해야할 TASK 개수
	unsigned int tasksComplete; // 완료한 TASK 개수
} THARG;

int _tmain(int argc, LPTSTR argv[])
{
	INT nThread, iThread; // nThread: 스레드 갯수 변수,  iThread: 스레드 id 변수 (몇번째 스레드인지)
	HANDLE* hWorkers, hMutex; // hWorkers: Worker 스레드 핸들(포인터), hMutex: MUTEX 핸들
	unsigned int tasksPerThread, totalTasksComplete; // taskPerThread: 스레드당 몇개의 task를 수행하냐, totalTasksComplete : 전체 스레드가 수행한 total task의 개수
	THARG** pThreadArgsArray, * pThreadArg; // 스레드 array, 스레드 인자 넘겨주는 포인터
	BOOL traceFlag = FALSE;  // traceFlag 변수의 초기값을 False로 할당

	if (argc < 3) { //인자 개수가 3보다 작으면 == 인자를 안 준 것이 있으면, 프로그램 끝낸다.
		_tprintf(_T("Usage: statsMX nThread ntasks [trace]\n"));
		return 1;
	}
	_tprintf(_T("%s %s %s %s\n"), argv[0], argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));  // argv가 4개 이상이면 arg[3] 으로 출력 아니면 ""
	/* 입력한 명령어 출력
	 1. argv[0] = statsMX
	 2. argv[1] = 스레드 개수
	 3. argv[2] = 스레드 당 태스크  수
	 4. argv[3]은 입력한 인자가 4개 이상일 경우(statsMX를 포함하여 4개를 모두 입력할 경우) arg[3]을 출력 */

	nThread = _ttoi(argv[1]); // _ttoi: 문자열을 정수로 바꾸기, 여기서는 argv[1]에 해당하는 스레드 개수를 숫자로 바꾼다.
	tasksPerThread = _ttoi(argv[2]); // 여기서는 argv[2]에 해당하는스레드 당 태스크  수를 숫자로 바꾼다.
	if (argc >= 4) workerDelay = _ttoi(argv[3]); // workerDelay의 default 값은 20이였는데, argv[3]을 입력했다면 argv[3] 인자로 이 값을 바꿀 수 있다.
	traceFlag = (argc >= 5); // argc가 5 이상이면 1 아니면 0
	// 즉, 다섯번째 인자를 입력받지 않으면 traceFlag는 0인데 다섯번째 인자를 commandline에서 입력받으면 traceFlag는 1이 됨
	// traceFlag: 각 스레드가 얼마나 일했는지 출력하는지, 다섯번째 인자를 통해 설정 가능

	// mutex hMutex 생성 
	// <파라미터>
	// 1. lpMutexAttributes : NULL 로 설정했음으로 child process로 상속될 수 없음
	// 2. bInitialOwner : mutex를 생성한 스레드가 소유권을 갖는지 여부 (False: 스레드 호출하는 순간 소유권을 획득하지 못한다.(TRUE: 스레드 호출하는 순간 소유권획득))
	// 3. lpName: mutex 이름을 지정 (NULL임으로 이름 없이 생성) 
	// return : 새롭게 생성된 mutex object의 handle

	/* Create the mutex */
	if ((hMutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
		ReportError(_T("Failed creating mutex."), 1, TRUE);


	hWorkers = (HANDLE*)malloc(nThread * sizeof(HANDLE)); // 스레드 핸들 hWorkers에 인자로 받은 스레드 개수 만큼 메모리 할당
	pThreadArgsArray = (THARG**)malloc(nThread * sizeof(THARG*));  // 위에서 선언한 각 스레드에게 넘겨줄 구조체 변수 THARG를 가리키는 포인터만큼 메모리를 할당 (THARG만큼 할당)
	
	// hWorkers, pThreadArgsArray의 메모리 할당을 둘중에 하나라도 실패시 에러 출력 
	// 즉, exitCode 2: hWorkers 또는 pThreadArgsArray의 메모리 할당 실패
	if (hWorkers == NULL || pThreadArgsArray == NULL)
		ReportError(_T("Cannot allocate working memory for worker handles or argument array."), 2, TRUE);

	// 인자로 입력 받은 스레드 개수 만큼 for 문을 반복
	for (iThread = 0; iThread < nThread; iThread++) {
		/* Fill in the thread arg */

		// _aligned_malloc: 
		// 1. size: 요청된 메모리 할당 크기 (THARG의 size)
		// 2. 정렬(alignment):  2의 정수 거듭제곱이어야 함 (CACHE_LINE_SIZE=64)
		// return value: 할당된 메모리 블록에 대한 포인터 이거나, 작업에 실패 한 경우 NULL.
		// CACHE_LINE_SIZE가 64바이트니까 스레드에 넘겨지는 인자들이 64바이트로 맞추어서 cache block을 다 차지하게 된다.
		// 즉, 스레드에 넘기는 인자 구조체 변수를 64바이트에 맞추고, 이것은 한 cache block에는 스레드 하나를 위한 인자 블록만 들어가도록 하게 함
		pThreadArg = (pThreadArgsArray[iThread] = (THARG*)_aligned_malloc(sizeof(THARG), CACHE_LINE_SIZE));

		// NULL를 리턴할 경우 작업에 실패했으므로 에러메세지 출력
		// exitCode 3:  pThreadArg 메모리 할당 실패
		if (NULL == pThreadArg) 	
			ReportError(_T("Cannot allocate memory for a thread argument structure."), 3, TRUE);
		pThreadArg->threadNumber = iThread;  // 몇번째 스레드인지 스레드 번호 할당
		pThreadArg->tasksToComplete = tasksPerThread; // 스레드당 처리할 수 있는 테스크 개수 할당 (예-256000)
		pThreadArg->tasksComplete = 0;  // 완료된 테스크는 0개로 할당
		pThreadArg->hMutex = hMutex; // 동기화시 상호배제에 필요한 mutex hMutex 할당

		// _beginthreadex: 스레드 생성 (여기서는 hWorkers 스레드를 생성한다.)
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 ( 0: 즉시 실행 )
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (NULL: 스레드 식별자 사용 하지 않음 )
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		hWorkers[iThread] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Worker, pThreadArg, 0, NULL);

		// 스레드 생성 실패 시(NULL), 에러 출력
		// exitCode 4: hWorker 스레드 생성 실패
		if (hWorkers[iThread] == NULL)
			ReportError(_T("Cannot create consumer thread"), 4, TRUE);
	}


	// 모든 스레드들이 완료될 때까지 대기
	// 인자로 입력 받은 스레드 개수 만큼 for 문을 반복
	/* Wait for the threads to complete */
	for (iThread = 0; iThread < nThread; iThread++)
		WaitForSingleObject(hWorkers[iThread], INFINITE); // 각 스레드의 hWorkers가 signaled state까지 대기, INFINITE를 설정하므로써 무한정대기한다.
	CloseHandle(hMutex); // hMutex handle 닫기
	free(hWorkers);  //hWorker 메모리 해제
	_tprintf(_T("Worker threads have terminated\n"));

	totalTasksComplete = 0;  // 완료된 태스크 수를 0으로 초기화 (처음엔 완료된 태스크가 없으므로)
	// 인자로 입력 받은 스레드 개수 만큼 for 문을 반복
	for (iThread = 0; iThread < nThread; iThread++) {
		pThreadArg = pThreadArgsArray[iThread]; // 각 스레드의 pThreadArgsArray를 pThreadArg에 할당

		// traceFlag가 1이면 몇번째 스레드가 몇개의 태스크를 완료했는지를 출력한다.
		if (traceFlag) _tprintf(_T("Tasks completed by thread %5d: %6d\n"), iThread, pThreadArg->tasksComplete);

		totalTasksComplete += pThreadArg->tasksComplete; // 모든 스레드가 완료한  total task 수를 담은 변수 totalTasksComplete에 해당 스레드가 완료한 태스크 수를 더해줌
		_aligned_free(pThreadArg); //_aligned_malloc을 사용하여 할당되었던 pThreadArg 자원 해제
	}
	free(pThreadArgsArray); // pThreadArgsArray 자원해제 

	// traceFlag가 1이면 total task의 수를 출력 
	if (traceFlag) _tprintf(_T("Total work performed: %d.\n"), totalTasksComplete);

	return 0;
}

DWORD WINAPI Worker(void* arg)
{
	THARG* threadArgs; // 각 스레드에게 넘겨줄 구조체 변수 THARG의 포인터선언
	int iThread; // 스레드 번호 iThread 선언 

	threadArgs = (THARG*)arg; // threadArgs에 파라미터로 받은 arg를 할당
	iThread = threadArgs->threadNumber;  // threadArgs의 threadNumbeer를 iThread에 할당

	// 완료한 태스크가 완료해야할 태스크보다 적으면 계속 루프 돌기
	while (threadArgs->tasksComplete < threadArgs->tasksToComplete) {
		delay_cpu(workerDelay); // workerDelay만큼 CPU를 Delay = 실제로 일하는 것처럼 보이게 하기 위해서
		WaitForSingleObject(threadArgs->hMutex, INFINITE);
		//  hMutex가 signaled state가 될때까지 기다림 
		// 인자 dwMilisecondes를 INFINITE로 설정했음으로 무한정 대기 
		// mutext는 WaitFoSingleObject 함수가 반환될 때 자동으로 non-signaled 상태가 된다. (auto-reset 이라서) 
		// 이 WaitForSingleObject 함수를 지나면 mutex는 non-signaled state가 된다.

		(threadArgs->tasksComplete)++; // 공유데이터에 접근하는 것으로 인식 => ++시켜서 완료 태스크 수 1씩 증가시킴
		ReleaseMutex(threadArgs->hMutex); // 호출 스레드가 점유하던 mutext 객체를 release (non-signaled state에서 signaled state로 바뀜)
	}

	return 0;
}

