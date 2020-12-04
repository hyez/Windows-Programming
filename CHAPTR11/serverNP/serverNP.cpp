/*  Chapter 11. ServerNP.
 *	Multi-threaded command line server. Named pipe version
 *	Usage:	Server [UserName GroupName]
 *	ONE THREAD AND PIPE INSTANCE FOR EVERY CLIENT. */

#include "Everything.h"
#include "ClientServer.h" /* Request and Response messages defined here */
#include <VersionHelpers.h>
# include <iostream>
using namespace std;

// 클라이언트 하나당 스레드 1개. 즉, 서버 스레드 구조체 1개
typedef struct {				/* Argument to a server thread. */
	HANDLE hNamedPipe;	 // 서버쪽에서 받아야하는 named pipe 핸들
	DWORD threadNumber; // 스레드 개수
	char tempFileName[MAX_PATH]; // 서버 스레드의 임시 파일 이름
} THREAD_ARG;

typedef THREAD_ARG * PTHREAD_ARG; // THREAD_ARG *를 PTHREAD_ARG로 typedef 정의함


volatile static LONG shutDown = 0; //종료 되어야 할때 1로 바뀌기 위해서 shutDown을 0으로 설정
static DWORD WINAPI Server(PTHREAD_ARG); // Server 함수 선언
static DWORD WINAPI Connect(PTHREAD_ARG); // Connect 함수 선언
static DWORD WINAPI ServerBroadcast(LPLONG); // ServerBroadcast 함수 선언
static BOOL  WINAPI Handler(DWORD); // Handler 함수 선언 (ctrl+c를 누르면 종료 되는거 막기 위해)
static CHAR shutRequest[] = "$ShutDownServer"; // shutRequest[] 초기화 
static THREAD_ARG threadArgs[MAX_CLIENTS]; // threadArgs 선언 (MAX_CLIENTS는 4)

int main(int argc, char *argv[])
{
	HANDLE hNp, hMonitor, hSrvrThread[MAX_CLIENTS]; // 각 hNP, hMonitor, hSrvrThread 핸들 선언
	DWORD iNp; // Named pipe index iNp 선언
	UINT monitorId, threadId;  // monitorId, threadId 선언
	DWORD AceMasks[] =	/* Named pipe access rights - described in Chapter 15 */
	{ STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0X1FF, 0, 0 };
	LPSECURITY_ATTRIBUTES pNPSA = NULL;  // pNPSA를 NULL로 초기화 (상속을 NULL로설정, 즉, 서버는 누구한테 상속 안할 것을 의미)

	//  윈도우 8보다 아래면 에러 출력
	if (!IsWindows8OrGreater())
		ReportError("You need at least Windows 8. Your Version is Not Supported", 1, FALSE);

	/* Console control handler to permit server shut down by Ctrl-C or Ctrl-Break */
	/*
	 SetConsoleCtrlHandler 함수
	 application-defined HandlerRoutine function을 추가할때 사용(컨트롤 핸들러 등록)
	 처리기 함수를 지정 하지 않으면 함수는 호출 프로세스에서 CTRL + C 신호를 무시 하는지 여부를 결정 하는 상속 가능한 특성을 설정 합니다.

	 1. HandlerRoutine: 추가 하거나 제거할 응용 프로그램 정의 HandlerRoutine 함수에 대 한 포인터
	 2. Add: 이 매개 변수가 TRUE 이면 처리기가 추가 됩니다. FALSE 이면 처리기가 제거 됩니다. (TRUE: 추가)

	 return value: 함수가 성공 하면 0이 아닌 값이 반환 됩니다.

	 */
	if (!SetConsoleCtrlHandler(Handler, TRUE))
		ReportError("Cannot create Ctrl handler", 1, TRUE);
	cout << "Ctrl handler 생성에 성공했습니다." << endl;


	cout << "thread broadcast pipe name을 생성합니다. " << endl;
	// _beginthreadex: 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달(ServerBroadcast)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정 (NULL)
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 (0: 즉시 실행)
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (monitorId)
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
	hMonitor = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)ServerBroadcast, NULL, 0, &monitorId);
	


	/*	Create a pipe instance for every server thread.
	 *	Create a temp file name for each thread.
	 *	Create a thread to service that pipe. */

	cout << "모든 서버 스레드마다 pipe instance, temp file name를 생성합니다." << endl;
	cout << "MAX_CLIENTS=4만큼 스레드를 생성합니다." << endl;
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		/*
		CreateNamedPipeA: CreateNamedPipe를 통해 Named Pipe의 인스턴스를 생성하고 파이프를 제어하기 위한 핸들러를 반환하는 함수
		1. lpName: 유일한 파이프의 이름으로 다음의 포맷을 가짐 ("\\\\.\\PIPE\\SERVER")
		 2. dwOpenMode: 파이프의 접근 방식과 권한 설정.  PIPE_ACCESS_DUPLEX로 읽기&쓰기 모드로 서버와 클라이언트는 양방향으로 읽고 쓸 수 있다. 서버는 GENERIC_READ, GENERIC_WRITE 파일 접근권한과 동일한 효과를 가진다
		 3. dwPipeMode: 파이프의 데이터 통신 방식을 설정. PIPE_READMODE_MESSAGE | PIPE_TYPE_MESSAGE | PIPE_WAIT
		 4. nMaxInstances: 생성할 수 있는 최대 파이프 갯수 -> MAX_CLIENTS = 4로 설정
		 5. nOutBufferSize: 출력 버퍼 사이즈. 0은 default 사용
		 6. nInBufferSize: 입력 버퍼 사이즈. 0은 default 사용
		 7. nDefaultTimeout:  INFINITE로 설정
		 8. lpSecurityAttributes: 보안속성 (pNPSA로 설정)
		 return value : 실패시 INVALID_HANDLE_VALUE 리턴
		*/
		cout << "named pipe를 생성합니다." << endl;
		hNp = CreateNamedPipeA(SERVER_PIPE, PIPE_ACCESS_DUPLEX,
			PIPE_READMODE_MESSAGE | PIPE_TYPE_MESSAGE | PIPE_WAIT,
			MAX_CLIENTS, 0, 0, INFINITE, pNPSA);

		cout << "named pipe 생성에 실패했으면 에러를 출력합니다." << endl;
		if (hNp == INVALID_HANDLE_VALUE)
			ReportError("Failure to open named pipe.", 1, TRUE);
		cout << "named pipe 생성에 완료했습니다." << endl;

		// 같은 스레드로 여러 서버랑 통신을 할 수 없으므로 스레드에게 타입핸들을 넘긴다.
		threadArgs[iNp].hNamedPipe = hNp;  // threadArgs[iNp].hNamedPipe를 hNp로 할당
		threadArgs[iNp].threadNumber = iNp; // threadArgs[iNp].threadNumber를 iNP로 할당

		/*
		* GetTempFileNameA: 디스크에 크기가 0바이트인 고유한 이름의 임시 파일을 만들고 해당 파일의 전체 경로를 반환합니다.
		 1. lpPathName: 현재 directory path로 설정 (".")
		 2. lpPrefixString: prefix string 설정. "CLP"
		 3. uUnique: 임시 파일을 생성할 때 사용되는 unsigned int(0으로 설정해서 현재 시스템 시간을 이용해서 unique file name을 만듬)
		 4. lpTempFileName: 임시 파일이름을 받는 버퍼의 포인터 (threadArgs[iNp].tempFileName로 설정) 
		 */

		GetTempFileNameA(".", "CLP", 0, threadArgs[iNp].tempFileName);

		// _beginthreadex: 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달(Server)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정 (threadArgs[iNp])
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 (0: 즉시 실행)
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (threadId)
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		cout << "서버 스레드를 생성합니다. 실패시 에러를 출력합니다." << endl;
		hSrvrThread[iNp] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Server,
			&threadArgs[iNp], 0, &threadId);


		if (hSrvrThread[iNp] == NULL)
			ReportError("Failure to create server thread.", 2, TRUE);
		cout << "서버 스레드 생성에 성공했습니다." << endl;
	}

	cout << "스레드 4개를 모두 생성했습니다." << endl;
	cout << "4개의 스레드가 모두 끝나기를 무한정 기다립니다." << endl;
	WaitForMultipleObjects(MAX_CLIENTS, hSrvrThread, TRUE, INFINITE);
	printf("All Server worker threads have shut down.\n");
	cout << "모든 서버 스레드가 종료되었습니다. " << endl;

	cout << "모니터 스레드가 끝날때까지 기다립니다." << endl;
	WaitForSingleObject(hMonitor, INFINITE);
	cout << "모니터 스레드가 종료되었습니다." << endl;
	printf("Monitor thread has shut down.\n");

	CloseHandle(hMonitor); // 모니터 핸들 close

	// 모든 파이프 핸들들을 닫고 임시 파일들 삭제함
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		/* Close pipe handles and delete temp files */
		/* Closing temp files is redundant, as the worker threads do it */
		CloseHandle(hSrvrThread[iNp]);
		DeleteFileA(threadArgs[iNp].tempFileName);
	}

	printf("Server process will exit.\n");
	return 0;
}


static DWORD WINAPI Server(PTHREAD_ARG pThArg)

/* Server thread function. There is a thread for every potential client. */
{
	/* Each thread keeps its own request, response,
		and bookkeeping data structures on the stack.
		Also, each thread creates an additional "connect thread"
		so that the main worker thread can test the shut down flag
		periodically while waiting for a client connection. */

	HANDLE hNamedPipe, hTmpFile = INVALID_HANDLE_VALUE, hConTh = NULL, hClient; // 각 hNamedPipe, hTmpFile, hConTh, hClient 핸들들을 선언 및 초기화
	DWORD nXfer, conThStatus, clientProcessId; // nXfer, conThStatus, clientProcessId 선언
	STARTUPINFO startInfoCh; // STARTUPINFO 구조체 startInfoCh선언
	SECURITY_ATTRIBUTES tempSA = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE }; // 보안 속성 설정
	PROCESS_INFORMATION procInfo; // 프로세스 info 구조체 선언
	FILE *fp; // 파일 오픈시 사용하는 변수
	REQUEST request; // request 변수 선언
	RESPONSE response; // response 변수 선언
	char clientName[256]; //clientName 변수 선언

	GetStartupInfo(&startInfoCh);  // startInfoCh에 표준 입출력 장치에 대한 핸들을 얻어옴
	hNamedPipe = pThArg->hNamedPipe; // hNamedPipe에 pThArg->hNamedPipe 할당

	cout << "startInfoCh로 현재 실행된 프로세스의 정보를 가져옵니다." << endl;
	// shutDown이 0인동안 루프를 반복함
	while (!shutDown) { 	/* Connection loop */

		/* Create a connection thread, and wait for it to terminate */
		/* Use a timeout on the wait so that the shut down flag can be tested */
		// _beginthreadex: hConTh 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달(Connect)
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정 (pThArg)
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 (0: 즉시 실행)
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (NULL)
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
		hConTh = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Connect, pThArg, 0, NULL);

		cout << "Connect 스레드를 생성합니다. 실패시 에러를 출력합니다." << endl;
		if (hConTh == NULL) {
			ReportError("Cannot create connect thread", 0, TRUE);
			_endthreadex(2);
		}
		cout << "Connect 스레드 생성에 성공했습니다." << endl;

		/* Wait for a client connection. */
		// Connect 스레드 핸들이 5초동안 signaled state될때까지 기다림
		cout << "client connection을 기다립니다." << endl;
		while (!shutDown && WaitForSingleObject(hConTh, CS_TIMEOUT) == WAIT_TIMEOUT)
		{ /* Empty loop body */
		}

		// shutDown이 0인경우 continue, 몇번째 스레드가 sutdown된건지 출력함
		if (shutDown) printf("Thread %d received shut down\n", pThArg->threadNumber);
		if (shutDown) continue;	/* Flag could also be set by a different thread */

		CloseHandle(hConTh); hConTh = NULL; // hConTh핸들을 닫음
		/* A connection now exists */

		/* GetNamedPipeClientComputerNameA
		* connection에 성공하면  GetNamedPipeClientComputerNameA를 통해 named pipe에 대한 client computer name을 얻어온다.
		1. Pipe: named pipe pThArg -> hNamedPipe
		2. ClientComputerName: 컴퓨터 이름 clientName
		3. ClientComputerNameLength: ClientComputerName 버퍼 사이즈
		클라이언트에 클라이언트 이름을 적어줌
		*/
		cout << "client computer name를 얻어오고, 실패시 clientName을 localhost로 할당합니다. " << endl;
		if (!GetNamedPipeClientComputerNameA(pThArg->hNamedPipe, clientName, sizeof(clientName))) {
			strcpy_s(clientName, sizeof(clientName) / sizeof(CHAR) - 1, "localhost"); // clientName을 localhost로 채워줌
		}

		/* GetNamedPipeClientProcessId
		 * named pipe에 대한 client 프로세스 id를 얻어온다.
		 1. Pipe: named pipe에 대한 핸들 PThArg->hNamedPipe
		 2. ClientProcessId: process identifier인 &clientProcessId
		 */
		GetNamedPipeClientProcessId(pThArg->hNamedPipe, &clientProcessId);
		printf("Connect to client process id: %d on computer: %s\n", clientProcessId, clientName);

		/* ReadFile 함수
		* - 특수 파일 혹은 입출력 장치로 부터 데이터를 읽는다.
		 1. hFile: 파일, 파일 스트림, 물리디스크, 테이프 드라이브, 소켓과 같은 장치의 핸들.  ( hNamedPipe)
		 2. lpBuffer: 데이터를 저장하기 위한 버퍼를 가리키는 포인터 ( &request)
		 3. nNumberOfBytesToRead: 읽을 최대 바이트 크기 (RQ_SIZE)
		 4. lpNumberOfByteRead: 동기 입출력 모드에서, 읽어들인 데이터의 바이트 수를 넘긴다 (&nXfer)
		 5. lpOverlapped: 동기 입출력이면 NULL을 사용
		 return value : 성공하면 0이 아닌 값을 리턴한다. 함수가 실패할때 혹은 비동기 입출력을 완료했을 때도 0을 반환한다.
		 */
		// shutDown이 0이 아니고 ReadFile을 통해 파일을 읽었으면 루프를 반복한다.
		while (!shutDown && ReadFile(hNamedPipe, &request, RQ_SIZE, &nXfer, NULL)) {
			// client가 연결이 끊길때까지 새로운 command를 계속해서 받는것
			printf("Command from client thread: %d. %s\n", clientProcessId, request.record); 

			shutDown = shutDown || (strcmp((const char *)request.record, shutRequest) == 0); //record안에 들어온 명령이 shutRequest인지 비교한거와 shutdown 둘중 하나라도 1이면 shutdown이 1이 됨
			if (shutDown)  continue; // shutdown이 1이면 continue

			/* Open the temporary results file used by all connections to this instance. */
			/* 
			* CreateFileA 함수: 파일이나 입출력 장치를 연다. 대표적인 입출력 장치로는 파일, 파일 스트림, 디렉토리, 물리적인 디스크, 볼륨, 콘솔 버퍼, 테이브 드라이브, 파이프 등이 있다.
			* 이 함수는 각 장치를 제어할 수 있는 handle(핸들)을 반환한다.
			* ThArg->tempFileName에 대해서 임시 파일 open
			1. lpFileName: 열고자 하는 파일 이름 (pThArg->tempFileName)
			2. dwDesiredAccess: 접근 방법을 명시하기 위해서 사용한다. 일반적으로 GENERIC_READ, GENERIC_WRITE 혹은 GENERIC_READ|GENERIC_WRITE를 사용한다. (GENERIC_READ | GENERIC_WRITE)
			3. dwShareMode: 개체의 공유 방식을 지정한다. 0을 지정하면 공유할 수 없는 상태가 되고, 핸들이 닫히기 전까지 다른 열기는 실패하게 된다. (FILE_SHARE_READ | FILE_SHARE_WRITE)
			4. lpSecurityAttributes: &tempSA
			5. dwCreationDisposition: 파일의 생성방식을 명시한다. (CREATE_ALWAYS)
			6. dwFlagsAndAttributes: 파일의 기타 속성을 지정한다. (FILE_ATTRIBUTE_TEMPORARY)
			7. hTemplateFile: 생성된 파일에 대한 속성을 제공하는 템플릿 (NULL)

			return value : 성공하면 파일에 대한 핸들을 반환한다. 실패하면 INVALID_HANDLE_VALUE를 반환한다.
			*/
			hTmpFile = CreateFileA(pThArg->tempFileName, GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, &tempSA,
				CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);

			// 임시파일 open에 실패시 에러 출력함
			if (hTmpFile == INVALID_HANDLE_VALUE) { /* About all we can do is stop the thread */
				ReportError("Cannot create temp file", 0, TRUE);
				_endthreadex(1); // 스레드 종료
			}

			/* Main command loop */
			/* Create a process to carry out the command. */
			startInfoCh.hStdOutput = hTmpFile ; // startInfoCh.hStdOutput에 hTmpFile 할당
			startInfoCh.hStdError = hTmpFile; // hTmpFile을 startInfoCh 구조체의 hStdError에 할당
			startInfoCh.hStdInput = GetStdHandle(STD_INPUT_HANDLE);//표준 입력 장치에 대한 핸들을 startInfoCh.hStdInput에 할당
			startInfoCh.dwFlags = STARTF_USESTDHANDLES; // 프로세스에 대한 표준 출력 핸들이 되도록 startInfoCh.dwFlags 설정

			/*
			* CreateProcessA: 프로세스 생성
			*  프로세스 생성에 실패할 경우 에러 출력하고  procInfo.hProcess를 NULL로 설정
			1. lpApplicationName : 프로세스를 생성할 실행 파일의 이름 (NULL로 설정)
			2. lpCommandLine : 명령행 인수.  request.record 로 설정
			3. pProcessAttributes : 프로세스 보안 속성 NULL로 설정 = 기본 보안 디스크립터
			4. lpThreadAttributes : 스레드 보안속성 NULL로 설정 = 기본 보안 디스크립터 사용
			5. bInheritHandles : 자식 프로세스가 생성될 때 보안 디스크립터 설정들을 상속시켜 줄 것인지 여부 (TRUE: 상속가능)
			6. dwCreationFlags : 프로세스의 플래그를 0으로 설정
			7. lpEnvironment : 생성될 프로세스가 사용할 환경 변수의 포인터를 NULL로 설정해 부모 프로세스의 환경 블록을 상속받음
			8. lpCurrentDirectory : &startInfoCh로 설정
			9. lpProcessInformation: &procInfo로 설정
			*/
			// 프로세스 생성 -> 프로세스 생성에 실패할 경우 에러 메세지 출력 및 procInfo의 hProcess를 NULL로 설정
			if (!CreateProcessA(NULL, (LPSTR)request.record, NULL,
				NULL, TRUE, /* Inherit handles. */
				0, NULL, NULL, (LPSTARTUPINFOA)&startInfoCh, &procInfo)) {
				PrintMsg(hTmpFile, "ERR: Cannot create process.");
				procInfo.hProcess = NULL;
			}

			CloseHandle(hTmpFile); // hTmpFile 핸들을 닫음

			// 서버 프로세스가 running 중이라면 (procInfo.hProcesss가 NULL이 아니라면) => 즉 제대로 만들어 진 것 !
			if (procInfo.hProcess != NULL) { /* Server process is running */
				CloseHandle(procInfo.hThread); // procInfo.hThread 스레드 핸들 닫음
				WaitForSingleObject(procInfo.hProcess, INFINITE); // procInfo.hProcess가 signaled state까지 무한정 대기
				CloseHandle(procInfo.hProcess); //  signaled state 되면 프로세스 핸들 닫음
			}

			/* Respond a line at a time. It is convenient to use
				C library line-oriented routines at this point. */
			// 프로세스 끝났으니까 결과는 임시 파일 안에 들어가 있음. 이제 이 임시파일을 읽어줌 (클라이언트에 넘기기 위해)
			if (fopen_s(&fp, pThArg->tempFileName, "r") != 0) {
				printf("Temp output file is: %s.\n", pThArg->tempFileName);
				perror("Failure to open command output file.");
				break;  /* Try the next command. */
			}

			/* Avoid an "information discovery" security exposure. */
			/* ZeroMemory(&response, sizeof(response));  */
			// 문자열로 최대길이MAX_RQRS_LEN= 4096만큼 읽어서 response.record에 채움
			while (fgets((char *)response.record, MAX_RQRS_LEN, fp) != NULL) {
				response.rsLen = (LONG32)(strlen((const char *)response.record) + 1); // +1 해주니까 1이면 NULL인것
				WriteFile(hNamedPipe, &response, response.rsLen + sizeof(response.rsLen), &nXfer, NULL); // 임시 결과의 내용까지 hNamedPipe에다 실제 내용을 쓴다
			}
			/* Write a terminating record. Messages use 8-bit characters, not UNICODE */
			response.record[0] = '\0'; // response.record[0]를 NULL로 할당
			response.rsLen = 0; //response.rsLen을 0으로 할당
			WriteFile(hNamedPipe, &response, sizeof(response.rsLen), &nXfer, NULL); // response 포인터가 시작되는 곳부터 sizeof(response.rsLen)만큼 hNamedPipe를 채움

			FlushFileBuffers(hNamedPipe); // FlushFileBuffers를 통해 hNamedPipe 기록
			fclose(fp); // fp 닫음

		}   /* End of main command loop. Get next command */

		/* Client has disconnected or there has been a shut down requrest */
		/* Terminate this client connection and then wait for another */
		FlushFileBuffers(hNamedPipe); // FlushFileBuffers를 통해  다시 한번 hNamedPipe 기록
		DisconnectNamedPipe(hNamedPipe); // 연결된 파이프 클라이언트의 핸들을 닫음 (파이프 서버에서 수행하는 함수)
	}

	/*  Force the connection thread to shut down if it is still active */
	cout << "Connect 스레드가 아직 죽지 않았는지 확인합니다." << endl;
	if (hConTh != NULL) {
		GetExitCodeThread(hConTh, &conThStatus); // hConTh의 exit code를 얻어옴
		if (conThStatus == STILL_ACTIVE) { 
			// 여전히 active이면
			// 아직 Connect의 ConnectNamedPipe서 접속이 안된것으로 판단해서 종료시키기 위해서 create file로 가짜로 접속함 

			/*
			* CreateFileA 함수: SERVER_PIPE에 대해서 open
			1. lpFileName: 열고자 하는 파일 이름 SERVER_PIPE
			2. dwDesiredAccess: 접근 방법을 명시하기 위해서 사용한다. 일반적으로 GENERIC_READ, GENERIC_WRITE 혹은 GENERIC_READ|GENERIC_WRITE를 사용한다. (GENERIC_READ | GENERIC_WRITE)
			3. dwShareMode: 개체의 공유 방식을 지정한다. 0을 지정하면 공유할 수 없는 상태가 되고, 핸들이 닫히기 전까지 다른 열기는 실패하게 된다. 
			4. lpSecurityAttributes: NULL로 설정
			5. dwCreationDisposition: 파일의 생성방식을 명시한다. (OPEN_EXISTING)
			6. dwFlagsAndAttributes: 파일의 기타 속성을 지정한다. (FILE_ATTRIBUTE_NORMAL)
			7. hTemplateFile: 생성된 파일에 대한 속성을 제공하는 템플릿 (NULL)

			return value : 성공하면 파일에 대한 핸들을 반환한다. 실패하면 INVALID_HANDLE_VALUE를 반환한다.
			*/
			hClient = CreateFileA(SERVER_PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			// 파일 생성에 성공하면 클라이언트 핸들 닫음
			if (hClient != INVALID_HANDLE_VALUE) CloseHandle(hClient);

			WaitForSingleObject(hConTh, INFINITE); // 다시 종료되길 기다림. hConTh가 signaled state까지 무한정 대기
		}
	}


	printf("Thread %d shutting down.\n", pThArg->threadNumber);
	/* End of command processing loop. Free resources and exit from the thread. */
	CloseHandle(hTmpFile); ; // hTmpFile 핸들을 닫음
	hTmpFile = INVALID_HANDLE_VALUE; ; // hTmpFile 핸들에 INVALID_HANDLE_VALUE 할당함

	if (!DeleteFileA(pThArg->tempFileName)) { // 임시파일 pThArg->tempFileName 삭제
		ReportError("Failed deleting temp file.", 0, TRUE); // 임시파일 삭제에 실패하면 에러 출력함
	}

	// 현재 존제하는 스레드 몇개인지 출력함
	printf("Exiting server thread number %d.\n", pThArg->threadNumber);
	return 0;
}

// 클라이언트와 파이프로 접속하는것
static DWORD WINAPI Connect(PTHREAD_ARG pThArg)
{
	BOOL fConnect; // connect flag 변수 선언

	/* 
	ConnectNamedPipe 함수 : 서버에서 네임드 파이프로 연결하는 클라이언트 프로세스를 기다린다. 
	- 클라이언트가 createfile하면 (혹은 CallNamedPipe) Connect에서 ConnectNamedPipe로 접속함
	- 생성된 파이프에 대해 클라이언트로부터 접속 요청이 들어오기 를 기다리는 서버측 함수
	1. hNamedPipe : CreteNamedPipe로 만들어진 파이프 핸들러. ( pThArg->hNamedPipe로 설정)
	2. lpOverlapped: NULL 이면 동기식 접속으로 클라이언트와 접속이 될 때까지 기다림
	return value: 만약 함수가 성공하면 0이 아닌 값을 반환한다. 실패하면 0을 반환한다. 
	 */
	/*	Pipe connection thread that allows the server worker thread
		to poll the shut down flag. */
	fConnect = ConnectNamedPipe(pThArg->hNamedPipe, NULL);
	_endthreadex(0);  // 스레드 종료
	return 0;
}

// 자신이 만든 파이프 이름을 클라이언트들이 알 수 있도록  MailSlot을 이용해 배포해주는 server broadcast
// 클라이언트가 locate 서버 통신하면서 파이프 이름을 얻어옴
static DWORD WINAPI ServerBroadcast(LPLONG pNull)
{
	MS_MESSAGE MsNotify; // MsNotify 선언
	DWORD nXfer, iNp; // nXfer, iNp 선언
	HANDLE hMsFile; // 핸들 hMsFile 선언 

	/* Open the mailslot for the MS "client" writer. */
	// shutDown이 0인동안 루프를 반복함
	while (!shutDown) { /* Run as long as there are server threads */
		/* Wait for another client to open a mailslot. */
		Sleep(CS_TIMEOUT); // // CS_TIMEOUT=5초동안 sleep, 5초마다 mailslot에다가 서버 파이프 이름을 전송하기 위함
		/* SERVER_PIPE에 대해서 파일 open
		* CreateFileA 함수: 파일이나 입출력 장치를 연다. 대표적인 입출력 장치로는 파일, 파일 스트림, 디렉토리, 물리적인 디스크, 볼륨, 콘솔 버퍼, 테이브 드라이브, 파이프 등이 있다.
		* 이 함수는 각 장치를 제어할 수 있는 handle(핸들)을 반환한다.

		1. lpFileName: 열고자 하는 파일 이름 (MS_CLTNAME)
		2. dwDesiredAccess: 접근 방법을 명시하기 위해서 사용한다.GENERIC_WRITE으로 설정함
		3. dwShareMode: 개체의 공유 방식을 지정한다. 0을 지정하면 공유할 수 없는 상태가 되고, 핸들이 닫히기 전까지 다른 열기는 실패하게 된다. (FILE_SHARE_READ)
		4. lpSecurityAttributes: NULL 값으로 사용하지 않음
		5. dwCreationDisposition: 파일의 생성방식을 명시한다. (OPEN_EXISTING: 파일이 존재하면 연다. 만약 파일이나 장치가 존재하지 않으면, 에러 코드로 ERROR_FILE_NOT_FOUND (2)를 설정한다.)
		6. dwFlagsAndAttributes: 파일의 기타 속성을 지정한다. (FILE_ATTRIBUTE_NORMAL: 다른 속성을 가지지 않는다.)
		7. hTemplateFile: 생성된 파일에 대한 속성을 제공하는 템플릿 (NULL)

		return value : 성공하면 파일에 대한 핸들을 반환한다. 실패하면 INVALID_HANDLE_VALUE를 반환한다.
		*/
		hMsFile = CreateFileA(MS_CLTNAME, GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		// 파일 생성에 실패하면 conitnue (다시 루프돔)
		if (hMsFile == INVALID_HANDLE_VALUE) {
			continue;
		}

		/* Send out the message to the mailslot. */
		cout << "mailslot에 대한 메세지를 보냅니다." << endl;
		MsNotify.msStatus = 0; // MsNotify.msStatus를 0으로 할당
		MsNotify.msUtilization = 0; // MsNotify.msUtilization를 0으로 할당
		strncpy_s(MsNotify.msName, sizeof(MsNotify.msName) / sizeof(CHAR), SERVER_PIPE, _TRUNCATE); // SERVER_PIPE를MsNotify.msName으로 복사

		// WriteFile를 통해 MsNotify 포인터가 시작되는 곳부터 MSM_SIZE만큼 hMsFile를 채움
		if (!WriteFile(hMsFile, &MsNotify, MSM_SIZE, &nXfer, NULL))
			ReportError("Server MS Write error.", 13, TRUE);
		CloseHandle(hMsFile); // hMsFIle 핸들 닫음
	}

	/* Cancel all outstanding NP I/O commands. See Chapter 14 for CancelIoEx */
	printf("Shut down flag set. Cancel all outstanding I/O operations.\n");
	/* This is an NT6 dependency. On Windows XP, outstanding NP I/O operations hang. */
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		CancelIoEx(threadArgs[iNp].hNamedPipe, NULL); // CancelIoEx 함수를 통해서 특정 비동기 I/O만을 취소
	}
	printf("Shuting down monitor thread.\n");

	_endthreadex(0); // 스레드 종료
	return 0;
}


// 시스템을 셧다운 함
BOOL WINAPI Handler(DWORD CtrlEvent)
{
	// handler 함수에 등록 안되어 있으면 ctrl+c 누를 경우 종료되는 것 막게 control handeler로 부터 shutDown 변수를 1 증가시킴  (동기화)
	printf("In console control handler\n");
	InterlockedIncrement(&shutDown); // ShutDown변수를 1 증가 (다른 스레드에서 쓰지 못하도록 InterlockedIncrement로 증가시킴)

	return TRUE;
}
