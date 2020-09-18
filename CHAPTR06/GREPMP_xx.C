/* Chapter 8. grepMP. */
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

#include "EvryThng.h"

int _tmain (int argc, LPTSTR argv [])

/*	Create a separate process to search each file on the
	command line. Each process is given a temporary file,
	in the current directory, to receive the results. */
{
	HANDLE hTempFile;
	SECURITY_ATTRIBUTES StdOutSA = /* SA for inheritable handle. */
			{sizeof (/*??*/), /*??*/, /*??*/};
	TCHAR CommandLine [MAX_PATH + 100];
	STARTUPINFO StartUpSearch, StartUp;
	PROCESS_INFORMATION ProcessInfo;
	int iProc;
	HANDLE *hProc;  /* Pointer to an array of proc handles. */
	typedef struct {TCHAR TempFile [MAX_PATH];} PROCFILE;
	PROCFILE *ProcFile; /* Pointer to array of temp file names. */

	if (argc < 3)
		ReportError (_T ("Usage: grepMP pattern files."), 1, FALSE);

	/* Startup info for each child search process as well as
		the child process that will display the results. */

	GetStartupInfo (&/*??*/);
	GetStartupInfo (&/*??*/);

	/* Allocate storage for an array of process data structures,
		each containing a process handle and a temporary file name. */

	ProcFile = malloc ((/*??*/) * sizeof (PROCFILE));
	hProc = malloc ((/*??*/2) * sizeof (HANDLE));

	/* Create a separate "grep" process for each file on the
		command line. Each process also gets a temporary file
		name for the results; the handle is communicated through
		the STARTUPINFO structure. argv [1] is the search pattern. */

	for (iProc = 0; iProc < argc - 2; iProc++) {

		/* Create a command line of the form: grep argv [1] argv [iProc + 2] */
		
		_stprintf (CommandLine, _T ("%s%s%s%s"),
			_T ("grep "), argv [1], _T (" "), argv [iProc + 2]);

		/* Create the temp file name for std output. */

		if (GetTemp/*??*/e (_T (/*??*/), _T ("gtm"), 0,
				ProcFile [iProc].TempFile) == 0)
			ReportError (_T ("Temp file failure."), 2, TRUE);

		/* Set the std output for the search process. */

		hTempFile = /* This handle is inheritable */
			CreateFile (ProcFile [iProc].TempFile,
				/*??*/,
				/*??*/, /*??*/,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hTempFile == INVALID_HANDLE_VALUE)
			ReportError (_T ("Failure opening temp file."), 3, TRUE);


		/* Specify that the new process takes its std output
			from the temporary file's handles. 
			You must set the std output handle as well; it
			is not inherited from the parent once the
			dwFlags member is set to STARTF_USESTDHANDLES.
			The std input handle would also be set here if
			the child processes did not take their std in
			from the command line. */

		StartUpSearch.dwFlags = /*??*/;
		StartUpSearch.hStdOutput = /*??*/;
		StartUpSearch.?? = /*??*/;

		/* Create a process to execute the command line. */

		if (!CreateProcess (NULL, /*??*/, NULL, NULL,
				/*??*/, 0, /*??*/, &/*??*/, &/*??*/o))
			ReportError (_T ("ProcCreate failed."), 4, TRUE);
		CloseHandle (hT/*??*/); CloseHandle (/*??*/.hT/*??*/);

		/* Save the process handle. */

		hProc [iProc] = /*??*/./*??*/;
	}

	/* Processes are all running. Wait for them to complete, then output
		the results - in the order of the command line file names. */

	WaitForMultipleObjects (argc - 2, hProc, /*??*/, /*??*/);
	for (iProc = 0; iProc < argc - 2; iProc++)
		CloseHandle (/*??*/);

	/* Result files sent to std output using "cat".
		Delete each temporary file upon completion. */

	for (iProc = 0; iProc < argc - 2; iProc++) {
		if (GetCompressedFileSize (ProcFile [iProc]./*??*/, NULL) > 2) {
			if (argc > 3) {		/* Display file name if more than one. */
				_ftprintf (stdout, _T ("%s:\n"), argv [iProc + 2]); 
				fflush (/*??*/);
			}
			_stprintf (CommandLine, _T (/*??*/), 
				_T ("cat "), ProcFile [iProc]./*??*/);
			if (!CreateProcess (NULL, CommandLine, NULL, NULL,
					TRUE, 0, NULL, NULL, &/*??*/, &/*??*/o))
				ReportError (_T ("Failure executing cat."), 5, TRUE);
			WaitForSingleObject (ProcessInfo.hProcess, INFINITE);
			CloseHandle (ProcessInfo.hProcess);
		}
		if (/*??*/File (/*??*/))
			ReportError (_T ("Cannot delete temp file."), 6, TRUE);
	}
	free (ProcFile); free (hProc);
	return 0;
}

