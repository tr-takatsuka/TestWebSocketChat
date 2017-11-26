
// TestWebSocketChatGuiDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "TestWebSocketChatGui.h"
#include "TestWebSocketChatGuiDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTestWebSocketChatGuiDlg ダイアログ

CTestWebSocketChatGuiDlg::CTestWebSocketChatGuiDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_TESTWEBSOCKETCHATGUI_DIALOG, pParent)
	, m_sPortNo(_T("11111"))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTestWebSocketChatGuiDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PORTNO, m_sPortNo);
}

BEGIN_MESSAGE_MAP(CTestWebSocketChatGuiDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_RUN, &CTestWebSocketChatGuiDlg::OnBnClickedButtonRun)
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CTestWebSocketChatGuiDlg メッセージ ハンドラー

BOOL CTestWebSocketChatGuiDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	// TODO: 初期化をここに追加します。
	SetTimer(1, 1, nullptr );

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。

void CTestWebSocketChatGuiDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 描画のデバイス コンテキスト

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// クライアントの四角形領域内の中央
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// アイコンの描画
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CTestWebSocketChatGuiDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestWebSocketChatGuiDlg::OnDestroy() 
{
	__super::OnDestroy();

	// TODO: この位置にメッセージ ハンドラ用のコードを追加してください
	m_upChatServer.reset();

}

void CTestWebSocketChatGuiDlg::OnTimer(UINT_PTR nIDEvent)
{
	try{
		m_ioService.poll();
	}catch(std::exception& e){
		std::cerr << "Exception: " << e.what() << "\n";
	}
}


void CTestWebSocketChatGuiDlg::OnBnClickedButtonRun()
{
	if( m_upChatServer ){
		m_upChatServer.reset();
	}else{
		uint16_t nPortNo = static_cast<uint16_t>(std::stoi(std::wstring(m_sPortNo)));
		boost::asio::ip::tcp::endpoint ep( boost::asio::ip::tcp::v4(), nPortNo );
		m_upChatServer = std::make_unique<CChatServer>(m_ioService,ep);
	}
}
