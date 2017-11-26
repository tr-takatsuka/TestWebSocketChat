
// TestWebSocketChatGui.h : PROJECT_NAME アプリケーションのメイン ヘッダー ファイルです。
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


// CTestWebSocketChatGuiApp:
// このクラスの実装については、TestWebSocketChatGui.cpp を参照してください。
//

class CTestWebSocketChatGuiApp : public CWinApp
{
public:
	CTestWebSocketChatGuiApp();

// オーバーライド
public:
	virtual BOOL InitInstance();

// 実装

	DECLARE_MESSAGE_MAP()
};

extern CTestWebSocketChatGuiApp theApp;