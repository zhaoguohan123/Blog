#pragma once
#include <strsafe.h>
#include <iostream>
#include <Windows.h>
#include "..\..\..\Common\logger.h"

enum DRV_IO_CONTROL_CODE
{
	IO_CREATE_CAD_EVENT,
	IO_DISABLE_CAD,
	IO_ENABLE_CAD,
};

#define IOCTL_CODE_TO_CREATE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_ANY_ACCESS)       // 创建事件
#define IOCTL_CODE_TO_ENABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)         // 开启cad屏蔽功能
#define IOCTL_CODE_TO_DISABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)        // 关闭屏蔽功能

class ControlDrv
{
public:
	ControlDrv(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude);
	~ControlDrv();

	// 给驱动发消息之前需要初始化
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