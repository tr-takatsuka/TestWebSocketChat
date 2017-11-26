
// TestWebSocketChatGuiDlg.h : ヘッダー ファイル
//

#pragma once

#include "ChatServer.h"

// CTestWebSocketChatGuiDlg ダイアログ
class CTestWebSocketChatGuiDlg : public CDialogEx
{
	boost::asio::io_service			m_ioService;
	std::unique_ptr<CChatServer>	m_upChatServer;

// コンストラクション
public:
	CTestWebSocketChatGuiDlg(CWnd* pParent = NULL);	// 標準コンストラクター

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TESTWEBSOCKETCHATGUI_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート


// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedButtonRun();
public:
	CString m_sPortNo;
};
