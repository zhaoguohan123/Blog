
// DriverControlDlg.h: 头文件
//

#pragma once
#include <iostream>
#include <winioctl.h>
#include "ControlDrv/ControlDrv.h"


#define NAME_CAD_EVENTA  "Global\\Kbd_fltr"
#define NAME_CAD_EVENTW  L"Global\\Kbd_fltr"

#define IOCTL_CODE_TO_CREATE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_ANY_ACCESS)       // 创建事件
#define IOCTL_CODE_TO_ENABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)         // 开启cad屏蔽功能
#define IOCTL_CODE_TO_DISABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)        // 关闭屏蔽功能

// CDriverControlDlg 对话框
class CDriverControlDlg : public CDialogEx
{
// 构造
public:
	CDriverControlDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DRIVERCONTROL_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButtonSelectDrive();
	afx_msg void OnBnClickedButtonOpenDriver();
	CButton m_OpneDrvButton;
	afx_msg void OnBnClickedButtonCloseDrv();
	CButton m_CloseDrvButton;
	afx_msg void OnBnClickedButtonEnablecad();
	afx_msg void OnBnClickedButtonDisablecad();
	CButton m_DisableCadButton;
	CButton m_EnableCadButton;
	std::shared_ptr<ControlDrv> m_ControlDrv;
	void CreateEvents();
	HANDLE cadEvent;
};
