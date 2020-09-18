/* Chapter 6. timepp. Simplified version with no special
	auxiliary functions or header files.  */

	/* timeprint: Execute a command line and display
		the time (elapsed, kernel, user) required. */

		/* This program illustrates:
			1. Creating processes.
			2. Obtaining the command line.
			3. Obtaining the elapsed times.
			4. Converting file times to system times.
			5. Displaying system times.
				Windows only. */

#include "Everything.h"
#include <VersionHelpers.h>

int _tmain(int argc, LPTSTR argv[])
{
	printf("\n<timep> 코드입니다.\n");
	printf("\n---생성하는 프로세스의 속성을 지정하기 위한 STARTUPINFO 변수 선언---\n");	
	STARTUPINFO startUp;
	printf("\n---생성된 프로세스와 기본 스레드에 대한 정보를 얻기 위해 PROCESS_INFORMATION 변수 선언---\n");
	PROCESS_INFORMATION procInfo;
	union {		/* Structure required for file time arithmetic. */
		LONGLONG li;
		FILETIME ft;
	} createTime, exitTime, elapsedTime;

	printf("\n---FILETIME 변수 kernelTime, userTime 선언---");
	FILETIME kernelTime, userTime; // FILETIME 구조체는 파일 시간은 파일이 생성되었거나 마지막으로 쓰여진 특정 날짜와 시간을 위해 사용
	printf("\n---SYSTEMTIME 구조체 변수 elTiSys, keTiSys, usTiSys 선언---");
	// SYSTEMTIME 구조체는 시간을 받기 위한 구조체로, wYear, wHour, wSecond 등의 시간과 관계된 멤버를 가진다. 
	// 하지만, SYSTEMTIME 구조체 또한 선언하면 그 안에는 쓰레기 값이 들어있어 사용하지 못한다. 
	// 이는 나중에 FILETIME을 SYSTEMTIME으로 바꿔주기 위해 사용
	SYSTEMTIME elTiSys, keTiSys, usTiSys;
	printf("\n---Long Pointer TCHAR string 변수 targv 선언---");
	LPTSTR targv = SkipArg(GetCommandLine());  // 첫번째 arg 없애줌(즉, timep 없애줌)
	printf("\n---HANDLE 구조체 변수 hProc 선언\n");
	HANDLE hProc; // Kernel object를 건드리기 위해  

	/*  Skip past the first blank-space delimited token on the command line
		A more general solution would account for tabs and new lines */
	printf("\n---command line에 입력한 내용이 timep command ... 의 형태가 아닐 경우 에러 출력---\n");
	// 즉, 최소 2의 인자가 있어야함
	if (argc <= 1)
		ReportError(_T("Usage: timep command ..."), 1, FALSE);

	printf("command line에 입력한 내용이 정상적으로 확인됨\n");

	printf("\n---window version 확인 시작---n");
	/* Determine is this is Windows 2000 or NT.  */
	if (!IsWindows7OrGreater())
	{
		ReportError(_T("You need at least Windows 7, Your Version Not Supported"), 2, TRUE); // 윈도우7보다 낮으면 지원안하겠다
	}

	printf("\n---window version 확인 종료---n");

	printf("\n\n---calling process가 생성될때 STARTUPINFO 구조체 내용을 가져온다.---\n");
	// 표준 핸들이 위치한 곳과 같이 현재 프로세스의 실행환경에 관한 세부 정보를 담고 있는 구조체를 검색한다.
	GetStartupInfo(&startUp);
	printf("\n\n---STARTUPINFO 구조체 내용 가져오기 완료---\n");

	printf("\n---Process가 생성 되었는지 검사---\n");
	printf("--> 만일 생성되지 않으면 에러를 출력한다.\n");
	/* Execute the command line and wait for the process to complete. */
	if (!CreateProcess(NULL, targv, NULL, NULL, TRUE,
		NORMAL_PRIORITY_CLASS, NULL, NULL, &startUp, &procInfo))
		ReportError(_T("\nError starting process. %d"), 3, TRUE);
	printf("\n---Process 생성 완료---n");

	printf("\n---새롭게 생성된 process의 handle은 hProc에 할당---\n");
	hProc = procInfo.hProcess;

	printf("--> hProc이 signal state이거나 time out까지 대기\n");
	printf("--> time out 시간을 INFINITE로 지정했으므로 프로세스가 종료될때까지 계속 대기");
	printf("\n--> object의 상태가 signal state가 될 경우 WAIT_OBJECT_0의 값을 return, 아닐경우 에러 출력\n");

	if (WaitForSingleObject(hProc, INFINITE) != WAIT_OBJECT_0) // process가 제대로 끝날때까지
		ReportError(_T("Failed waiting for process termination. %d"), 5, TRUE);;

	printf("\n---Process가 생성된 후 경과한 시간, 종료 시간, 커널 모드 실행시간, 사용자 모드 실행을 알려줌---\n");
	// 성공할 경우 return value = nonzero, 실패할경우 return value = 0

	if (!GetProcessTimes(hProc, &createTime.ft, // 프로세스에 대한 시간을 얻어냄
		&exitTime.ft, &kernelTime, &userTime)) // 종료시간, 커널모드시간, 사용자모드시간
		ReportError(_T("Can not get process times. %d"), 6, TRUE);
	printf("\n---Process가 생성된 후 경과한 시간, 종료 시간, 커널 모드 실행시간, 사용자 모드 실행 획득완료---\n");


	printf("\n---획득한 정보는 millisecond로 SYSTEMTIME 구조체 변수에 저장해 SystemTime으로 바꿔줌 (세계 시간 기준 = UTC---\n");
	elapsedTime.li = exitTime.li - createTime.li;
	FileTimeToSystemTime(&elapsedTime.ft, &elTiSys); // 파일타임을 시스템타임으로 변경해주는거
	FileTimeToSystemTime(&kernelTime, &keTiSys);
	FileTimeToSystemTime(&userTime, &usTiSys);
	printf("\n---System time으로 변경 완료---\n");


	printf("\n---결과 출력---\n");
	_tprintf(_T("Real Time: %02d:%02d:%02d.%03d\n"),
		elTiSys.wHour, elTiSys.wMinute, elTiSys.wSecond,
		elTiSys.wMilliseconds);
	_tprintf(_T("User Time: %02d:%02d:%02d.%03d\n"),
		usTiSys.wHour, usTiSys.wMinute, usTiSys.wSecond,
		usTiSys.wMilliseconds);
	_tprintf(_T("Sys Time:  %02d:%02d:%02d.%03d\n"),
		keTiSys.wHour, keTiSys.wMinute, keTiSys.wSecond,
		keTiSys.wMilliseconds);

	printf("\n---열려있는 object handle을 닫는다(파일 갯수만큼 반복).단, 그 handle과 연결된 object가 종료되는 것은 아님---\n");
	CloseHandle(procInfo.hThread); CloseHandle(procInfo.hProcess);
	printf("\--열려있는 object handle 닫기 완료--\n");
	return 0;
}
