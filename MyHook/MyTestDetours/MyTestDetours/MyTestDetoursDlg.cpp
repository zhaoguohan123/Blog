
// MyTestDetoursDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MyTestDetours.h"
#include "MyTestDetoursDlg.h"
#include "afxdialogex.h"
#include "..\..\..\Common\x86\detours.h"


#pragma comment(lib, "..\\..\\..\\Common\\x86\\detours.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#pragma warning(disable:4996)
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
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


// CMyTestDetoursDlg dialog



CMyTestDetoursDlg::CMyTestDetoursDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_MYTESTDETOURS_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMyTestDetoursDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMyTestDetoursDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CMyTestDetoursDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CMyTestDetoursDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CMyTestDetoursDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON4, &CMyTestDetoursDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_BUTTON5, &CMyTestDetoursDlg::OnBnClickedButton5)
END_MESSAGE_MAP()


// CMyTestDetoursDlg message handlers

BOOL CMyTestDetoursDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
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

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMyTestDetoursDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMyTestDetoursDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMyTestDetoursDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void DebugPrint(const char* format, ...)
{
	if (format == NULL)
	{
		return;
	}
	char line[1024 * 4] = { 0 };
	int len = _snprintf(line, sizeof(line) - 1, "[ZGHPRINTTEST]");

	va_list ap;
	va_start(ap, format);
	int len2 = _vsnprintf(line + len, sizeof(line) - len - 1, format, ap);
	va_end(ap);
	_snprintf(line + len + len2, sizeof(len) - len - len2 - 1, "\n");
	OutputDebugStringA(line);
}

//原函数类型定义
typedef int (WINAPI* MsgBoxW)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
MsgBoxW OldMsgBoxW = NULL;			//指向原函数的指针
FARPROC pfOldMsgBoxW;				//指向函数的远指针
BYTE OldCode[5];					//原系统API入口代码
BYTE NewCode[5];					//原系统API新的入口代码 (jmp xxxxxxxx)
HANDLE hProcess = NULL;				//本程序进程句柄
void HookOn();
void HookOff();

int WINAPI MyMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	TRACE(lpText);
	int nRet = 0;
	HookOff();//调用原函数之前，记得先恢复HOOK呀，不然是调用不到的
			  //如果不恢复HOOK，就调用原函数，会造成死循环
			  //毕竟调用的还是我们的函数，从而造成堆栈溢出，程序崩溃。
	DebugPrint("调用我的函数");
	nRet = ::MessageBoxW(hWnd, _T("哈哈，MessageBoxW被HOOK了"), lpCaption, uType);


	HookOn();//调用完原函数后，记得继续开启HOOK，不然下次会HOOK不到。 

	return nRet;

}
//开启钩子的函数
void HookOn() {
	ASSERT(hProcess != NULL);

	DWORD dwTemp = 0;
	DWORD dwOldProtect;

	//修改API函数入口前5个字节为jmp xxxxxx
	if (!VirtualProtectEx(hProcess, pfOldMsgBoxW, 5, PAGE_READWRITE, &dwOldProtect))
	{
		DebugPrint(" HookOff VirtualProtectEx errcode = %d", GetLastError());
	}

	if (!WriteProcessMemory(hProcess, pfOldMsgBoxW, NewCode, 5, 0))
	{
		DebugPrint(" HookOff WriteProcessMemory errcode = %d", GetLastError());
	}

	if (!VirtualProtectEx(hProcess, pfOldMsgBoxW, 5, dwOldProtect, &dwTemp))
	{
		DebugPrint(" HookOff VirtualProtectEx2 errcode = %d", GetLastError());
	}


}

//关闭钩子的函数
void HookOff()
{
	ASSERT(hProcess != NULL);

	DWORD dwTemp = 0;
	DWORD dwOldProtect;

	//恢复API函数入口前5个字节
	if (!VirtualProtectEx(hProcess, pfOldMsgBoxW, 5, PAGE_READWRITE, &dwOldProtect))
	{
		DebugPrint(" HookOff VirtualProtectEx errcode = %d",GetLastError());
	}
	
	if (!WriteProcessMemory(hProcess, pfOldMsgBoxW, OldCode, 5, 0))
	{
		DebugPrint(" HookOff WriteProcessMemory errcode = %d", GetLastError());
	}
	 
	if (!VirtualProtectEx(hProcess, pfOldMsgBoxW, 5, dwOldProtect, &dwTemp))
	{
		DebugPrint(" HookOff VirtualProtectEx2 errcode = %d", GetLastError());
	}
	
}

//获取API函数入口前5个字节
//旧入口前5个字节保存在前面定义的字节数组BYTE OldCode[5]
//新入口前5个字节保存在前面定义的字节数组BYTE NewCode[5]
void GetApiEntrance()
{

	//获取原API入口地址
	HMODULE hmod = ::LoadLibrary(_T("User32.dll"));
	OldMsgBoxW = (MsgBoxW)::GetProcAddress(hmod, "MessageBoxW");
	pfOldMsgBoxW = (FARPROC)OldMsgBoxW;

	if (pfOldMsgBoxW == NULL)
	{
		DebugPrint("GetProcAddress faield, errcode = %d",GetLastError());
		return;
	}

	// 将原API的入口前5个字节代码保存到OldCode[]
	_asm
	{
		lea edi, OldCode		//获取OldCode数组的地址,放到edi
		mov esi, pfOldMsgBoxW //获取原API入口地址，放到esi
		cld	  //方向标志位，为以下两条指令做准备
		movsd //复制原API入口前4个字节到OldCode数组 原函数指针
		movsb //复制原API入口第5个字节到OldCode数组
	}


	NewCode[0] = 0xe9;//实际上0xe9就相当于jmp指令

					  //获取MyMessageBoxW的相对地址,为Jmp做准备
					  //int nAddr= UserFunAddr – SysFunAddr - （我们定制的这条指令的大小）;
					  //Jmp nAddr;
					  //（我们定制的这条指令的大小）, 这里是5，5个字节嘛
	_asm
	{
		lea eax, MyMessageBoxW //获取我们的MyMessageBoxW函数地址
		mov ebx, pfOldMsgBoxW  //原系统API函数地址
		sub eax, ebx			 //int nAddr= UserFunAddr – SysFunAddr
		sub eax, 5			 //nAddr=nAddr-5
		mov dword ptr[NewCode + 1], eax //将算出的地址nAddr保存到NewCode后面4个字节
										//注：一个函数地址占4个字节
	}


	//填充完毕，现在NewCode[]里的指令相当于Jmp MyMessageBoxW
	//既然已经获取到了Jmp MyMessageBoxW
	//现在该是将Jmp MyMessageBoxW写入原API入口前5个字节的时候了
	//为什么是5个字节？
	//Jmp指令相当于0xe9,占一个字节的内存空间
	//MyMessageBoxW是一个地址，其实是一个整数，占4个字节的内存空间
	//int n=0x123;   n占4个字节和MyMessageBoxW占4个字节是一样的
	//1+4=5
	HookOn();
}

void CMyTestDetoursDlg::OnBnClickedButton1()
{
	// TODO: Add your control notification handler code here
	DWORD dwPid = ::GetCurrentProcessId();
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, dwPid);

	GetApiEntrance();
	SetDlgItemText(IDC_STATIC, _T("Hook已启动"));
}


void CMyTestDetoursDlg::OnBnClickedButton2()
{
	// TODO: Add your control notification handler code here
	HookOff();
	SetDlgItemText(IDC_STATIC, _T("Hook未启动"));
}


void CMyTestDetoursDlg::OnBnClickedButton3()
{
	::MessageBoxW(NULL, _T("正常的MessageBox函数"), _T("111"), 0);
}


void CMyTestDetoursDlg::OnBnClickedButton4()
{
	//1. 初始化,挂起线程
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	//2. 替换windows的API
	DetourAttach(&(PVOID&)OldMsgBoxW, MyMessageBoxW);

	//3. 恢复被替换的API，将线程转成运行态
	DetourTransactionCommit();
	SetDlgItemText(IDC_STATIC, _T("Hook已启动"));
}


void CMyTestDetoursDlg::OnBnClickedButton5()
{
	// TODO: Add your control notification handler code here
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)OldMsgBoxW, MyMessageBoxW);
	DetourTransactionCommit();
	SetDlgItemText(IDC_STATIC, _T("Hook未启动"));
}
