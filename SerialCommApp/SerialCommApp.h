// SerialCommApp.h : SerialCommApp.DLL �̃��C�� �w�b�_�[ �t�@�C��
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH �ɑ΂��Ă��̃t�@�C�����C���N���[�h����O�� 'stdafx.h' ���C���N���[�h���Ă�������"
#endif

#include "resource.h"		// ���C�� �V���{��
#include "SerialComm.h"

DWORD WINAPI ReaderProc( LPVOID lpVoid );
DWORD WINAPI WriterProc( LPVOID lpVoid );

// CSerialCommAppApp
// ���̃N���X�̎����Ɋւ��Ă� SerialCommApp.cpp ���Q�Ƃ��Ă��������B
//

class CSerialCommAppApp : public CWinApp
{
public:
	CSerialCommAppApp();

// �I�[�o�[���C�h
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
