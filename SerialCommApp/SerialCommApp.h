// SerialCommApp.h : SerialCommApp.DLL �̃��C�� �w�b�_�[ �t�@�C��
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH �ɑ΂��Ă��̃t�@�C�����C���N���[�h����O�� 'stdafx.h' ���C���N���[�h���Ă�������"
#endif

#include "resource.h"		// ���C�� �V���{��


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

	DECLARE_MESSAGE_MAP()
};
