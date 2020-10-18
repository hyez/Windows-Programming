#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

int main(void)
{
	HANDLE handle[3]; // 3개의 프로세스를 위한 핸들 3개
	STARTUPINFO startUp[3]; // 새롭게 생성할 프로세스에 정보를 입력하기 위한 구조체인 STARTUPINFO를 프로세스 갯수만큼 구조체 배열로 선언
	PROCESS_INFORMATION processInfo[3]; // 새롭게 생성할 프로세스에 정보를 입력받기 위한 구조체(프로세스핸들,쓰레드핸들,프로세스id,쓰레드id)로 프로세스 갯수만큼 구조체 배열로 선언
	DWORD result; //WaitForMultipleObjects의 return value를 확인하기 위한 변수 선언(DWORD 32 BIT = 4 BYTE)
	char file[200]; //입력받을 프로그램과 인자를 위한 변수 선언
	int i; // for 문에 사용될 변수
	int final_state; // 강제 종료 (TerminateProcess가 잘 작동했는지 확인하기 위한 변수)

	//while문 사용 이유 = 문제 조건에서 다시 처음으로 돌아간다고 되어있어 while문 설정
	while (1)
	{
		printf(">\n");

		for (i = 0; i < 3; i++)
		{
			// STARTUPINFO , PROCESS_INFORMATION 변수들의 메모리 영역을 ZeroMemory라는 매크로를 사용하여 0x00으로 채움(메모리 할당)
			ZeroMemory(&startUp[i], sizeof(startUp[i]));
			startUp[i].cb = sizeof(startUp[i]);
			ZeroMemory(&processInfo[i], sizeof(processInfo[i]));
		}

		for (i = 0; i < 3; i++)
		{
			printf("명령어를 입력하세요\n");
			gets_s(file, 200); // 공백이 있을 수 있기 때문에 gets_s 사용
			// CreateProcess 
			//--> 1. 프로세스를 생성할 실행파일 이름을 NULL
			//--> 2. gets_s로 입력받은 것을 프로세스에게 전달 
			//--> 3. 프로세스 기본 보안 속성 NULL로 지정
			//--> 4. 쓰레드 기본 보안 속성 NULL로 지정
			//--> 5. 상속 속성을 FALSE로 지정 = 핸들들은 상속되지 않음
			//--> 6. 프로세스의 우선순위는 0으로 지정하지 않음
			//--> 7. 생성될 프로세스가 사용할 환경변수의 포인터를 NULL로 지정 (부모 프로세스의 환경블록을 상속받음)
			//--> 8. 현재 프로세스가 실행중인 디렉토리로 설정하기 위해 NULL로 지정
			//--> 9. STARTUPINFO 구조체 변수를 초기화한 다음에 이 변수의 포인터를 인수로 전달
			//--> 10. 생성하는 프로세스 정보를 얻기 위한 것으로 PROCESS_INFORMATION 구조체 변수의 주소값을 인자로 전달  
			cout << file;
			if (!CreateProcess(NULL, file, NULL, NULL, FALSE, 0, NULL, NULL, &startUp[i], &processInfo[i]))
				printf("프로세스 생성 실패\n");
			handle[i] = processInfo[i].hProcess; //프로세스의 핸들 할당
			fflush(stdin); //입력 버퍼 삭제 
		}

		// 프로세스 종료 대기 --> 프로세스가 3개 임으로 WaitForMultipleObjects를 사용
		// --> 1. 핸들 갯수 = 3개
		// --> 2. 핸들을 저장하고 있는 배열의 주소 정보를 전달
		// --> 3. 지정된 프로세스가 모두 종료할때까지 기다려라 = TRUE
		// --> 4. 30초 대기 (30s = 30000miliseconds)
		result = WaitForMultipleObjects(3, handle, TRUE, 30000);
		if (result == WAIT_TIMEOUT) // WAIT_TIMEOUT일 경우 30초가 경과한 것임
		{
			printf("\n3개의 프로세스가 모두 종료되지는 않았습니다.\n");
			for (i = 0; i < 3; i++)
			{
				//TerminateProcess를 사용해서 Process 강제 종료 (성공할 경우 0을 return)
				final_state = TerminateProcess(handle[i], 0);
				if (final_state != 0) //프로세스 강제 종료 성공
					printf("\n%d번째 프로세스 강제 종료되었습니다\n", i);
				else // 프로세스 강제 종료 실패
					printf("\n%d번째 프로세스 강제 종료되지 않았습니다\n", i);
			}
		}
		else if (result == WAIT_FAILED) // 함수 호출에 실패한 경우 WAIT_FAILED return 
		{
			printf("\n함수 호출 실패\n");
		}
		else // 대기중인 3개의 프로세스가 모두 종료된 경우
		{
			printf("\n모두 종료되었습니다\n");
		}

		for (i = 0; i < 3; i++)
		{
			// 핸들들을 닫음 
			CloseHandle(processInfo[i].hProcess);
			CloseHandle(processInfo[i].hThread);
			CloseHandle(handle[i]);
		}

	}
	return 0;

}

