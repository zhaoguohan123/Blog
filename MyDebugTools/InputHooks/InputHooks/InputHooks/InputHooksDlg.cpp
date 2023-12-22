
// InputHooksDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "InputHooks.h"
#include "InputHooksDlg.h"
#include "afxdialogex.h"
#include "logger.h"
#include <map>
#include "EncodingConversion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

HHOOK g_user_hook;

KBDLLHOOKSTRUCT kbdStruct;

KeyCapturedCallback g_KeyCapturedCallback = nullptr;

void* g_pObject = nullptr;

void* CInputHooksDlg::m_pObject = nullptr;

std::map<int, std::string> keyname{
{VK_BACK, "[BACKSPACE]" },
{VK_RETURN,	"[ENTER]" },
{VK_SPACE,	"[SPACE]" },
{VK_TAB,	"[TAB]" },
{VK_SHIFT,	"[SHIFT]" },
{VK_LSHIFT,	"[LSHIFT]" },
{VK_RSHIFT,	"[RSHIFT]" },
{VK_CONTROL,	"[CONTROL]" },
{VK_LCONTROL,	"[LCONTROL]" },
{VK_RCONTROL,	"[RCONTROL]" },
{VK_LMENU,	"[LALT]" },
{VK_RMENU,	"[RALT]" },
{VK_LWIN,	"[LWIN]" },
{VK_RWIN,	"[RWIN]" },
{VK_ESCAPE,	"[ESCAPE]" },
{VK_END,	"[END]" },
{VK_HOME,	"[HOME]" },
{VK_LEFT,	"[LEFT]" },
{VK_RIGHT,	"[RIGHT]" },
{VK_UP,		"[UP]" },
{VK_DOWN,	"[DOWN]" },
{VK_PRIOR,	"[PG_UP]" },
{VK_NEXT,	"[PG_DOWN]" },
{VK_OEM_PERIOD,	"." },
{VK_DECIMAL,	"." },
{VK_OEM_PLUS,	"+" },
{VK_OEM_MINUS,	"-" },
{VK_ADD,		"+" },
{VK_SUBTRACT,	"-" },
{VK_CAPITAL,	"[CAPSLOCK]" },
};

void Record(int key_stroke, WPARAM wParam)
{
	std::wstring result;

	static TCHAR lastwindows[MAX_PATH] = { 0 };

	//鼠标点击键值为1和2
	if (key_stroke == 1 || key_stroke == 2)
	{
		return;
	}

	HWND foreground = GetForegroundWindow();
	DWORD threadID;
	HKL layout = NULL;

	if (foreground)
	{
		threadID = GetWindowThreadProcessId(foreground, NULL);
		layout = GetKeyboardLayout(threadID);
	}

	TCHAR window_title[MAX_PATH] = { 0 };
	if (foreground)
	{
		GetWindowTextW(foreground, window_title, MAX_PATH);
	}

	
	std::wstring wnd = window_title;

	if (wnd.size()<=0)
	{
		wnd = L"windows";
	}

	std::string key_record = "";

	if (keyname.find(key_stroke) != keyname.end())
	{
		key_record += keyname.at(key_stroke);
	}
	else
	{
		char key;
		bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);

		if ((GetKeyState(VK_SHIFT) & 0x1000) != 0 || (GetKeyState(VK_LSHIFT) & 0x1000) != 0
			|| (GetKeyState(VK_RSHIFT) & 0x1000) != 0)
		{
			lowercase = !lowercase;
		}

		key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);

		if (!lowercase)
		{
			key = tolower(key);
		}
		key_record += char(key);
	}

	std::wstring key_opt;

	if (wParam == WM_KEYDOWN)
	{
		key_opt = L"Key Down";
	}

	if (wParam == WM_KEYUP)
	{
		key_opt = L"Key Up";
	}

	LOGGER_INFO("{}: {} :{} key_stroke: {}", UTF16ToUTF8(wnd), UTF16ToUTF8(key_opt),  key_record, key_stroke);

	if (g_KeyCapturedCallback == nullptr)
	{
		LOGGER_ERROR("MyKeyCapturedCallback is nullptr!");
		return;
	}

	if (g_pObject == nullptr)
	{
		LOGGER_ERROR("g_pObject is nullptr!");
		return ;
	}
	
	g_KeyCapturedCallback(key_opt, ANSIToUTF16(key_record), g_pObject);
}

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);

		// 输出键盘扫描码
		Record(kbdStruct.vkCode, wParam);
	}

	return CallNextHookEx(g_user_hook, nCode, wParam, lParam);
}


void MyKeyCapturedCallback(const std::wstring & col1, const std::wstring& col2, void* pObject) {
	// 类型转换为类的指针并调用成员函数
	CInputHooksDlg* pobj = static_cast<CInputHooksDlg*>(pObject);
	if (pobj != nullptr) {
		pobj->m_list_user_kb_ctrl.InsertItem(0, col1.c_str());
		pobj->m_list_user_kb_ctrl.SetItemText(0, 1, col2.c_str());
	}
}



class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


CInputHooksDlg::CInputHooksDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_INPUTHOOKS_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CInputHooksDlg::InstallUserHook()
{
	if (!(g_user_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0)))
	{
		LOGGER_ERROR("SetWindowsHookEx  WH_KEYBOARD_LL failed! err:{}", GetLastError());
		return;
	}
}

void CInputHooksDlg::UnInstallUserHook()
{
	if (g_user_hook)
	{
		UnhookWindowsHookEx(g_user_hook);
		g_user_hook = NULL;
	}
}

void CInputHooksDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list_user_kb_ctrl);
	DDX_Control(pDX, ID_OPEN_HOOK, set_hook_btn_);
	DDX_Control(pDX, ID_CLOSE_HOOK, un_set_hook_btn_);
}

BEGIN_MESSAGE_MAP(CInputHooksDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_OPEN_HOOK, &CInputHooksDlg::OnBnClickedOpenHook)
	ON_BN_CLICKED(ID_CLOSE_HOOK, &CInputHooksDlg::OnBnClickedCloseHook)
	ON_BN_CLICKED(IDC_BUTTON_CLEAN, &CInputHooksDlg::OnBnClickedButtonClean)
END_MESSAGE_MAP()


// CInputHooksDlg 消息处理程序

BOOL CInputHooksDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	un_set_hook_btn_.EnableWindow(FALSE);
	/// R3键盘HOOK
	m_list_user_kb_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list_user_kb_ctrl.SetView(LV_VIEW_DETAILS); // 设置为详细视图模式
	m_list_user_kb_ctrl.InsertColumn(0, _T(""), LVCFMT_LEFT, 100);
	m_list_user_kb_ctrl.InsertColumn(1, _T(""), LVCFMT_LEFT, 100);

	SetKeyCapRecordCallBack(MyKeyCapturedCallback);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CInputHooksDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CInputHooksDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CInputHooksDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CInputHooksDlg::OnBnClickedOpenHook()
{
	(void)InstallUserHook();

	un_set_hook_btn_.EnableWindow(TRUE);
	set_hook_btn_.EnableWindow(FALSE);
}


void CInputHooksDlg::OnBnClickedCloseHook()
{
	(void)UnInstallUserHook();

	set_hook_btn_.EnableWindow(TRUE);
	un_set_hook_btn_.EnableWindow(FALSE);
}

void CInputHooksDlg::SetKeyCapRecordCallBack(KeyCapturedCallback callback)
{
	m_pObject = this;
	g_pObject = m_pObject;
	g_KeyCapturedCallback = callback;
}


void CInputHooksDlg::OnBnClickedButtonClean()
{
	// TODO: 在此添加控件通知处理程序代码
	m_list_user_kb_ctrl.DeleteAllItems();
}
