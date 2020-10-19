/* Chapter 7. grepMT. */
/* Parallel grep - Multiple thread version. */

/* grep pattern files.
	Search one or more files for the pattern.
	The results are listed in the order in which the
	threads complete, not in the order the files are
	on the command line. This is primarily to illustrate
	the non-determinism of thread completion. To obtain
	ordered output, use the technique in Program 8-1. */

	/* Be certain to define _MT in Environment.h
		or use the build...settings...C/C++...CodeGeneration...Multithreaded library. */

#include "Everything.h"
#include <iostream>
using namespace std;

// GREP_THREAD_ARG 구조체 선언
typedef struct {	/* grep thread's data structure. */
	int argc;
	char targv[4][MAX_COMMAND_LINE];
} GREP_THREAD_ARG;

typedef GREP_THREAD_ARG* PGR_ARGS;
static DWORD WINAPI ThGrep(PGR_ARGS pArgs);

int main(int argc, char** argv)

/* Create a separate THREAD to search each file on the command line.
	Report the results as they come in.
	Each thread is given a temporary file, in the current
	directory, to receive the results.
	This program modifies Program 8-1, which used processes. */
{
	PGR_ARGS  gArg;		/* Points to array of thread args. */ // 스레드 args
	HANDLE* tHandle;	/* Points to array of thread handles. */ // 스레드 핸들
	TCHAR commandLine[MAX_COMMAND_LINE];
	BOOL ok;
	DWORD threadIndex, exitCode;
	int iThrd, threadCount;
	STARTUPINFO startUp;
	PROCESS_INFORMATION processInfo;

	/* Start up info for each new process. */
	printf("표준 핸들이 위치한 곳과 같이 현재 프로세스의 실행환경에 관한 세부 정보를 담고 있는 구조체를 검색합니다.\n");
	// STARTUPINFO 구조체인 startUp의 정보를 가져옴
	GetStartupInfo(&startUp);

	printf("인자 개수가 3보다 작을 경우, 즉 파일 이름을 입력하지 않을 경우 에러를 출력합니다.\n");
	// grepmt pattern files 으로 입력해야함
	if (argc < 3)
		ReportError(_T("No file names."), 1, TRUE);

	/* Create a separate "grep" thread for each file on the command line.
		Each thread also gets a temporary file name for the results.
		argv[1] is the search pattern. */
	
	printf("스레드 생성을 위한 메모리를 할당합니다.\n");
	// 각 파일별로 thread를 생성하기 위함
	tHandle = (HANDLE*)malloc((argc - 2) * sizeof(HANDLE)); // 파일 수만큼 HANDLE 메모리 할당
	gArg = (PGR_ARGS)malloc((argc - 2) * sizeof(GREP_THREAD_ARG)); // 파일 수만큼 GREP_THREAD_ARG 메모리 할당

	printf("파일의 개수(%d)만큼 스레드를 생성합니다.\n", argc - 2);
	for (iThrd = 0; iThrd < argc - 2; iThrd++) {

		/* Set:	targv[1] to the pattern
			targv[2] to the input file
			targv[3] to the output file. */

		// argv[1]: pattern
		// argv[iThrd + 2]: 파일명
		strcpy_s(gArg[iThrd].targv[1], (const char*)argv[1]); // pattern을 gArg[iThrd]에 복사
		strcpy_s(gArg[iThrd].targv[2], (const char*)argv[iThrd + 2]);  // searchfile를 gArg[iThrd]에 복사
		

		// GetTempFileName
		//  -> 임시 파일을 만듬
		// <파라미터>
		//  1. "." : temp file 경로 (현재 디렉토리)
		//  2. "Gre" : temp file name prefix (임시파일 이름 앞에 Gre가 붙게됨)
		//  3. "0" : 1: 임시 파일의 이름만 형성, 0: 이름 뿐 아니라 해당 파일까지 생성
		//  4. gArg[iThrd].targv[3] : temp file name을 받을 buffer에 대한 포인터
		// return: 실패시 0,  성공시 임시파일 이름에서 사용된 unique numeric value return

		if (GetTempFileNameA   // temp file 생성
		(".", "Gre", 0, gArg[iThrd].targv[3]) == 0)
			ReportError(_T("Temp file failure."), 3, TRUE);

		/* Output file. */
		gArg[iThrd].argc = 4;

		printf("%d 번째 스레드를 생성합니다.\n", iThrd);
		/* Create a thread to execute the command line. */
		// _beginthreadex: 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달(ThGrep)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 (0: 즉시 실행)
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (NULL: 사용하지 않음)
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		tHandle[iThrd] = (HANDLE)_beginthreadex(
			NULL, 0, (_beginthreadex_proc_type)ThGrep, &gArg[iThrd], 0, NULL);
		
		// 실패할 경우 에러 출력
		if (tHandle[iThrd] == 0)
			ReportError(_T("ThreadCreate failed."), 4, TRUE);
	}

	/* Threads are all running. Wait for them to complete
		one at a time and put out the results. */
		/*  Redirect output for "cat" process listing results. */

	startUp.dwFlags = STARTF_USESTDHANDLES; //시작 플래그로 표준 입출력 핸들 지정
	startUp.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);  // 표준 출력 장치
	startUp.hStdError = GetStdHandle(STD_ERROR_HANDLE); // 표준 에러 장치

	// 스레드 개수 = 파일 개수
	threadCount = argc - 2; // 파일 개수 (인자에서 grepmp, pattern을 제외한 것)

	while (threadCount > 0) {
		// WaitForMultipleObjects
		//  -> 기다리기로 한 모든 객체가 신호 상태에 놓일 때까지 기다린다. 대기 시간을 정할 수도 있다.
		// <파라미터>
		//  1. DWORD nCount: 배열에 저장되어 있는 핸들 개수를 전달한다. (threadCount)
		//  2. const HANDLE *lpHandles: 핸들을 저장하고 있는 배열의 주소 정보를 전달한다. (tHandle)
		//  3. BOOL bWaitAll: 관찰 대상이 모두 Signaled 상태가 되기를 기다리고자하는지 아니면 하나라도 Signaled 상태가 되면 반환할것인지 결정한다. (FALSE: 하나라도 signal state가 되면 반환)
		//  4. DWORD dwMilliseconds: 커널오브젝트가 Singaled상태가 될때까지 기다릴 수 있는 최대 시간. 만약 상수 INFINITE를 인자로 전달하면 커널 오브젝트가 Signaled 상태가 될 때까지 반환하지 않고 무한정 대기함
		// <return>
		// - WAIT_FAILED : 대기 동작이 실패했음 ,GetLastError을 통해 자세한 정보를 얻음
		// - WAIT_OBJECT_0 : 커널 객체가 주어진 시간 간격 안에 시그널된 상태로 변함
		// - WAIT_TIMEOUT: 커널 객체가 시그널되기 전에 주어진 시간 초과 간격이 모두 지났음
		// - WAIT_ABANDONED_0 : 커널 객체가 뮤텍스를 사용했고, 뮤텍스를 소유한 스레드가 자발적으로 뮤텍스의 소유권을 하제하지 않고 종료
		printf("WaitForMultipleObjects를 사용하여 multi thread를 대기합니다.\n");
		threadIndex = WaitForMultipleObjects(threadCount, tHandle, FALSE, INFINITE); 

		iThrd = (int)threadIndex - (int)WAIT_OBJECT_0; // 대기 에러 검사를 위한 식, 결과값과 WAIT_OBJECT_0의 차를 구함
		if (iThrd < 0 || iThrd >= threadCount) 
			ReportError(_T("Thread wait error."), 5, TRUE);

		// 스레드의 상태(종료 여부)를 반환한다. 성공시 nonzero, 실패시 0 반환
		GetExitCodeThread(tHandle[iThrd], &exitCode);

		printf("스레드 핸들을 닫아줍니다.\n");
		// _beginthreadex 는 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		CloseHandle(tHandle[iThrd]); 

		/* List file contents (if a pattern match was found)
			and wait for the next thread to terminate. */

		// exitCode가 0이면 스레드 종료 실패 
		if (exitCode == 0) {
			// 파일이 1개 이상이면 파일명 출력
			if (argc > 3) {		/* Print  file name if more than one. */
				cout << gArg[iThrd].targv[2];
				fflush(stdout);
			}
			// cat gArg[iThrd].targv[3] 실행 
			_stprintf_s(commandLine, _T("cat \"%s\""), (char *)gArg[iThrd].targv[3]);

			// CreateProcess
			//  -> 프로세스를 생성하는 함수
			// <파라미터>
			//  1. LPCTSTR lpApplicationName: 프로세스를 생성할 실행파일 이름을 NULL
			//  2. LPSTR lpCommandLine: commandLine을 프로세스에게 전달
			//  3. LPSECURITY_ATTRIBUTES lpProcessAttributes: 프로세스 기본 보안 속성 (NULL로 지정)
			//  4. LPSECURITY_ATTRIBUTES lpThreadAttributes: 쓰레드 기본 보안 속성 (NULL로 지정)
			//  5. BOOL bInheritHandles: 상속 속성을 TRUE로 지정(핸들 상속)
			//  6. DWORD dwCreationFlags: 프로세스의 우선순위는 0으로 지정하지 않음
			//  7. LPVOID lpEnvironment: 생성하는 프로세스의 Environment Block 지정 (NULL: 부모 프로세스의 환경 블록 복사)
			//  8. LPCTSTR lpCurrentDirectory: 생성하는 프로세스의 현재 디렉터리 설정 (NULL: 부모프로세스의 현제 디렉터리)
			//  9. LPSTARTUPINFO lpStartupInfo: startUp 변수를 초기화한 후 변수의 포인터를 인수로 전달
			//  10. LPPROCESS_INFORMATION lpProcessInformation: 생성하는 프로세스 정보를 얻기 위한 것으로 PROCESS_INFORMATION 구조체 변수의 주소값을 인자로 전달
			// return: 성공시 nonzero 실패시 0
			printf(" 프로세스를 생성합니다.\n");
			ok = CreateProcess(NULL, commandLine, NULL, NULL,
				TRUE, 0, NULL, NULL, &startUp, &processInfo);


			// 프로세스 생성 실패시 에러 출력
			if (!ok) ReportError(_T("Failure executing cat."), 6, TRUE);

			printf("WaitForSingleObject를 사용하여 프로세스를 대기합니다.\n");
			// 지정한 오브젝트가 시그널 상태가 되거나 타임아웃이 되면 제어를 돌려준다.
			WaitForSingleObject(processInfo.hProcess, INFINITE); // INFINITE를 지정하면 오브젝트가 시그널 상태가 될때까지 무한정 기다린다.

			// 프로세스 핸들, 스레드 핸들을 닫아줌
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
		
		printf("temp file을 삭제합니다.\n");
		// DeleteFile를 사용해서 tempfile 삭제 (성공시 nonzero,  실패시 zero)
		if (!DeleteFileA(gArg[iThrd].targv[3]))
			ReportError(_T("Cannot delete temp file."), 7, TRUE);

		/* Move the handle of the last thread in the list
			to the slot occupied by thread that just completed
			and decrement the thread count. Do the same for
			the temp file names. */

		// 마지막 스레드의 핸들을 방금 완료한 스레드가 있는 슬롯으로 이동
		tHandle[iThrd] = tHandle[threadCount - 1];
		strcpy_s(gArg[iThrd].targv[3], gArg[threadCount - 1].targv[3]); // gArg[threadCount - 1].targv[3]를 gArg[iThrd].targv[3]에 복사 (while문을 돌아갈 스레드) 
		strcpy_s(gArg[iThrd].targv[2], gArg[threadCount - 1].targv[2]); // gArg[threadCount - 1].targv[2]를 gArg[iThrd].targv[2]에 복사 (while문을 돌아갈 스레드)
		threadCount--; // 스레드 count 감소

		cout << endl;
	}

	printf("메모리를 해제합니다.\n");
	free(tHandle);
	free(gArg);

	return 0;
}

/* Source code for grep follows and is omitted from text. */
/* The form of the code is:
	static DWORD WINAPI ThGrep (GR_ARGS pArgs)
	{
	}
*/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*	grep, written as a function to be executed on a thread. */
/*	Copyright 1995, Alan R. Feuer. */
/*	Modified version: The input and output file names
	are taken from the argument data structure for the file.
	This function uses the C library and therefore must
	be invoked by _beginthreadex. */

	/*	grep pattern file(s)
		Looks for pattern in files. A file is scanned line-by-line.

		These metacharacters are used:
			*	matches zero or more characters
			?	matches exactly one character
			[...]	specifies a character class
				matches any character in the class
			^	matches the beginning of the line
			$	matches the end of the line
		In addition, the standard C escape sequences are
		understood: \a, \b, \f, \t, \v */

		/*	Codes for pattern metacharacters. */

#define ASTRSK		1
#define QM		2
#define BEGCLASS	3
#define ENDCLASS	4
#define ANCHOR		5

FILE* openFile(char*, char*);
void prepSearchString(char*, char*);

BOOL patternMatch(char*, char*);

/* Other codes and definitions. */

#define EOS _T('\0')

/* Options for pattern match. */

static BOOL ignoreCase = FALSE;

static DWORD WINAPI ThGrep(PGR_ARGS pArgs)
{
	/* Modified version - the first argument is the
		pattern, the second is the input file and the
		third is the output file.
		argc is not used but is assumed to be 4. */

	char* file;
	int i, patternSeen = FALSE, showName = FALSE, argc, result = 1;
	char pattern[256];
	char string[2048];
	char argv[4][MAX_COMMAND_LINE];
	FILE* fp, * fpout;

	argc = pArgs->argc;
	strcpy_s(argv[1], pArgs->targv[1]);
	strcpy_s(argv[2], pArgs->targv[2]);
	strcpy_s(argv[3], pArgs->targv[3]);
	if (argc < 3) {
		puts("Usage: grep output_file pattern file(s)");
		return 1;
	}

	/* Open the output file. */

	fpout = openFile(file = (char*)argv[argc - 1], (char*)"wb");
	if (fpout == NULL) {
		printf("Failure to open output file.");
		return 1;
	}

	for (i = 1; i < argc - 1; ++i) {
		if (argv[i][0] == _T('-')) {
			switch (argv[i][1]) {
			case _T('y'):
				ignoreCase = TRUE;
				break;
			}
		}
		else {
			if (!patternSeen++)
				prepSearchString((char*)argv[i], pattern);
			else if ((fp = openFile(file = (char*)argv[i], (char*)"rb"))
				!= NULL) {
				if (!showName && i < argc - 2) ++showName;
				while (fgets(string, sizeof(string), fp)
					!= NULL && !feof(fp)) {
					if (ignoreCase) _strlwr_s(string);
					if (patternMatch(pattern, string)) {
						result = 0;
						if (showName) {
							fputs(file, fpout);
							fputs(string, fpout);
						}
						else fputs(string, fpout);
					}
				}
				fclose(fp);
				fclose(fpout);
			}
		}
	}
	return result;
}

static FILE*
openFile(char* file, char* mode)
{
	FILE* fp;
	errno_t r;

	/* printf ("Opening File: %s", file); */
	r = fopen_s(&fp, file, mode);
	if (r != 0)
		perror(file);
	return (fp);
}

static void
prepSearchString(char* p, char* buf)

/* Copy prep'ed search string to buf. */
{
	register int c;
	register int i = 0;

	if (*p == _T('^')) {
		buf[i++] = ANCHOR;
		++p;
	}

	for (;;) {
		switch (c = *p++) {
		case EOS: goto Exit;
		case _T('*'): if (i >= 0 && buf[i - 1] != ASTRSK)
			c = ASTRSK; break;
		case _T('?'): c = QM; break;
		case _T('['): c = BEGCLASS; break;
		case _T(']'): c = ENDCLASS; break;

		case _T('\\'):
			switch (c = *p++) {
			case EOS: goto Exit;
			case _T('a'): c = _T('\a'); break;
			case _T('b'): c = _T('\b'); break;
			case _T('f'): c = _T('\f'); break;
			case _T('t'): c = _T('\t'); break;
			case _T('v'): c = _T('\v'); break;
			case _T('\\'): c = _T('\\'); break;
			}
			break;
		}

		buf[i++] = (ignoreCase ? tolower(c) : c);
	}

Exit:
	buf[i] = EOS;
}

static BOOL
patternMatch(char* pattern, char* string)

/* Return TRUE if pattern matches string. */
{
	register char pc, sc;
	char* pat;
	BOOL anchored;

	if (anchored = (*pattern == ANCHOR))
		++pattern;

Top:			/* Once per char in string. */
	pat = pattern;

Again:
	pc = *pat;
	sc = *string;

	if (sc == _T('\n') || sc == EOS) {
		/* at end of line or end of text */
		if (pc == EOS) goto Success;
		else if (pc == ASTRSK) {
			/* patternMatch (pat + 1,base, index, end) */
			++pat;
			goto Again;
		}
		else return (FALSE);
	}
	else {
		if (pc == sc || pc == QM) {
			/* patternMatch (pat + 1,string + 1) */
			++pat;
			++string;
			goto Again;
		}
		else if (pc == EOS) goto Success;
		else if (pc == ASTRSK) {
			if (patternMatch(pat + 1, string)) goto Success;
			else {
				/* patternMatch (pat, string + 1) */
				++string;
				goto Again;
			}
		}
		else if (pc == BEGCLASS) { /* char class */
			BOOL clmatch = FALSE;
			while (*++pat != ENDCLASS) {
				if (!clmatch && *pat == sc) clmatch = TRUE;
			}
			if (clmatch) {
				++pat;
				++string;
				goto Again;
			}
		}
	}

	if (anchored) return (FALSE);

	++string;
	goto Top;

Success:
	return (TRUE);
}

