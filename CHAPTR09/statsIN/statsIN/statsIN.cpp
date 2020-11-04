/* Chapter 9. statsIN.c                                        */
/* Simple boss/worker system, where each worker reports        */
/* its work output back to the boss for display.            */
/* MUTEX VERSION                                            */

#include "Everything.h"
#define DELAY_COUNT 20
#define CACHE_LINE_SIZE 64

/* Usage: statsIN nThread ntasks [delay [trace]]                */
/* start up nThread worker threads, each assigned to perform    */
/* "ntasks" work units. Each thread reports its progress        */
/* in its own unshard slot in a work performed array            */

DWORD WINAPI Worker(void*);
int workerDelay = DELAY_COUNT;

__declspec(align(CACHE_LINE_SIZE))
typedef struct _THARG {
    int threadNumber;
    unsigned int tasksToComplete;
    volatile unsigned int tasksComplete;
} THARG;

int _tmain(int argc, LPTSTR argv[])
{
    INT nThread, iThread;
    HANDLE* hWorkers;
    unsigned int tasksPerThread, totalTasksComplete;
    THARG** pThreadArgsArray, * pThreadArg;
    BOOL traceFlag = FALSE;

    if (argc < 3) {
        _tprintf(_T("Usage: statsIN nThread ntasks [trace]\n"));
        return 1;
    }
    _tprintf(_T("statsIN %s %s %s\n"), argv[1], argv[2], argc >= 4 ? argv[3] : _T(""));

    nThread = _ttoi(argv[1]);
    tasksPerThread = _ttoi(argv[2]);
    if (argc >= 4) workerDelay = _ttoi(argv[3]);
    traceFlag = (argc >= 5);

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
        hWorkers[iThread] = (HANDLE)_beginthreadex(NULL, 0, _beginthreadex_proc_type(Worker), pThreadArg, 0, NULL);
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
        InterlockedIncrement(&(threadArgs->tasksComplete));
        // tasksComplete를 1씩 증사시킨다.
        // 다른 스레드에서도 taskComplete를 사용할 수 있기 때문에 다른 스레드에 의해 방해받지 않기 위해 InterlockedIncrement로 묶어주는 것 
        // 단, 값을 2만큼 증가시키기 위해서 InterlockedIncrement() 함수를 2번 호출하지않고, InterlockedExchangeAdd() 함수를 사용해야한다.
     
    }

    return 0;
}

