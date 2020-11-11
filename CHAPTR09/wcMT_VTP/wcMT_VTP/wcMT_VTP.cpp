/* Chapter 9. wcMT_VTP.c									*/
/*															*/
/* wcMT_VTP file1 file2 ... fileN							*/
/* WARNING: This code is NOT UNICODE enabled				*/
/*															*/
/* Parallel word count - NT6 (Vista) thread pool version	*/
/*															*/
/* Indpependent procesing of multiple files using			*/
/* the boss/worker model									*/
/* count the total number of characters, words, and lines	*/
/* in the files specified on the command line, just			*/
/* as is done by the UNIX wc utility						*/
/* Each file is processed by a separate worker in the TP.	*/
/* The main thread (the boss thread) accumulates			*/
/* the separate results to produce the final results		*/
/* As with all the other "wc" implementations, this uses 8-bit characters */
#include "Everything.h"
#include <VersionHelpers.h>
#define CACHE_LINE_SIZE 64 

__declspec(align(CACHE_LINE_SIZE))

// 스레드 인자에 대한 구조체 변수 선언
typedef struct { /* Structure for thread argument*/
	char* filename; // 파일명
	// 여러 스레드 함수를 사용하므로 volatile로 선언
	volatile unsigned int kchar; // char count
	volatile unsigned int kword;  // word count
	volatile unsigned int kline; // line count
	volatile unsigned int wcerror; 
} WORK_OBJECT_ARG;

VOID CALLBACK wcfunc(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK);
/*
<CALLBACK 함수 wcfunc 선언>
- 스레드 함수와 같은 개념.
- CreateThreadpoolWork 함수의 인자로 넣어 콜백하여 리턴 값을 저장하는 함수
- SubmitThreadpoolWork 함수 호출 발생 시마다 스레드 풀은 이 함수를 한번씩 호출한다.
- 파라미터
1. PTP_CALLBACK_INSTANCE Instance : 이 콜백 함수의 식별 정보를 담고 있는 구조체 인스턴스 (포인터)
2. PVOID Context : 콜백 함수에 줄 인자 블록에 해당하는 포인터
3. PTP_WORK Work : 콜백함수를 SUBMIT한 오브젝트의 포인터
*/

volatile unsigned int WorkerId = 0;	// WorkerId 초기화

int _tmain(int argc, LPTSTR argv[])
{
	DWORD nchar = 0, nword = 0, nline = 0; // nchar, nword, nline 을 각각 0으로 초기화한다.
	PTP_WORK* pWorkObjects;
	WORK_OBJECT_ARG** pWorkObjArgsArray, * pObjectArg;
	TP_CALLBACK_ENVIRON cbe;  
	int nThread, iThrd;

	if (!IsWindows8OrGreater()) // 윈도우 OS 버전을 만족하지 못하면 에러 출력
		ReportError(_T("This program requires Windows NT 8.0 or greater"), 1, TRUE); // exitCode: 1

	if (argc < 2) { // 인자 개수가 3보다 작으면 == 인자를 안 준 것이 있으면, 프로그램 끝낸다.
		printf("Usage: wcMT_vtp filename ... filename\n");
		return 1;
	}

	// command line에서 입력한 각각의 파일에 대한 worker thread 생성

	nThread = (DWORD)argc - 1; //  입력한 인자 총 개수에서 1 빼준 값 (즉, wcMT_vtp명령어를 제외한 파일 개수를 의미)

	pWorkObjects = (PTP_WORK*)malloc(nThread * sizeof(PTP_WORK));
	pWorkObjArgsArray = (WORK_OBJECT_ARG**)NULL;

	if (pWorkObjects != NULL)
		pWorkObjArgsArray = (WORK_OBJECT_ARG**)malloc(nThread * sizeof(WORK_OBJECT_ARG*));

	if (pWorkObjects == NULL || pWorkObjArgsArray == NULL)
		ReportError(_T("Cannot allocate working memory for worke item or argument array."), 2, TRUE);

	InitializeThreadpoolEnvironment(&cbe); 

	for (iThrd = 0; iThrd < nThread; iThrd++) {
		pObjectArg = (pWorkObjArgsArray[iThrd] = (WORK_OBJECT_ARG*)_aligned_malloc(sizeof(WORK_OBJECT_ARG), CACHE_LINE_SIZE));

		if (NULL == pObjectArg)
			ReportError(_T("Cannot allocate memory for a thread argument structure."), 3, TRUE);

		pObjectArg->filename = (char*)argv[iThrd + 1];  // 각 OBJECT에 파일 이름 할당
		pObjectArg->kword = pObjectArg->kchar = pObjectArg->kline = 0; // 각 OBJECT에 대한 kword, kchar, kline를 모두 0으로 초기화
		pWorkObjects[iThrd] = (PTP_WORK)CreateThreadpoolWork(wcfunc, pObjectArg, &cbe);
		

		if (pWorkObjects[iThrd] == NULL)
			ReportError(_T("Cannot create consumer thread"), 4, TRUE);
		SubmitThreadpoolWork(pWorkObjects[iThrd]);
	}
	for (iThrd = 0; iThrd < nThread; iThrd++) {
		WaitForThreadpoolWorkCallbacks(pWorkObjects[iThrd], FALSE);
		CloseThreadpoolWork(pWorkObjects[iThrd]);
	}
	free(pWorkObjects);

	// 스레드 결과 누적
	for (iThrd = 0; iThrd < argc - 1; iThrd++) {
		pObjectArg = pWorkObjArgsArray[iThrd]; //i번째 스레드의 pWorkObjArgsArray를 pObjectArg에 할당
		nchar += pObjectArg->kchar; // 각 오브젝트의 kchar(character count)를 누적 
		nword += pObjectArg->kword; //각 오브젝트의 kword(word count)를 누적
		nline += pObjectArg->kline; //각 오브젝트의 kline(line count)을 누적 
		printf("%10d %9d %9d %s\n", pObjectArg->kline,
			pObjectArg->kword, pObjectArg->kchar,
			pObjectArg->filename); // i번째 스레드의 kline, kchar, filename 출력
	}
	free(pWorkObjArgsArray); // pWorkObjArgsArray 메모리 해제
	printf("%10d %9d %9d \n", nline, nword, nchar); // 총 line, word, char count (누적한 결과)를 출력
	return 0;
}


// 인자로 받은 ch가 공백인지 아닌지 판별하는 함수이다.
int is_a_space(int ch) {
	if (ch == ' ' || ch == '\t' || ch == '\n' ||  ch == '\f' || ch == '\r') { // ch가 space(공백), tab(\t), newline(\n), form feed(\f), carriage return(\r)일 경우
		return 1; // 1 리턴
	}
	else {
		return 0; // 아닐 경우 0 리턴
	}
}

// map file을 위한 구조체 변수 MAPPED_FILE_HANDLE 선언
typedef struct {
	void* pInFile;
	HANDLE hInMap;  
	HANDLE hIn; 
} MAPPED_FILE_HANDLE; 


// 파일 매핑 함수
void* map_file(LPCSTR filename, unsigned int* pFsLow, int* error, MAPPED_FILE_HANDLE* pmFH)
{
	HANDLE hIn, hInMap; // hIn, hInMap 핸들 선언
	LARGE_INTEGER fileSize; // fileSize 선언
	char* pInFile; // pInFile 선언

	*error = 0; // error가 가리키는 변수의 값을 0으로 설정 (즉, 에러의 초기값을 0으로 설정)
	hIn = CreateFile((LPCWSTR)filename, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	/* CreateFile:
	- 파일이나 입출력 장치를 연다. 
	- 대표적인 입출력 장치로는 파일, 파일 스트림, 디렉토리, 물리적인 디스크, 볼륨, 콘솔 버퍼, 테이브 드라이브, 파이프 등이 있다. 
	- 이 함수는 각 장치를 제어할 수 있는 handle(핸들)을 반환한다.
	 1. LPCTSTR lpFileName: 열고자 하는 파일 이름, filename으로 지정
	 2. DWORD dwDesiredAccess: 엑세스 권한 설정, GENERIC_READ로 지정하여 읽기 권한으로 설정
	 3. DWORD dwShareMode: 개체의 공유 방식을 지정,  0으로 설정했음으로 모든 프로세스의 접근을 차단한다.
	 4. LPSECURITY_ATTRIBUTES lpSecurityAttributes: 사용하지 않음으로 NULL로 지정함
	 5. DWORD dwCreationDisposition: 파일이 존재할 경우에만 파일을 열기 위해 OPEN_EXISTING로 지정
	 6. DWORD dwFlagsAndAttributes: 파일의 기타 속성을 지정한다. FILE_ATTRIBUTE_NORMAL로 지정해 다른 속성을 가지지 않는다.
	 7. HANDLE hTemplateFile: 템플릿 파일의 유효한 핸들을 사용하지 않음으로 NULL로 지정함

	 - return value: 성공시 file handle return , 실패시 INVALID_HANDLE_VALUE return
	 */

	// 파일 생성을 실패할 경우
	if (hIn == INVALID_HANDLE_VALUE) {
		*error = 2; // error가 가리키는 값을 2로 지정
		return NULL; // NULL 리턴
	}

	// 파일 생성에 성공할 경우 파일 매핑 오브젝트 생성

	// Create a file mapping object on the input file. Use the file size. 
	hInMap = CreateFileMapping(hIn, NULL, PAGE_READONLY, 0, 0, NULL);
	// CreateFileMapping 함수
	//  -> 지정된 파일에 대한 파일 매핑 오브젝트를 작성 또는 오픈
	//  -> 파일의 크기와 접근 방식을 고려하여, 파일 매핑 커널 오브젝트를 생성한다.
	//  -> 파일 매핑 : 파일을 임의의 메모리에 매핑시켜 마치 메모리처럼 다루게 해준다.
	// <파라미터>
	//  1. HANDLE hFile: 매핑 오브젝트를 작성하기 위한 파일의 핸들을 지정 (CreateFile 함수로 해당파일을 열고 리턴받은 값 hIn로 지정)
	//  2. LPSECURITY_ATTRIBUTES lpFileMappingAttributes: 반환된 핸들을 자식프로세스에 상속할지를 결정 (NULL: 상속X)
	//  3. DWORLD flProtect: 보호 속성, 읽기 전용으로 할건지 읽기 쓰기를 할건지 설정하는 옵션 (PAGE_READONLY로 지정)
	//  4. DWORLD dwMaximumSizeHigh: 매핑할 파일의 최대 크기를 상위 바이트 단위로 설정 
	//  5. DWORLD dwMaximumSizeLow: 매핑할 파일의 최대 크기를 하위 바이트 단위로 설정
	//  6. LPCSTR lpName: 파일매핑 객체이기 때문에 객체에 이름을 부여할때 사용. 보통  NULL을 주고 이름을 붙여도 다른곳에서 참조할때는 이름을 문자열로 이름을 지정한다.(NULL로 설정)
	// return: 실패시 INVALID_HANDLE_VALUE, 성공시 새롭게 형성된 파일의 매핑 오브젝트에 관한 핸들 반환

	// 파일 매핑에 실패할 경우
	if (hInMap == INVALID_HANDLE_VALUE) {
		CloseHandle(hIn); // hIn 핸들을 닫고 
		*error = 3; //error가 가리키는 변수의 값을 3로 설정
		return NULL;
	}

	// input file을 매핑
	pInFile = (char*)MapViewOfFile(hInMap, FILE_MAP_READ, 0, 0, 0);
	// MapViewOfFile
	//  -> 프로세스의 주소 공간 상에 파일 매핑 오브젝트의 전체나 일부를 매핑시킨다.
	//  -> 파일 매핑 오브젝트를 생성했다 하더라도, 파일의 데이터에 접근하기 위한 영역을 프로세스 주소 공간 내에 확보해야 하며,
	//  -> 이 영역에 임의의 파일을 물리 저장소로 사용하기 위한 커밋 단계를 거쳐야 한다. MapViewFile함수가 이를 해준다.
	//  -> 메모리 맵 파일을 더 이상 사용할 필요가 없다면, 다음이 세 가지 단계를 수행해야 한다.
	//    - 주소 공간으로부터 파일 매핑 오브젝트의 매핑을 해제한다 (UnmapViewOfFile)
	//    - 파일 커널 매핑 오브젝트를 닫는다(CloseHandle)
	//    - 파일 커널 오브젝트를 닫는다(CloseHandle)
	// <파라미터>
	//  1. HANDLE hFileMappingObject:  CreateFileMapping으로 얻은 핸들을 넘겨준다. (hInMap: 파일 매핑된 핸들)
	//  2. DWORD dwDesiredAccess: 데이터의 액세스 타입을 FILE_MAP_READ로 지정
	//  3. DWORD dwFileOffsetHigh: 매핑 시작 오프셋의 상위 32비트를 0으로 지정 (파일의 어디부터 매핑할 것인가?)
	//  4. DWORD dwFileOffsetLow: 매핑 시작 오프셋의 하위 32비트를 0으로 지정 (파일의 어디부터 매핑할 것인가?)
	//  5. DWORD dwNumberOfBytesToMap: 파일의 얼마만큼을 매핑할 것인가? (0으로 지정시 매핑 오브젝트 전체가 지정 ->  offset으로부터 파일의 끝까지)
	// return: 성공시 매핑된 뷰의 시작주소가 리턴, 실패시 NULL 리턴

	// *** 뷰 ***
	// - 파일을 주소공간에 매핑할 때 파일 전체를 한꺼번에 매핑할 수도 있고, 일부분만 매핑할 수도 있는데, 주소공간에 매핑된 영역을 뷰 라고 한다. 

	// input file을 매핑 실패시
	if (pInFile == NULL) {
		CloseHandle(hInMap); // hInMap 핸들 close
		CloseHandle(hIn); // hIn 핸들 close
		*error = 4; // error가 가리키는 변수의 값을 4로 설정
		return NULL;
	}

	// GetFileSizeEx를 통해 input file 크기를 얻음, 이 갑싱 0 이 아니거나 HighPart가 0이 아닐 경우
	if (!GetFileSizeEx(hIn, &fileSize) || fileSize.HighPart != 0) {
		UnmapViewOfFile(pInFile); // 주소 공간으로부터 파일 매핑 오브젝트의 매핑을 해제
		CloseHandle(hInMap);// hInMap 핸들 close 
		CloseHandle(hIn);//hIn 핸들 close
		*error = 5; //error가 가리키는 변수의 값을 5로 바꾼다
		return NULL;
	}

	*pFsLow = fileSize.LowPart; // pFsLow가 가리키는 변수의 값을 fileSize->LowPart로 지정

	pmFH->pInFile = pInFile; // pmFH->pInFile을 지역변수 pInFile로 변경
	pmFH->hInMap = hInMap; // pmFH->hInMap를 지역변수 pInMap으로 변경
	pmFH->hIn = hIn; // pmFH->hIn를 지역변수 pIn으로 변경

	return pInFile; // pInFile 리턴
}

// 파일 매핑 해제하는 함수
void UnMapFile(MAPPED_FILE_HANDLE* pmFH)
{
	UnmapViewOfFile(pmFH->pInFile); // 프로세스의 주소 공간으로부터 파일 매핑 오브젝트의 매핑을 해제한다.
	CloseHandle(pmFH->hInMap); // pmFH->hInMap 핸들 close
	CloseHandle(pmFH->hIn); // pmFH->hIn 핸들 close
}

// 콜백함수 wcfunc 정의
VOID CALLBACK wcfunc(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)

{
	WORK_OBJECT_ARG* threadArgs; // WORK_OBJECT_ARG 구조체 포인터 변수 threadArgs 선언
	MAPPED_FILE_HANDLE fhandle; // MAPPED_FILE_HANDLE 구조체 변수 fhandle 선언
	
	int iThrd; // 몇번째 스레드인지

	unsigned int ch, c, nl, nw, nc; // unsigned int형 변수 ch, c, nl, nw, nc 선언
	int isspace_c;       // 현재 character 변수
	int isspace_ch = 1;  // 이전 character 변수 isspace_ch 를 선언, 1로 초기화

	int error; // error 변수 선언
	unsigned int fsize; // file size
	char* fin, * fend; // char 포인터 변수 fin, fend 선언


	iThrd = InterlockedIncrement(&WorkerId) - 1;
	// WorkerId(공유자원)를 1씩 증사시킨다. (0부터 시작하므로 1빼줌)
	// 멀티 스레드 환경에서 다른 스레드에 의해 방해받지 않기 위해 InterlockedIncrement로 묶어주는 것 
	// 단, 값을 2만큼 증가시키기 위해서 InterlockedIncrement() 함수를 2번 호출하지않고, InterlockedExchangeAdd() 함수를 사용해야한다.


	threadArgs = (WORK_OBJECT_ARG*)Context; // 콜백 함수에 줄 인자 블록 Context로 할당
	threadArgs->wcerror = 1; // 현재 에러 값을 1로 설정한다.

	fin = (char*)map_file(threadArgs->filename, &fsize, &error, &fhandle); // map_file 함수를 통해 파일 매핑
	
	if (NULL == fin) { // 파일 매핑에 실패하면 리턴
		return;
	}

	fend = fin + fsize; // fend = fin + fsize (즉, 파일의 끝을 가리킴)

	ch = nw = nc = nl = 0; // ch, nw, nc, nl의 값을 0으로 할당

	// fin이 fend보다 작을 경우 (즉, 파일의 끝까지 갈때까지 while 문 수행)
	while (fin < fend) {
		c = *fin++; // fin이 가리키고 있는 변수의 값을 1 증가하고 c에 대입한다.
		isspace_c = is_a_space(c); // c가 공백인지 아닌지 판별하여 공백일경우 1, 아닐경우 0을 isspace_c에 대입한다. 
		if (!isspace_c && isspace_ch) { // 현재 character는 공백이 아니고, 이전 character는 공백일경우 nw를 1 증가시킴 (즉, 이전이 공백이라는 것은 새로운 단어로 인식)
			nw++;
		}
		isspace_ch = isspace_c; // isspace_c의 값을 isspace_ch에 대입 (즉, 다음 루프를 위해 현재 character값을 이전 character를 담고 있는 변수  isspace_ch에 넣어줌)
		
		// c가 줄바꿈이면, line이 바뀐 것이므로 nl을 1 증가시킴
		if (c == '\n') {
			nl++; 
			isspace_ch = 1; // isspace_ch를 1로 설정하여 새로운 단어에 대해서 보기 시작
		}
	}

	UnMapFile(&fhandle); // 파일 매핑 오브젝트의 매핑 해제
	threadArgs->kchar = fsize; // 해당 함수를 호출하는 thread의 인자의 kchar를 지역변수  fsize로 할당받음
	threadArgs->kword = nw;// 해당 함수를 호출하는 thread의 인자의 kword를 지역변수 nw로 할당받음
	threadArgs->kline = nl;// 해당 함수를 호출하는 thread의 인자의 kline을 지역변수 nl로 할당받음
	threadArgs->wcerror = 0; // 해당 함수를 호출하는 thread의 인자의 wcerror를 0으로 할당받음 (끝까지 수행했으므로 에러는 0)
	return;
}