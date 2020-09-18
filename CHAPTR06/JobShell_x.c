/*	Chapter 6 */
/*  DELIBERATELY BROKEN
JobMgr.c One program combining all three
job management programs:
	Jobbg	- Run a job in the background
	jobs	- List all background jobs
	kill	- Terminate a specified job of job family
			  There is an option to generate a console
			  control signal.
*/

#include "EvryThng.h"
#include "JobMgt.h"

static int Jobbg (int, LPTSTR *, LPTSTR);
static int Jobs  (int, LPTSTR *, LPTSTR);
static int Kill  (int, LPTSTR *, LPTSTR);

int _tmain (int argc, LPTSTR argv [])
{
	BOOL Exit = FALSE;
	TCHAR Command [MAX_COMMAND_LINE+10], *pc;
	DWORD i, LocArgc; /* Local argc */
	TCHAR argstr [MAX_ARG] [MAX_COMMAND_LINE];
	LPTSTR pArgs [MAX_ARG];

	for (i = 0; i < MAX_ARG; i++)
		pArgs[i] = argstr[i];

	_tprintf (_T("Win32 Job Mangement\n"));
	while (!Exit) {
		_tprintf (_T("%s"), _T("JM$"));
		_fgetts (Command, MAX_COMMAND_LINE, stdin);
		pc = strchr (Command, '\n');
		*pc = '\0';

/*		printf ("%s\n", Command);*/
		GetArgs (Command, &LocArgc, pArgs);
		CharLower (argstr [0]);

/*		printf ("%s %s %s %s", argstr[0], argstr[1],
			argstr[2], argstr[3]); */

		if (_tcscmp (argstr[0], _T("jobbg")) == 0) {
			Jobbg (LocArgc, pArgs, Command);
		}
		else if (_tcscmp (argstr[0], _T("jobs")) == 0) {
			Jobs (LocArgc, pArgs, Command);
		}
		else if (_tcscmp (argstr[0], _T("kill")) == 0) {
			Kill (LocArgc, pArgs, Command);
		}
		else if (_tcscmp (argstr[0], _T("quit")) == 0) {
			Exit = TRUE;
		}
		else Exit = TRUE;
	}


	return 0;
}

/* jobbg: Execute a command line in the background, put
	the job identity in the user's job file, and exit.
	Related commands (jobs, fg, kill, and suspend) can be used to manage the jobs. */

/* jobbg [options] command-line
		-c: Give the new process a console.
		-d: The new process is detached, with no console.
	These two options are mutually exclusive.
	If neither is set, the background process shares the console with jobbg. */

/* This new features this program illustrates:
		1. Creating detached and with separate consoles.
		2. Maintaining a Jobs/Process list in a shared file.
		3. Determining a process status from a process ID.*/

/* Standard include files. */
/* - - - - - - - - - - - - - - - - - - - - - - - - - */

int Jobbg (int argc, LPTSTR argv [], LPTSTR Command)
{
	/*	Similar to timep.c to process command line. */
	/*	- - - - - - - - - - - - - - - - - - - - - - */
	/*	Execute the command line (targv) and store the job id,
		the process id, and the handle in the jobs file. */

	DWORD fCreate;
	LONG JobNo;
	BOOL Flags [2];

	STARTUPINFO StartUp;
	PROCESS_INFORMATION ProcessInfo;
	LPTSTR targv = SkipArg (Command);
	
	GetStartupInfo (&StartUp);


		/* Determine the options. */
	Options (argc, argv, _T ("cd"), &Flags [0], &Flags [1], NULL);

		/* Skip over the option field as well, if it exists. */
	if (argv [1] [0] == '-')
		targv = SkipArg (targv);

	fCreate = Flags [0] ? /* */ :
			Flags [1] ? /* */ : 0;

		/* Create the job/thread suspended.
			Resume it once the job is entered properly. */
	if (!CreateProcess (NULL, targv, NULL, NULL, TRUE,
			fCreate | /* */ | CREATE_NEW_PROCESS_GROUP,
			NULL, NULL, &StartUp, &ProcessInfo))  {
		ReportError (_T ("Error starting process."), 0, TRUE);
		return 4;
	}

		/* Create a job number and enter the process Id and handle
			into the Job "data base" maintained by the
			GetJobNumber function (part of the job management library). */
	
	JobNo = GetJobNumber (&ProcessInfo, targv);
	if (JobNo >= 0)
		ResumeThread (ProcessInfo.hThread);
	else {
		TerminateProcess (ProcessInfo.hProcess, 3); /* Can you do better than this? */
		CloseHandle (ProcessInfo.hProcess);
		ReportError (_T ("Error: No room in job control list."), 0, FALSE);
		return 5;
	}

	CloseHandle (ProcessInfo.hThread);
	CloseHandle (ProcessInfo.hProcess);
	_tprintf (_T (" [%d] %d\n"), JobNo, ProcessInfo.dwProcessId);
	return 0;
}

/* jobs: List all running or stopped jobs that have
	been created by this user under job management;
	that is, have been started with the jobbg command.
	Related commands (jobbg and kill) can be used to manage the jobs. */
/* jobbg - no options or command line arguments. */
/* This new features this program illustrates:
	1. Determining process status.
	2. Maintaining a Jobs/Process list in a shared file. */

int Jobs (int argc, LPTSTR argv [], LPTSTR Command)
{
	if (!DisplayJobs ()) return 1;
	return 0;
}

/* kill [options] JobNumber
	Terminate the process associated with the specified job number. */
/* This new features this program illustrates:
	1. Using TerminateProcess 
	2. Console control events */

/* Options:
	-b  Generate a Ctrl-Break 
	-c  Generate a Ctrl-C
		Otherwise, terminate the process. */


/* The Job Management data structures, error codes,
	constants, and so on are in the following file. */


int Kill (int argc, LPTSTR argv [], LPTSTR Command)
{
	DWORD ProcessId, JobNumber, iJobNo;
	HANDLE hProcess;
	BOOL CntrlC, CntrlB, Killed;

	iJobNo = Options (argc, argv, _T ("bc"), &CntrlB, &CntrlC, NULL);
	
	/* Find the process ID associated with this job. */

	JobNumber = _ttoi (argv [1]);
	ProcessId = FindProcessId (JobNumber);
	if (ProcessId == 0)	{
		ReportError (_T ("Job number not found.\n"), 0, FALSE);
		return 1;
	}
	hProcess = OpenProcess (/* */ ProcessId);
	if (hProcess == NULL) {
		ReportError (_T ("Process already terminated.\n"), 0, FALSE);
		return 2;
	}

	if (CntrlB)
		Killed = GenerateConsoleCtrlEvent (/* */, ProcessId);
	else if (CntrlC)
		Killed = GenerateConsoleCtrlEvent (/* */, ProcessId);
	else
		Killed = TerminateProcess (hProcess, JM_EXIT_CODE);

	if (!Killed) { 
		ReportError (_T ("Process termination failed."), 0, TRUE);
		return 3;
	}
	

	CloseHandle (hProcess);

	_tprintf (_T ("Job [%d] terminated or timed out\n"), JobNumber);
	return 0;
}
