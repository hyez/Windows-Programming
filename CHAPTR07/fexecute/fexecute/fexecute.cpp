#include <stdlib.h>
#include "Everything.h"
#include <iostream>
using namespace std;

int main(int argc, char** argv)
{
	// fexecute 명령어1 명령어2 명령어3
	cout << "fexecute 명령어1 명령어2 명령어3 으로 입력해주세요." << endl;
	if (argc != 4) {
		ReportError(_T("Usage: fexecute 명령어1 명령어2 명령어3"), 1, FALSE);
	}
	for (int i = 1; i <= 3; i++) {
		//cout << _T("test");
		system(argv[i]);
	}
	return 0;
}