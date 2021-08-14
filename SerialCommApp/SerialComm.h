
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

//----------------------------------------------------------------------------
// 通信パラメータ
//----------------------------------------------------------------------------
typedef struct _SERIAL_DATA
{
} T_SERIAL_DATA;

//
// data 構造体
//
struct TTYInfoStruct
{
    HANDLE  hCommPort, hReaderStatus, hWriter ;
    DWORD   dwEventFlags;
    CHAR    Screen[MAXCOLS * MAXROWS];
    CHAR    chFlag, chXON, chXOFF;
    WORD    wXONLimit, wXOFFLimit;
    DWORD   fRtsControl;
    DWORD   fDtrControl;
    BOOL    fConnected, fTransferring, fRepeating,
            fLocalEcho, fNewLine,
            fDisplayErrors, fAutowrap,
            fCTSOutFlow, fDSROutFlow, fDSRInFlow, 
            fXonXoffOutFlow, fXonXoffInFlow,
            fTXafterXoffSent,
            fNoReading, fNoWriting, fNoEvents, fNoStatus,
            fDisplayTimeouts;
    BYTE    bPort, bByteSize, bParity, bStopBits ;
    DWORD   dwBaudRate ;
    WORD    wCursorState ;
    HFONT   hTTYFont ;
    LOGFONT lfTTYFont ;
    DWORD   rgbFGColor ;
    COMMTIMEOUTS timeoutsorig;
    COMMTIMEOUTS timeoutsnew;
    int     xSize, ySize, xScroll, yScroll, xOffset, yOffset,
            nColumn, nRow, xChar, yChar , nCharPos;

} TTYInfo;


#ifdef __cplusplus
extern "C" {
#endif

SERIALCOMM_API HANDLE WINAPI serialOpenComm( T_SERIAL_DATA* pCryptoData );
SERIALCOMM_API void WINAPI serialCloseComm( HANDLE hSerial );

SERIALCOMM_API bool WINAPI serialWriteComm( HANDLE hSerial, string strData );
SERIALCOMM_API bool WINAPI serialReadComm( HANDLE hSerial, string& strData );

#ifdef __cplusplus
}
#endif

#endif // !defined(_SERIALCOMM_H_INCLUDE_)