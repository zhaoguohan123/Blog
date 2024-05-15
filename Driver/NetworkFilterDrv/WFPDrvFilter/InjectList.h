#pragma once
#include <ntifs.h>

typedef struct _INJECT_PROCESSID_LIST {
	LIST_ENTRY	link;
	HANDLE pid;
	BOOLEAN	inject;
}INJECT_PROCESSID_LIST, * PINJECT_PROCESSID_LIST;

VOID RtlUnInitInjectList();
VOID RtlInitInjectList();
VOID PsSetInjectListStatus(HANDLE processid);
BOOLEAN RtlAddInjectList(HANDLE processid);
BOOLEAN PsQueryInjectListStatus(HANDLE Processid);
VOID RtlDeleteInjectList(HANDLE processId);