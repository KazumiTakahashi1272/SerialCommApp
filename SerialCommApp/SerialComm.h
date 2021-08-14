
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

//----------------------------------------------------------------------------
//	à√çÜâªÉpÉâÉÅÅ[É^
//----------------------------------------------------------------------------
typedef struct _SERIAL_DATA
{
} T_SERIAL_DATA;


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