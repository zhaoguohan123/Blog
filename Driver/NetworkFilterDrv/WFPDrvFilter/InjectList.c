#include "InjectList.h"
#define INJECT_POOL_TAG 'gggg'

INJECT_PROCESSID_LIST	injectList = { 0 };
ERESOURCE			    injectListRes = { 0 };
NPAGED_LOOKASIDE_LIST	injectListLookaside = { 0 };

VOID RtlUnInitInjectList()
{
	while (!IsListEmpty(&injectList.link))
	{
		PINJECT_PROCESSID_LIST next = (PINJECT_PROCESSID_LIST)injectList.link.Blink;
		RemoveEntryList(&next->link);
		ExFreeToNPagedLookasideList(&injectListLookaside, &next->link);
	}
	ExDeleteResourceLite(&injectListRes);
	ExDeleteNPagedLookasideList(&injectListLookaside);
}

VOID RtlInitInjectList()
{
	InitializeListHead((PLIST_ENTRY)&injectList);
	ExInitializeNPagedLookasideList(&injectListLookaside, NULL, NULL, NULL, sizeof(INJECT_PROCESSID_LIST), INJECT_POOL_TAG, NULL);
	ExInitializeResourceLite(&injectListRes);
}

VOID PsSetInjectListStatus(HANDLE processid)
{
	ExAcquireResourceExclusiveLite(&injectListRes, TRUE);
	PLIST_ENTRY	head = &injectList.link;
	PINJECT_PROCESSID_LIST next = (PINJECT_PROCESSID_LIST)injectList.link.Blink;

	while (head != (PLIST_ENTRY)next)
	{
		if (next->pid == processid)
		{
			next->inject = TRUE;
			break;
		}
		next = (PINJECT_PROCESSID_LIST)(next->link.Blink);
	}
	ExReleaseResourceLite(&injectListRes);
}

BOOLEAN RtlAddInjectList(HANDLE processid)
{
	PINJECT_PROCESSID_LIST newLink = (PINJECT_PROCESSID_LIST)ExAllocateFromNPagedLookasideList(&injectListLookaside);
	if (newLink == NULL)
	{
		return FALSE;
	}
	newLink->pid = processid;
	newLink->inject = FALSE;
	ExAcquireResourceExclusiveLite(&injectListRes, TRUE);
	InsertTailList(&injectList.link, (PLIST_ENTRY)newLink);
	ExReleaseResourceLite(&injectListRes);
	return TRUE;
}

BOOLEAN PsQueryInjectListStatus(HANDLE Processid)
{
	BOOLEAN result = TRUE;
	ExAcquireResourceSharedLite(&injectListRes, TRUE);
	PLIST_ENTRY	head = &injectList.link;
	PINJECT_PROCESSID_LIST next = (PINJECT_PROCESSID_LIST)injectList.link.Blink;
	while (head != (PLIST_ENTRY)next)
	{
		if (next->pid == Processid)
		{
			if (next->inject == FALSE)
			{
				result = FALSE;
			}
			break;
		}
		next = (PINJECT_PROCESSID_LIST)(next->link.Blink);
	}
	ExReleaseResourceLite(&injectListRes);
	return result;
}

VOID RtlDeleteInjectList(HANDLE processId)
{
	ExAcquireResourceExclusiveLite(&injectListRes, TRUE);
	PLIST_ENTRY	head = &injectList.link;
	PINJECT_PROCESSID_LIST next = (PINJECT_PROCESSID_LIST)injectList.link.Blink;

	while (head != (PLIST_ENTRY)next)
	{
		if (next->pid == processId)
		{
			RemoveEntryList(&next->link);
			ExFreeToNPagedLookasideList(&injectListLookaside, &next->link);
			break;
		}
		next = (PINJECT_PROCESSID_LIST)(next->link.Blink);
	}
	ExReleaseResourceLite(&injectListRes);
}