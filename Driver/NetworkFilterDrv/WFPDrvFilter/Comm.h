#pragma once
#include <ntifs.h>
#include <minwindef.h>
// ------------------------------------------------ 
// 通信结构体,Filter过滤
//typedef enum _FILTER_INDEX
//{
//  SCREEN_SHOT = 0,
//	IP_BLOCK,
//	PROCESS_BLOCK,
//	WEB_BLOCK,
//	// ... 要继续添加链表从这里开始
//
//	FILTER_LIST_SIZE
//}FILTER_INDEX;
//
//typedef struct _FILTER
//{
//	ULONG ip;
//	WCHAR processName[MAX_PATH];
//	FILTER_INDEX filterIndex;	// 表示当前节点在哪个链表中
//}FILTER, * PFILTER;

// 通信结构体,Recrd时间
//typedef struct _PROCESS_TIME_RECORD
//{
//    BOOLEAN verifyFlag;
//    BOOLEAN endFlag;
//    HANDLE processID;
//    LARGE_INTEGER startTime;
//    LARGE_INTEGER endTime;
//	  LARGE_INTEGER curTime;
//}PROCESS_TIME_RECORD, * PPROCESS_TIME_RECORD;

// 通信结构体，进程信息与禁令
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
