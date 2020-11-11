/* Chapter 9. statsSRW_VTP.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20 // DELAY_COUNT 정의
#define CACHE_LINE_SIZE 64 // CACHE_LINE_SIZE 정의

/* Usage: statsSRW_VTP nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

VOID CALLBACK Worker(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK); 
/*
<CALLBACK 함수 Worker 선언>
- 스레드 함수와 같은 개념.
- CreateThreadpoolWork 함수의 인자로 넣어 콜백하여 리턴 값을 저장하는 함수
- SubmitThreadpoolWork 함수 호출 발생 시마다 스레드 풀은 이 함수를 한번씩 호출한다.
- 파라미터
1. PTP_CALLBACK_INSTANCE Instance : 이 콜백 함수의 식별 정보를 담고 있는 구조체 인스턴스 (포인터)
2. PVOID Context : 콜백 함수에 줄 인자 블록에 해당하는 포인터
3. PTP_WORK Work : 콜백함수를 SUBMIT한 오브젝트의 포인터
*/


int workerDelay = DELAY_COUNT; // 전역변수 workerDelay 선언 및 정의 
BOOL traceFlag = FALSE;  // 각 스레드가 얼마나 일했는지를 출력하거나 안하거나를 나타내는 flag 변수

__declspec(align(CACHE_LINE_SIZE))
/* __declspec은 저장소 클래스 정보를 지정 하기 위한 MS 확장 문법이다.
   __declspec(align(#))를 사용하여 사용자 정의 데이터(예: 함수의 정적 할당 또는 자동 데이터)를 정확하게 제어가능
 */

typedef struct _THARG {
	SRWLOCK slimRWL;

	/*
	* 슬림 리더 라이터 락 - Slim Reader Writer Lock (SRW Lock)
	- 기존 CriticalSection 비해 조금 더 적은 메모리 사용과 빠른 수행 속도를 가진다. (CRITICAL_SECTION와 달리 SRWLOCK은 공유 자원을 읽기만 하는 스레드와 값을 수정하려고 하는 스레드를 구분해서 관리한다)
	- Reader 쓰레드와 Writer 쓰레드가 완전히 분리될 수 있을 경우 사용될 수 있다.
	- Reader 쓰레드와 Writer 쓰레드가 분리되어 있는 경우 아래와 같이 두 가지 모드로 SRW 락을 사용할 수 있다.
		- Shared mode : 복수의 쓰레드가 read-only로 접근시 훨씬 더 강력한 성능 발휘
		- Exclusive mode : 복수의 쓰레드가 읽고 쓸 때 사용하는 모드

	<진입할때 다음 함수 사용>
	void AcquireSRWLockExclusive(PSRWLOCK SRWLock);

	<해제시 다음 함수 사용>
	void ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
	*/

	int objectNumber; // 스레드 풀에 제출할 오브젝트(WORK) 수
	unsigned int tasksToComplete; // 완료해야할 태스크 수
	unsigned int tasksComplete; // 완료한 태스크 수
} THARG;

int _tmain(int argc, LPTSTR argv[])
{
	INT nThread, iThread;  // nThread: 스레드 갯수 변수,  iThread: 스레드 id 변수 (몇번째 스레드인지)
	PTP_WORK *pWorkObjects;  // 생성될 Work 오브젝트를 위한 포인터 변수
	SRWLOCK srwl; // SRWLOCK 변수 선언
	unsigned int tasksPerThread, totalTasksComplete;  // taskPerThread: 스레드당 몇개의 task를 수행하냐, totalTasksComplete : 전체 스레드가 수행한 total task의 개수
	THARG ** pWorkObjArgsArray, *pThreadArg;  // Work object 인자에 대한 2차원 포인터 변수 pWorkObjArgsArray, 스레드 인자 넘겨주는 1차원 포인터 변수 pThreadArg 선언
	TP_CALLBACK_ENVIRON cbe;  // Callback environment (콜백 환경)
	 
	if (argc < 3) { //인자 개수가 3보다 작으면 == 인자를 안 준 것이 있으면, 프로그램 끝낸다.
		_tprintf(_T("Usage: statsSRW_VTP nThread ntasks [trace]\n"));
		return 1;
	}
	_tprintf(_T("%s %s %s %s\n"), argv[0], argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));   // argv가 4개 이상이면 arg[3] 으로 출력 아니면 ""
	/* 입력한 명령어 출력
	 1. argv[0] = statsSRW_TP
	 2. argv[1] = 스레드 개수
	 3. argv[2] = 스레드 당 태스크  수
	 4. argv[3]은 입력한 인자가 4개 이상일 경우(statsMX를 포함하여 4개를 모두 입력할 경우) arg[3]을 출력 */

	nThread = _ttoi(argv[1]); // _ttoi: 문자열을 정수로 바꾸기, 여기서는 argv[1]에 해당하는 스레드 개수를 숫자로 바꾼다.
	tasksPerThread = _ttoi(argv[2]); // 여기서는 argv[2]에 해당하는스레드 당 태스크  수를 숫자로 바꾼다.
	if (argc >= 4) workerDelay = _ttoi(argv[3]) ;// workerDelay의 default 값은 20이였는데, argv[3]을 입력했다면 argv[3] 인자로 이 값을 바꿀 수 있다
	traceFlag = (argc >= 5); // argc가 5 이상이면 1 아니면 0
	// 즉, 다섯번째 인자를 입력받지 않으면 traceFlag는 0인데 다섯번째 인자를 commandline에서 입력받으면 traceFlag는 1이 됨
	// traceFlag: 각 스레드가 얼마나 일했는지 출력하는지, 다섯번째 인자를 통해 설정 가능



	/* Initialize the SRW lock */
	InitializeSRWLock(&srwl); // SRWLock  초기화 (인자는 SRWLock 에 대한 포인터)

	pWorkObjects = (PTP_WORK *)malloc(nThread * sizeof(PTP_WORK));  // 스레드 개수만큼 Work object 포인터(PTP_WORK) 할당
	pWorkObjArgsArray = (THARG **)NULL;   //  일단 NULL로 초기화
	if (pWorkObjects != NULL) //pWorkObjects가 NULL이 아니면,
		pWorkObjArgsArray = (THARG **)malloc(nThread * sizeof(THARG *)); // pWorkObjArgsArray에 스레드 개수만큼 다시 메모리 할당 

	if (pWorkObjects == NULL || pWorkObjArgsArray == NULL) // pWorkObjects 또는 pWorkObjArgsArray가 메모리 할당에 실패할 경우 
		ReportError(_T("Cannot allocate working memory for worke item or argument array."), 2, TRUE); // 에러 출력 (exitCode : 2)

	// 스레드 풀 환경 Initialize, 인자로는 콜백 환경에서 정의된 TP_CALLBACK_ENVIRON cbe를 넘겨준다.
	// return value: 없음
	InitializeThreadpoolEnvironment(&cbe);

	// command line에서 입력 받은 스레드 개수 만큼 for 문을 반복
	for (iThread = 0; iThread < nThread; iThread++) {
		/* Fill in the thread arg */
		// _aligned_malloc: 
		// 1. size: 요청된 메모리 할당 크기 (THARG의 size)
		// 2. 정렬(alignment):  2의 정수 거듭제곱이어야 함 (CACHE_LINE_SIZE=64)
		// return value: 할당된 메모리 블록에 대한 포인터 이거나, 작업에 실패 한 경우 NULL.
		// CACHE_LINE_SIZE가 64바이트니까 스레드에 넘겨지는 인자들이 64바이트로 맞추어서 cache block을 다 차지하게 된다.
		// 즉, 스레드에 넘기는 인자 구조체 변수를 64바이트에 맞추고, 이것은 한 cache block에는 스레드 하나를 위한 인자 블록만 들어가도록 하게 함
		pThreadArg = (pWorkObjArgsArray[iThread] = (THARG *)_aligned_malloc(sizeof(THARG), CACHE_LINE_SIZE));

		// NULL를 리턴할 경우 작업에 실패했으므로 에러메세지 출력
		// exitCode 3:  pThreadArg 메모리 할당 실패
		if (NULL == pThreadArg)
			ReportError(_T("Cannot allocate memory for a thread argument structure."), 3, TRUE);


		pThreadArg->objectNumber = iThread;  // 몇번째 스레드인지 스레드 번호 할
		pThreadArg->tasksToComplete = tasksPerThread;  // 스레드당 처리할 수 있는 테스크 개수 할당 (예-256000)
		pThreadArg->tasksComplete = 0;  // 완료된 테스크는 0개로 초기화
		pThreadArg->slimRWL = srwl; // SRWLock 구조체 srwl를 pThreadArg->SRWL에 할당
		pWorkObjects[iThread] = (PTP_WORK)CreateThreadpoolWork(Worker, pThreadArg, &cbe);
		/* CreateThreadpoolWork
		- work object를 생성하는 함수이다.
		 1. Worker: Worker 스레드는 SubmitThreadpoolWork가 호출될 때마다 제출된 work object에 있는 이 함수를 호출한다.
		 2. pThreadArg: 콜백 함수에 전달할 인자
		 3. &cbe: InitializeThreadpoolEnvironment로 초기화했던 콜백 환경 변수 주소를 넘겨준다. 
		  
		 return value: 성공시 TP_WORK 구조체 포인터를 리턴 (pWorkObjects), 실패시 NULL 리턴
		 */

		if (pWorkObjects[iThread] == NULL) // 작업에 실패할 경우 (work object 생성 실패)
			ReportError(_T("Cannot create consumer thread"), 4, TRUE); // 에러 출력 (exitCode:4)

		SubmitThreadpoolWork(pWorkObjects[iThread]); // 스레드 풀에 pWorkObjects[iThread] 를 제출
		// 단, 콜백 함수들은 병렬 실행된다
		// 효율성을 위해 스레드 풀은 스레드 개수를 조절(throttle)가능하다.
	}

	// 모든 스레드들이 완료될 때까지 대기
	// command line에서 입력 받은 스레드 개수 만큼 for 문을 반복
	/* Wait for the threads to complete */
	for (iThread = 0; iThread < nThread; iThread++) {
		/* Wait for the thread pool work item to complete */
		WaitForThreadpoolWorkCallbacks(pWorkObjects[iThread], FALSE); // submit한 오브젝트의 Work 콜백 함수들의 실행이 완료되기를 기다린다.
		// 1. pWorkObjects[iThread]: 끝나기 기다릴 work object의 포인터 (for문을 통해 모든 오브젝트가 끝나기를 기다림)
		// 2. 아직 실행하지 않은 콜백 함수가 있으면 취소하는지 안하는지 (False => 취소안함)

		CloseThreadpoolWork(pWorkObjects[iThread]); // 생성했던 work object(pWorkObjects[iThread])를 닫음
	}

	free(pWorkObjects); // work object pWorkObjects 메모리 해제
	_tprintf(_T("Worker threads have terminated\n"));
	totalTasksComplete = 0;  // 완료된 태스크 수를 0으로 초기화 (처음엔 완료된 태스크가 없으므로)

	// command line에서 입력 받은 스레드 개수 만큼 for 문을 반복
	for (iThread = 0; iThread < nThread; iThread++) {
		pThreadArg = pWorkObjArgsArray[iThread];  // 각 pWorkObjArgsArray를 pThreadArg에 할당

		// traceFlag가 1이면 몇번째 스레드가 몇개의 태스크를 완료했는지를 출력한다.
		if (traceFlag) _tprintf(_T("Tasks completed by thread %5d: %6d\n"), iThread, pThreadArg->tasksComplete);

		// 모든 스레드가 완료한  total task 수를 담은 변수 totalTasksComplete에 해당 스레드가 완료한 태스크 수를 더해줌
		totalTasksComplete += pThreadArg->tasksComplete; 
		_aligned_free(pThreadArg);  //_aligned_malloc을 사용하여 할당되었던 pThreadArg 자원 해제
	}
	free(pWorkObjArgsArray); // pWorkObjArgsArray 메모리 할당 해제 
	
	// traceFlag가 1이면 total task의 수를 출력 
	if (traceFlag) _tprintf(_T("Total work performed: %d.\n"), totalTasksComplete);

	return 0;
}

// 콜백함수 Worker 정의
VOID CALLBACK Worker(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
	THARG * threadArgs;

	threadArgs = (THARG *)Context;  //콜백함수에 줄 인자 블록에 해당하는 포인터 할당

	if (traceFlag) // traceFlag가 1이면 == 각 스레드가 얼마나 일했는지를 출력
		// 즉, 스레드 풀에 제출할 work object, 스레드 수 출력
		_tprintf(_T("Worker: %d. Thread Number: %d.\n"), threadArgs->objectNumber, GetCurrentThreadId());

	// 완료한 태스크가 완료해야할 태스크보다 적으면 계속 루프 돌기
	while (threadArgs->tasksComplete < threadArgs->tasksToComplete) {
		delay_cpu(workerDelay); // workerDelay만큼 CPU를 Delay = 실제로 일하는 것처럼 보이게 하기 위해서
		AcquireSRWLockExclusive(&(threadArgs->slimRWL)); // 쓰기용 lock을 얻으면 lock을 해제할때까지 다른 스레드가 공유자원에 접근할 수 없다.
		(threadArgs->tasksComplete)++;  // lock 안에서 완료 태스크를 1 증가한다.
		ReleaseSRWLockExclusive(&(threadArgs->slimRWL)); // 쓰기용 lock을 해제한다.
	}

	return;
}

