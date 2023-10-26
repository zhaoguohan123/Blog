#pragma once
#include "Common.h"

void MyDebugPrintA(const char* format, ...);
class ControlDrv
{
public:
	ControlDrv(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude);
	~ControlDrv();
	
	// 初始化
	BOOL Init();

	// 安装驱动
	BOOL InstallDriver();

	// 启动驱动
	BOOL StartDriver();

	// 停止驱动
	BOOL StopDriver();

	// 删除驱动
	BOOL DeleteDriver();

	// 给驱动发送控制消息
	VOID SendCmdToDrv(DWORD dwCode);

private:
	LPCTSTR lpszDriverName_;			 // 驱动的名称
	LPCTSTR lpszDriverPath_;			 // 驱动的路径
	LPCTSTR lpszAltitude_;				 // 驱动的启动优先级

	HANDLE KbdDevice_;					 // 设备对象
};