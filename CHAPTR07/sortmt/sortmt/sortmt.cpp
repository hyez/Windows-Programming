/* Chapter 7 SortMT. Work crew model
	File sorting with multiple threads and merge sort.
	sortMT [options] nt file. Work crew model.  */

	/*  This program is based on sortHP.
		It allocates a block for the sort file, reads the file,
		then creates a "nt" threads (if 0, use the number of
		processors) so sort a piece of the file. It then
		merges the results in pairs. */

		/* LIMITATIONS:
			1.	The number of threads must be a power of 2
			2.	The number of 64-byte records must be a multiple
				of the number of threads.
			An exercise asks you to remove these limitations. */

#include "Everything.h"

			/* Definitions of the record structure in the sort file. */

// 데이터 길이와 키 길이 정의
#define DATALEN 56  /* Correct length for presdnts.txt and monarchs.txt. */
#define KEYLEN 8

// RECORD 구조체 선언
typedef struct _RECORD {
	char key[KEYLEN];
	char data[DATALEN];
} RECORD;

// 변수 RECORD의 크기 RECSIZE 정의
#define RECSIZE sizeof (RECORD)

// RECORD 포인터변수LPRECORD 정의
typedef RECORD* LPRECORD;

// THREAD 구조체 선언
typedef struct _THREADARG {	/* Thread argument */
	DWORD iTh;		/* Thread number: 0, 1, 3, ... */ // 스레드 번호
	LPRECORD lowRecord;	/* Low Record */ // 하위 레코드
	LPRECORD highRecord;	/* High record */ //상위 레코드
} THREADARG, * PTHREADARG;

// SortThread 및 KeyCompare 함수 선언
static DWORD WINAPI SortThread(PTHREADARG pThArg);
static int KeyCompare(LPCTSTR, LPCTSTR);

// 정렬되어질 레코드의 총 수
static DWORD nRec;	/* Total number of records to be sorted. */

// 멀티 쓰레드를 위한 핸들 수
static HANDLE* pThreadHandle;

int _tmain(int argc, LPTSTR argv[])
{
	/* The file is the first argument. Sorting is done in place. */
	/* Sorting is done in memory heaps. */

	HANDLE hFile, mHandle;
	LPRECORD pRecords = NULL;
	DWORD lowRecordNum, nRecTh, numFiles, iTh;
	LARGE_INTEGER fileSize;
	BOOL noPrint;
	int iFF, iNP;
	PTHREADARG threadArg;
	char * stringEnd;

	printf("sortmt [options] nTh files\n");

	// option을 제외한 sortmt 다음에 오는 인자의 인덱스를 return 받음
	iNP = Options(argc, (LPCTSTR *)argv, _T("n"), &noPrint, NULL); // iNP = 1 (즉, nTh)
	iFF = iNP + 1; // iFF = 2 (즉, files)

	printf("사용자로부터 입력받은 명령어가 'sortmt [options] nTh files' 를 만족하지 않는다면 에러를 출력합니다.\n");
	if (argc <= iFF)
		ReportError(_T("Usage: sortmt [options] nTh files."), 1, FALSE);
	printf("명령어가 형식에 맞게 입력되었습니다.\n");

	// 문자열 argv[iNP](=nTh)를 int로 변환하여 numFiles에 저장 (파일의 갯수)
	numFiles = _ttoi(argv[iNP]); // _ttoi: CString에서 int로 변환


	/* Open the file and map it */
	
	
	printf("CreateFile을 통해 두번째 인자로 입력 받은 파일을 엽니다.\n");
	// CreateFile
	//  -> 파일을 오픈하는 함수
	// <파라미터>
	//  1. LPCTSTR lpFileName: 열고자하는 파일 이름 (사용자로부터 입력받은 인자)
	//  2. DWORD dwDesiredAccess: 읽기 또는 쓰기 권한으로 지정
	//  3. DWORD dwShareMode: 파일의 공유모드 지정 (0:모든 프로세스의 접근을 차단하고 다른 프로세스가 접근을 시도할 경우 엑세스가 거부)
	//  4. LPSECURITY_ATTRIBUTES lpSecurityAttributes: 파일의 보안 속성을 지정하는 SECURITY_ATTRIBUTES 구조체의 포인터, 자식 프로세스로 핸들을 상속하는 여부가 결정 (NULL: 핸들은 상속X)
	//  5. DWORD dwCreationDisposition: 파일을 생성할 것인지 열 것인지를 지정하는 부분(OPEN_EXISTING: 이미 존재하는 파일 열기)
	//  6. DWORD dwFlagsAndAttributes: 생성될 파일 속성(0으로 지정하므로 이 속성은 ignored)
	//  7. HANDLE hTemplateFile: 생성될 파일의 속성을 제공할 템플릿 파일 (NULL로 지정) (
	hFile = CreateFile(argv[iFF], GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);


	// CreateFile: 성공시 파일의 핸들을 리턴, 실패시 INVALID_HANDLE_VALUE를 리턴
	printf("CreateFile이 실패할 경우 에러를 출력합니다.\n");
	if (hFile == INVALID_HANDLE_VALUE)
		ReportError(_T("Failure to open input file."), 2, TRUE);
	printf("CreateFile이 성공했습니다.\n");


	// For technical reasons, we need to add bytes to the end.
	/* SetFilePointer is convenient as it's a short addition from the file end */


	// SetFilePointer 함수
	//  -> 파일 포인터를 원하는 위치로 옮길 때 사용하는 함수
	//  -> win 32bit에서 파일의 최대 크기는 64bit이기 때문에 파일포인터도 64bit의 범위 내에서 이동할 수 있습니다. 
	//  -> 파일 크기가 4GB이하일 경우 세번째 인수는 null값을 주고 두번째 인수만으로 파일 위치를 지정합니다.
	//  <파라미터>
	//  1. HANDLE hFile: 파일포인터를 옮기고자 하는 대상 파일의 핸들. (hFile로 대상 파일의 핸들을 지정함)
	//  2. LONG IDistanceToMove : 파일포인터를 옮길 위치를 지정. (2byte로 지정함)
	//	3. PLONG lpDistanceToMoveHigh : 파일의 크기가 4GB이상일 경우 파일 포인터를 옮길 위치를 지정. (0으로 지정함)
	// 	4. DWORD dwMoveMethod : 파일 포인터의 이동 시작 위치를 지정. (FILE_END : 파일의 끝에서부터 파일 포인터를 이동시킴)
	// return: 실패하면 0, 성공하면 0 이외의 값	
	if (!SetFilePointer(hFile, 2, 0, FILE_END) || !SetEndOfFile(hFile))
		ReportError(_T("Failure position extend input file."), 3, TRUE);


	printf("CreateFileMapping 함수를 통해 파일을 매핑합니다.\n");
	// CreateFileMapping 함수
	//  -> 지정된 파일에 대한 파일 매핑 오브젝트를 작성 또는 오픈
	//  -> 파일의 크기와 접근 방식을 고려하여, 파일 매핑 커널 오브젝트를 생성한다.
	//  -> 파일 매핑 : 파일을 임의의 메모리에 매핑시켜 마치 메모리처럼 다루게 해준다.
	// <파라미터>
	//  1. HANDLE hFile: 매핑 오브젝트를 작성하기 위한 파일의 핸들을 지정 (CreateFile 함수로 해당파일을 열고 리턴받은 값 hFIle로 지정)
	//  2. LPSECURITY_ATTRIBUTES lpFileMappingAttributes: 반환된 핸들을 자식프로세스에 상속할지를 결정 (NULL: 상속X)
	//  3. DWORLD flProtect: 보호 속성, 읽기 전용으로 할건지 읽기 쓰기를 할건지 설정하는 옵션
	//  4. DWORLD dwMaximumSizeHigh: 매핑할 파일의 최대 크기를 상위 바이트 단위로 설정 (파일의 크기가 4GB를 넘지 않는다면, 항상 0 이 될 것)
	//  5. DWORLD dwMaximumSizeLow: 매핑할 파일의 최대 크기를 하위 바이트 단위로 설정
	//  6. LPCSTR lpName: 파일매핑 객체이기 때문에 객체에 이름을 부여할때 사용. 보통   NULL을 주고 이름을 붙여도 다른곳에서 참조할때는 이름을 문자열로 이름을 지정한다.(NULL로 설정)
	// return: 실패시 NULL, 성공시 새롭게 형성된 파일의 매핑 오브젝트에 관한 핸들 반환
	
	mHandle = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	if (NULL == mHandle)
		ReportError(_T("Failure to create mapping handle on input file."), 4, TRUE);
	printf("파일 매핑이 성공했습니다.\n");

	/* Get the file size. */
	printf("GetFileSizeEx 함수를 통해 파일 사이즈를 얻습니다.\n");
	if (!GetFileSizeEx(hFile, &fileSize)) // 실패시 0, 성공시 nonzero 리턴
		ReportError(_T("Error getting file size."), 5, TRUE);
	printf("파일 사이즈를 얻는 것을 성공했습니다.\n");

	printf("레코드의 개수를 계산합니다.\n");
	nRec = (DWORD)fileSize.QuadPart / RECSIZE;	// 64bit인 경우 QuadPart 사용 (32bit는 LowPart)

	printf("스레드당 레코드의 수를 계산합니다.\n");
	nRecTh = nRec / numFiles;	/* Records per thread. */

	printf("THREADARG의 메모리를 할당합니다.\n");
	threadArg = (PTHREADARG)malloc(numFiles * sizeof(THREADARG));	/* Array of thread args. */

	printf("HANDLE의 메모리를 할당합니다.\n");
	pThreadHandle = (HANDLE *)malloc(numFiles * sizeof(HANDLE));

	/* Map the entire file */
	printf("전체 파일을 매핑합니다.\n");
	// MapViewOfFile
	//  -> 프로세스의 주소 공간 상에 파일 매핑 오브젝트의 전체나 일부를 매핑시킨다.
	//  -> 파일 매핑 오브젝트를 생성했다 하더라도, 파일의 데이터에 접근하기 위한 영역을 프로세스 주소 공간 내에 확보해야 하며,
	//  -> 이 영역에 임의의 파일을 물리 저장소로 사용하기 위한 커밋 단계를 거쳐야 한다. MapViewFile함수가 이를 해준다.
	//  -> 메모리 맵 파일을 더 이상 사용할 필요가 없다면, 다음이 세 가지 단계를 수행해야 한다.
	//    - 주소 공간으로부터 파일 매핑 오브젝트의 매핑을 해제한다 (UnmapViewOfFile)
	//    - 파일 커널 매핑 오브젝트를 닫는다(CloseHandle)
	//    - 파일 커널 오브젝트를 닫는다(CloseHandle)
	// <파라미터>
	//  1. HANDLE hFileMappingObject:  CreateFileMapping으로 얻은 핸들을 넘겨준다. (mHandle: 파일 매핑된 핸들)
	//  2. DWORD dwDesiredAccess: 데이터의 액세스 타입을 FILE_MAP_ALL_ACCESS로 지정 (== FILE_MAP_READ | FILE_MAP_WRITE | FILE_MAP_COPY)
	//  3. DWORD dwFileOffsetHigh: 매핑 시작 오프셋의 상위 32비트를 0으로 지정 (파일의 어디부터 매핑할 것인가?)
	//  4. DWORD dwFileOffsetLow: 매핑 시작 오프셋의 하위 32비트를 0으로 지정 (파일의 어디부터 매핑할 것인가?)
	//  5. DWORD dwNumberOfBytesToMap: 파일의 얼마만큼을 매핑할 것인가? (0으로 지정시 매핑 오브젝트 전체가 지정 ->  offset으로부터 파일의 끝까지)
	// return: 성공시 매핑된 뷰의 시작주소가 리턴, 실패시 NULL 리턴

	// *** 뷰 ***
	// - 파일을 주소공간에 매핑할 때 파일 전체를 한꺼번에 매핑할 수도 있고, 일부분만 매핑할 수도 있는데, 주소공간에 매핑된 영역을 뷰 라고 합니다. 
	pRecords = (LPRECORD)MapViewOfFile(mHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (NULL == pRecords)
		ReportError(_T("Failure to map input file."), 6, TRUE);
	printf("전체 파일 매핑에 성공했습니다.\n");

	printf("파일 커널 매핑 오브젝트를 닫습니다.\n");
	CloseHandle(mHandle); 


	printf("sorting threads를 생성합니다.\n");
	/* Create the sorting threads. */
	lowRecordNum = 0;

	printf("스레드를 파일 개수(numFiles)만큼 생성합니다.\n");
	for (iTh = 0; iTh < numFiles; iTh++) {
		threadArg[iTh].iTh = iTh; //몇번째 thread인지 값을 할당
		threadArg[iTh].lowRecord = pRecords + lowRecordNum; // 하위 레코드는 pRecord + lowRecordNum
		threadArg[iTh].highRecord = pRecords + (lowRecordNum + nRecTh); // 상위 레코드는 pRecords + (lowRecordNum + nRecTh);
		lowRecordNum += nRecTh; // nRecTh(스레드당 레코드의 수)만큼 증가

		printf("%d 번째 스레드를 생성합니다.\n", iTh);
		// _beginthreadex: 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달(쓰레드에 의해 SortThread가 호출되는데 이 포인터를 인자로 전달)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 (CREATE_SUSPENDED: 일시 중단된 상태로 스레드를 만듬)
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (NULL: 사용하지 않음)
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		pThreadHandle[iTh] = (HANDLE)_beginthreadex(
			NULL, 0, (_beginthreadex_proc_type)SortThread, &threadArg[iTh], CREATE_SUSPENDED, NULL);

	}

	/* Resume all the initially suspened threads. */
	printf("일시중단된 스레드를 재실행합니다.\n");
	// 인자는 재실행 시키고자 하는 쓰레드의 핸들
	// 쓰레드의 중단 카운트를 하나 감소 시키며, 중단 카운트가 0이 됬을 때 쓰레드는 재 실행된다.
	for (iTh = 0; iTh < numFiles; iTh++)
		ResumeThread(pThreadHandle[iTh]);

	/* Wait for the sort-merge threads to complete. */
	printf("WaitForSingleObject를 사용하여 thread를 대기합니다.\n");
	// 지정한 오브젝트가 시그널 상태가 되거나 타임아웃이 되면 제어를 돌려준다.
	WaitForSingleObject(
		pThreadHandle[0], // sort 후 merge된 thread
		INFINITE // INFINITE를 지정하면 오브젝트가 시그널 상태가 될때까지 기다린다.
	); 

	printf("스레드 핸들을 닫아줍니다.\n");
	// _beginthreadex 는 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
	for (iTh = 0; iTh < numFiles; iTh++)
		CloseHandle(pThreadHandle[iTh]);

	/*  Print out the entire sorted file. Treat it as one single string. */
	printf("정렬된 전체 파일을 출력합니다.\n");
	stringEnd = (char *)pRecords + nRec * RECSIZE;
	*stringEnd = '\0';
	if (!noPrint) {
		printf("%s", (char *)pRecords);
	}

	printf("주소 공간으로부터 파일 매핑 오브젝트의 매핑을 해제합니다.\n");
	UnmapViewOfFile(pRecords);
	// Restore the file length

	/* SetFilePointer is convenient as it's a short addition from the file end */
	printf("지정된 파일의 파일 종단(EOF)의 위치를, 현재의 파일 포인터의 위치에 이동시킵니다.\n");
	if (!SetFilePointer(hFile, -2, 0, FILE_END) || !SetEndOfFile(hFile))
		ReportError(_T("Failure restore input file lenght."), 7, TRUE);

	printf("hFile 핸들을 닫습니다.\n");
	CloseHandle(hFile);

	printf("메모리 할당을 해제합니다.\n");
	free(threadArg); free(pThreadHandle);
	return 0;

} /* End of _tmain. */

static VOID MergeArrays(LPRECORD, DWORD);

DWORD WINAPI SortThread(PTHREADARG pThArg)
{
	DWORD groupSize = 2, myNumber, twoToI = 1;
	/* twoToI = 2^i, where i is the merge step number. */
	DWORD_PTR numbersInGroup;
	LPRECORD first;

	myNumber = pThArg->iTh;
	first = pThArg->lowRecord;
	numbersInGroup = (DWORD)(pThArg->highRecord - first);

	/* Sort this portion of the array. */
	qsort(first, numbersInGroup, RECSIZE, (_CoreCrtNonSecureSearchSortCompareFunction)KeyCompare);

	/* Either exit the thread or wait for the adjoining thread. */
	while ((myNumber % groupSize) == 0 && numbersInGroup < nRec) {
		/* Merge with the adjacent sorted array. */
		WaitForSingleObject(pThreadHandle[myNumber + twoToI], INFINITE);
		MergeArrays(first, numbersInGroup);
		numbersInGroup *= 2;
		groupSize *= 2;
		twoToI *= 2;
	}
	return 0;
}

static VOID MergeArrays(LPRECORD p1, DWORD nRecs)
{
	/* Merge two adjacent arrays, each with nRecs records. p1 identifies the first */
	DWORD iRec = 0, i1 = 0, i2 = 0;
	LPRECORD pDest, p1Hold, pDestHold, p2 = p1 + nRecs;

	pDest = pDestHold = (LPRECORD) malloc(2 * nRecs * RECSIZE);
	p1Hold = p1;

	while (i1 < nRecs && i2 < nRecs) {
		if (KeyCompare((LPCTSTR)p1, (LPCTSTR)p2) <= 0) {
			memcpy(pDest, p1, RECSIZE);
			i1++; p1++; pDest++;
		}
		else {
			memcpy(pDest, p2, RECSIZE);
			i2++; p2++; pDest++;
		}
	}
	if (i1 >= nRecs)
		memcpy(pDest, p2, RECSIZE * (nRecs - i2));
	else	memcpy(pDest, p1, RECSIZE * (nRecs - i1));

	memcpy(p1Hold, pDestHold, 2 * nRecs * RECSIZE);
	free(pDestHold);
	return;
}

int KeyCompare(LPCTSTR pRec1, LPCTSTR pRec2)
{
	DWORD i;
	TCHAR b1, b2;
	LPRECORD p1, p2;
	int Result = 0;

	p1 = (LPRECORD)pRec1;
	p2 = (LPRECORD)pRec2;
	for (i = 0; i < KEYLEN && Result == 0; i++) {
		b1 = p1->key[i];
		b2 = p2->key[i];
		if (b1 < b2) Result = -1;
		if (b1 > b2) Result = +1;
	}
	return  Result;
}
