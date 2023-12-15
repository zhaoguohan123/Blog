
// InputHooksDlg.h: 头文件
//

#pragma once
#include "UserKbHook/UserKbHook.h"
#include <memory>

// CInputHooksDlg 对话框
class CInputHooksDlg : public CDialogEx
{
// 构造
public:
	CInputHooksDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_INPUTHOOKS_DIALOG };
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
	afx_msg void OnBnClickedOpenHook();
	afx_msg void OnBnClickedCloseHook();
	CListCtrl m_list_user_kb_ctrl;
	std::shared_ptr<UserKbHook> user_kb_hook_;
};
