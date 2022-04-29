
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

//ԭ�������Ͷ���
typedef int (WINAPI* MsgBoxW)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
MsgBoxW OldMsgBoxW = NULL;			//ָ��ԭ������ָ��
FARPROC pfOldMsgBoxW;				//ָ������Զָ��
BYTE OldCode[5];					//ԭϵͳAPI��ڴ���
BYTE NewCode[5];					//ԭϵͳAPI�µ���ڴ��� (jmp xxxxxxxx)
HANDLE hProcess = NULL;				//��������̾��
void HookOn();
void HookOff();

int WINAPI MyMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	TRACE(lpText);
	int nRet = 0;
	HookOff();//����ԭ����֮ǰ���ǵ��Ȼָ�HOOKѽ����Ȼ�ǵ��ò�����
			  //������ָ�HOOK���͵���ԭ�������������ѭ��
			  //�Ͼ����õĻ������ǵĺ������Ӷ���ɶ�ջ��������������
	DebugPrint("�����ҵĺ���");
	nRet = ::MessageBoxW(hWnd, _T("������MessageBoxW��HOOK��"), lpCaption, uType);


	HookOn();//������ԭ�����󣬼ǵü�������HOOK����Ȼ�´λ�HOOK������ 

	return nRet;

}
//�������ӵĺ���
void HookOn() {
	ASSERT(hProcess != NULL);

	DWORD dwTemp = 0;
	DWORD dwOldProtect;

	//�޸�API�������ǰ5���ֽ�Ϊjmp xxxxxx
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

//�رչ��ӵĺ���
void HookOff()
{
	ASSERT(hProcess != NULL);

	DWORD dwTemp = 0;
	DWORD dwOldProtect;

	//�ָ�API�������ǰ5���ֽ�
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

//��ȡAPI�������ǰ5���ֽ�
//�����ǰ5���ֽڱ�����ǰ�涨����ֽ�����BYTE OldCode[5]
//�����ǰ5���ֽڱ�����ǰ�涨����ֽ�����BYTE NewCode[5]
void GetApiEntrance()
{

	//��ȡԭAPI��ڵ�ַ
	HMODULE hmod = ::LoadLibrary(_T("User32.dll"));
	OldMsgBoxW = (MsgBoxW)::GetProcAddress(hmod, "MessageBoxW");
	pfOldMsgBoxW = (FARPROC)OldMsgBoxW;

	if (pfOldMsgBoxW == NULL)
	{
		DebugPrint("GetProcAddress faield, errcode = %d",GetLastError());
		return;
	}

	// ��ԭAPI�����ǰ5���ֽڴ��뱣�浽OldCode[]
	_asm
	{
		lea edi, OldCode		//��ȡOldCode����ĵ�ַ,�ŵ�edi
		mov esi, pfOldMsgBoxW //��ȡԭAPI��ڵ�ַ���ŵ�esi
		cld	  //�����־λ��Ϊ��������ָ����׼��
		movsd //����ԭAPI���ǰ4���ֽڵ�OldCode���� ԭ����ָ��
		movsb //����ԭAPI��ڵ�5���ֽڵ�OldCode����
	}


	NewCode[0] = 0xe9;//ʵ����0xe9���൱��jmpָ��

					  //��ȡMyMessageBoxW����Ե�ַ,ΪJmp��׼��
					  //int nAddr= UserFunAddr �C SysFunAddr - �����Ƕ��Ƶ�����ָ��Ĵ�С��;
					  //Jmp nAddr;
					  //�����Ƕ��Ƶ�����ָ��Ĵ�С��, ������5��5���ֽ���
	_asm
	{
		lea eax, MyMessageBoxW //��ȡ���ǵ�MyMessageBoxW������ַ
		mov ebx, pfOldMsgBoxW  //ԭϵͳAPI������ַ
		sub eax, ebx			 //int nAddr= UserFunAddr �C SysFunAddr
		sub eax, 5			 //nAddr=nAddr-5
		mov dword ptr[NewCode + 1], eax //������ĵ�ַnAddr���浽NewCode����4���ֽ�
										//ע��һ��������ַռ4���ֽ�
	}


	//�����ϣ�����NewCode[]���ָ���൱��Jmp MyMessageBoxW
	//��Ȼ�Ѿ���ȡ����Jmp MyMessageBoxW
	//���ڸ��ǽ�Jmp MyMessageBoxWд��ԭAPI���ǰ5���ֽڵ�ʱ����
	//Ϊʲô��5���ֽڣ�
	//Jmpָ���൱��0xe9,ռһ���ֽڵ��ڴ�ռ�
	//MyMessageBoxW��һ����ַ����ʵ��һ��������ռ4���ֽڵ��ڴ�ռ�
	//int n=0x123;   nռ4���ֽں�MyMessageBoxWռ4���ֽ���һ����
	//1+4=5
	HookOn();
}

void CMyTestDetoursDlg::OnBnClickedButton1()
{
	// TODO: Add your control notification handler code here
	DWORD dwPid = ::GetCurrentProcessId();
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, dwPid);

	GetApiEntrance();
	SetDlgItemText(IDC_STATIC, _T("Hook������"));
}


void CMyTestDetoursDlg::OnBnClickedButton2()
{
	// TODO: Add your control notification handler code here
	HookOff();
	SetDlgItemText(IDC_STATIC, _T("Hookδ����"));
}


void CMyTestDetoursDlg::OnBnClickedButton3()
{
	::MessageBoxW(NULL, _T("������MessageBox����"), _T("111"), 0);
}


void CMyTestDetoursDlg::OnBnClickedButton4()
{
	//1. ��ʼ��,�����߳�
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	//2. �滻windows��API
	DetourAttach(&(PVOID&)OldMsgBoxW, MyMessageBoxW);

	//3. �ָ����滻��API�����߳�ת������̬
	DetourTransactionCommit();
	SetDlgItemText(IDC_STATIC, _T("Hook������"));
}


void CMyTestDetoursDlg::OnBnClickedButton5()
{
	// TODO: Add your control notification handler code here
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)OldMsgBoxW, MyMessageBoxW);
	DetourTransactionCommit();
	SetDlgItemText(IDC_STATIC, _T("Hookδ����"));
}
