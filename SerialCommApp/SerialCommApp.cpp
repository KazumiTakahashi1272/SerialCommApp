// SerialCommApp.cpp : DLL の初期化ルーチンです。
//

#include "stdafx.h"
#include "SerialCommApp.h"
#include "SerialComm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define AMOUNT_TO_READ          512
#define NUM_READSTAT_HANDLES    4

CRITICAL_SECTION gStatusCritical;
CRITICAL_SECTION gcsWriterHeap;
CRITICAL_SECTION gcsDataHeap;

HANDLE ghWriterHeap = NULL;
HANDLE ghWriterEvent = NULL;
HANDLE ghTransferCompleteEvent = NULL;
HANDLE hTransferAbortEvent = NULL;

COMMTIMEOUTS gTimeoutsDefault = { 0x01, 0, 0, 0, 0 };
DWORD  gdwReceiveState = NULL;
HANDLE ghThreadExitEvent = NULL;


DWORD WINAPI TransferThreadProc(LPVOID lpVoid)
{
	DWORD  dwPacketSize, dwMaxPackets, dwFileSize;
    DWORD  dwTransferPos;
    HANDLE hDataHeap;
    BOOL fStarted = TRUE;
    BOOL fAborting = FALSE;

	CSerialCommAppApp *pApp = (CSerialCommAppApp*)lpVoid;

	dwPacketSize = MAX_WRITE_BUFFER;
	dwMaxPackets = pApp->m_SerialData.pWriteComm->dwSize / dwPacketSize;
	dwPacketSize = pApp->m_SerialData.pWriteComm->dwSize % dwPacketSize;
	dwFileSize = pApp->m_SerialData.pWriteComm->dwSize;
	fAborting = TRUE;

    if ( !fAborting )
	{
        SYSTEM_INFO sysInfo;

        GetSystemInfo( &sysInfo );
        hDataHeap = HeapCreate( 0, sysInfo.dwPageSize * 2, sysInfo.dwPageSize * 10 );
        if ( hDataHeap == NULL )
		{
            OutputDebugString("HeapCreate (Data Heap)\r\n");
            fAborting = TRUE;
        }
    }

    if ( !fAborting )
	{
        if ( !pApp->WriterAddNewNode(WRITE_FILESTART, dwFileSize, 0, NULL, NULL) )
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
				lpDataBuf[dwRead] = pApp->m_SerialData.pWriteComm->lpBuf[dwTransferPos++];

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
                    OutputDebugString("HeapFree (Data block)\r\n");
            }

            if ( pWrite )
			{
                EnterCriticalSection( &gcsWriterHeap );
                fRes = HeapFree( ghWriterHeap, 0, pWrite );
                LeaveCriticalSection( &gcsWriterHeap );
                if ( !fRes )
                    OutputDebugString("HeapFree (Writer block)");
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

            case WAIT_TIMEOUT:                                   break;
            default:
                OutputDebugString("WaitForMultipleObjects(Transfer Complete Event and Transfer Abort Event)\r\n");
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
            OutputDebugString("HeapDestroy (data heap)\r\n");
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
		OutputDebugString( "CreateEvent API (Reader Event)\r\n" );

    osStatus.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( osStatus.hEvent == NULL )
		OutputDebugString( "CreateEvent API (Status Event)\r\n" );

    hArray[0] = osReader.hEvent;
    hArray[1] = osStatus.hEvent;
    hArray[2] = NULL;
    hArray[3] = NULL;

    while ( !fThreadDone )
	{
        if ( !fWaitingOnRead )
		{
			if ( !ReadFile(COMDEV(pApp->m_SerialData.TTYInfo), lpBuf, AMOUNT_TO_READ, &dwRead, &osReader) )
			{
                if ( GetLastError() != ERROR_IO_PENDING )	  // read not delayed?
                    OutputDebugString( "ReaderAndStatusProcのReadFile\r\n" );

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
                    lpfnCallBack( lpBuf, dwRead );
				}
            }
        }

        if ( dwStoredFlags != EVENTFLAGS(pApp->m_SerialData.TTYInfo) )
		{
            dwStoredFlags = EVENTFLAGS( pApp->m_SerialData.TTYInfo );
            if ( !SetCommMask(COMDEV(pApp->m_SerialData.TTYInfo), dwStoredFlags) )
                OutputDebugString("SetCommMask API");
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
                        OutputDebugString( "WaitCommEvent API\r\n" );
                    else
                        fWaitingOnStat = TRUE;
                }
                else
				{
                    //ReportStatusEvent( dwCommEvent );
				}
            }
        }

        if ( fWaitingOnStat && fWaitingOnRead )
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
                            OutputDebugString("GetOverlappedResult API (in Reader)");
                    }
                    else
					{      // read completed successfully
                        if ( (dwRead != MAX_READ_BUFFER) && SHOWTIMEOUTS(pApp->m_SerialData.TTYInfo) )
                            OutputDebugString("読み取りタイムアウトが重複しています。\r\n");

                        if ( dwRead )
						{
							LPFNRECEPTION lpfnCallBack = NULL;

							lpfnCallBack = (LPFNRECEPTION)LPFNCALLBACK( pApp->m_SerialData.TTYInfo );
							lpfnCallBack( lpBuf, dwRead );
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
                            OutputDebugString("GetOverlappedResult API (in Reader)");
                    }
                    else
					{
                        //ReportStatusEvent( dwCommEvent );
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
                    if ( !NOSTATUS(pApp->m_SerialData.TTYInfo) )
					{
                        //CheckModemStatus( FALSE );
                        //CheckComStat( FALSE );
                    }
                    break;                       

                default:
                    OutputDebugString("WaitForMultipleObjects(Reader & Status handles)\r\n");
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
        OutputDebugString( "HeapCreate (write request heap)\r\n" );

    ghWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if ( ghWriterEvent == NULL )
        OutputDebugString("CreateEvent(writ request event)\r\n");

    ghTransferCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if ( ghTransferCompleteEvent == NULL )
        OutputDebugString("CreateEvent(transfer complete event)\r\n");

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
			OutputDebugString("WaitForMultipleObjects( writer proc )\r\n");
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
							OutputDebugString("HeapFree(file transfer buffer)\r\n");
						break;

					case WRITE_FILEEND:
						if ( !SetEvent(ghTransferCompleteEvent) )
							OutputDebugString("SetEvent (transfer complete event)\r\n");
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
								OutputDebugString("SetEvent (transfer complete event)\r\n");
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
        OutputDebugString("CreateEvent (Thread exit event)\r\n");        

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

	LPFNCALLBACK( m_SerialData.TTYInfo )	= pSerialData->lpfnReception;

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
        OutputDebugString( "CreateFile\r\n" );
		return NULL;
    }

	if ( !GetCommTimeouts( COMDEV(m_SerialData.TTYInfo), &(TIMEOUTSORIG(m_SerialData.TTYInfo))) )
        OutputDebugString( "GetCommTimeouts\r\n" );

	UpdateConnection();

    SetupComm( COMDEV(m_SerialData.TTYInfo), MAX_READ_BUFFER, MAX_WRITE_BUFFER );
    if ( !EscapeCommFunction(COMDEV(m_SerialData.TTYInfo), SETDTR) )
        OutputDebugString( "EscapeCommFunction (SETDTR)\r\n" );

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
        OutputDebugString("CreateThread(Reader)\r\n");

	WRITERTHREAD(m_SerialData.TTYInfo) = ::CreateThread( NULL, 
                          0, 
                          (LPTHREAD_START_ROUTINE) WriterProc, 
                          (LPVOID)&theApp, 
                          0, 
                          &dwWriterId );
                   
    if ( WRITERTHREAD(m_SerialData.TTYInfo) == NULL )
        OutputDebugString("CreateThread (Writer)\r\n");

    return;
}

BOOL CSerialCommAppApp::UpdateConnection(void)
{
    DCB dcb = { NULL };
    
    dcb.DCBlength = sizeof(dcb);

    if ( !GetCommState(COMDEV(m_SerialData.TTYInfo), &dcb) )
		OutputDebugString( "GetCommState\r\n" );

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
		OutputDebugString( "SetCommState\r\n" );

    if ( !SetCommTimeouts(COMDEV(m_SerialData.TTYInfo), &(TIMEOUTSNEW(m_SerialData.TTYInfo))) )
		OutputDebugString( "SetCommTimeouts\r\n" );

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
		OutputDebugString( "BreakDownCommPort:DTR信号がOFFにできませんでした\r\n" );

    if ( !SetCommTimeouts(COMDEV(m_SerialData.TTYInfo),  &(TIMEOUTSORIG(m_SerialData.TTYInfo))) )
		OutputDebugString( "BreakDownCommPort:タイムアウトが発生\r\n" );

    if ( !PurgeComm(COMDEV(m_SerialData.TTYInfo), PURGE_FLAGS) )
		OutputDebugString( "BreakDownCommPort:バッファクリアでエラーが発生\r\n" );

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
                OutputDebugString("リーダー/ステータススレッドが終了しませんでした。\n\r");

            if (WaitForSingleObject(WRITERTHREAD(m_SerialData.TTYInfo), 0) == WAIT_TIMEOUT)
                OutputDebugString("ライタースレッドが終了しませんでした。\n\r");

            break;

        default:
            break;
    }

    ResetEvent( m_hThreadExitEvent );

    return dwRes;
}

void CSerialCommAppApp::WriterGeneric(char* lpBuf, DWORD dwToWrite)
{
    OVERLAPPED osWrite = {0};
    HANDLE hArray[2];
    DWORD dwWritten;
    DWORD dwRes;

    if ( NOWRITING(m_SerialData.TTYInfo) )
        return ;

    osWrite.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if ( osWrite.hEvent == NULL )
        OutputDebugString("CreateEvent (overlapped write hEvent)\r\n");

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
				OutputDebugString("WaitForMultipleObjects (WriterGeneric)\r\n");
				break;

			case WAIT_OBJECT_0:
				SetLastError( ERROR_SUCCESS );
				if ( !GetOverlappedResult(COMDEV(m_SerialData.TTYInfo), &osWrite, &dwWritten, FALSE) )
				{
					if ( GetLastError() == ERROR_OPERATION_ABORTED )
						OutputDebugString("書き込みが中止されました\r\n");
					else
						OutputDebugString("GetOverlappedResult(in Writer)");

					if ( dwWritten != dwToWrite )
					{
						if ( (GetLastError() == ERROR_SUCCESS) && SHOWTIMEOUTS(m_SerialData.TTYInfo) )
							OutputDebugString("書き込みがタイムアウトしました。(overlapped)\r\n");
						else
							OutputDebugString("ポートへのデータの書き込み中にエラーが発生しました。(overlapped)");
					}
				}
				break;

			case WAIT_OBJECT_0 + 1:
				break;

			case WAIT_TIMEOUT:
				OutputDebugString("WriterGenericでタイムアウトを待ちます。\r\n");
				break;

			case WAIT_FAILED:
				break;
			}
		}
		else
		{
			OutputDebugString("WriteFile (in Writer)\r\n");
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
        OutputDebugString("HeapAlloc (writer packet)\r\n");
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
        OutputDebugString("SetEvent( writer packet )\r\n");
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
            OutputDebugString("HeapAlloc (writer packet)\r\n");
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
            OutputDebugString("HeapAlloc (writer packet)");
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
        OutputDebugString("SetEvent( writer packet )\r\n");
}


SERIALCOMM_API HANDLE WINAPI serialOpenComm( BOOL TTYCommMode, SERIALDATA* pSerialData )
{
	if ( pSerialData == NULL )
		return NULL;
	if ( pSerialData->lpfnReception == NULL )
		return NULL;

	if ( TTYCommMode == TTY_COMM_INIT )
	{
		theApp.InitTTYInfo( pSerialData );
		return &theApp;
	}

	theApp.m_SerialData = *(pSerialData);

	theApp.SetupCommPort();

	return &theApp;
}

SERIALCOMM_API void WINAPI serialCloseComm( HANDLE hSerial )
{
	if ( hSerial == NULL )
		return;

	CSerialCommAppApp* pApp = (CSerialCommAppApp*)hSerial;
	if ( pApp != &theApp )
		return;

	pApp->BreakDownCommPort();
}


