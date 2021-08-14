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

HANDLE ghWriterHeap = NULL;
HANDLE ghWriterEvent = NULL;
HANDLE ghTransferCompleteEvent = NULL;

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

COMMTIMEOUTS gTimeoutsDefault = { 0x01, 0, 0, 0, 0 };
DWORD  gdwReceiveState = NULL;

// CSerialCommAppApp 初期化

BOOL CSerialCommAppApp::InitInstance()
{
	CWinApp::InitInstance();

    InitializeCriticalSection( &gStatusCritical );

	return TRUE;
}

int CSerialCommAppApp::ExitInstance()
{
    DeleteCriticalSection( &gStatusCritical );

	return CWinApp::ExitInstance();
}

BOOL CSerialCommAppApp::InitTTYInfo( T_SERIAL_DATA* pSerialData )
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


SERIALCOMM_API HANDLE WINAPI serialOpenComm( BOOL TTYCommMode, T_SERIAL_DATA* pSerialData )
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


