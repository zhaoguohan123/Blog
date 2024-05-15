#include "FilterList.h"
#define FILTER_POOL_TAG 'gggg'

ERESOURCE				filterListsRes[FILTER_LIST_SIZE];
FILTER_ENTRY			filterLists[FILTER_LIST_SIZE] = { 0 };
PAGED_LOOKASIDE_LIST	filterListsLookaside = { 0 };

// �Ƚ������Ժ�ɾ�����ѯ�ıȽϹ������ı���������
// �൱��C++�ıȽ���,operator>()
BOOLEAN FilterCustomCmp(PFILTER buf1, PFILTER buf2)
{
	if (buf1->filterIndex != PROCESS_BLOCK)
	{
		if (buf1->ip == buf2->ip) return TRUE;
		else return FALSE;
	}
	return memcmp(buf1, buf2, sizeof(FILTER)) == 0;
}

VOID RtlUnInitFilterLists()
{
	for (FILTER_INDEX filterIndex = 0; filterIndex < FILTER_LIST_SIZE; filterIndex++)
	{
		PFILTER_ENTRY	head = &filterLists[filterIndex].link;
		if (!IsListEmpty(head))
		{
			PFILTER_ENTRY cur = (PFILTER_ENTRY)filterLists[filterIndex].link.Flink;
			PFILTER_ENTRY tmp = cur;
			while(head != cur)
			{
				cur = cur->link.Flink;
				RemoveEntryList(&tmp->link);
				ExFreeToNPagedLookasideList(&filterListsLookaside, tmp);
				tmp = cur;
			}
		}
		ExDeleteResourceLite(&filterListsRes[filterIndex]);
	}
	ExDeleteNPagedLookasideList(&filterListsLookaside);
}

VOID RtlInitFilterLists()
{
	NTSTATUS stat = STATUS_SUCCESS;
	for (FILTER_INDEX filterIndex = 0; filterIndex < FILTER_LIST_SIZE; filterIndex++)
	{
		InitializeListHead(&filterLists[filterIndex]);
		ExInitializeResourceLite(&filterListsRes[filterIndex]);
	}
	ExInitializeNPagedLookasideList(&filterListsLookaside, NULL, NULL, NULL, sizeof(FILTER_ENTRY), FILTER_POOL_TAG, NULL);
}

// elementUnique = TRUE  ��ʾ��ǰ����Ԫ�ؼ��Ψһ��
BOOLEAN RtlAddFilterToList(PFILTER filter, BOOLEAN isUnique)
{
	if (filter->filterIndex >= FILTER_LIST_SIZE)
	{
		KdPrint(("[error]:FilterList_RtlAddFilterToList -- filterIndex������������"));
		return FALSE;
	}

	// ������filter�Ѵ��ڱ㲻�����
	if (isUnique && RtlQueryFilterToList(filter,FALSE))
	{
		KdPrint(("[info]:FilterList_RtlAddFilterToList -- �ظ����filter"));
		return FALSE;
	}

	PFILTER_ENTRY newFilterEntry = ExAllocateFromNPagedLookasideList(&filterListsLookaside);
	if (newFilterEntry == NULL)
	{
		KdPrint(("[info]:FilterList_RtlAddFilterToList -- �ڴ�����ʧ��"));
		return FALSE;
	}
	memcpy(&newFilterEntry->data, filter, sizeof(FILTER));
	
	// ���뵽��Ӧ��������,д��
	ExAcquireResourceExclusiveLite(&filterListsRes[filter->filterIndex], TRUE);
	InsertTailList(&filterLists[filter->filterIndex].link, newFilterEntry);
	ExReleaseResourceLite(&filterListsRes[filter->filterIndex]);

	return TRUE;
}

BOOLEAN RtlDeleteFilterToList(PFILTER filter)
{
	BOOLEAN res = FALSE;
	if (filter->filterIndex >= FILTER_LIST_SIZE)
	{
		KdPrint(("[error]:FilterList_RtlDeleteFilterToList -- filterIndex������������"));
		return res;
	}

	// ��ѯɾ��,д��
	ExAcquireResourceExclusiveLite(&filterListsRes[filter->filterIndex], TRUE);
	PFILTER_ENTRY head = &filterLists[filter->filterIndex].link;
	if (!IsListEmpty(head))
	{
		PFILTER_ENTRY cur = (PFILTER_ENTRY)filterLists[filter->filterIndex].link.Flink;
		while (head != cur)
		{
			if (FilterCustomCmp(&cur->data, filter) == TRUE)
			{
				res = RemoveEntryList(&cur->link);
				ExFreeToNPagedLookasideList(&filterListsLookaside, cur);
				break;
			}
			cur = cur->link.Flink;
		}
	}
	ExReleaseResourceLite(&filterListsRes[filter->filterIndex]);
	return res;
}

BOOLEAN RtlDeleteAllFilterToList()
{
	DbgPrint("<test>RtlDeleteAllFilterToList begin -- ɾ������IP���˹���");

	FILTER_INDEX filterIndex = IP_BLOCK;
	ExAcquireResourceExclusiveLite(&filterListsRes[filterIndex], TRUE);

	DbgPrint("<test>1111");
    PFILTER_ENTRY head = &filterLists[filterIndex].link;
    if (!IsListEmpty(head))
    {
        PFILTER_ENTRY cur = (PFILTER_ENTRY)filterLists[filterIndex].link.Flink;
        PFILTER_ENTRY tmp = cur;
        while (head != cur)
        {
            cur = cur->link.Flink;
            RemoveEntryList(&tmp->link);
			DbgPrint("<test>222");
            ExFreeToNPagedLookasideList(&filterListsLookaside, tmp);
            tmp = cur;
        }
    }

	DbgPrint("<test>3333");
	ExReleaseResourceLite(&filterListsRes[filterIndex]);

	DbgPrint("<test>RtlDeleteAllFilterToList end -- ɾ������IP���˹���");
}

BOOLEAN RtlQueryFilterToList(PFILTER filter,const BOOLEAN isDpc)
{
	if (filter->filterIndex >= FILTER_LIST_SIZE)
	{
		KdPrint(("[error]:FilterList_RtlQueryFilterToList -- filterIndex������������\r\n"));
		return FALSE;
	}

	BOOLEAN result = FALSE;

	// ��ѯ,����
	if (isDpc != TRUE)
	{
		ExAcquireResourceSharedLite(&filterListsRes[filter->filterIndex], TRUE);
	}
	
	PLIST_ENTRY	head = &filterLists[filter->filterIndex].link;
	if (!IsListEmpty(head))
	{
		PFILTER_ENTRY cur = (PFILTER_ENTRY)filterLists[filter->filterIndex].link.Flink;
		while (head != cur)
		{
			if (FilterCustomCmp(&cur->data, filter) == TRUE)
			{
				result = TRUE;
				break;
			}
			cur = cur->link.Flink;
		}
	}
	if (isDpc != TRUE)
	{
		ExReleaseResourceLite(&filterListsRes[filter->filterIndex]);
	}
	return result;
}
