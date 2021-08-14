// SerialCommApp.h : SerialCommApp.DLL のメイン ヘッダー ファイル
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


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

	DECLARE_MESSAGE_MAP()
};
