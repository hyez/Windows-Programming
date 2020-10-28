/* Chapter 8. eventPC.c											*/
/* Maintain two threads, a producer and a consumer				*/
/* The producer periodically creates checksummed mData buffers, 	*/
/* or "message block" signalling the consumer that a message	*/
/* is ready, and which the consumer displays when prompted		*/
/* by the user. The conusmer reads the next complete 			*/
/* set of mData and validates it before display					*/
/* This is a reimplementation of simplePC.c to use an event		*/

#include "Everything.h"
#include <time.h>
#include <iostream>
using namespace std;

#define DATA_SIZE 256 // 데이터 사이즈 정의

// 메세지 하나에 해당하는 블록
typedef struct MSG_BLOCK_TAG { /* Message block */
	HANDLE	mGuard;	// 메세지 블록을 보호하기 위해 mutex 사용
	HANDLE	mReady; // auto-reset event: 이벤트가 signal state로 바뀌고 누군가가 진입할때, 진입하자마자 자동으로 리셋 
	DWORD	fReady, fStop; // 준비되었는지 스탑할껀지 나타내는 flag 변수
	/* ready state flag, stop flag	*/
	volatile DWORD mSequence; // mSequence: 생산자가 생산한 메세지 개수
	volatile DWORD nCons, nLost; 
	// nCons: consumer가 소비한 메시지의 개수,  (여러 스레드를 사용함으로 volatile로 선언)
	// nLost: 생산한거(mSequence)에서 소비한거(nCons)뺀거 (잃어버린거) 담을 변수
	time_t mTimestamp; // 언제 생산한지 시간을 담는 변수
	DWORD	mChecksum; // 데이터 일관성을 위해 생산자는 테이블에 놓인 데이터의 Checksum을 계산, 소비자는 데이터가 온전한지를 확인하기 위한 변수 
	DWORD	mData[DATA_SIZE]; // 실제 256크기의 데이터가 담길 변수
} MSG_BLOCK;
/*	One of the following conditions holds for the message block 	*/
/*	  1)	!fReady || fStop										*/
/*			 nothing is assured about the mData		OR				*/
/*	  2)	fReady && mData is valid								*/
/*			 && mChecksum and mTimestamp are valid					*/
/*  Also, at all times, 0 <= nLost + nCons <= mSequence				*/

//  nLost+nCons는 0 이상, mSequence 이하의 범위의 값을 가진다.

/* Single message block, ready to fill with a new message 	*/
MSG_BLOCK mBlock = { 0, 0, 0, 0, 0 }; // 메세지 블록 하나 초기화, mGuard, fReady, fStop, nCons, mSequence  (MSG_BLOCK 앞에 5개 0으로 초기화)

DWORD WINAPI Produce(void*); // 생산자 스레드함수 선언
DWORD WINAPI Consume(void*); // 소비자 스레드함수 선언
void MessageFill(MSG_BLOCK*); // 실제 메시지 내용 채우는 함수 선언
void MessageDisplay(MSG_BLOCK*); // 메세지 내용 출력하는 함수 선언 

int _tmain(int argc, LPTSTR argv[])
{
	cout << "2개의 thread(producer, consumer)가 동작하는 eventPC코드입니다." << endl;

	DWORD status; // 각 스레드의 대기를 위한 변수 
	HANDLE hProduce, hConsume; // 각 스레드 핸들

	/* Initialize the message block mutex and event (auto-reset) */
	cout << "mutex mGuard를 생성합니다." << endl;
	// mutex mGuard 생성 
	// <파라미터>
	// 1. lpMutexAttributes : NULL 로 설정했음으로 child process로 상속될 수 없음
	// 2. bInitialOwner : mutex를 생성한 스레드가 소유권을 갖는지 여부 (False: 스레드 호출하는 순간 소유권을 획득하지 못한다.(TRUE: 스레드 호출하는 순간 소유권획득))
	// 3. lpName: mutex 이름을 지정 (NULL임으로 이름 없이 생성) 
	// return : 새롭게 생성된 mutex object의 handle
	mBlock.mGuard = CreateMutex(NULL, FALSE, NULL); // mutex 하나 생성
	
	cout << "event mReady를 생성합니다." << endl;
	// event mReady 생성
	// 이벤트 = 특정 사건의 발생을 다른 스레드에 알리기 위해 사용하며, 이벤트는 signaled state 일때 접근 가능하며 아닐겨우 접근할 수 없다.
	// 1. lpEventAttributes: NULL 로 설정했음으로 child process로 상속될 수 없음
	// 2. bManualReset: 수동 리셋인지 아닌지 (FALSE: auto-reset 이벤트 생성)
	//		(auto-reset: 대기하던 스레드 중 하나만 진입, 진입하고 나면 이벤트의 상태가 자동으로 non-signaled state 가 된다.)
	// 3. bInitialState:  이벤트 object의 초기 상태를 나타냄. (FALSE: non-signaled (TRUE이면 signaled))
	// 4. lpName: 생성할 이벤트 객체에 고유 이름 지정 (NULL임으로 이름 없이 생성) 
	mBlock.mReady = CreateEvent(NULL, FALSE, FALSE, NULL);

	/* Create the two threads */
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
		ReportError(_T("Cannot create producer thread"), 1, TRUE);
	hConsume = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Consume, NULL, 0, NULL); // 소비자 스레드 생성
	if (hConsume == NULL) // 스레드 생성이 잘못될 경우 
		ReportError(_T("Cannot create consumer thread"), 2, TRUE);

	/* Wait for the producer and consumer to complete */

	cout << "WaitForSingleObject를 사용하여 스레드를 대기합니다." << endl;
	// 지정한 오브젝트가 시그널 상태가 되거나 타임아웃이 되면 제어를 돌려준다.
	// WAIT_OBJECT_0이면 성공, 아니면 에러처리

	cout << "Consumer 핸들이 시그널 상태까지 대기" << endl;
	status = WaitForSingleObject(hConsume, INFINITE);
	if (status != WAIT_OBJECT_0) // WAIT_OBJECT_0이면 제대로 넘어옴, 아니면 에러처리
		ReportError(_T("Failed waiting for consumer thread"), 3, TRUE); //  3일 경우 Consumer 스레드 대기 실패

	cout << "Produce 핸들이 시그널 상태까지 대기" << endl;
	status = WaitForSingleObject(hProduce, INFINITE);
	if (status != WAIT_OBJECT_0) // WAIT_OBJECT_0이면 제대로 넘어옴, 아니면 에러처리
		ReportError(__T("Failed waiting for producer thread"), 4, TRUE); // 4일 경우 Producer 스레드 대기 실패

	CloseHandle(mBlock.mGuard); // 자원 반환, 객체 핸들 mGuard를 닫는다 (성공시 nonzero, 실패시 zero 반환)
	CloseHandle(mBlock.mReady); // 자원 반환, 객체 핸들 mReady를 닫는다 (성공시 nonzero, 실패시 zero 반환)

	_tprintf(_T("Producer and consumer threads have terminated\n"));
	_tprintf(_T("Messages produced: %d, Consumed: %d, Known Lost: %d\n"),
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
		Sleep(rand() / 5);  // random 넘버만큼 sleep, consumer한테 일할 기회를 부여함 (random한 시간을 5로 나눈 값 만큼 지연)

			/* Adjust the divisor to change message generation rate */
		/* Get the buffer, fill it */

		WaitForSingleObject(mBlock.mGuard, INFINITE);
		//  mutexmGuard가 signaled state가 될때까지 기다림 
		// 인자 dwMilisecondes를 INFINITE로 설정했음으로 무한정 대기 
		// mutext는 WaitFoSingleObject 함수가 반환될 때 자동으로 non-signaled 상태가 된다. (auto-reset 이라서) 
		// 따라서 이 WaitForSingleObject 함수를 지나면 mutex는 non-signaled state가 된다.

		__try { // mutex가 보호하고있는 임계영역 
			if (!mBlock.fStop) { // stop flag가 0인 경우 if문 실행
				mBlock.fReady = 0; // ready확인을 위해 flag를 0으로 바꾼다.
				MessageFill(&mBlock); // MessageFill를 호출하여 메세지 블록 채우기
				mBlock.fReady = 1; // 소비자한테 알려주기위해서 ready flag를 1로 바꾼다. (단순한 flag 사용한 동기화 기법)

				// mSequence 하나 증가시킴
				// consumer에서도 mSequence를 읽어볼 수 있기 때문에 InterlockedIncrement로 증가시켜서 방해 안받음
				InterlockedIncrement(&mBlock.mSequence);
				cout << mBlock.mSequence << "번째 생산자가 메세지를 생산했습니다." << endl;
			
				SetEvent(mBlock.mReady); /* Signal that a message is ready. */
				// setEvent를 통해 consumer가 들어갈 수 있다는 사실을 알려줌 (즉, 이벤트객체 핸들인 mReady를 signaled state로 만들어준 것)
			}
		}
		__finally { 
			ReleaseMutex(mBlock.mGuard); // 호출 스레드가 점유하던 mutext 객체를 release (non-signaled state에서 signaled state로 바뀜)
		}
	}
	return 0;
}

DWORD WINAPI Consume(void* arg)
{
	cout << "Consume 스레드 함수 입니다." << endl;

	DWORD ShutDown = 0;
	CHAR command[10]; // 문자 10개짜리 버퍼
	/* Consume the NEXT message when prompted by the user */
	while (!ShutDown) { /* This is the only thread accessing stdin, stdout */
		_tprintf(_T("\n**Enter 'c' for Consume; 's' to stop: "));// 소비자 동작을 하려면 c 입력, 멈추려면 s 입력
		_tscanf_s(_T("%9s"), command, sizeof(command) - 1); // command에 문자 입력받음 (s or c)
		if (command[0] == _T('s')) { // s 입력받으면 stop
			//  이 부분과 produce의 보호되는 영역이 같은 mutex로 보호
			WaitForSingleObject(mBlock.mGuard, INFINITE);
			// mutex mGuard가 signaled state가 될때까지 기다림 
			// 인자 dwMilisecondes를 INFINITE로 설정했음으로 무한정 대기 
			// mutext는 WaitFoSingleObject 함수가 반환될 때 자동으로 non-signaled 상태가 된다. (auto-reset 이라서) 
			// 따라서 이 WaitForSingleObject 함수를 지나면 mutex는 non-signaled state가 된다.

			ShutDown = mBlock.fStop = 1; // shotDown도 1로 바꿔서 루프 바져나오고, stop flag를 1로 바꿔서  Producer의 임계 영역 부분도 빠져나가게 된다
			ReleaseMutex(mBlock.mGuard); // 호출 스레드가 점유하던 mutex 객체를 release (non-signaled state에서 signaled state로 바뀜)
		}
		else if (command[0] == _T('c')) { // c 입력받으면 소비자 역할
			WaitForSingleObject(mBlock.mReady, INFINITE);
			// mReady가 signaled state가 될때까지 기다림 
			// 인자 dwMilisecondes를 INFINITE로 설정했음으로 무한정 대기 
			// mutext는 WaitFoSingleObject 함수가 반환될 때 자동으로 non-signaled 상태가 된다. (auto-reset 이라서) 
			// 따라서 이 WaitForSingleObject 함수를 지나면 mutex는 non-signaled state가 된다.

			WaitForSingleObject(mBlock.mGuard, INFINITE);
			__try {
				// fReady가 0일 경우 읽을 데이터가 없기 때문에 __finally로 이동
				// try 안에 _leave 키워드를 쓰면 __try 블록 끝으로 이동하게 되어, 자연스레 __finally문으로 이동하여 비용이 거의 들지 않게됨
				if (!mBlock.fReady) _leave;

				/* Wait for the event indicating a message is ready */
				MessageDisplay(&mBlock); // 메세지 출력
				
				// nCons 하나 증가시킴
				// Producer 에서도 nCons를 읽어볼 수 있기 때문에 InterlockedIncrement로 증가시켜서 방해 안받음
				InterlockedIncrement(&mBlock.nCons);
				cout << mBlock.nCons << "번째 소비자가 메세지를 소비했습니다." << endl;

				mBlock.nLost = mBlock.mSequence - mBlock.nCons; // 잃어버린거 계산
				mBlock.fReady = 0; // 다른 consumer 스레드가 또 있다면 if 문에서 빠져나감(메세지를 출력했으므로 fReady를 0으로 바꿔줌)
			}
			__finally { 
				ReleaseMutex(mBlock.mGuard);  // 호출 스레드가 점유하던 mutex 객체를 release (non-signaled state에서 signaled state로 바뀜)
			}
		}
		else {
			//command line에서 잘못 입력한 경우
			_tprintf(_T("Illegal command. Try again.\n"));
		}
	}
	return 0;
}

void MessageFill(MSG_BLOCK* msgBlock)
{
	/* Fill the message buffer, and include mChecksum and mTimestamp	*/
	/* This function is called from the producer thread while it 	*/
	/* owns the message block mutex					*/

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
	/* This function is called from the consumer thread while it 	*/
	/* owns the message block mutex					*/

	// Message block을 출력하기 위한 함수, 소비자에서 호출됨

	DWORD i, tcheck = 0;
	TCHAR timeValue[26];

	for (i = 0; i < DATA_SIZE; i++)
		tcheck ^= msgBlock->mData[i]; // MessageFill에서 만든 랜덤 데이터 읽어온걸 exclusive or로 누적해서 tcheck 채움

	_tctime_s(timeValue, 26, &(msgBlock->mTimestamp)); // 몇번째 데이터고 언제 생산됐는지 출력
	_tprintf(_T("\nMessage number %d generated at: %s"),
		msgBlock->mSequence, timeValue);
	_tprintf(_T("First and last entries: %x %x\n"),
		msgBlock->mData[0], msgBlock->mData[DATA_SIZE - 1]); // 첫데이터와 마지막데이터만 출력
	if (tcheck == msgBlock->mChecksum) // MessageDisplay에서 읽은 tcheck와 MessageFill에서 만든  mChecksum의 값이 같을 경우 제대로 읽음
		_tprintf(_T("GOOD ->mChecksum was validated.\n"));
	else // tcheck와 mChecksum의 값이 다를 경우 잘못 읽음
		_tprintf(_T("BAD  ->mChecksum failed. message was corrupted\n"));

	return;
}