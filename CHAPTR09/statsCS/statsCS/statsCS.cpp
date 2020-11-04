/* Chapter 9. statsCS.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20
#define CACHE_LINE_SIZE 64

/* Usage: statsCS nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

DWORD WINAPI Worker(void*);
int workerDelay = DELAY_COUNT;

__declspec(align(CACHE_LINE_SIZE))
typedef struct _THARG {
    CRITICAL_SECTION* pCS; // 임계 영역을 위한 포인터 변수 : 현재 수행중인 스레드가 임계영역을 벗어나기 전까지는 같은 자원에 접근하지 못하게 하기 위해서, 메세지 블록을 보호할 자료구조
    int threadNumber;
    unsigned int tasksToComplete;
    unsigned int tasksComplete;
} THARG;

int _tmain(int argc, LPTSTR argv[])
{
    INT nThread, iThread;
    HANDLE* hWorkers;
    CRITICAL_SECTION cs; // 임계영역을 위한 변수, 현재 수행중인 스레드가 임계영역을 벗어나기 전까지는 같은 자원에 접근하지 못하게 하기 위해서, 메세지 블록을 보호할 자료구조
    unsigned int tasksPerThread, totalTasksComplete;
    THARG** pThreadArgsArray, * pThreadArg;
    BOOL traceFlag = FALSE;

    if (argc < 3) {
        _tprintf(_T("Usage: statsCS nThread ntasks [trace]\n"));
        return 1;
    }
    _tprintf(_T("statsCS %s %s %s\n"), argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));

    nThread = _ttoi(argv[1]);
    tasksPerThread = _ttoi(argv[2]);
    if (argc >= 4) workerDelay = _ttoi(argv[3]);
    traceFlag = (argc >= 5);

    /* Initialize the CS */
    InitializeCriticalSection(&cs); // 임계영역 초기화

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
        pThreadArg->pCS = &cs; // 각스레드의 pThreadArg->pCS 에 cs(임계영역) 주소 할당
        hWorkers[iThread] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Worker, pThreadArg, 0, NULL);
        if (hWorkers[iThread] == NULL)
            ReportError(_T("Cannot create consumer thread"), 4, TRUE);
    }

    /* Wait for the threads to complete */
    for (iThread = 0; iThread < nThread; iThread++)
        WaitForSingleObject(hWorkers[iThread], INFINITE);
    DeleteCriticalSection(&cs); // 임계영역 삭제

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
        EnterCriticalSection(threadArgs->pCS); // 임계영역으로 진입한다.  파라미터는 임계 영역 오브젝트(threadArgs->pCS)에 대한 주소이다.
        (threadArgs->tasksComplete)++; // 임계영역 내에서 수행할 것. 완료 태스크를 증가시킨다.
        LeaveCriticalSection(threadArgs->pCS); // 임계영역에서 빠져나온다.
        // 단,  EnterCriticalSection를 한 후에  LeaveCriticalSection를 호출하지 않는다면, 영원히 접근하지 못하는 데드락 현상 발생
    }

    return 0;
}

