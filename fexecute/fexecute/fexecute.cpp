#include "Everything.h"
#include <iostream>
#include <string>
#define MAXLEN 100
#define NUM 3
using namespace std;

int main(void)
{
	HANDLE handle[NUM]; // 3개의 프로세스를 위한 핸들
	STARTUPINFO startUp[NUM]; 	// 생성하는 프로세스의 속성을 지정하기 위해 STARTUPINFO 구조체 변수 startUp 선언
	PROCESS_INFORMATION processInfo[NUM]; // 생성된 프로세스와 기본 스레드에 대한 정보를 얻기 위해 PROCESS_INFORMATION 변수 선언
															// 넘겨 받을 HANDLE, THREAD ID에 대한 정보
	DWORD result; //WaitForMultipleObjects의 return value를 확인하기 위한 변수 선언(DWORD 32 BIT = 4 BYTE)
	TCHAR command[MAXLEN]; // 사용자로부터 입력받을 명령어
	int exit; // 강제 종료

	while (TRUE) // 강제종료 후 처음으로 돌아가기 위해 while문 사용
	{
		printf(">\n");

		for (int i = 0; i < NUM; i++)
		{
			// STARTUPINFO , PROCESS_INFORMATION 메모리 할당
			ZeroMemory(&startUp[i], sizeof(startUp[i]));
			startUp[i].cb = sizeof(startUp[i]);
			ZeroMemory(&processInfo[i], sizeof(processInfo[i]));
		}

		for (int i = 0; i < NUM; i++)
		{
			printf("명령어를 입력하세요.\n");
			_getws_s(command, MAXLEN); // command 변수에 명령어를 TCHAR 타입으로 입력받는다.

			// CreateProcess
			//  -> 프로세스를 생성하는 함수
			// <파라미터>
			//  1. LPCTSTR lpApplicationName: 프로세스를 생성할 실행파일 이름을 NULL
			//  2. LPSTR lpCommandLine: commandLine을 프로세스에게 전달
			//  3. LPSECURITY_ATTRIBUTES lpProcessAttributes: 프로세스 기본 보안 속성 (NULL로 지정)
			//  4. LPSECURITY_ATTRIBUTES lpThreadAttributes: 쓰레드 기본 보안 속성 (NULL로 지정)
			//  5. BOOL bInheritHandles: 상속 속성을 False로 지정(핸들 상속 안함)
			//  6. DWORD dwCreationFlags: 프로세스의 우선순위는 0으로 지정하지 않음
			//  7. LPVOID lpEnvironment: 생성하는 프로세스의 Environment Block 지정 (NULL: 부모 프로세스의 환경 블록 복사)
			//  8. LPCTSTR lpCurrentDirectory: 생성하는 프로세스의 현재 디렉터리 설정 (NULL: 부모프로세스의 현제 디렉터리)
			//  9. LPSTARTUPINFO lpStartupInfo: startUp 변수를 초기화한 후 변수의 포인터를 인수로 전달
			//  10. LPPROCESS_INFORMATION lpProcessInformation: 생성하는 프로세스 정보를 얻기 위한 것으로 PROCESS_INFORMATION 구조체 변수의 주소값을 인자로 전달
			// return: 성공시 nonzero 실패시 0

			printf("\n---프로세스가 생성 되었는지 검사---\n");
			if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startUp[i], &processInfo[i]))
				printf("프로세스 생성 실패\n");
				
			printf("\n프로세스가 생성됨\n");

			handle[i] = processInfo[i].hProcess; //프로세스의 핸들 할당
			fflush(stdin);
		}

		// WaitForMultipleObjects
		//  -> 기다리기로 한 모든 객체가 신호 상태에 놓일 때까지 기다린다. 대기 시간을 정할 수도 있다.
		// <파라미터>
		//  1. DWORD nCount: 배열에 저장되어 있는 핸들 개수를 전달한다. (THEARD_NUM)
		//  2. const HANDLE *lpHandles: 핸들을 저장하고 있는 배열의 주소 정보를 전달한다. (handle)
		//  3. BOOL bWaitAll: 관찰 대상이 모두 Signaled 상태가 되기를 기다리고자하는지 아니면 하나라도 Signaled 상태가 되면 반환할것인지 결정한다. (TRUE; 지정된 프로세스가 모두 종료할때까지 기다린다.)
		//  4. DWORD dwMilliseconds: 커널오브젝트가 Singaled상태가 될때까지 기다릴 수 있는 최대 시간. (여기서는 30초)
		// <return>
		// - WAIT_FAILED : 대기 동작이 실패했음 ,GetLastError을 통해 자세한 정보를 얻음
		// - WAIT_OBJECT_0 : 커널 객체가 주어진 시간 간격 안에 시그널된 상태로 변함
		// - WAIT_TIMEOUT: 커널 객체가 시그널되기 전에 주어진 시간 초과 간격이 모두 지났음
		// - WAIT_ABANDONED_0 : 커널 객체가 뮤텍스를 사용했고, 뮤텍스를 소유한 스레드가 자발적으로 뮤텍스의 소유권을 하제하지 않고 종료
		printf("\n---프로세스가 종료될때까지 대기---\n");
		result = WaitForMultipleObjects(NUM, handle, TRUE, 30000);
		if (result == WAIT_TIMEOUT) // 30 초 초과된 경우
		{
			printf("\n3개의 프로세스가 모두 종료되지는 않았습니다.\n");
			for (int i = 0; i < NUM; i++)
			{
				// Process 강제 종료 (성공할 경우 0 return)
				exit = TerminateProcess(handle[i], 0);
				if (exit != 0) //프로세스 강제 종료 성공
					printf("\n%d번째 프로세스 강제 종료되었습니다\n", i);
				else // 프로세스 강제 종료 실패
					printf("\n%d번째 프로세스 강제 종료되지 않았습니다\n", i);
			}
		}
		else if (result == WAIT_FAILED) // 함수 호출에 실패한 경우 
		{
			printf("\n함수 호출 실패\n");
		}
		else // 대기중인 3개의 프로세스가 모두 종료된 경우
		{
			printf("\n모두 종료되었습니다\n");
		}

		for (int i = 0; i < NUM; i++)
		{
			// 핸들들을 닫음 
			CloseHandle(processInfo[i].hProcess);
			CloseHandle(processInfo[i].hThread);
			CloseHandle(handle[i]);
		}

	}
	return 0;

}

