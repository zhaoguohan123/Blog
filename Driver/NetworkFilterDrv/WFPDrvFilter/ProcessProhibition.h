#pragma once
#include <ntifs.h>
#include <minwindef.h>
#include "Unrevealed.h"

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

VOID UnInitProcessProhibition(PDRIVER_OBJECT pDrv);
NTSTATUS InitProcessProhibition(PDRIVER_OBJECT pDrv);

NTSTATUS GetCreateProcessInfoBeNotify(OUT PPROCESS_CREATE_INFO desProcessInfo);
NTSTATUS SetProcessRuntimeStat(PPROCESS_RUNTIME_STAT runStat);

BOOLEAN IsProcessInject();