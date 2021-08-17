
#if !defined(_SERIALCOMM_H_INCLUDE_)
#define _SERIALCOMM_H_INCLUDE_

#include "windows.h"
#include <string>
#include <vector>
using namespace std;

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef SERIALCOMM_EXPORTS
#define SERIALCOMM_API __declspec(dllexport)
#else
#define SERIALCOMM_API __declspec(dllimport)
#pragma comment(lib, "SerialComm.lib")
#pragma message("Automatically linking with SerialComm.dll")
#endif

//
//
//
typedef void (CALLBACK* LPFNRECEPTION)(char* lpData, DWORD dwBufLen);

//
//
//
#define TTY_COMM_INIT	0	// TTY通信構造体の初期化
#define TTY_COMM_SET	1	// TTY通信構造体の設定

//
// hard coded maximum number of ports
//
#define MAXPORTS        10

//
// terminal size
//
#define MAXROWS         50
#define MAXCOLS         80

//
// cursor states
//
#define CS_HIDE         0x00
#define CS_SHOW         0x01

//
// ascii definitions
//
#define ASCII_BEL       0x07
#define ASCII_BS        0x08
#define ASCII_LF        0x0A
#define ASCII_CR        0x0D
#define ASCII_XON       0x11
#define ASCII_XOFF      0x13

//
// GLOBAL DEFINES
//
#define TTY_BUFFER_SIZE         MAXROWS * MAXCOLS
#define MAX_STATUS_BUFFER       20000
#define MAX_WRITE_BUFFER        1024
#define MAX_READ_BUFFER         2048
#define READ_TIMEOUT            500
#define STATUS_CHECK_TIMEOUT    500
#define WRITE_CHECK_TIMEOUT     500
#define PURGE_FLAGS             PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR 
#define EVENTFLAGS_DEFAULT      EV_BREAK | EV_CTS | EV_DSR | EV_ERR | EV_RING | EV_RLSD
#define FLAGCHAR_DEFAULT        '\n'

//
// Write request types
//
#define WRITE_CHAR          0x01
#define WRITE_FILE          0x02
#define WRITE_FILESTART     0x03
#define WRITE_FILEEND       0x04
#define WRITE_ABORT         0x05
#define WRITE_BLOCK         0x06

//
// Read states
//
#define RECEIVE_TTY         0x01
#define RECEIVE_CAPTURED    0x02

//
// window coords
//
#define MAXXWINDOW          820//750
#define MAXYWINDOW          530
#define STARTXWINDOW        80
#define STARTYWINDOW        70

#define SETTINGSFACTOR      5
#define STATUSFACTOR        5

#define MAX_WRITE_BUFFER	1024

#pragma pack(push)
#pragma pack(1)

//----------------------------------------------------------------------------
// 通信 data 構造体
//----------------------------------------------------------------------------
typedef struct _TTYInfoStruct
{
    HANDLE  hCommPort;
	HANDLE	hReaderStatus;
	HANDLE	hWriter;
    DWORD   dwEventFlags;
    //CHAR    Screen[MAXCOLS * MAXROWS];
    CHAR    chFlag;
	CHAR	chXON;
	CHAR	chXOFF;
    WORD    wXONLimit;
	WORD	wXOFFLimit;
    DWORD   fRtsControl;
    DWORD   fDtrControl;
    BOOL    fOutxCtsFlow;
	BOOL	fOutxDsrFlow;
	BOOL	fDsrSensitivity;
	BOOL	fOutX;
	BOOL	fInX;
	BOOL	fTXContinueOnXoff;
	BOOL	fConnected;
	BOOL	fTransferring;
	BOOL	fRepeating;
	BOOL	fLocalEcho;
	BOOL	fNewLine;
    BOOL    fDisplayErrors;
	BOOL	fAutowrap;
    BOOL    fCTSOutFlow;
	BOOL	fDSROutFlow;
	BOOL	fDSRInFlow; 
    BOOL    fXonXoffOutFlow;
	BOOL	fXonXoffInFlow;
    BOOL    fTXafterXoffSent;
    BOOL    fNoReading;
	BOOL	fNoWriting;
	BOOL	fNoEvents;
	BOOL	fNoStatus;
    BOOL    fDisplayTimeouts;
    BYTE    bPort;
	BYTE	bByteSize;
	BYTE	bParity;
	BYTE	bStopBits;
    DWORD   dwBaudRate ;
    WORD    wCursorState ;
    //HFONT   hTTYFont ;
    //LOGFONT lfTTYFont ;
    //DWORD   rgbFGColor ;
    COMMTIMEOUTS timeoutsorig;
    COMMTIMEOUTS timeoutsnew;
    int     xSize;
	int		ySize;
	int		xScroll;
	int		yScroll;
	int		xOffset;
	int		yOffset;
    int		nColumn;
	int		nRow;
	int		xChar;
	int		yChar;
	int		nCharPos;

	LPVOID	lpfnCallBack;

} TTYInfoStruct;

//----------------------------------------------------------------------------
// 通信 macros
//----------------------------------------------------------------------------
#define COMDEV( x )         (x.hCommPort)
#define CURSORSTATE( x )    (x.wCursorState)
#define PORT( x )           (x.bPort)
#define SCREEN( x )         (x.Screen)
#define CONNECTED( x )      (x.fConnected)
#define TRANSFERRING( x )   (x.fTransferring)
#define REPEATING( x )      (x.fRepeating)
#define LOCALECHO( x )      (x.fLocalEcho)
#define NEWLINE( x )        (x.fNewLine)
#define AUTOWRAP( x )       (x.fAutowrap)
#define BYTESIZE( x )       (x.bByteSize)
#define PARITY( x )         (x.bParity)
#define STOPBITS( x )       (x.bStopBits)
#define BAUDRATE( x )       (x.dwBaudRate)
#define HTTYFONT( x )       (x.hTTYFont)
#define LFTTYFONT( x )      (x.lfTTYFont)
#define FGCOLOR( x )        (x.rgbFGColor)
#define XSIZE( x )          (x.xSize)
#define YSIZE( x )          (x.ySize)
#define XSCROLL( x )        (x.xScroll)
#define YSCROLL( x )        (x.yScroll)
#define XOFFSET( x )        (x.xOffset)
#define YOFFSET( x )        (x.yOffset)
#define COLUMN( x )         (x.nColumn)
#define ROW( x )            (x.nRow)
#define XCHAR( x )          (x.xChar)
#define YCHAR( x )          (x.yChar)
#define DISPLAYERRORS( x )  (x.fDisplayErrors)
#define TIMEOUTSORIG( x )   (x.timeoutsorig)
#define TIMEOUTSNEW( x )    (x.timeoutsnew)
#define WRITERTHREAD( x )   (x.hWriter)
#define READSTATTHREAD( x ) (x.hReaderStatus)
#define EVENTFLAGS( x )     (x.dwEventFlags)
#define FLAGCHAR( x )       (x.chFlag)
#define SCREENCHAR( x, col, row )   (x.Screen[row * MAXCOLS + col])

#define LPFNCALLBACK( x )	(x.lpfnCallBack)

#define DTRCONTROL( x )     (x.fDtrControl)
#define RTSCONTROL( x )     (x.fRtsControl)
#define XONCHAR( x )        (x.chXON)
#define XOFFCHAR( x )       (x.chXOFF)
#define XONLIMIT( x )       (x.wXONLimit)
#define XOFFLIMIT( x )      (x.wXOFFLimit)
#define CTSOUTFLOW( x )     (x.fCTSOutFlow)
#define DSROUTFLOW( x )     (x.fDSROutFlow)
#define DSRINFLOW( x )      (x.fDSRInFlow)
#define XONXOFFOUTFLOW( x ) (x.fXonXoffOutFlow)
#define XONXOFFINFLOW( x )  (x.fXonXoffInFlow)
#define TXAFTERXOFFSENT(x)  (x.fTXafterXoffSent)

#define NOREADING( x )      (x.fNoReading)
#define NOWRITING( x )      (x.fNoWriting)
#define NOEVENTS( x )       (x.fNoEvents)
#define NOSTATUS( x )       (x.fNoStatus)
#define SHOWTIMEOUTS( x )   (x.fDisplayTimeouts)

typedef struct WRITEREQUEST
{
	DWORD      dwWriteType;        // char, file start, file abort, file packet
	DWORD      dwSize;             // size of buffer
	char       ch;                 // ch to send
	char *     lpBuf;              // address of buffer to send
	HANDLE     hHeap;              // heap containing buffer
	struct WRITEREQUEST *pNext;    // next node in the list
	struct WRITEREQUEST *pPrev;    // prev node in the list
} WRITEREQUEST, *PWRITEREQUEST;

struct WRITEREQUEST *gpWriterHead;
struct WRITEREQUEST *gpWriterTail;

//----------------------------------------------------------------------------
// 通信パラメータ
//----------------------------------------------------------------------------
typedef struct _SERIAL_DATA
{
	TTYInfoStruct	TTYInfo;
	LPFNRECEPTION	lpfnCallBack;
	WRITEREQUEST	WriteData;
} SERIALDATA, *LPSERIALDATA;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

SERIALCOMM_API HANDLE WINAPI serialOpenComm( BOOL TTYCommMode, LPSERIALDATA pSerialData, LPFNRECEPTION lpfnReception );
SERIALCOMM_API void WINAPI serialCloseComm( HANDLE hSerial );
SERIALCOMM_API void WINAPI serialBreakDownComm( HANDLE hSerial );

SERIALCOMM_API bool WINAPI serialWriteComm( HANDLE hSerial, string strData, DWORD dwDataSize );
//SERIALCOMM_API bool WINAPI serialReadComm( HANDLE hSerial, string& strData );

#ifdef __cplusplus
}
#endif

#endif // !defined(_SERIALCOMM_H_INCLUDE_)