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
	virtual int ExitInstance();
	void WriterGeneric(char* lpBuf, DWORD dwToWrite);
	PWRITEREQUEST RemoveFromLinkedList(PWRITEREQUEST pNode);
	BOOL WriterAddNewNode(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap);
	void AddToLinkedList(PWRITEREQUEST pNode);
	BOOL WriterAddFirstNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout);
	BOOL WriterAddNewNodeTimeout(DWORD dwRequestType, DWORD dwSize, char ch, char* lpBuf, HANDLE hHeap, DWORD dwTimeout);
	void AddToFrontOfLinkedList(PWRITEREQUEST pNode);
};
