/* Chapter 8. simplePC.c										*/
/* Maintain two threads, a Producer and a Consumer				*/
/* The Producer periodically creates checksummed mData buffers, 	*/
/* or "message block" which the Consumer displays when prompted	*/
/* by the user. The conusmer reads the most recent complete 	*/
/* set of mData and validates it before display					*/

#include "Everything.h"
#include <time.h>
#include <iostream>
using namespace std;

#define DATA_SIZE 256 // 데이터 사이즈 정의

// 메세지 하나에 해당하는 블록
typedef struct MSG_BLOCK_TAG { /* Message block */
	CRITICAL_SECTION mGuard;	// 임계영역을 위한 변수, 메세지 블록을 보호할 자료구조
	DWORD fReady, fStop; // 준비되었는지 스탑할껀지 나타내는 flag 변수
	/* ready state flag, stop flag	*/
	volatile DWORD nCons, mSequence; // nCons: consumer가 소비한 메시지의 개수, mSequence: 생산자가 생산한 메세지 개수 (여러 스레드를 사용함으로 volatile로 선언)
	DWORD nLost; // 생산한거(mSequence)에서 소비한거(nCons)뺀거 (잃어버린거) 담을 변수
	time_t mTimestamp; // 언제 생산한지 시간을 담는 변수
	DWORD mChecksum; // 데이터 일관성을 위해 생산자는 테이블에 놓인 데이터의 Checksum을 계산, 소비자는 데이터가 온전한지를 확인하기 위한 변수 
	DWORD mData[DATA_SIZE]; // 실제 256크기의 데이터가 담길 변수
} MSG_BLOCK;
/*	One of the following conditions holds for the message block 	*/
/*	  1)	!fReady || fStop										*/
/*			 nothing is assured about the mData		OR				*/
/*	  2)	fReady && mData is valid								*/
/*			 && mChecksum and mTimestamp are valid					*/
/*  Also, at all times, 0 <= nLost + nCons <= mSequence				*/

//  nLost+nCons는 0 이상, mSequence 이하의 범위의 값을 가진다.

/* Single message block, ready to fill with a new message 	*/
MSG_BLOCK mBlock = { 0, 0, 0, 0, 0 }; // 메세지 블록 초기화, mGuard, fReady, fStop, nCons, mSequence  (MSG_BLOCK 앞에 5개 0으로 초기화)

DWORD WINAPI Produce(void*); // 생산자 스레드함수 선언
DWORD WINAPI Consume(void*); // 소비자 스레드함수 선언
void MessageFill(MSG_BLOCK*); // 실제 메시지 내용 채우는 함수 선언
void MessageDisplay(MSG_BLOCK*); // 메세지 내용 출력하는 함수 선언 

int _tmain(int argc, LPTSTR argv[])
{
	cout << "2개의 thread(producer, consumer)가 동작하는 simplePC코드입니다." << endl;

	DWORD status; // 각 스레드의 대기를 위한 변수 
	HANDLE hProduce, hConsume; // 각 스레드 핸들

	cout << "임계영역을 초기화합니다." << endl;
	InitializeCriticalSection(&mBlock.mGuard); // CriticalSection 초기화

	cout << "생산자, 소비자 스레드를 각각 생성 및 실행합니다. 실패시 에러를 출력합니다." << endl;
	// _beginthreadex: 스레드 생성
		//  -> C / C++ Runtime-Library 에서 제공
		//  -> 내부적으로 새로 생성한 쓰레드의 핸들을 닫지 않기 때문에 명시적으로 ::CloseHandle( ) 함수를 호출하여 쓰레드의 핸들을 수동으로 닫아 주어야 한다.
		//  -> 스레드를 생성할 때 호출하는 함수이다.
		// <파라미터>
		//  1. void *security: 생성하려는 쓰레드의 보안에 관련된 설정을 위해 필요한 옵션 (NULL로 지정)
		//  2. unsigned stack_size: 쓰레드를 생성하는 경우, 모든 메모리 공간은 스택 공간은 독립적으로 생성된다. 쓰레드, 생성 시 요구되는 스택의 크기를 인자로 전달한다. (0: 디폴트로 설정되어 있는 스택의 크기)
		//	3. unsigned (*start_address)(void*): 쓰레드에 의해 호출되는 함수의 포인터를 인자로 전달
		//	4. void* arglist: lpStartAddress 가 가리키는 함수 호출 시, 전달할 인자를 지정
		//	5. unsigned initflag:  새로운 쓰레드 생성 이후에 바로 실행 가능한 상태가 되느냐, 아니면 대기 상태로 들어가느냐를 결정 ( 0: 즉시 실행 )
		//	6. unsigned* thrdaddr: 쓰레드 생성 시 쓰레드id가 리턴되는데, 이를 저장 (NULL: 스레드 식별자 사용 하지 않음 )
		// return: 성공시  새로 만든 스레드에 핸들을 반환, 실패시 0 반환
	// 스레드 2개가 생성됨과 동시에 즉시 실행된다.
	hProduce = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Produce, NULL, 0, NULL); // 생산자 스레드 생성
	if (hProduce == NULL) // 스레드 생성이 잘못될 경우 
		ReportError(_T("Cannot create Producer thread"), 1, TRUE);
	hConsume = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Consume, NULL, 0, NULL); // 소비자 스레드 생성
	if (hConsume == NULL) // 스레드 생성이 잘못될 경우 
		ReportError(_T("Cannot create Consumer thread"), 2, TRUE);

	/* Wait for the Producer and Consumer to complete */
	cout << "WaitForSingleObject를 사용하여 스레드를 대기합니다." << endl;
	// 지정한 오브젝트가 시그널 상태가 되거나 타임아웃이 되면 제어를 돌려준다.
	// WAIT_OBJECT_0이면 성공, 아니면 에러처리
	cout << "Consumer 핸들이 시그널 상태까지 대기" << endl;
	status = WaitForSingleObject(hConsume, INFINITE);
	if (status != WAIT_OBJECT_0) // WAIT_OBJECT_0이면 제대로 넘어옴, 아니면 에러처리
		ReportError(_T("Failed waiting for Consumer thread"), 3, TRUE); //  3일 경우 Consumer 스레드 대기 실패
	cout << "Produce 핸들이 시그널 상태까지 대기" << endl;
	status = WaitForSingleObject(hProduce, INFINITE);
	if (status != WAIT_OBJECT_0)
		ReportError(__T("Failed waiting for Producer thread"), 4, TRUE); // 4일 경우 Producer 스레드 대기 실패

	// 임계영역 삭제
	DeleteCriticalSection(&mBlock.mGuard);


	_tprintf(_T("Producer and Consumer threads have terminated\n"));
	_tprintf(_T("Messages Produced: %d, Consumed: %d, Lost: %d.\n"),
		mBlock.mSequence, mBlock.nCons, mBlock.mSequence - mBlock.nCons); // 생산한거, 소비한거,  잃어버린거 출력
	return 0;
}

DWORD WINAPI Produce(void* arg)
/* Producer thread - Create new messages at random intervals */
{
	cout << "Produce 스레드 함수 입니다." << endl;
	// 랜덤하게 새로운 메시지를 생성 

	// random number 초기화
	srand((DWORD)time(NULL)); /* Seed the random # generator 	*/

	// stop flag가 0인 동안 루프를 돈다. 처음엔 0으로 초기화, 1이면 루프 끝남
	// 이 때, stop flag바꿔주는건 소비자가 한다.
	while (!mBlock.fStop) {
		/* Random Delay */
		Sleep(rand() / 100); // 0~ 999까지 ms단위로 쉰다.

		/* Get the buffer, fill it */
		// 버퍼 채우기
		cout << "Produce: 임계영역에 들어갑니다." << endl;
		EnterCriticalSection(&mBlock.mGuard); // 임계영역으로 진입한다.  파라미터는 임계 영역 오브젝트(mGuard)에 대한 주소이다.
		__try { // 임계영역
			if (!mBlock.fStop) { // stop flag가 0인 경우 if문 실행
				mBlock.fReady = 0;  // ready확인을 위해 flag를 0으로 바꾼다.
				MessageFill(&mBlock); // MessageFill를 호출하여 메세지 블록 채우기
				mBlock.fReady = 1; // 소비자한테 알려주기위해서 ready flag를 1로 바꾼다. (단순한 flag 사용한 동기화 기법)
				InterlockedIncrement(&mBlock.mSequence); 
				// mSequence 하나 증가시킴
				// consumer에서도 mSequence를 읽어볼 수 있기 때문에 InterlockedIncrement로 증가시켜서 방해 안받음
				cout << mBlock.mSequence << "번째 생산자가 메세지를 생산했습니다."  << endl;
			}
		}
		__finally { 
			// 임계영역 나갈때 반드시 LeaveCriticalSection 함수 사용해야하므로 finally 구문에 호출한다.
			LeaveCriticalSection(&mBlock.mGuard); 
			cout << "Produce: 임계영역에서 빠져나왔습니다.." << endl;
		}
	}
	return 0;
}

DWORD WINAPI Consume(void* arg)
{
	CHAR command; // c 또는 s를 사용자로부터 입력받기 위한 변수
	/* Consume the NEXT message when prompted by the user */
	// stop flag가 0인 동안 루프를 돈다. 처음엔 0으로 초기화, 1이면 루프 끝남
	while (!mBlock.fStop) { /* This is the only thread accessing stdin, stdout */
		_tprintf(_T("\n**Enter 'c' for Consume; 's' to stop: ")); // 소비자 동작을 하려면 c 입력, 멈추려면 s 입력
		scanf_s("%c", &command, 1); // command에 문자 입력받음 (s or c)
		if (command == _T('s')) { // s 입력받으면 stop
			/* ES not needed here. This is not a read/modify/write.
			 * The Producer will see the new value after the Consumer returns */
			mBlock.fStop = 1; // stop flag를 1로 바꿔서 루프 빠져나옴 (이때 Producer의 임계 영역 부분도 빠져나가게 된다 )
		}
		else if (command == _T('c')) { // c 입력받으면 소비자 역할
			cout << "Comsumer: 임계영역에 들어갑니다." << endl;
			EnterCriticalSection(&mBlock.mGuard); // 임계영역에 들어감, mBlock.mGuard: 락변수 (소비자, 생산자 하나에만 들어갈수있음=>동기화가능)
			__try {
				if (mBlock.fReady == 0)
					_tprintf(_T("No new messages. Try again later\n")); // 읽을 데이터가 없다.
				else { // ready가 1일경우 읽는다.
					MessageDisplay(&mBlock); // 메세지 출력
					mBlock.nLost = mBlock.mSequence - mBlock.nCons + 1; // 잃어버린거 저장
					mBlock.fReady = 0; // 메세지를 출력했으므로 ready 0 으로 만든다.

					InterlockedIncrement(&mBlock.nCons); 
					// nCons 하나 증가시킴
					// Producer 에서도 nCons를 읽어볼 수 있기 때문에 InterlockedIncrement로 증가시켜서 방해 안받음
					cout << mBlock.nCons << "번째 소비자가 메세지를 소비했습니다." << endl;
				}
			}
			__finally { 
				// 임계영역 나갈때 반드시 LeaveCriticalSection 함수 사용해야하므로 finally 구문에 호출한다.
				LeaveCriticalSection(&mBlock.mGuard);
				cout << "Consumer: 임계영역에서 빠져나왔습니다.." << endl;
			} 
		}
		else {
			_tprintf(_T("Illegal command. Try again.\n"));
		}
	}
	return 0;
}

void MessageFill(MSG_BLOCK* msgBlock)
{
	/* Fill the message buffer, and include mChecksum and mTimestamp	*/
	/* This function is called from the Producer thread while it 	*/
	/* owns the message block mutex			
	*/
	// Message block을 채우기 위한 함수, 생산자에서 호출됨
	DWORD i;

	msgBlock->mChecksum = 0; // mChecksum 초기화
	for (i = 0; i < DATA_SIZE; i++) {
		msgBlock->mData[i] = rand(); // random 숫자를 mData에 할당
		msgBlock->mChecksum ^= msgBlock->mData[i];  //exclusive or로 누적해서 mChecksum 채움
	}
	msgBlock->mTimestamp = time(NULL); // 초단위의 현재 경과 시간 저장
	return;
}

void MessageDisplay(MSG_BLOCK* msgBlock)
{
	/* Display message buffer, mTimestamp, and validate mChecksum	*/
	/* This function is called from the Consumer thread while it 	*/
	/* owns the message block mutex					*/

	// Message block을 출력하기 위한 함수, 소비자에서 호출됨

	DWORD i, tcheck = 0;

	for (i = 0; i < DATA_SIZE; i++)
		tcheck ^= msgBlock->mData[i]; // MessageFill에서 만든 랜덤 데이터 읽어온걸 exclusive or로 누적해서 tcheck 채움
	_tprintf(_T("\nMessage number %d generated at: %s"),
		msgBlock->mSequence, _tctime(&(msgBlock->mTimestamp))); // 몇번째 데이터고 언제 생산됐는지 출력
	_tprintf(_T("First and last entries: %x %x\n"),
		msgBlock->mData[0], msgBlock->mData[DATA_SIZE - 1]); // 첫데이터와 마지막데이터만 출력
	if (tcheck == msgBlock->mChecksum) // MessageDisplay에서 읽은 tcheck와 MessageFill에서 만든  mChecksum의 값이 같을 경우 제대로 읽음
		_tprintf(_T("GOOD ->mChecksum was validated.\n"));
	else // tcheck와 mChecksum의 값이 다를 경우 잘못 읽음
		_tprintf(_T("BAD  ->mChecksum failed. message was corrupted\n"));

	return;
}