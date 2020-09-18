/* Chapter 6. grepMP. */
/* Multiple process version of grep command. */
/* grep pattern files.
	Search one or more files for the pattern.
	List the complete line on which the pattern occurs.
	Include the file name if there is more than one
	file on the command line. No Options. */

	/* This program illustrates:
		1. 	Creating processes.
		2. 	Setting a child process standard I/O using the process start-up structure.
		3. 	Specifying to the child process that the parent's file handles are inheritable.
		4. 	Synchronizing with process termination using WaitForMultipleObjects
			and WaitForSingleObject.
		5.	Generating and using temporary files to hold the output of each process. */

#include "Everything.h"

int _tmain(int argc, LPTSTR argv[])

/*	Create a separate process to search each file on the
	command line. Each process is given a temporary file,
	in the current directory, to receive the results. */
{	
	printf("\n<grepmp> 코드입니다.\n");
	printf("\n---kernel object를 사용하기 위해 HANDLE 구조체 변수 선언---\n");
	HANDLE hTempFile; // kernel object를 사용하기 위해 HANDLE 변수 선언

	printf("\n---핸들을 상속이 가능하도록 하는 SECURITY_ATTRIBUTES 변수 선언---\n");
	SECURITY_ATTRIBUTES stdOutSA = /* SA for inheritable handle. */
	{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE }; // 상속이 가능하도록 하게 하는것
	TCHAR commandLine[MAX_PATH + 100]; // TCHAR: char랑 똑같은데 multibyte 처리 가능한 타입

	printf("\n---생성하는 프로세스의 속성을 지정하기 위한 STARTUPINFO 변수 선언---\n");
	STARTUPINFO startUpSearch, startUp; // 표준 입출력장치를 미리 변경시키고 실행시키고 싶을 때 사용함
	// 생성하는 프로세스의 속성을 지정하기 위해 STARTUPINFO 구조체 변수 startUpSearch, startUp 선언

	printf("\n---생성된 프로세스와 기본 스레드에 대한 정보를 얻기 위해 PROCESS_INFORMATION 변수 선언---\n");
	PROCESS_INFORMATION processInfo; 
	// 생성된 프로세스와 기본 스레드에 대한 정보를 얻기 위해 PROCESS_INFORMATION 변수 선언
	// 넘겨 받을 HANDLE, THREAD ID에 대한 정보

	DWORD exitCode, dwCreationFlags = 0; 
	int iProc;
	HANDLE* hProc;  /* Pointer to an array of proc handles. */
	typedef struct { TCHAR tempFile[MAX_PATH]; } PROCFILE; //  임시 파일 이름의 배열, 인자로 들어온 파일 개수만큼 할당
	PROCFILE* procFile; /* Pointer to array of temp file names. */

#ifdef UNICODE
	dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;  // 자식 프로세스의 환경 블록 내 유니코드 문자열이 사용 가능하도록 함

#endif

	printf("\ncommand line에 입력한 내용이 'timep pattern file1 file2 file3...' 의 형태가 아닐 경우 에러 출력(최소 파일 1개의 입력. 즉, 3개의 인자를 받아야함)\n");
	if (argc < 3) // 최소 3개 이상을 입력받기 위해, 3보다 작으면 잘못되었습니다.
		ReportError(_T("Usage: grepMP pattern files."), 1, FALSE);
	else
		printf("--> Command line에 입력 확인 완료\n");

	/* Startup info for each child search process as well as
		the child process that will display the results. */

	printf("\n\n---calling process가 생성될때 STARTUPINFO 구조체 내용을 가져온다.---\n");
	// 표준 핸들이 위치한 곳과 같이 현재 프로세스의 실행환경에 관한 세부 정보를 담고 있는 구조체를 검색한다.
	GetStartupInfo(&startUpSearch);
	GetStartupInfo(&startUp);
	printf("\n\n---STARTUPINFO 구조체 내용 가져오기 완료---\n");
	/* Allocate storage for an array of process data structures,
		each containing a process handle and a temporary file name. */

	printf("\n---command line에서 입력받은 파일의 개수만큼 grep 생성하기 위한 메모리 할당---\n");
	procFile = (PROCFILE*)malloc((argc - 2) * sizeof(PROCFILE)); // 파일 개수만큼 담음
	hProc = (HANDLE*)malloc((argc - 2) * sizeof(HANDLE)); //  핸들을 담을 배열을 파일 개수만큼

	if (!procFile == NULL && !hProc == NULL)
		printf("\n---메모리 할당 완료---\n");
	/* Create a separate "grep" process for each file on the
		command line. Each process also gets a temporary file
		name for the results; the handle is communicated through
		the STARTUPINFO structure. argv[1] is the search pattern. */

	// 처리해야할 파일만큼 루프 돌기
	printf("\n---처리해야할 파일만큼 루프를 돌면서 grep 생성---\n");

	printf("\n처리해야할 파일 :%d\n", argc - 2 );
	for (iProc = 0; iProc < argc - 2; iProc++) {
		printf("\n\n\n[%d번째 iProc]\n", iProc);
		/* Create a command line of the form: grep argv[1] argv[iProc + 2] */
		/* Allow spaces in the file names. */
		// 커멘드라인 형성(grep 패턴 파일이름)
		wsprintf(commandLine, _T("grep \"%s\" \"%s\""), argv[1], argv[iProc + 2]); // _T : 멀티바이트 문자열로 변형해줌
		// 실행 시킬 명령어를 문자열로 만듬
		_tprintf(_T("%s\n"), commandLine);
		/* Create the temp file name for std output. */

		printf("\n-temp file 생성 준비-\n");
		if (GetTempFileName(_T("."), _T("gtm"), 0, procFile[iProc].tempFile) == 0) // 파일의 임시파일 이름에 대한 문자열
			// 임시파일 이름을 얻을 수 없으면 에러 출력
			ReportError(_T("Temp file failure."), 2, TRUE);
		_tprintf(_T("%s\n"), procFile[iProc].tempFile);
		/* Set the std output for the search process. */

		printf("\n-temp file 생성 시작-\n");
		printf("--> 성공하면 파일에 대한 핸들을 반환, 실패시 INVALID_HANDLE_VALUE 반환 (이 경우, 에처 출력)\n");
		hTempFile = /* This handle is inheritable */
			// 여기서 파일 생성함
			CreateFile(procFile[iProc].tempFile,
				/** GENERIC_READ | Read access not required **/ GENERIC_WRITE, // 쓰기 전용으로 만든다
				// FILE_SHARE_READ, FILE_SHARE_WRITE로 파일의 공유 모드를 지정한다
				FILE_SHARE_READ | FILE_SHARE_WRITE, &stdOutSA, // stdOutSA: 자식프로세스에게 상속이 될것 (상속 가능한 SECURITY_ATTRIBUTES)
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hTempFile == INVALID_HANDLE_VALUE)
			ReportError(_T("Failure opening temp file."), 3, TRUE);

		printf("\ntemp file 생성 성공 \n");

		/* Specify that the new process takes its std output
			from the temporary file's handles. */

		printf("\n---표준출력장치, 표준에러장치를 temp file로, handle을 표준 입력 장치로 변경---\n");
		// startupinfo에 새로 만들 프로세스(그랩)의 표준 입력, 출력, 에러출력장치를 변경함
		startUpSearch.dwFlags = STARTF_USESTDHANDLES;
		startUpSearch.hStdOutput = hTempFile; // 표준 출력장치
		startUpSearch.hStdError = hTempFile; // 표준 에러출력장치
		startUpSearch.hStdInput = GetStdHandle(STD_INPUT_HANDLE); // 표준 입력장치
		/* Create a process to execute the command line. */

		printf("\n---프로세스가 생성 되었는지 검사---\n");
		if (!CreateProcess(NULL, commandLine, NULL, NULL,
			TRUE, dwCreationFlags, NULL, NULL, &startUpSearch, &processInfo)) {
			printf("프로세스가 생성되지 않았음. 에러 출력.\n");
			ReportError(_T("ProcCreate failed."), 4, TRUE); // 에러 나면 에러 리포트하고 프로그램 종료
		}
		else
		{
			printf("\n프로세스가 생성됨\n");
		}
		/* Close unwanted handles */
		printf("\n---열려있는 object handle을 닫는다---\n단, 그 handle과 연결된 object가 종료되는 것은 아님\n");
		CloseHandle(hTempFile); CloseHandle(processInfo.hThread); // 에러 안나면 핸들 close
		printf("\n---CloseHandle 성공---\n");

		/* Save the process handle. */
		printf("\n---hProc[iProc]에 프로세스 핸들을 저장해둠---\n");
		hProc[iProc] = processInfo.hProcess; // grep 프로세스 핸들을 저장해둠
	}

	/* Processes are all running. Wait for them to complete, then output
		the results - in the order of the command line file names. */
	printf("\n---프로세스가 종료될때까지 대기---\n");
	// 다 끝날때까지 기다림
	for (iProc = 0; iProc < argc - 2; iProc += MAXIMUM_WAIT_OBJECTS)
		WaitForMultipleObjects(min(MAXIMUM_WAIT_OBJECTS, argc - 2 - iProc), // 남은 프로세스랑 MAXIMUM_WAIT_OBJECTS 중에 작은거 
			&hProc[iProc], TRUE, INFINITE); // &hProc[iProc] 중에 모든 프로세스가 종료가 되면 WaitForMultipleObjects에서 빠져나옴 
	// 남은 프로세스랑 MAXIMUM_WAIT_OBJECTS 중에 작은거 만큼 대기
	// 즉, 100 개가 들어가면 MAXIMUM_WAIT_OBJECTS(64)개 종료 될때까지 기다렸다가 종료된 후 나머지를 기다림 
	printf("\n---대기 종료---\n");
	/* Result files sent to std output using "cat".
		Delete each temporary file upon completion. */

	printf("\n---grep이 제대로 끝났는지 확인---\n--> exitCode가 0 일 경우 파일(들) 안에서 그 패턴을 찾은것이므로 제대로 끝난 것이다.\n");
	for (iProc = 0; iProc < argc - 2; iProc++) {
		printf("\n\n\n[%d번째 iProc]\n", iProc);
		if (GetExitCodeProcess(hProc[iProc], &exitCode) && exitCode == 0) { // exit코드를 받아보고 제대로 끝났는지 확인 (0이면 제대로 끝남)
			// 패턴을 찾은 결과가 있을 경우
			/* Pattern was detected - List results. */
			/* List the file name if there is more than one file to search */
			printf("\n---cat을 사용해 표준 출력 장치로 전달---\n");
			if (argc > 3) _tprintf(_T("%s:\n"), argv[iProc + 2]);
			swprintf_s(commandLine, MAX_PATH, _T("cat \"%s\""), procFile[iProc].tempFile); // cat으로 파일을 찾은 결과를 담은 임시파일이름 결과 출력함
			if (!CreateProcess(NULL, commandLine, NULL, NULL,
				TRUE, dwCreationFlags, NULL, NULL, &startUp, &processInfo))
				ReportError(_T("Failure executing cat."), 0, TRUE);
			else {
				WaitForSingleObject(processInfo.hProcess, INFINITE); // cat잘 끝났는지 무한정 기다림
				CloseHandle(processInfo.hProcess);
				CloseHandle(processInfo.hThread);
			}
		}

		printf("\n---열려있는 object handle을 닫는다(파일 갯수만큼 반복).단, 그 handle과 연결된 object가 종료되는 것은 아님---\n");
		CloseHandle(hProc[iProc]);

		// 임시파일 삭제 
		if (!DeleteFile(procFile[iProc].tempFile))
			ReportError(_T("Cannot delete temp file."), 6, TRUE);
		printf("\n---임시파일 삭제 완료---\n");
	}

	printf("\n\n\n\n\n---procFile, hProc 메모리 해제 완료---\n");
	free(procFile); free(hProc);

	return 0;
}

