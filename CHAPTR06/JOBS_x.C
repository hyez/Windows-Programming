/* Chapter 8. jobs. */

/* jobs: List all running or stopped jobs that have
	been created by this user under job management;
	that is, have been started with the jobbg command.
	Related commands (jobbg and kill) can be used to manage the jobs. */
/* jobbg - no options or command line arguments. */
/* This new features this program illustrates:
	1. Determining process status.
	2. Maintaining a Jobs/Process list in a shared file. */

#include "EvryThng.h"
#include "JobMgt.h"
int _tmain (int argc, LPTSTR argv [])
{
	if (!DisplayJobs (/*??*/)) return 1;
	return 0;
}
