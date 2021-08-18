// SerialCommApp.cpp : DLL の初期化ルーチンです。
//

#include "stdafx.h"
#include "SerialCommApp.h"
#include "SerialComm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define AMOUNT_TO_READ          1024
#define NUM_READSTAT_HANDLES    1

CRITICAL_SECTION gStatusCritical;
CRITICAL_SECTION gcsWriterHeap;
CRITICAL_SECTION gcsDataHeap;

HANDLE ghWriterHeap = NULL;
HANDLE ghWriterEvent = NULL;
HANDLE ghTransferCompleteEvent = NULL;
HANDLE hTransferAbortEvent = NULL;
HANDLE hTransferThread = NULL;

COMMTIMEOUTS gTimeoutsDefault = { 0x01, 0, 0, 0, 0 };
DWORD  gdwReceiveState = NULL;
HANDLE ghThreadExitEvent = NULL;

void ErrorReporter( int nLine )
{
	char* szExtended;

	DWORD dwErr = GetLastError();

	DWORD dwExtSize = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | 80 ,
									 NULL, dwErr,
									 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
									 (LPTSTR)&szExtended, 0, NULL );

	char* szFinal = (char*)LocalAlloc( LPTR, dwExtSize + 128 );

	wsprintf( szFinal, "L%d: [%d]%s\n", nLine, dwErr, szExtended );
	OutputDebugString( szFinal );

	LocalFree( szFinal );
	LocalFree( (HLOCAL)*szExtended );
}

DWORD WINAPI TransferThreadProc(LPVOID lpVoid)
{
	DWORD  dwPacketSize, dwMaxPackets, dwFileSize;
    DWORD  dwTransferPos;
    HANDLE hDataHeap = NULL;
    BOOL fStarted = TRUE;
    BOOL fAborting = FALSE;

	CSerialCommAppApp *pApp = (CSerialCommAppApp*)lpVoid;

	//dwPacketSize = MAX_WRITE_BUFFER;
	dwMaxPackets = pApp->m_SerialData.WriteData.dwSize / MAX_WRITE_BUFFER;
	dwPacketSize = pApp->m_SerialData.WriteData.dwSize % MAX_WRITE_BUFFER;
	dwFileSize = pApp->m_SerialData.WriteData.dwSize;

    if ( !fAborting )
	{
        SYSTEM_INFO sysInfo;

        GetSystemInfo( &sysInfo );
        hDataHeap = HeapCreate( 0, sysInfo.dwPageSize * 2, sysInfo.dwPageSize * 10 );
        if ( hDataHeap == NULL )
		{
			ErrorReporter( __LINE__ );
            fAborting = TRUE;
        }
    }

    if ( !fAborting )
	{
		if ( !pApp->WriterAddNewNode(WRITE_FILESTART, dwFileSize, 0, pApp->m_SerialData.WriteData.lpBuf, NULL) )
            fAborting = TRUE;
    }

    if ( WaitForSingleObject(hTransferAbortEvent, 0) == WAIT_OBJECT_0 )
        fAborting = TRUE;

	dwTransferPos = 0;

	while ( !fAborting )
	{
        char* lpDataBuf;
        PWRITEREQUEST pWrite;

        lpDataBuf = (char*)HeapAlloc( hDataHeap, 0, dwPacketSize );
        pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
		if ( (lpDataBuf != NULL) && (pWrite != NULL) )
		{
			DWORD dwRead;

			for ( dwRead = 0 ; dwRead < dwPacketSize ; dwRead++ )
				lpDataBuf[dwRead] = pApp->m_SerialData.WriteData.lpBuf[dwTransferPos++];

		    pWrite->dwWriteType  = WRITE_FILE;
		    pWrite->dwSize       = dwRead;
		    pWrite->ch           = 0;
		    pWrite->lpBuf        = lpDataBuf;
		    pWrite->hHeap        = hDataHeap;
			pApp->AddToLinkedList( pWrite );

			if ( dwRead != dwPacketSize )
				break;
		}
		else
		{
			BOOL fRes;

            if ( lpDataBuf )
			{
                EnterCriticalSection( &gcsDataHeap );
                fRes = HeapFree( hDataHeap, 0, lpDataBuf );
                LeaveCriticalSection( &gcsDataHeap );
                if ( !fRes )
					ErrorReporter( __LINE__ );
            }

            if ( pWrite )
			{
                EnterCriticalSection( &gcsWriterHeap );
                fRes = HeapFree( ghWriterHeap, 0, pWrite );
                LeaveCriticalSection( &gcsWriterHeap );
                if ( !fRes )
					ErrorReporter( __LINE__ );
            }

			OutputDebugString("Xfer: A heap is full.  Waiting...\n");

            if ( WaitForSingleObject(hTransferAbortEvent, 200) == WAIT_OBJECT_0 )
                fAborting = TRUE;
		}

        if ( WaitForSingleObject(hTransferAbortEvent, 0) == WAIT_OBJECT_0 )
            fAborting = TRUE;
	}

	OutputDebugString("Xfer: Done sending packets.\n");

    if ( fAborting )
	{
        OutputDebugString("Xfer: Sending Abort Packet to writer\n");
        pApp->WriterAddFirstNodeTimeout( WRITE_ABORT, dwFileSize, 0, NULL, NULL, 500 );
    }
    else
	{
        pApp->WriterAddNewNodeTimeout( WRITE_FILEEND, dwFileSize, 0, NULL, NULL, 500 );
	}

    {
        HANDLE hEvents[2];
        DWORD dwRes;
        BOOL  fTransferComplete;
        
        hEvents[0] = ghTransferCompleteEvent;
        hEvents[1] = hTransferAbortEvent;

        OutputDebugString("Xfer: Waiting for transfer complete signal from writer\n");
        do
		{
            ResetEvent( hTransferAbortEvent );

            dwRes = WaitForMultipleObjects( 2, hEvents, FALSE, INFINITE );
            switch ( dwRes )
			{
            case WAIT_OBJECT_0:      
                fTransferComplete = TRUE;   
                OutputDebugString("Transfer complete signal rec'd\n");
                break;

            case WAIT_OBJECT_0 + 1:  
                fAborting = TRUE;           
                OutputDebugString("Transfer abort signal rec'd\n");
                OutputDebugString("Xfer: Sending Abort Packet to writer\n");
                if ( !pApp->WriterAddFirstNodeTimeout(WRITE_ABORT, dwFileSize, 0, NULL, NULL, 500) )
                    OutputDebugString("Can't add abort packet\n");
                break;

            case WAIT_TIMEOUT:
				ErrorReporter( __LINE__ );
				break;

            default:
				ErrorReporter( __LINE__ );
                fTransferComplete = TRUE;
                break;
            }
        } while ( !fTransferComplete );
    }

	OutputDebugString("Xfer: transfer complete\n");

    //if ( !fAborting )
    //    ShowTransferStatistics( GetTickCount(), dwStartTime, dwFileSize );

    if ( hDataHeap != NULL )
	{
        if ( !HeapDestroy(hDataHeap) )
            ErrorReporter( __LINE__ );
    }

    //if ( !fAborting )
    //    PostMessage(ghwndMain, WM_COMMAND, ID_TRANSFER_ABORTSENDING, 0);

	return 1;
}

DWORD WINAPI ReaderProc( LPVOID lpVoid )
{
    OVERLAPPED osReader = { NULL };  // overlapped structure for read operations
    OVERLAPPED osStatus = { NULL };  // overlapped structure for status operations
    HANDLE     hArray[NUM_READSTAT_HANDLES];
    DWORD      dwStoredFlags = 0xFFFFFFFF;      // local copy of event flags
    DWORD      dwCommEvent;     // result from WaitCommEvent
    DWORD      dwOvRes;         // result from GetOverlappedResult
    DWORD 	   dwRead;          // bytes actually read
    DWORD      dwRes;           // result from WaitForSingleObject
    BOOL       fWaitingOnRead = FALSE;
    BOOL       fWaitingOnStat = FALSE;
    BOOL       fThreadDone = FALSE;
    char   	   lpBuf[AMOUNT_TO_READ];

	CSerialCommAppApp *pApp = (CSerialCommAppApp*)lpVoid;

    osReader.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if ( osReader.hEvent == NULL )
		ErrorReporter( __LINE__ );

    osStatus.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( osStatus.hEvent == NULL )
		ErrorReporter( __LINE__ );

    hArray[0] = osReader.hEvent;
    hArray[1] = osStatus.hEvent;
    hArray[2] = NULL;
    hArray[3] = NULL;

    while ( !fThreadDone )
	{
        if (NOREADING( pApp->m_SerialData.TTYInfo ) )
            fWaitingOnRead = TRUE;

        if ( !fWaitingOnRead )
		{
			if ( !ReadFile(COMDEV(pApp->m_SerialData.TTYInfo), lpBuf, AMOUNT_TO_READ, &dwRead, &osReader) )
			{
                if ( GetLastError() != ERROR_IO_PENDING )	  // read not delayed?
					ErrorReporter( __LINE__ );

                fWaitingOnRead = TRUE;
            }
            else
			{
                if ( (dwRead != MAX_READ_BUFFER) && SHOWTIMEOUTS(pApp->m_SerialData.TTYInfo) )
                    OutputDebugString( "読取りはすぐにタイムアウトしました。\r\n" );

                if ( dwRead )
				{
					LPFNRECEPTION lpfnCallBack = NULL;

					lpfnCallBack = (LPFNRECEPTION)LPFNCALLBACK( pApp->m_SerialData.TTYInfo );
                    lpfnCallBack( COMDEV(pApp->m_SerialData.TTYInfo), lpBuf, dwRead );
				}
            }
        }

        if ( dwStoredFlags != EVENTFLAGS(pApp->m_SerialData.TTYInfo) )
		{
            dwStoredFlags = EVENTFLAGS( pApp->m_SerialData.TTYInfo );
            if ( !SetCommMask(COMDEV(pApp->m_SerialData.TTYInfo), dwStoredFlags) )
                ErrorReporter( __LINE__ );
        }

        if ( NOEVENTS(pApp->m_SerialData.TTYInfo) )
            fWaitingOnStat = TRUE;

        if ( !fWaitingOnStat )
		{
            if ( NOEVENTS(pApp->m_SerialData.TTYInfo) )
                fWaitingOnStat = TRUE;
            else
			{
                if ( !WaitCommEvent(COMDEV(pApp->m_SerialData.TTYInfo), &dwCommEvent, &osStatus) )
				{
                    if (GetLastError() != ERROR_IO_PENDING)	  // Wait not delayed?
                        ErrorReporter( __LINE__ );
                    else
                        fWaitingOnStat = TRUE;
                }
                else
				{
                    //ReportStatusEvent( dwCommEvent );
					ErrorReporter( __LINE__ );
				}
            }
        }

        //if ( fWaitingOnStat && fWaitingOnRead )
        if ( fWaitingOnRead )
		{
            dwRes = WaitForMultipleObjects( NUM_READSTAT_HANDLES, hArray, FALSE, STATUS_CHECK_TIMEOUT );
            switch ( dwRes )
            {
                case WAIT_OBJECT_0:
                    if ( !GetOverlappedResult(COMDEV(pApp->m_SerialData.TTYInfo), &osReader, &dwRead, FALSE) )
					{
                        if ( GetLastError() == ERROR_OPERATION_ABORTED )
                            OutputDebugString("読取りが中止されました\r\n");
                        else
							ErrorReporter( __LINE__ );
                    }
                    else
					{      // read completed successfully
                        if ( (dwRead != MAX_READ_BUFFER) && SHOWTIMEOUTS(pApp->m_SerialData.TTYInfo) )
                            OutputDebugString("読み取りタイムアウトが重複しています。\r\n");

                        if ( dwRead )
						{
							LPFNRECEPTION lpfnCallBack = NULL;

							lpfnCallBack = (LPFNRECEPTION)LPFNCALLBACK( pApp->m_SerialData.TTYInfo );
							lpfnCallBack( COMDEV(pApp->m_SerialData.TTYInfo), lpBuf, dwRead );
						}
                    }

                    fWaitingOnRead = FALSE;
                    break;

                case WAIT_OBJECT_0 + 1: 
                    if ( !GetOverlappedResult(COMDEV(pApp->m_SerialData.TTYInfo), &osStatus, &dwOvRes, FALSE) )
					{
                        if ( GetLastError() == ERROR_OPERATION_ABORTED )
                            OutputDebugString("WaitCommEvent API が中止\r\n");
                        else
                            ErrorReporter( __LINE__ );
                    }
                    else
					{
                        //ReportStatusEvent( dwCommEvent );
						ErrorReporter( __LINE__ );
					}

                    fWaitingOnStat = FALSE;
                    break;

                case WAIT_OBJECT_0 + 2:
                    //StatusMessage();
                    break;

                case WAIT_OBJECT_0 + 3:
                    fThreadDone = TRUE;
                    break;

                case WAIT_TIMEOUT:
					ErrorReporter( __LINE__ );
                    break;                       

                default:
                    ErrorReporter( __LINE__ );
                    break;
            }
        }
	}

    CloseHandle( osReader.hEvent );
    CloseHandle( osStatus.hEvent );

	return 1;
}

DWORD WINAPI WriterProc( LPVOID lpVoid )
{
    SYSTEM_INFO sysInfo;
    HANDLE hArray[2];
    DWORD dwRes;
    DWORD dwSize;
    BOOL fDone = FALSE;

	CSerialCommAppApp *pApp = (CSerialCommAppApp*)lpVoid;

    GetSystemInfo(&sysInfo);
    ghWriterHeap = HeapCreate( 0, sysInfo.dwPageSize*2, sysInfo.dwPageSize*4 );
    if (ghWriterHeap == NULL)
        ErrorReporter( __LINE__ );

    ghWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if ( ghWriterEvent == NULL )
        ErrorReporter( __LINE__ );

    ghTransferCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if ( ghTransferCompleteEvent == NULL )
        ErrorReporter( __LINE__ );

    dwSize = sizeof(WRITEREQUEST);
    gpWriterHead = (WRITEREQUEST*)HeapAlloc(ghWriterHeap, HEAP_ZERO_MEMORY, dwSize);
    gpWriterTail = (WRITEREQUEST*)HeapAlloc(ghWriterHeap, HEAP_ZERO_MEMORY, dwSize);
    gpWriterHead->pNext = gpWriterTail;
    gpWriterTail->pPrev = gpWriterHead;

    hArray[0] = ghWriterEvent;
    hArray[1] = ghThreadExitEvent;

    while ( !fDone )
	{
        dwRes = WaitForMultipleObjects( 2, hArray, FALSE, WRITE_CHECK_TIMEOUT );
        switch ( dwRes )
        {
		case WAIT_TIMEOUT:
			break;

		case WAIT_FAILED:
			ErrorReporter( __LINE__ );
			break;

		case WAIT_OBJECT_0:
			{
			    PWRITEREQUEST pWrite;
			    BOOL fRes;

				pWrite = gpWriterHead->pNext;
				while ( pWrite != gpWriterTail )
				{
					switch ( pWrite->dwWriteType )
					{
					default:
						OutputDebugString("不正な書き込みリクエスト\r\n");
						break;

					case WRITE_CHAR:
						pApp->WriterGeneric( &(pWrite->ch), 1 );
						break;

					case WRITE_FILESTART:
						break;

					case WRITE_FILE:
						pApp->WriterGeneric( pWrite->lpBuf, pWrite->dwSize );

						EnterCriticalSection( &gcsDataHeap );
						fRes = HeapFree( pWrite->hHeap, 0, pWrite->lpBuf );
						LeaveCriticalSection( &gcsDataHeap );

						if ( !fRes )
							ErrorReporter( __LINE__ );
						break;

					case WRITE_FILEEND:
						if ( !SetEvent(ghTransferCompleteEvent) )
							ErrorReporter( __LINE__ );
						break;

					case WRITE_ABORT:
						{
						    PWRITEREQUEST pCurrent;
							PWRITEREQUEST pNextNode;
							BOOL fRes;
							int i = 0;
							char szMessage[128];

							EnterCriticalSection(&gcsWriterHeap);
							pCurrent = pWrite->pNext;

							while ( pCurrent != gpWriterTail )
							{
								pNextNode = pCurrent->pNext;
								fRes = HeapFree(ghWriterHeap, 0, pCurrent);
								if ( !fRes )
									break;
								i++;
								pCurrent = pNextNode;
							}

							pWrite->pNext = gpWriterTail;
							gpWriterTail->pPrev = pWrite;
							LeaveCriticalSection(&gcsWriterHeap);

							wsprintf(szMessage, "%d packets ignored.\n", i);
							OutputDebugString(szMessage);

							if ( !fRes )
								OutputDebugString("HeapFree (Writer heap)\r\n");

							if ( !SetEvent(ghTransferCompleteEvent) )
								ErrorReporter( __LINE__ );
						}
						break;

					case WRITE_BLOCK:
						pApp->WriterGeneric( pWrite->lpBuf, pWrite->dwSize );
						break;
					}
				}

				pWrite = pApp->RemoveFromLinkedList( pWrite );
				pWrite = gpWriterHead->pNext;
			}
			break;

		case WAIT_OBJECT_0 + 1:
			fDone = TRUE;
			break;
        }
    }

    CloseHandle( ghTransferCompleteEvent );
    CloseHandle( ghWriterEvent );

	HeapDestroy(ghWriterHeap );

	return 1;
}


//
//TODO: この DLL が MFC DLL に対して動的にリンクされる場合、
//		MFC 内で呼び出されるこの DLL からエクスポートされたどの関数も
//		関数の最初に追加される AFX_MANAGE_STATE マクロを
//		持たなければなりません。
//
//		例:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// 通常関数の本体はこの位置にあります
//		}
//
//		このマクロが各関数に含まれていること、MFC 内の
//		どの呼び出しより優先することは非常に重要です。
//		これは関数内の最初のステートメントでなければな 
//		らないことを意味します、コンストラクタが MFC
//		DLL 内への呼び出しを行う可能性があるので、オブ
//		ジェクト変数の宣言よりも前でなければなりません。
//
//		詳細については MFC テクニカル ノート 33 および
//		58 を参照してください。
//

// CSerialCommAppApp

BEGIN_MESSAGE_MAP(CSerialCommAppApp, CWinApp)
END_MESSAGE_MAP()


// CSerialCommAppApp コンストラクション

CSerialCommAppApp::CSerialCommAppApp()
{
	// TODO: この位置に構築用コードを追加してください。
	// ここに InitInstance 中の重要な初期化処理をすべて記述してください。
}


// 唯一の CSerialCommAppApp オブジェクトです。

CSerialCommAppApp theApp;

// CSerialCommAppApp 初期化

BOOL CSerialCommAppApp::InitInstance()
{
	CWinApp::InitInstance();

    InitializeCriticalSection( &gStatusCritical );
    InitializeCriticalSection( &gcsWriterHeap );
    InitializeCriticalSection( &gcsDataHeap );

    ghThreadExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ghThreadExitEvent == NULL)
        ErrorReporter( __LINE__ );        

	return TRUE;
}

int CSerialCommAppApp::ExitInstance()
{
    DeleteCriticalSection( &gStatusCritical );
    DeleteCriticalSection( &gcsWriterHeap );
    DeleteCriticalSection( &gcsDataHeap );

	if ( ghThreadExitEvent != NULL )
	    CloseHandle( ghThreadExitEvent );

	return CWinApp::ExitInstance();
}

void CSerialCommAppApp::TransferTextStart(PWRITEREQUEST pWriteComm)
{
    DWORD dwThreadId;

	m_SerialData.WriteData.dwSize = pWriteComm->dwSize;
	m_SerialData.WriteData.lpBuf = pWriteComm->lpBuf;

    hTransferAbortEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if (hTransferAbortEvent == NULL)
        ErrorReporter( __LINE__ );

	hTransferThread = ::CreateThread( NULL, 0, 
                                (LPTHREAD_START_ROUTINE)TransferThreadProc, 
                                (LPVOID)&theApp, 0, &dwThreadId );
    if (hTransferThread == NULL)
	{
        ErrorReporter( __LINE__ );

	    SetEvent( hTransferAbortEvent );

		OutputDebugString( "Waiting for transfer thread...\n" );

	    if ( WaitForSingleObject(hTransferThread, 3000) != WAIT_OBJECT_0 )
		{
	        ErrorReporter( __LINE__ );
			TerminateThread(hTransferThread, 0);
		}
		else
			OutputDebugString("Transfer thread exited\n");

		CloseHandle(hTransferAbortEvent);
		CloseHandle(hTransferThread);

		TRANSFERRING(m_SerialData.TTYInfo) = FALSE;
    }
    else
	{
        TRANSFERRING(m_SerialData.TTYInfo) = TRUE;
	}
}

BOOL CSerialCommAppApp::InitTTYInfo( SERIALDATA* pSerialData )
{
	COMDEV( m_SerialData.TTYInfo )        = NULL ;
    CONNECTED( m_SerialData.TTYInfo )     = FALSE ;
    LOCALECHO( m_SerialData.TTYInfo )     = FALSE ;
    CURSORSTATE( m_SerialData.TTYInfo )   = CS_HIDE ;
    PORT( m_SerialData.TTYInfo )          = 1 ;
    BAUDRATE( m_SerialData.TTYInfo )      = CBR_9600 ;
    BYTESIZE( m_SerialData.TTYInfo )      = 8 ;
    PARITY( m_SerialData.TTYInfo )        = NOPARITY ;
    STOPBITS( m_SerialData.TTYInfo )      = ONESTOPBIT ;
    AUTOWRAP( m_SerialData.TTYInfo )      = TRUE;
    NEWLINE( m_SerialData.TTYInfo )       = FALSE;
    XSIZE( m_SerialData.TTYInfo )         = 0 ;
    YSIZE( m_SerialData.TTYInfo )         = 0 ;
    XSCROLL( m_SerialData.TTYInfo )       = 0 ;
    YSCROLL( m_SerialData.TTYInfo )       = 0 ;
    COLUMN( m_SerialData.TTYInfo )	      = 0 ;
    ROW( m_SerialData.TTYInfo )           = MAXROWS - 1 ;
    //DISPLAYERRORS( m_SerialData.TTYInfo ) = TRUE ;

	LPFNCALLBACK( m_SerialData.TTYInfo )	= pSerialData->lpfnCallBack;

    TIMEOUTSNEW( m_SerialData.TTYInfo )   = gTimeoutsDefault;

    gdwReceiveState				          = RECEIVE_TTY;
    EVENTFLAGS( m_SerialData.TTYInfo )    = EVENTFLAGS_DEFAULT;
    FLAGCHAR( m_SerialData.TTYInfo )      = FLAGCHAR_DEFAULT;

    DTRCONTROL( m_SerialData.TTYInfo )    = DTR_CONTROL_ENABLE;
    RTSCONTROL( m_SerialData.TTYInfo )    = RTS_CONTROL_ENABLE;
    XONCHAR( m_SerialData.TTYInfo )       = ASCII_XON;
    XOFFCHAR( m_SerialData.TTYInfo )      = ASCII_XOFF;
    XONLIMIT( m_SerialData.TTYInfo )      = 0;
    XOFFLIMIT( m_SerialData.TTYInfo )     = 0;
    CTSOUTFLOW( m_SerialData.TTYInfo )    = FALSE;
    DSROUTFLOW( m_SerialData.TTYInfo )    = FALSE;
    DSRINFLOW( m_SerialData.TTYInfo )     = FALSE;
    XONXOFFOUTFLOW( m_SerialData.TTYInfo )  = FALSE;
    XONXOFFINFLOW( m_SerialData.TTYInfo )   = FALSE;
    TXAFTERXOFFSENT( m_SerialData.TTYInfo ) = FALSE;

    NOREADING( m_SerialData.TTYInfo )		= FALSE;
    NOWRITING( m_SerialData.TTYInfo )		= FALSE;
    NOEVENTS( m_SerialData.TTYInfo )		= FALSE;
    NOSTATUS( m_SerialData.TTYInfo )		= FALSE;
    SHOWTIMEOUTS( m_SerialData.TTYInfo )	= FALSE;

	return 0;
}

HANDLE CSerialCommAppApp::SetupCommPort(void)
{
	TCHAR szPort[32] = { NULL };

	wsprintf( szPort, "COM%d", PORT(m_SerialData.TTYInfo) );
    COMDEV( m_SerialData.TTYInfo ) = CreateFile( szPort,  
                                      GENERIC_READ | GENERIC_WRITE, 
                                      0, 
                                      0, 
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                      0 );

    if ( COMDEV(m_SerialData.TTYInfo) == INVALID_HANDLE_VALUE )
	{   
        ErrorReporter( __LINE__ );
		return NULL;
    }

	if ( !GetCommTimeouts( COMDEV(m_SerialData.TTYInfo), &(TIMEOUTSORIG(m_SerialData.TTYInfo))) )
        ErrorReporter( __LINE__ );

	UpdateConnection();

    SetupComm( COMDEV(m_SerialData.TTYInfo), MAX_READ_BUFFER, MAX_WRITE_BUFFER );
    if ( !EscapeCommFunction(COMDEV(m_SerialData.TTYInfo), SETDTR) )
        ErrorReporter( __LINE__ );

	StartThreads();

	CONNECTED( m_SerialData.TTYInfo ) = TRUE ;

	return COMDEV(m_SerialData.TTYInfo);
}

void CSerialCommAppApp::StartThreads(void)
{
    DWORD dwReadStatId;
    DWORD dwWriterId;

	READSTATTHREAD(m_SerialData.TTYInfo) = ::CreateThread( NULL, 
                          0,
                          (LPTHREAD_START_ROUTINE) ReaderProc,
                          (LPVOID)&theApp, 
                          0, 
                          &dwReadStatId);

    if ( READSTATTHREAD(m_SerialData.TTYInfo) == NULL )
        ErrorReporter( __LINE__ );

	WRITERTHREAD(m_SerialData.TTYInfo) = ::CreateThread( NULL, 
                          0, 
                          (LPTHREAD_START_ROUTINE) WriterProc, 
                          (LPVOID)&theApp, 
                          0, 
                          &dwWriterId );
                   
    if ( WRITERTHREAD(m_SerialData.TTYInfo) == NULL )
        ErrorReporter( __LINE__ );

    return;
}

BOOL CSerialCommAppApp::UpdateConnection(void)
{
    DCB dcb = { NULL };
    
    dcb.DCBlength = sizeof(dcb);

    if ( !GetCommState(COMDEV(m_SerialData.TTYInfo), &dcb) )
		ErrorReporter( __LINE__ );

	dcb.BaudRate = BAUDRATE( m_SerialData.TTYInfo );
    dcb.ByteSize = BYTESIZE( m_SerialData.TTYInfo );
    dcb.Parity   = PARITY( m_SerialData.TTYInfo );
    dcb.StopBits = STOPBITS( m_SerialData.TTYInfo );

    if ( EVENTFLAGS(m_SerialData.TTYInfo) & EV_RXFLAG )
		dcb.EvtChar = FLAGCHAR( m_SerialData.TTYInfo );      
    else
		dcb.EvtChar = '\0';

    dcb.fDtrControl     = DTRCONTROL( m_SerialData.TTYInfo );
    dcb.fRtsControl     = RTSCONTROL( m_SerialData.TTYInfo );

    dcb.fOutxCtsFlow    = CTSOUTFLOW( m_SerialData.TTYInfo );
    dcb.fOutxDsrFlow    = DSROUTFLOW( m_SerialData.TTYInfo );
    dcb.fDsrSensitivity = DSRINFLOW( m_SerialData.TTYInfo );
    dcb.fOutX           = XONXOFFOUTFLOW( m_SerialData.TTYInfo );
    dcb.fInX            = XONXOFFINFLOW( m_SerialData.TTYInfo );
    dcb.fTXContinueOnXoff = TXAFTERXOFFSENT( m_SerialData.TTYInfo );
    dcb.XonChar         = XONCHAR( m_SerialData.TTYInfo );
    dcb.XoffChar        = XOFFCHAR( m_SerialData.TTYInfo );
    dcb.XonLim          = XONLIMIT( m_SerialData.TTYInfo );
    dcb.XoffLim         = XOFFLIMIT( m_SerialData.TTYInfo );

    dcb.fParity			= TRUE;

    if ( !SetCommState(COMDEV(m_SerialData.TTYInfo), &dcb) )
		ErrorReporter( __LINE__ );

    if ( !SetCommTimeouts(COMDEV(m_SerialData.TTYInfo), &(TIMEOUTSNEW(m_SerialData.TTYInfo))) )
		ErrorReporter( __LINE__ );

	return TRUE;
}

BOOL CSerialCommAppApp::BreakDownCommPort(void)
{
    if ( !CONNECTED(m_SerialData.TTYInfo) )
        return FALSE;

    CONNECTED( m_SerialData.TTYInfo ) = FALSE;

    if ( WaitForThreads(20000) != WAIT_OBJECT_0 )
		OutputDebugString( "BreakDownCommPort:スレッドのクローズでエラー発生\r\n" );

	if ( !EscapeCommFunction(COMDEV(m_SerialData.TTYInfo), CLRDTR) )
		ErrorReporter( __LINE__ );

    if ( !SetCommTimeouts(COMDEV(m_SerialData.TTYInfo),  &(TIMEOUTSORIG(m_SerialData.TTYInfo))) )
		ErrorReporter( __LINE__ );

    if ( !PurgeComm(COMDEV(m_SerialData.TTYInfo), PURGE_FLAGS) )
		ErrorReporter( __LINE__ );

    CloseHandle( COMDEV(m_SerialData.TTYInfo) );
    CloseHandle( READSTATTHREAD(m_SerialData.TTYInfo) );
    CloseHandle( WRITERTHREAD(m_SerialData.TTYInfo) );

	return TRUE;
}

DWORD CSerialCommAppApp::WaitForThreads(DWORD dwTimeout)
{
    HANDLE hThreads[2];
    DWORD  dwRes;

    hThreads[0] = READSTATTHREAD( m_SerialData.TTYInfo );
    hThreads[1] = WRITERTHREAD( m_SerialData.TTYInfo );

    SetEvent( m_hThreadExitEvent );

    dwRes = WaitForMultipleObjects(2, hThreads, TRUE, dwTimeout);
    switch ( dwRes )
    {
        case WAIT_OBJECT_0:
        case WAIT_OBJECT_0 + 1: 
            dwRes = WAIT_OBJECT_0;
            break;

        case WAIT_TIMEOUT:
            
            if (WaitForSingleObject(READSTATTHREAD(m_SerialData.TTYInfo), 0) == WAIT_TIMEOUT)
                ErrorReporter( __LINE__ );

            if (WaitForSingleObject(WRITERTHREAD(m_SerialData.TTYInfo), 0) == WAIT_TIMEOUT)
                ErrorReporter( __LINE__ );

            break;

        default:
            break;
    }

    ResetEvent( m_hThreadExitEvent );

    return dwRes;
}

void CSerialCommAppApp::WriterGeneric(char* lpBuf, DWORD dwToWrite)
{
    OVERLAPPED osWrite = { NULL };
    HANDLE hArray[2];
    DWORD dwWritten;
    DWORD dwRes;

    if ( NOWRITING(m_SerialData.TTYInfo) )
        return ;

    osWrite.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if ( osWrite.hEvent == NULL )
        ErrorReporter( __LINE__ );

    hArray[0] = osWrite.hEvent;
    hArray[1] = ghThreadExitEvent;

	if ( !WriteFile(COMDEV(m_SerialData.TTYInfo), lpBuf, dwToWrite, &dwWritten, &osWrite) )
	{
		if ( GetLastError() == ERROR_IO_PENDING )
		{
			dwRes = WaitForMultipleObjects( 2, hArray, FALSE, INFINITE );
			switch ( dwRes )
			{
			default:
				ErrorReporter( __LINE__ );
				break;

			case WAIT_OBJECT_0:
				SetLastError( ERROR_SUCCESS );
				if ( !GetOverlappedResult(COMDEV(m_SerialData.TTYInfo), &osWrite, &dwWritten, FALSE) )
				{
					if ( GetLastError() == ERROR_OPERATION_ABORTED )
						OutputDebugString("書き込みが中止されました\r\n");
					else
						ErrorReporter( __LINE__ );

					if ( dwWritten != dwToWrite )
					{
						if ( (GetLastError() == ERROR_SUCCESS) && SHOWTIMEOUTS(m_SerialData.TTYInfo) )
							OutputDebugString("書き込みがタイムアウトしました。(overlapped)\r\n");
						else
							ErrorReporter( __LINE__ );
					}
				}
				break;

			case WAIT_OBJECT_0 + 1:
				break;

			case WAIT_TIMEOUT:
				ErrorReporter( __LINE__ );
				break;

			case WAIT_FAILED:
				ErrorReporter( __LINE__ );
				break;
			}
		}
		else
		{
			ErrorReporter( __LINE__ );
		}
	}
	else
	{
		if ( dwWritten != dwToWrite )
			OutputDebugString("書き込みがタイムアウトしました。(immediate)\r\n");
	}

	CloseHandle( osWrite.hEvent );
}

PWRITEREQUEST CSerialCommAppApp::RemoveFromLinkedList(PWRITEREQUEST pNode)
{
    PWRITEREQUEST pNextNode;
    PWRITEREQUEST pPrevNode;
    BOOL bRes;

    EnterCriticalSection(&gcsWriterHeap);
    
    pNextNode = pNode->pNext;
    pPrevNode = pNode->pPrev;    
    
    bRes = HeapFree(ghWriterHeap, 0, pNode);
    
    pPrevNode->pNext = pNextNode;
    pNextNode->pPrev = pPrevNode;

    LeaveCriticalSection(&gcsWriterHeap);

    if (!bRes)
        OutputDebugString("HeapFree(write request)\r\n");

    return pNextNode;     // return the freed node's pNext (maybe the tail)
}

BOOL CSerialCommAppApp::WriterAddNewNode(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap)
{
    PWRITEREQUEST pWrite;

    pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
    if (pWrite == NULL)
	{
        ErrorReporter( __LINE__ );
        return FALSE;
    }

    pWrite->dwWriteType  = dwRequestType;
    pWrite->dwSize       = dwSize;
    pWrite->ch           = ch;
    pWrite->lpBuf        = lpBuf;
    pWrite->hHeap        = hHeap;

    AddToLinkedList( pWrite );

	return TRUE;
}

void CSerialCommAppApp::AddToLinkedList(PWRITEREQUEST pNode)
{
    PWRITEREQUEST pOldLast;

	EnterCriticalSection( &gcsWriterHeap );

    pOldLast = gpWriterTail->pPrev;

    pNode->pNext = gpWriterTail;
    pNode->pPrev = pOldLast;

    pOldLast->pNext = pNode;
    gpWriterTail->pPrev = pNode;

    LeaveCriticalSection(&gcsWriterHeap);

    if ( !SetEvent(ghWriterEvent) )
        ErrorReporter( __LINE__ );
}

BOOL CSerialCommAppApp::WriterAddFirstNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout)
{
    PWRITEREQUEST pWrite;

    pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
    if ( pWrite == NULL )
	{
        Sleep( dwTimeout );

		pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
        if ( pWrite == NULL )
		{
            ErrorReporter( __LINE__ );
            return FALSE;
        }
    }

	pWrite->dwWriteType  = dwRequestType;
    pWrite->dwSize       = dwSize;
    pWrite->ch           = ch;
    pWrite->lpBuf        = lpBuf;
    pWrite->hHeap        = hHeap;

    AddToFrontOfLinkedList( pWrite );
    
    return TRUE;
}

BOOL CSerialCommAppApp::WriterAddNewNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout)
{
    PWRITEREQUEST pWrite;

    pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
    if ( pWrite == NULL )
	{
        Sleep( dwTimeout );

		pWrite = (PWRITEREQUEST)HeapAlloc( ghWriterHeap, 0, sizeof(WRITEREQUEST) );
        if ( pWrite == NULL )
		{
            ErrorReporter( __LINE__ );
            return FALSE;
        }
    }

    pWrite->dwWriteType  = dwRequestType;
    pWrite->dwSize       = dwSize;
    pWrite->ch           = ch;
    pWrite->lpBuf        = lpBuf;
    pWrite->hHeap        = hHeap;

    AddToLinkedList( pWrite );
    
    return TRUE;
}

void CSerialCommAppApp::AddToFrontOfLinkedList(PWRITEREQUEST pNode)
{
    PWRITEREQUEST pNextNode;

	EnterCriticalSection( &gcsWriterHeap );

    pNextNode = gpWriterHead->pNext;
    
    pNextNode->pPrev = pNode;
    gpWriterHead->pNext = pNode;
    
    pNode->pNext = pNextNode;
    pNode->pPrev = gpWriterHead;

    LeaveCriticalSection( &gcsWriterHeap );

    if ( !SetEvent(ghWriterEvent) )
        ErrorReporter( __LINE__ );
}

//----------------------------------------------------------------------------
// オープン
//----------------------------------------------------------------------------
SERIALCOMM_API HANDLE WINAPI serialOpenComm( BOOL TTYCommMode, LPSERIALDATA pSerialData )
{
	if ( pSerialData == NULL )
		return NULL;

	if ( TTYCommMode == TTY_COMM_INIT )
	{
		theApp.InitTTYInfo( pSerialData );
		return &theApp;
	}

	LPFNCALLBACK( theApp.m_SerialData.TTYInfo )		= LPFNCALLBACK( pSerialData->TTYInfo );
	LPFNNOTIFY( theApp.m_SerialData.TTYInfo)		= LPFNNOTIFY( pSerialData->TTYInfo );

	PORT( theApp.m_SerialData.TTYInfo )				= PORT( pSerialData->TTYInfo );
	BAUDRATE( theApp.m_SerialData.TTYInfo )			= BAUDRATE( pSerialData->TTYInfo );
	BYTESIZE( theApp.m_SerialData.TTYInfo )			= BYTESIZE( pSerialData->TTYInfo );
    PARITY( theApp.m_SerialData.TTYInfo )			= PARITY( pSerialData->TTYInfo );
    STOPBITS( theApp.m_SerialData.TTYInfo )			= STOPBITS( pSerialData->TTYInfo );
	FLAGCHAR( theApp.m_SerialData.TTYInfo )			= FLAGCHAR( pSerialData->TTYInfo );

	DTRCONTROL( theApp.m_SerialData.TTYInfo )		= DTRCONTROL( pSerialData->TTYInfo );
	RTSCONTROL( theApp.m_SerialData.TTYInfo )		= RTSCONTROL( pSerialData->TTYInfo );

    CTSOUTFLOW( theApp.m_SerialData.TTYInfo )		= CTSOUTFLOW( pSerialData->TTYInfo );
    DSROUTFLOW( theApp.m_SerialData.TTYInfo )		= DSROUTFLOW( pSerialData->TTYInfo );
    DSRINFLOW( theApp.m_SerialData.TTYInfo )		= DSRINFLOW( pSerialData->TTYInfo );
    XONXOFFOUTFLOW( theApp.m_SerialData.TTYInfo )	= XONXOFFOUTFLOW( pSerialData->TTYInfo );
    XONXOFFINFLOW( theApp.m_SerialData.TTYInfo )	= XONXOFFINFLOW( pSerialData->TTYInfo );
    TXAFTERXOFFSENT( theApp.m_SerialData.TTYInfo )	= TXAFTERXOFFSENT( pSerialData->TTYInfo );
    XONCHAR( theApp.m_SerialData.TTYInfo )			= XONCHAR( pSerialData->TTYInfo );
    XOFFCHAR( theApp.m_SerialData.TTYInfo )			= XOFFCHAR( pSerialData->TTYInfo );
    XONLIMIT( theApp.m_SerialData.TTYInfo )			= XONLIMIT( pSerialData->TTYInfo );
    XOFFLIMIT( theApp.m_SerialData.TTYInfo )		= XOFFLIMIT( pSerialData->TTYInfo );

	//PARITY( theApp.m_SerialData.TTYInfo )			= pSerialData->TTYInfo.bParity;

	theApp.SetupCommPort();

	return &theApp;
}

//----------------------------------------------------------------------------
// クローズ
// 現在は何もしない
//----------------------------------------------------------------------------
SERIALCOMM_API void WINAPI serialCloseComm( HANDLE hSerial )
{
	if ( hSerial == NULL )
		return;

	CSerialCommAppApp* pApp = (CSerialCommAppApp*)hSerial;
	if ( pApp != &theApp )
		return;
}

//----------------------------------------------------------------------------
// 切断
//----------------------------------------------------------------------------
SERIALCOMM_API void WINAPI serialBreakDownComm( HANDLE hSerial )
{
	if ( hSerial == NULL )
		return;

	CSerialCommAppApp* pApp = (CSerialCommAppApp*)hSerial;
	if ( pApp != &theApp )
		return;

	pApp->BreakDownCommPort();
}

//----------------------------------------------------------------------------
// データ送信
//----------------------------------------------------------------------------
SERIALCOMM_API bool WINAPI serialWriteComm( HANDLE hSerial, char* lpData, DWORD dwDataSize )
{
	if ( hSerial == NULL )
		return false;

	CSerialCommAppApp* pApp = (CSerialCommAppApp*)hSerial;
	if ( pApp != &theApp )
		return false;

	pApp->m_SerialData.WriteData.dwSize = dwDataSize;
	pApp->m_SerialData.WriteData.lpBuf = lpData;

	pApp->WriterGeneric( pApp->m_SerialData.WriteData.lpBuf,
						 pApp->m_SerialData.WriteData.dwSize );

	return true;
}
