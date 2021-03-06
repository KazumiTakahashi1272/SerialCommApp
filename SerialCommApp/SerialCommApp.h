// SerialCommApp.h : SerialCommApp.DLL のメイン ヘッダー ファイル
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル
#include "SerialComm.h"

DWORD WINAPI ReaderProc( LPVOID lpVoid );
DWORD WINAPI WriterProc( LPVOID lpVoid );

void ErrorReporter(void);

// CSerialCommAppApp
// このクラスの実装に関しては SerialCommApp.cpp を参照してください。
//

class CSerialCommAppApp : public CWinApp
{
public:
	CSerialCommAppApp();

// オーバーライド
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	BOOL InitTTYInfo( SERIALDATA* pSerialData );

	DECLARE_MESSAGE_MAP()

public:
	SERIALDATA m_SerialData;
	HANDLE m_hThreadExitEvent;

	BOOL BreakDownCommPort(void);
	DWORD WaitForThreads(DWORD dwTimeout);
	HANDLE SetupCommPort(void);
	BOOL UpdateConnection(void);
	void StartThreads(void);
	void WriterGeneric(char* lpBuf, DWORD dwToWrite);
	PWRITEREQUEST RemoveFromLinkedList(PWRITEREQUEST pNode);
	BOOL WriterAddNewNode(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap);
	void AddToLinkedList(PWRITEREQUEST pNode);
	BOOL WriterAddFirstNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout);
	BOOL WriterAddNewNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout);
	void AddToFrontOfLinkedList(PWRITEREQUEST pNode);
	void TransferTextStart(PWRITEREQUEST pWriteComm);
	void ReportCommStatus(void);
	void ReportModemStatus(DWORD dwStatus);
	void CheckModemStatus(bool bUpdateNow);
	void CheckComStat(bool bUpdateNow);
};
