#pragma once
#include <ntifs.h>
#include <minwindef.h>

typedef enum _FILTER_INDEX
{
	SCREEN_SHOT = 0,
	IP_BLOCK,
	PROCESS_BLOCK,
	WEB_BLOCK,
	// ... Ҫ���������������￪ʼ

	FILTER_LIST_SIZE
}FILTER_INDEX;

typedef struct _FILTER
{
	ULONG ip;
	WCHAR processName[MAX_PATH];
	FILTER_INDEX filterIndex;	// ��ʾ��ǰ�ڵ����ĸ�������
}FILTER, * PFILTER;

typedef struct _FILTER_ENTRY 
{
	LIST_ENTRY	link;
	FILTER data;
}FILTER_ENTRY, * PFILTER_ENTRY;

VOID RtlUnInitFilterLists();
VOID RtlInitFilterLists();
BOOLEAN RtlAddFilterToList(PFILTER filter,BOOLEAN isUnique);
BOOLEAN RtlDeleteFilterToList(PFILTER filter);
BOOLEAN RtlDeleteAllFilterToList();
BOOLEAN RtlQueryFilterToList(PFILTER filter, const BOOLEAN isDpc);