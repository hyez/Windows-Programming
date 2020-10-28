/* Chapter 8. eventPC_xxx.c		BROKEN							*/
/* Maintain two threads, a producer and a consumer				*/
/* The producer periodically creates checksummed data buffers, 	*/
/* or "message block" signalling the consumer that a message	*/
/* is ready, and which the consumer displays when prompted		*/
/* by the user. The conusmer reads the next complete 			*/
/* set of data and validates it before display					*/
/* This is a reimplementation of simplePC.c to use an event		*/

#include "Everything.h"
#include <time.h>
#define DATA_SIZE 256

typedef struct msg_block_tag { /* Message block */
	volatile DWORD	f_ready, f_stop; 
		/* ready state flag, stop flag	*/
	volatile DWORD sequence; /* Message block sequence number	*/
	volatile DWORD nCons, nLost;
	time_t timestamp;
	HANDLE	mguard;	/* Mutex to uard the message block structure	*/
	HANDLE	mready; /* "Message ready" event			*/
	DWORD	checksum; /* Message contents checksum		*/
	DWORD	data[DATA_SIZE]; /* Message Contents		*/
} MSG_BLOCK;
/*	One of the following conditions holds for the message block 	*/
/*	  1)	!f_ready || f_stop										*/
/*			 nothing is assured about the data		OR				*/
/*	  2)	f_ready && data is valid								*/
/*			 && checksum and timestamp are valid					*/ 
/*  Also, at all times, 0 <= nLost + nCons <= sequence				*/

/* Single message block, ready to fill with a new message 	*/
MSG_BLOCK mblock; 

DWORD WINAPI produce (void *);
DWORD WINAPI consume (void *);
void MessageFill (MSG_BLOCK *);
void MessageDisplay (MSG_BLOCK *);
	
int _tmain (int argc, LPTSTR argv[])
{
}

DWORD WINAPI consume (void *arg)
{
}

void MessageFill (MSG_BLOCK *mblock)
{
	/* Fill the message buffer, and include checksum and timestamp	*/
	/* This function is called from the producer thread while it 	*/
	/* owns the message block mutex					*/
	
	DWORD i;
	
	mblock->checksum = 0;	
	for (i = 0; i < DATA_SIZE; i++) {
		mblock->data[i] = rand();
		mblock->checksum ^= mblock->data[i];
	}
	mblock->timestamp = time(NULL);
	return;
}

void MessageDisplay (MSG_BLOCK *mblock)
{
	/* Display message buffer, timestamp, and validate checksum	*/
	/* This function is called from the consumer thread while it 	*/
	/* owns the message block mutex					*/
	DWORD i, tcheck = 0;
	
	for (i = 0; i < DATA_SIZE; i++) 
		tcheck ^= mblock->data[i];
	_tprintf (_T("\nMessage number %d generated at: %s"), 
		mblock->sequence, _tctime (&(mblock->timestamp)));
	_tprintf (_T("First and last entries: %x %x\n"),
		mblock->data[0], mblock->data[DATA_SIZE-1]);
	if (tcheck == mblock->checksum)
		_tprintf (_T("GOOD ->Checksum was validated.\n"));
	else
		_tprintf (_T("BAD  ->Checksum failed. message was corrupted\n"));
		
	return;

}