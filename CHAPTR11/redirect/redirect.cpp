#include "Everything.h"
# include <iostream>
using namespace std;
// 두개의 프로그램을 실행시켜서 첫번째의 output이 두번째의 input으로 가도록 redirect하는 것

int main(int argc, char * argv[])

/* Pipe together two programs whose names are on the command line:
		Redirect command1 = command2
	where the two commands are arbitrary strings.
	command1 uses standard input, and command2 uses standard output.
	Use = so as not to conflict with the DOS pipe. */
{
	DWORD i; 
	/* 
		hReadPipe: 파이프 reader 프로세스를 위한 핸들 
			- 파이프에 기록된 내용을 reader 프로세스가 읽어오는 것으로  두번째 프로그램인 find가 파이프에서 읽어올 것
	   hWritePipe: 파이프 writer 프로세스를 위한 핸들 
			- 파이프에 두번째 input으로 넘겨주기 위해서 파이프에 기록하기 위한 핸들임
	*/
	HANDLE hReadPipe, hWritePipe;
	CHAR command1[MAX_PATH]; // command line에서 입력받은것을 저장
	
	/* 
	pipeSA:	파이프를 상속 가능하도록 하기 위한 SECURITY_ATTRIBUTES 구조체변수 초기화
	 1. nLength : SECURITY_ATTRIBUTES의 크기로 설정 => sizeof(SECURITY_ATTRIBUTES)
	 2. IpSecurityDescriptor : NULL로 설정
	 3. bInheritable : TRUE로 설정. (pipeSA는 상속 가능한 핸들이라는걸 의미)
	 */
	SECURITY_ATTRIBUTES pipeSA = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };


	PROCESS_INFORMATION procInfo1, procInfo2; // 파이프 reader 프로세스,파이프 writer 프로세스 ( 2개의 프로세스)를 만들어야함으로 PROCESS_INFORMATION 구조체 2개 선언
	STARTUPINFO startInfoCh1, startInfoCh2; // grepMT, find 명령어를 사용하기 위해 STARTUPINFO 구조체 2개 선언
	const char *targv = GetCommandLineA(); // command line에서 입력 받은 것을 하나씩 분리해서 저장함

	/* Startup info for the two child processes. */

	cout << "* 현재 실행된 프로세스의 정보를 startInfoCh1 구조체로 가져옵니다." << endl;
	GetStartupInfo(&startInfoCh1);

	cout << "* 현재 실행된 프로세스의 정보를 startInfoCh2 구조체로 가져옵니다." << endl;
	GetStartupInfo(&startInfoCh2);

	cout << "command line에서 입력받은 값이 없으면 에러를 출력합니다." << endl;
	if (targv == NULL) // command line에서 입력받은 값이 없으면
		ReportError("\nCannot read command line.", 1, TRUE);


	targv = SkipArg(targv); // SkipArg는 첫번째 인자 다음을 가르킴 => targv에서 두번째 인자인 grepMT를 targv에 넣음

	i = 0;		

	cout << "command separator('=') 를 찾습니다." << endl;
	// 2개의 command를 얻는다
	// 만약 targv가 =이 아니고 NULL도 아니면 루프를 반복한다.
	while (*targv != '=' && *targv != '\0') { 
		command1[i] = *targv;  // command1[i] 에 targv 값 저장
		targv++; i++; // targv, i는 1씩 증가
	} 

	// while문을 빠져나옴
	// while문을 빠져나온건 command1은 입력한 문자열에서 = 전까지를 의미함 (=이면 빠져나오므로)
	command1[i] = '\0'; // command1의 끝에 NULL을 할당


	// targv 값이 NULL이면 =을 못찾은것을 의미
	// command separator('=') 을 못찾았다는 에러메세지를 출력한다.
	cout << "command separator('=') 를 못찾았으면 에러를 출력합니다." << endl;
	if (*targv == '\0') 
		ReportError("No command separator('=') found.", 2, FALSE);

	targv = SkipArg(targv); // 다음 인자인 find를 할당함

	// default size의 anonymous pipe를 생성
	// 핸들은 상속가능
	/*
	* CreatePipe: pipe 생성
	1. &hReadPipe : CreatePipe가 pipe reader process들을 위해 * phReadPipe를 채움
	2. &hWritePipe : 생성된 write handle이 채워질 포인터
	3. &pipeSA: 보안 속성을 설정(즉 상속 가능)
	4. 0: 파이프 바이트 사이즈를 디폴트로 설정
	*/
	cout << "anonymous pipe를 생성합니다. 생성에 실패하면 에러를 출력합니다." << endl;
	if (!CreatePipe(&hReadPipe, &hWritePipe, &pipeSA, 0))
		ReportError("Anon pipe create failed.", 3, TRUE);

	/* Set the output handle to the inheritable pipe handle,
		and create the first processes. */
	cout << "anonymous pipe 생성에 성공했습니다." << endl;

	cout << "상속가능한 핸들에 output 핸들을 설정하고, 첫번째 프로세스를 생성합니다." << endl;
	startInfoCh1.hStdInput = GetStdHandle(STD_INPUT_HANDLE); // 표준 입력 장치에 대한 핸들을  startInfoCh1.hStdInput 에 할당
	startInfoCh1.hStdError = GetStdHandle(STD_ERROR_HANDLE); // 표준 에러 장치에 대한 핸들을  startInfoCh1.hStdError 에 할당
	startInfoCh1.hStdOutput = hWritePipe; // hWritePipe를 startInfoCh1.hStdOutput에 할당
	startInfoCh1.dwFlags = STARTF_USESTDHANDLES; // 프로세스에 대한 표준 출력 핸들이 되도록 startInfoCh1.dwFlags 설정

	cout << "첫번째 프로세스를 생성합니다. 프로세스 생성에 실패 시 에러를 출력합니다." << endl;
	/*
	* CreateProcessA: 프로세스 생성
	1. lpApplicationName : 프로세스를 생성할 실행 파일의 이름 (NULL로 설정)
	2. lpCommandLine : 명령행 인수.  command1로(입력한 문자열에서 = 전까지를 의미) 설정
	3. pProcessAttributes : 프로세스 보안 속성 NULL로 설정 = 기본 보안 디스크립터
	4. lpThreadAttributes : 스레드 보안속성 NULL로 설정 = 기본 보안 디스크립터 사용
	5. bInheritHandles : 자식 프로세스가 생성될 때 보안 디스크립터 설정들을 상속시켜 줄 것인지 여부 (TRUE: 상속가능)
	6. dwCreationFlags : 프로세스의 플래그를 0으로 설정
	7. lpEnvironment : 생성될 프로세스가 사용할 환경 변수의 포인터를 NULL로 설정해 부모 프로세스의 환경 블록을 상속받음
	8. lpCurrentDirectory : NULL로 현재 프로세스가 실행중인 디렉터리로 설정
	9. lpProcessInformation: ((LPSTARTUPINFOA)&startInfoCh1, &procInfo1)로 설정
	*/
	if (!CreateProcessA(NULL, command1, NULL, NULL,
		TRUE,			/* Inherit handles. */
		0, NULL, NULL, (LPSTARTUPINFOA)&startInfoCh1, &procInfo1)) {
		ReportError("CreateProc1 failed.", 4, TRUE);
	}
	cout << "첫번째 프로세스 생성에 성공했습니다." << endl;

	CloseHandle(procInfo1.hThread); // 이제 사용을 안하므로 procInfo1.hThread를 close
	CloseHandle(hWritePipe); // 이제 사용을 안하므로 hWritePipe을 close

	/* Repeat (symmetrically) for the second process. */
	cout << "마찬가지로 두번째 프로세스를 생성합니다." << endl;
	startInfoCh2.hStdInput = hReadPipe; // startInfoCh2.hStdInput를 hReadPipe로 설정. 즉, 표준 입력이 파이프라인의 read용으로 find는 표준 입력 장치로 읽음
	startInfoCh2.hStdError = GetStdHandle(STD_ERROR_HANDLE);   // 표준 에러 장치에 대한 핸들을  startInfoCh2.hStdError 에 할당
	startInfoCh2.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); // 표준 출력 장치에 대한 핸들을  startInfoCh2.hStdOutput 에 할당
	startInfoCh2.dwFlags = STARTF_USESTDHANDLES; // 프로세스에 대한 표준 출력 핸들이 되도록 startInfoCh2.dwFlags 설정

	cout << "두번째 프로세스를 생성합니다. 프로세스 생성에 실패 시 에러를 출력합니다." << endl;
	/*
	* CreateProcessA: 프로세스 생성
	1. lpApplicationName : 프로세스를 생성할 실행 파일의 이름 (NULL로 설정)
	2. lpCommandLine : 명령행 인수.  targv로 설정 (find부터 의미)
	3. pProcessAttributes : 프로세스 보안 속성 NULL로 설정 = 기본 보안 디스크립터
	4. lpThreadAttributes : 스레드 보안속성 NULL로 설정 = 기본 보안 디스크립터 사용
	5. bInheritHandles : 자식 프로세스가 생성될 때 보안 디스크립터 설정들을 상속시켜 줄 것인지 여부 (TRUE: 상속가능)
	6. dwCreationFlags : 프로세스의 플래그를 0으로 설정
	7. lpEnvironment : 생성될 프로세스가 사용할 환경 변수의 포인터를 NULL로 설정해 부모 프로세스의 환경 블록을 상속받음
	8. lpCurrentDirectory : NULL로 현재 프로세스가 실행중인 디렉터리로 설정
	9. lpProcessInformation: ((LPSTARTUPINFOA)&startInfoCh2, &procInfo2)로 설정
	*/
	if (!CreateProcessA(NULL, (LPSTR)targv, NULL, NULL,
		TRUE,			/* Inherit handles. */
		0, NULL, NULL, (LPSTARTUPINFOA)&startInfoCh2, &procInfo2))
		ReportError("CreateProc2 failed.", 5, TRUE);

	cout << "두번째 프로세스 생성에 성공했습니다." << endl;

	CloseHandle(procInfo2.hThread); // 이제 사용을 안하므로 procInfo2.hThread를 close
	CloseHandle(hReadPipe); // 이제 사용을 안하므로 hReadPipe을 close

	/* Wait for both processes to complete.
		The first one should finish first, although it really does not matter. */

	cout << "첫번째 프로세스가 signaled state까지 무한정 대기합니다. " << endl;
	WaitForSingleObject(procInfo1.hProcess, INFINITE);
	cout << "두번째 프로세스가 signaled state까지 무한정 대기합니다. " << endl;
	WaitForSingleObject(procInfo2.hProcess, INFINITE);
	CloseHandle(procInfo1.hProcess); // procInfo1.hProcess를 닫음
	CloseHandle(procInfo2.hProcess); // procInfo2.hProcess를 닫음
	return 0;
}
