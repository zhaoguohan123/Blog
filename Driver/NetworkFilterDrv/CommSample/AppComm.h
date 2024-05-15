#pragma once
#include <Windows.h>

// ----------------------------------------------------------------------
// Filter
typedef enum _FILTER_INDEX
{
	SCREEN_SHOT = 0,
	IP_BLOCK,
	PROCESS_BLOCK,
	WEB_BLOCK,
	// ... 要继续添加链表从这里开始

	FILTER_LIST_SIZE
}FILTER_INDEX;

typedef struct _FILTER
{
	ULONG ip;
	WCHAR processName[MAX_PATH];
	FILTER_INDEX filterIndex;
}FILTER, * PFILTER;

// Records
typedef struct _PROCESS_TIME_RECORD
{
    BOOLEAN isValid;
    BOOLEAN endFlag;
    HANDLE processID;
    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    LARGE_INTEGER curTime;
}PROCESS_TIME_RECORD, * PPROCESS_TIME_RECORD;

// ProcessProhibition
typedef struct PROCESS_CREATE_INFO
{
    HANDLE processId;
    HANDLE parentProcessId;
    WCHAR filePath[MAX_PATH];
    WCHAR fileName[MAX_PATH];
}PROCESS_CREATE_INFO, * PPROCESS_CREATE_INFO;

typedef struct PROCESS_RUNTIME_STAT
{
    BOOL isProhibitCreate;
    BOOL isInject;
}PROCESS_RUNTIME_STAT, * PPROCESS_RUNTIME_STAT;
// ----------------------------------------------------------------------

// 通信类 -- 单例
class AppComm
{
public:
    static AppComm* GetInstance()
    {
        static AppComm comm;
        return &comm;
    }

    bool Initialize();

    bool UnInitlize();

    bool isSuccess() { return initFlag; }
    HANDLE GetHitWebEvent() { return hitWebEvent; }
    HANDLE GetRecordTimeEvent() { return recordTimeEvent; }
    HANDLE GetProhibitEvent() { return prohibitEvent; }

    // 返回值为true：与驱动通信成功
    bool SetFilter(PFILTER filter, ULONG filterSize);

    bool DropFilter(PFILTER filter, ULONG filterSize);

    bool DropAllFilter();

    bool BlockAccessDns(BOOLEAN enable);

    bool EnableIPWhiteList(BOOLEAN enable);

    bool BlockMailWeb(BOOLEAN isBlock);
    bool SetMailWebEvent();

    bool SetRecordTimeEvent();
    bool GetRecordTimeInfo(OUT LPVOID buffer, IN LONG buferSize);
    bool GetProcessStartCurTime(IN HANDLE pid, OUT LPVOID buffer, IN LONG buferSize);

    bool SetProhibitEvent();
    bool GetCreateProcessInfo(OUT LPVOID buffer, IN LONG buferSize);
    bool SetProcessRuntimeStat(PPROCESS_RUNTIME_STAT runTimeStat, LONG statSize);

private:
    AppComm();
    ~AppComm();
    AppComm(const AppComm&) = delete;
    AppComm& operator= (const AppComm&) = delete;

private:
    BOOL initFlag;
    HANDLE commIoHandle;
    HANDLE hitWebEvent;
    HANDLE recordTimeEvent;
    HANDLE prohibitEvent;
};