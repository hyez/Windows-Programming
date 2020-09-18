/* SkipArg.c
	Skip one command line argument - skip tabs and spaces. */

#include "Everything.h"

LPTSTR SkipArg (LPCTSTR targv)
{
	LPTSTR p;

	p = (LPTSTR)targv;
		/* Skip up to the next tab or space. */
	while (*p != _T('\0') && *p != _T(' ') && *p != _T('\t')) p++;
		/* Skip over tabs and spaces to the next arg. */
	while (*p != _T('\0') && (*p == _T(' ') || *p == _T('\t'))) p++;
	return p;
}


