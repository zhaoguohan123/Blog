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

#define IOCTL_CODE_TO_CREATE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_ANY_ACCESS)       // �����¼�
#define IOCTL_CODE_TO_ENABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)         // ����cad���ι���
#define IOCTL_CODE_TO_DISABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)        // �ر����ι���

class ControlDrv
{
public:
	ControlDrv(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude);
	~ControlDrv();

	// ����������Ϣ֮ǰ��Ҫ��ʼ��
	BOOL Init();

	// ��װ����
	BOOL InstallDriver();

	// ��������
	BOOL StartDriver();

	// ֹͣ����
	BOOL StopDriver();

	// ɾ������
	BOOL DeleteDriver();

	// ���������Ϳ�����Ϣ
	VOID SendCmdToDrv(DWORD dwCode);

private:
	LPCTSTR lpszDriverName_;			 // ����������
	LPCTSTR lpszDriverPath_;			 // ������·��
	LPCTSTR lpszAltitude_;				 // �������������ȼ�

	HANDLE KbdDevice_;					 // �豸����
};