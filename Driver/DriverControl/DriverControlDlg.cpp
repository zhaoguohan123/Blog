﻿
// DriverControlDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "DriverControl.h"
#include "DriverControlDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

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


// CDriverControlDlg 对话框



CDriverControlDlg::CDriverControlDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DRIVERCONTROL_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDriverControlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BUTTON_OPEN_DRIVER, m_OpneDrvButton);
	DDX_Control(pDX, IDC_BUTTON_CLOSE_DRV, m_CloseDrvButton);
	DDX_Control(pDX, IDC_BUTTON_DISABLECAD, m_DisableCadButton);
	DDX_Control(pDX, IDC_BUTTON_ENABLECAD, m_EnableCadButton);
}

BEGIN_MESSAGE_MAP(CDriverControlDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_SELECT_DRIVE, &CDriverControlDlg::OnBnClickedButtonSelectDrive)
	ON_BN_CLICKED(IDC_BUTTON_OPEN_DRIVER, &CDriverControlDlg::OnBnClickedButtonOpenDriver)
	ON_BN_CLICKED(IDC_BUTTON_CLOSE_DRV, &CDriverControlDlg::OnBnClickedButtonCloseDrv)
	ON_BN_CLICKED(IDC_BUTTON_ENABLECAD, &CDriverControlDlg::OnBnClickedButtonEnablecad)
	ON_BN_CLICKED(IDC_BUTTON_DISABLECAD, &CDriverControlDlg::OnBnClickedButtonDisablecad)
END_MESSAGE_MAP()


// CDriverControlDlg 消息处理程序

BOOL CDriverControlDlg::OnInitDialog()
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

	// 先将按钮都置灰
	m_OpneDrvButton.EnableWindow(0);
	m_CloseDrvButton.EnableWindow(0);
	m_DisableCadButton.EnableWindow(0);
	m_EnableCadButton.EnableWindow(0);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CDriverControlDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CDriverControlDlg::OnPaint()
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
HCURSOR CDriverControlDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CDriverControlDlg::OnBnClickedButton1()
{
	// TODO: 在此添加控件通知处理程序代码
}

// 选择驱动位置
void CDriverControlDlg::OnBnClickedButtonSelectDrive()
{
	// 获取当前执行文件的路径
	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);

	// 从路径中提取目录部分
	PathRemoveFileSpec(exePath);

	CFileDialog dlg(TRUE, _T("sys"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("System Files (*.sys)|*.sys| All Files (*.*)|*.*||"), this);

	// 设置初始目录为当前执行文件所在目录
	dlg.m_ofn.lpstrInitialDir = exePath;

	// 显示文件对话框
	if (dlg.DoModal() == IDOK)
	{
		// 获取选定的文件名并进行处理
		CString filePath = dlg.GetPathName();

		// 在此处执行您想要的操作，例如打开文件、读取文件内容等
		SetDlgItemText(IDC_STATIC_3, filePath); // 假设 IDC_STATIC3 是静态文本控件的 ID
	}

	m_OpneDrvButton.EnableWindow(1);
	m_CloseDrvButton.EnableWindow(0);
	m_DisableCadButton.EnableWindow(0);
	m_EnableCadButton.EnableWindow(0);
}


void CDriverControlDlg::OnBnClickedButtonOpenDriver()
{
	// TODO: 在此添加控件通知处理程序代码
	m_OpneDrvButton.EnableWindow(0);
	m_CloseDrvButton.EnableWindow(1);
	m_DisableCadButton.EnableWindow(1);
	m_EnableCadButton.EnableWindow(1);
}


void CDriverControlDlg::OnBnClickedButtonCloseDrv()
{
	// TODO: 在此添加控件通知处理程序代码

	m_OpneDrvButton.EnableWindow(1);
	m_CloseDrvButton.EnableWindow(0);
	m_DisableCadButton.EnableWindow(0);
	m_EnableCadButton.EnableWindow(0);
}


void CDriverControlDlg::OnBnClickedButtonEnablecad()
{
	// TODO: 在此添加控件通知处理程序代码
}


void CDriverControlDlg::OnBnClickedButtonDisablecad()
{
	// TODO: 在此添加控件通知处理程序代码
}
