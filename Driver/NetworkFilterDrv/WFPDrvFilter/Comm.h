#pragma once
#include <ntifs.h>
#include <minwindef.h>
// ------------------------------------------------ 
// ͨ�Žṹ��,Filter����
//typedef enum _FILTER_INDEX
//{
//  SCREEN_SHOT = 0,
//	IP_BLOCK,
//	PROCESS_BLOCK,
//	WEB_BLOCK,
//	// ... Ҫ���������������￪ʼ
//
//	FILTER_LIST_SIZE
//}FILTER_INDEX;
//
//typedef struct _FILTER
//{
//	ULONG ip;
//	WCHAR processName[MAX_PATH];
//	FILTER_INDEX filterIndex;	// ��ʾ��ǰ�ڵ����ĸ�������
//}FILTER, * PFILTER;

// ͨ�Žṹ��,Recrdʱ��
//typedef struct _PROCESS_TIME_RECORD
//{
//    BOOLEAN verifyFlag;
//    BOOLEAN endFlag;
//    HANDLE processID;
//    LARGE_INTEGER startTime;
//    LARGE_INTEGER endTime;
//	  LARGE_INTEGER curTime;
//}PROCESS_TIME_RECORD, * PPROCESS_TIME_RECORD;

// ͨ�Žṹ�壬������Ϣ�����
//typedef struct PROCESS_CREATE_INFO
//{
//	HANDLE processId;
//	HANDLE parentProcessId;
//	WCHAR filePath[MAX_PATH];
//	WCHAR fileName[MAX_PATH];
//}PROCESS_CREATE_INFO, * PPROCESS_CREATE_INFO;
//
//typedef struct PROCESS_RUNTIME_STAT
//{
//	BOOL isProhibitCreate;
//  BOOL isInject
//}PPROCESS_RUNTIME_STAT, * PPPROCESS_RUNTIME_STAT;

// --------------------------------------------------

BOOLEAN IsEnalbeIPWhiteList();
BOOLEAN IsBlockAccessDns();
BOOLEAN IsBlockWeb();
LONG NotifyHitWeb();
LONG NotifyRecordTime();
LONG NotifyProcessProhibition();

NTSTATUS WaitNotifyHitWeb(ULONG waitMiliSeconds);
NTSTATUS WaitNotifyRecordTime(ULONG waitMiliSeconds);
NTSTATUS WaitNotifyProcessProhibition(ULONG waitMiliSeconds);

VOID UnInitComm();
NTSTATUS InitializeComm(PDRIVER_OBJECT ObjDrv);
