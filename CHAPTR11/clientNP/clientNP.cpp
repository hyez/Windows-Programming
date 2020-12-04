/* Chapter 11 - Client/Server system. CLIENT VERSION.
	ClientNP - Connection-oriented client. Named Pipe version */
	/* Execute a command line (on the server) and display the response. */

	/*  The client creates a long-lived
		connection with the server (consuming a pipe instance) and prompts
		the user for the command to execute. */

		/* This program illustrates:
			1. Named pipes from the client side.
			2. Long-lived connections with a single request but multiple responses.
			3. Reading responses in server messages until the end of response is received.
		*/

		/* Special commands recognized by the server are:
			1. $Statistics: Give performance statistics.
			2. $ShutDownThread: Terminate a server thread.
			3. $Quit. Exit from the client. */

#include "Everything.h"
#include "ClientServer.h" /* Defines the resquest and response records */
# include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
	
	HANDLE hNamedPipe = INVALID_HANDLE_VALUE; // 서버쪽에서 받을 핸들 hNamedPipe 를  INVALID_HANDLE_VALUE로 초기화한다. 
	const char quitMsg[] = "$Quit"; // command line에서 입력받을때 Quit인지 아닌지 판단하기 위한 const 변수
	char serverPipeName[MAX_PATH + 1]; // 서버로부터 파이프 이름을 받기 위한 변수
	REQUEST request; // 클라이언트가 서버에 요청할 request 구조체
	RESPONSE response;	// 클라이언트가 서버에서 받을 response 구조체
	DWORD nRead, nWrite, npMode = PIPE_READMODE_MESSAGE | PIPE_WAIT; // nRead, nWrite, npMode 초기화

	std::cout << "클라이언트는 LocateServer 함수를 통해 서버가 만든 파이프이름을 받아옵니다. " << endl;
	LocateServer(serverPipeName, MAX_PATH); // 파라미터로 serverPipeName와 MAX_PATH를 넘겨준다. 

	// hNamedPipe이 INVALID_HANDLE_VALUE이면 계속해서 while문을 반복 (hNamedPipe 얻을때까지) 
	std::cout << "hNamedPipe를 얻을때까지 루프를 반복합니다." << endl;
	while (INVALID_HANDLE_VALUE == hNamedPipe) { /* Obtain a handle to a NP instance */
		
		/*
		* WaitNamedPipeA 함수: 지정된 시간내에 파이프로 연결을 기다린다. 지정된 시간내에 연결이 없으면 에러와 함께 반환한다. 
		* ConnectNamedPipe의 timeout 버전이다. 서버측 프로그램에서 사용한다.

		1. lpNamedPipeName: 파이프의 이름 (serverPipeName)
		2. nTimeOut: 기다릴 시간으로 밀리세컨드 단위다. (NMPWAIT_WAIT_FOREVER=연결이 있을 때까지 계속 기다린다)

		return value: 제한 시간내에 연결이 성공하면 0보다 큰 수를 반환한다. 만약 제한 시간내에 연결이 이루어지지 않닸다면 0을 반환한다. 
		*/
		std::cout << "파이프의 연결이 있을때까지 계속 기다립니다." << endl;
		if (!WaitNamedPipeA(serverPipeName, NMPWAIT_WAIT_FOREVER))
			ReportError("WaitNamedPipe error.", 2, TRUE);
		/*
		* CreateFileA 함수: 파일이나 입출력 장치를 연다. 대표적인 입출력 장치로는 파일, 파일 스트림, 디렉토리, 물리적인 디스크, 볼륨, 콘솔 버퍼, 테이브 드라이브, 파이프 등이 있다. 
		* 이 함수는 각 장치를 제어할 수 있는 handle(핸들)을 반환한다.

		1. lpFileName: 열고자 하는 파일 이름 (serverPipeName)
		2. dwDesiredAccess: 접근 방법을 명시하기 위해서 사용한다. 일반적으로 GENERIC_READ, GENERIC_WRITE 혹은 GENERIC_READ|GENERIC_WRITE를 사용한다. (GENERIC_READ | GENERIC_WRITE)
		3. dwShareMode: 개체의 공유 방식을 지정한다. 0을 지정하면 공유할 수 없는 상태가 되고, 핸들이 닫히기 전까지 다른 열기는 실패하게 된다. (0)
		4. lpSecurityAttributes: NULL 값으로 사용하지 않음
		5. dwCreationDisposition: 파일의 생성방식을 명시한다. (OPEN_EXISTING: 파일이 존재하면 연다. 만약 파일이나 장치가 존재하지 않으면, 에러 코드로 ERROR_FILE_NOT_FOUND (2)를 설정한다.)
		6. dwFlagsAndAttributes: 파일의 기타 속성을 지정한다. (FILE_ATTRIBUTE_NORMAL: 다른 속성을 가지지 않는다.)
		7. hTemplateFile: 생성된 파일에 대한 속성을 제공하는 템플릿 (NULL)

		return value : 성공하면 파일에 대한 핸들을 반환한다. 실패하면 INVALID_HANDLE_VALUE를 반환한다. 
		*/

		std::cout << "serverPipeName을 생성하고, hNamedPipe에 반환 값을 저장합니다." << endl;
		hNamedPipe = CreateFileA(serverPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		std::cout << "hNamedPipe이 잘 넘어왔는지 확인합니다. hNamedPipe 얻기에 실패했으면 다시 루프를 반복합니다." << endl;
	}

	/*  Read the NP handle in waiting, message mode. Note that the 2nd argument
	 *  is an address, not a value. Client and server may be on the same system, so
	 *  it is not appropriate to set the collection mode and timeout (last 2 args)
	 */
	 /* SetNamedPipeHandleState 함수 : 파이프의 속성을 변경시키는 함수 
	 * 서버와 똑같은 것으로 세팅하기 위해서 사용
	  1. hNamedPipe : 파이프와의 연결 속성을 변경시키기 위한 핸들 (hNamedPipe)
	  2. lpMode : 데이터 송·수신 모드와 함수의 리턴모드에 대한 값 전달 (&npMode)
	  3. lpMaxCollectionCount : 버퍼링할 수 있는 최대 바이트 크기, 클라이언트와 서버가 같은 PC일 경우 NULL을 전달 (NULL)
	  4. lpCollectDataTimeOut : 버퍼링을 허용하는 최대 시간(밀리세컨드), 클라이언트와 서버가 같은 PC일 경우 NULL을 전달 (NULL)
	  */
	if (!SetNamedPipeHandleState(hNamedPipe, &npMode, NULL, NULL))
		ReportError("SetNamedPipeHandleState error.", 2, TRUE);

	/* Prompt the user for commands. Terminate on "$quit". */
	request.rqLen = RQ_SIZE; // request 메세지 사이즈

	std::cout << "ConsolePrompt를 통해 command를 입력받습니다. 종료 메세지를 입력받을때까지 루프를 반복합니다." << endl;
	// 입력받은 command는 request.record로 들어감. 길이는 MAX_RQRS_LEN - 1
	// 입력받은 request.record가 quitMsg가 아닐경우 while문을 반복합니다.
	while (ConsolePrompt("\nEnter Command: ", (char*)request.record, MAX_RQRS_LEN - 1, TRUE)
		&& (strcmp((const char*)request.record, quitMsg) != 0)) {

		/* WriteFile 함수
		* - 파일 데이터를 쓰는 함수입니다.
		 1. hFile: 파일이나 I/O 장치의 핸들.  ( hNamedPipe)
		 2. lpBuffer: 데이터를 저장하기 위한 버퍼를 가리키는 포인터 ( &response)
		 3. nNumberOfBytesToRead: 기록할 바이트 크기 (RQ_SIZE)
		 4. lpNumberOfByteRead: 기록한 데이터 바이트의 수를 리턴받는 인수 (&nWrite)
		 5. lpOverlapped: 동기 입출력이면 NULL을 사용
		 return value : 성공하면 0이 아닌 값을 리턴한다. 함수가 실패할때 혹은 비동기 입출력을 완료했을 때도 0을 반환한다.
		 */
		std::cout << "서버로 request를 보내고, 값을 write 합니다." << endl;
		if (!WriteFile(hNamedPipe, &request, RQ_SIZE, &nWrite, NULL))
			ReportError("Write NP failed", 0, TRUE);

		/* Read each response and send it to std out */
		/* ReadFile 함수
		* - 특수 파일 혹은 입출력 장치로 부터 데이터를 읽는다.
		 1. hFile: 파일, 파일 스트림, 물리디스크, 테이프 드라이브, 소켓과 같은 장치의 핸들.  ( hNamedPipe)
		 2. lpBuffer: 데이터를 저장하기 위한 버퍼를 가리키는 포인터 ( &response)
		 3. nNumberOfBytesToRead: 읽을 최대 바이트 크기 (RS_SIZE)
		 4. lpNumberOfByteRead: 동기 입출력 모드에서, 읽어들인 데이터의 바이트 수를 넘긴다 (&nRead)
		 5. lpOverlapped: 동기 입출력이면 NULL을 사용
		 return value : 성공하면 0이 아닌 값을 리턴한다. 함수가 실패할때 혹은 비동기 입출력을 완료했을 때도 0을 반환한다. 
		 */
		std::cout << "서버에서 받은 response를 읽고, 읽은 데이터를 출력합니다." << endl;
		while (ReadFile(hNamedPipe, &response, RS_SIZE, &nRead, NULL))
		{
			// 서버에서 받은 response가 1 또는 0이면 break
			if (response.rsLen <= 1)	/* 0 length indicates response end */
				break;
			printf("%s", response.record);
		}
	}

	
	std::cout << "Quit command를 받았습니다. Disconnect 시킵니다. " << endl;

	std::cout << "핸들 hNamedPipe를 close합니다. " << endl;
	CloseHandle(hNamedPipe);
	return 0;
}
