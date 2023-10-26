#pragma once
#include "Common.h"

void MyDebugPrintA(const char* format, ...);
class ControlDrv
{
public:
	ControlDrv(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude);
	~ControlDrv();
	
	// ��ʼ��
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