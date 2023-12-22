
// InputHooksDlg.h: 头文件
//

#pragma once
#include <memory>
#include <string>
#include <Windows.h>

typedef void (*KeyCapturedCallback)(const std::wstring& col1, const std::wstring& col2, void* pObject);
void Record(int key_stroke, WPARAM wParam);
LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam);

// CInputHooksDlg 对话框
class CInputHooksDlg : public CDialogEx
{
// 构造
public:
	CInputHooksDlg(CWnd* pParent = nullptr);	// 标准构造函数
	void InstallUserHook();
	void UnInstallUserHook();

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
	void SetKeyCapRecordCallBack(KeyCapturedCallback callback);
	CListCtrl m_list_user_kb_ctrl;
	CButton set_hook_btn_;
	CButton un_set_hook_btn_;

	static void* m_pObject;
	afx_msg void OnBnClickedButtonClean();
};
