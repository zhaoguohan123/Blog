#pragma onc
#include <ntifs.h>
#include "Unrevealed.h"
#include "ProcessTimeList.h"

NTSTATUS GetTimeRecordBeNotify(OUT PPROCESS_TIME_RECORD desRecord);
NTSTATUS GetSingleProcessRunTime(IN HANDLE pid, OUT PPROCESS_TIME_RECORD record);

VOID UnInitProcessTimeRecorder(PDRIVER_OBJECT pDrv);
NTSTATUS InitProcessTimeRecorder(PDRIVER_OBJECT pDrv);