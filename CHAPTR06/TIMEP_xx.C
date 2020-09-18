/* Chapter 8. timep. */

/* timeprint: Execute a command line and display
	the time (elapsed, kernel, user) required. */

/* This program illustrates:
	1. Creating processes.
	2. Obtaining the command line.
	3. Obtaining the elapsed times.
	4. Converting file times to system times.
	5. Displaying system times.
		Windows NT only. */

#include "EvryThng.h"

int _tmain (int argc, LPTSTR argv [])
{
	STARTUPINFO StartUp;
	PROCESS_INFORMATION ProcInfo;
	union {		/* Structure required for file time arithmetic. */
		LONGLONG li;
		/*??*/ ft;
	} CreateTime, ExitTime, ElapsedTime;

	/*??*/TIME KernelTime, UserTime;
	/*??*/TIME ElTiSys, KeTiSys, UsTiSys, StartTimeSys, ExitTimeSys;
	LPTSTR targv = SkipArg (GetCommandLine ());
	OSVERSIONINFO OSVer;
	BOOL IsNT;

	/* Determine is this is Windows NT. Assume W95 otherwise */
	OSVer.dwOSVersionInfoSize = sizeof(/*??*/);
	if (!GetVersionEx (&/*??*/))
		ReportError (_T("Can not get OS Version info"), 3, TRUE);
	IsNT = (OSVer.dwPlat/*??*/ == VER_PLATFORM/*??*/);

	GetStartupInfo (&/*??*/);
	GetSystemTime (&/*??*/);

	/* Execute the command line and wait for the process to complete. */

	if (!CreateProcess (NULL, targv, NULL, NULL, TRUE,
			NORMAL_PRIORITY_CLASS, NULL, NULL, &StartUp, &ProcInfo))
		ReportError (_T ("\nError starting process."), 2, TRUE);
	WaitForSingleObject (/*??*/, INFINITE);
	GetSystemTime (&Exit/*??*/);

	if (IsNT) {	/* Windows NT. Elapsed, Kernel, & System times. */

		GetProcessTimes (/*??*/, &/*??*/.ft,
			&ExitTime.ft, &Kernel/*??*/, &UserTime);
		ElapsedTime/*??*/ = /*??*/.li - /*??*/.li;

		FileTimeToSystemTime (&/*??*/, &ElTiSys);
		FileTimeToSystemTime (&KernelTime, &KeTiSys);
		FileTimeToSystemTime (&UserTime, &UsTiSys);
		_tprintf (_T ("Real Time: %02d:%02d:%02d:%03d\n"),
			ElTiSys.wHour, ElTiSys.wMinute, ElTiSys.wSecond,
			ElTiSys.wMilliseconds);
		_tprintf (_T ("User Time: %02d:%02d:%02d:%03d\n"),
			UsTiSys.wHour, UsTiSys.wMinute, UsTiSys.wSecond,
			UsTiSys.wMilliseconds);
		_tprintf (_T ("Sys Time: %02d:%02d:%02d:%03d\n"),
			KeTiSys.wHour, KeTiSys.wMinute, KeTiSys.wSecond,
			KeTiSys.wMilliseconds);
	} else {

		/* Windows 95. Elapsed time only. */
		SystemTimeToFileTime (&StartTimeSys, &/*??*/.ft);
		SystemTimeToFileTime (&ExitTimeSys, &/*??*/.ft);
		ElapsedTime.li = /*??*/.li - /*??*/.li;
		FileTime/*??*/ (&/*??*/, &ElTiSys);
		_tprintf (_T ("Real Time: %02d:%02d:%02d:%03d\n"),
			/*??*/.wHour, ElTiSys.wMinute, ElTiSys.wSecond,
			ElTiSys.wMilliseconds);
	}
	CloseHandle (/*??*/); CloseHandle (/*??*/);	
	return 0;
}




