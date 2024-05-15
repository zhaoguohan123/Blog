#pragma once
#include <ntifs.h>

typedef struct _PROCESS_TIME_RECORD
{
    BOOLEAN isValid;
    BOOLEAN endFlag;
    HANDLE processID;
    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    LARGE_INTEGER curTime;
}PROCESS_TIME_RECORD, * PPROCESS_TIME_RECORD;

typedef struct _TIME_RECORD_ENTRY
{
	LIST_ENTRY	link;
    PROCESS_TIME_RECORD data;
}TIME_RECORD_ENTRY, * PTIME_RECORD_ENTRY;

VOID RtlInitTimeRecordList();
VOID RtlUnInitTimeRecordList();
BOOLEAN RtlAddRecordToList(PPROCESS_TIME_RECORD timeRecord, BOOLEAN isUnique);
BOOLEAN RtlDeleteRecordToList(PPROCESS_TIME_RECORD timeRecord);
BOOLEAN RtlQueryRecordToList(PPROCESS_TIME_RECORD timeRecord);

VOID RefreshRecordList(LARGE_INTEGER curTime, ULONG seconds);
NTSTATUS GetTimeRecordByHandle(HANDLE pid, OUT PPROCESS_TIME_RECORD outBuf, UINT32 outSize);