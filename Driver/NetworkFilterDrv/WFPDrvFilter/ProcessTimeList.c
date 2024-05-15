#include "ProcessTimeList.h"
#define RECORD_POOL_TAG 'gggg'

ERESOURCE				timeRecordListRes;
TIME_RECORD_ENTRY		timeRecordList = { 0 };
NPAGED_LOOKASIDE_LIST	timeRecordListLookaside = { 0 };

// �Ƚ������Ժ�ɾ�����ѯ�ıȽϹ������ı���������
BOOLEAN RecordCustomCmp(PPROCESS_TIME_RECORD buf1, PPROCESS_TIME_RECORD buf2)
{
	return buf1->processID == buf2->processID;
}

NTSTATUS GetTimeRecordByHandle(HANDLE pid, OUT PPROCESS_TIME_RECORD outBuf, UINT32 outSize)
{
	NTSTATUS stat = STATUS_UNSUCCESSFUL;
	if (pid == 0 || (ULONG_PTR)(pid) % 4 != 0 || outBuf == NULL || outSize == 0)
	{
		return stat;
	}
	if (MmIsAddressValid(outBuf))
	{
		// ��ѯ,����
		ExAcquireResourceSharedLite(&timeRecordListRes, TRUE);
		PLIST_ENTRY	head = &timeRecordList.link;
		if (!IsListEmpty(head))
		{
			PTIME_RECORD_ENTRY cur = (PTIME_RECORD_ENTRY)timeRecordList.link.Flink;
			while (head != cur)
			{
				if (cur->data.processID == pid)
				{
					__try
					{
						RtlCopyMemory(outBuf, &cur->data, outSize);
						stat = STATUS_SUCCESS;
					}
					__except (1)
					{
						KdPrint(("[error]:ProcessTimeList_GetTimeRecordByHandle -- RtlCopyMemory�����쳣"));
					}
					break;
				}
				cur = cur->link.Flink;
			}
		}
		ExReleaseResourceLite(&timeRecordListRes);
	}
	return stat;
}

// ˢ��,ɾ������ʱ�䳬��x��ļ�¼
VOID RefreshRecordList(LARGE_INTEGER curTime, ULONG seconds)
{
	ExAcquireResourceExclusiveLite(&timeRecordListRes, TRUE);
	PLIST_ENTRY	head = &timeRecordList.link;
	if (!IsListEmpty(head))
	{
		PTIME_RECORD_ENTRY cur = (PTIME_RECORD_ENTRY)timeRecordList.link.Flink;
		PTIME_RECORD_ENTRY tmp = cur;
		while (head != cur)
		{
			cur = cur->link.Flink;
			if (tmp->data.endFlag)
			{
				ULONGLONG curSeconds = curTime.QuadPart / 10000000;
				ULONGLONG endSeconds = tmp->data.endTime.QuadPart / 10000000;
				if (tmp->data.isValid == FALSE || curSeconds - endSeconds > seconds)
				{
					RemoveEntryList(&tmp->link);
					ExFreeToNPagedLookasideList(&timeRecordListLookaside, tmp);
				}
			}
			tmp = cur;
		}
	}
	ExReleaseResourceLite(&timeRecordListRes);
	return ;
}

// ---------------------------------------------------------

VOID RtlInitTimeRecordList()
{
	InitializeListHead(&timeRecordList);
	ExInitializeResourceLite(&timeRecordListRes);
	ExInitializeNPagedLookasideList(&timeRecordListLookaside, NULL, NULL, NULL, sizeof(TIME_RECORD_ENTRY), RECORD_POOL_TAG, NULL);
}

VOID RtlUnInitTimeRecordList()
{
	PLIST_ENTRY	head = &timeRecordList.link;
	if (!IsListEmpty(head))
	{
		PTIME_RECORD_ENTRY cur = (PTIME_RECORD_ENTRY)timeRecordList.link.Flink;
		PTIME_RECORD_ENTRY tmp = cur;
		while (head != cur)
		{
			cur = cur->link.Flink;
			RemoveEntryList(&tmp->link);
			ExFreeToNPagedLookasideList(&timeRecordListLookaside, tmp);
			tmp = cur;
		}
	}
	ExDeleteResourceLite(&timeRecordListRes);
	ExDeleteNPagedLookasideList(&timeRecordListLookaside);
	return;
}

BOOLEAN RtlAddRecordToList(PPROCESS_TIME_RECORD timeRecord, BOOLEAN isUnique)
{
	if (timeRecord->processID == 0 || (ULONG_PTR)(timeRecord->processID) % 4 != 0)
	{
		KdPrint(("[error]:ProcessTimeList_RtlAddRecordToList -- processID����"));
		return FALSE;
	}

	if (isUnique && RtlQueryRecordToList(timeRecord))
	{
		KdPrint(("[info]:ProcessTimeList_RtlAddRecordToList -- �ظ����Record"));
		return FALSE;
	}
	
	PTIME_RECORD_ENTRY newRecordEntry = ExAllocateFromNPagedLookasideList(&timeRecordListLookaside);

	if (newRecordEntry == NULL)
	{
		KdPrint(("[info]:ProcessTimeList_RtlAddRecordToList -- �ڴ�����ʧ��"));
		return FALSE;
	}
	memcpy(&newRecordEntry->data, timeRecord, sizeof(PROCESS_TIME_RECORD));

	ExAcquireResourceExclusiveLite(&timeRecordListRes, TRUE);
	InsertTailList(&timeRecordList, newRecordEntry);
	ExReleaseResourceLite(&timeRecordListRes);

	return TRUE;
}

BOOLEAN RtlDeleteRecordToList(PPROCESS_TIME_RECORD timeRecord)
{
	BOOLEAN res = FALSE;
	if (timeRecord->processID == 0 || (ULONG_PTR)(timeRecord->processID) % 4 != 0)
	{
		KdPrint(("[error]:ProcessTimeList_RtlDeleteRecordToList -- processID����"));
		return res;
	}

	// ��ѯɾ��,д��
	ExAcquireResourceExclusiveLite(&timeRecordListRes, TRUE);
	PTIME_RECORD_ENTRY head = &timeRecordList.link;
	if (!IsListEmpty(head))
	{
		PTIME_RECORD_ENTRY cur = (PTIME_RECORD_ENTRY)timeRecordList.link.Flink;
		while (head != cur)
		{
			if (RecordCustomCmp(&cur->data, timeRecord) == TRUE)
			{
				res = RemoveEntryList(&cur->link);
				ExFreeToNPagedLookasideList(&timeRecordListLookaside, cur);
				break;
			}
			cur = cur->link.Flink;
		}
	}
	ExReleaseResourceLite(&timeRecordListRes);
	return res;
}

BOOLEAN RtlQueryRecordToList(PPROCESS_TIME_RECORD timeRecord)
{
	if (timeRecord->processID == 0 || (ULONG_PTR)(timeRecord->processID) % 4 != 0)
	{
		KdPrint(("[error]:ProcessTimeList_RtlQueryRecordToList -- processID����"));
		return FALSE;
	}

	// ��ѯ,����
	BOOLEAN result = FALSE;
	ExAcquireResourceSharedLite(&timeRecordListRes, TRUE);
	PLIST_ENTRY	head = &timeRecordList.link;
	if (!IsListEmpty(head))
	{
		PTIME_RECORD_ENTRY cur = (PTIME_RECORD_ENTRY)timeRecordList.link.Flink;
		while (head != cur)
		{
			if (RecordCustomCmp(&cur->data, timeRecord) == TRUE)
			{
				result = TRUE;
				break;
			}
			cur = cur->link.Flink;
		}
	}
	ExReleaseResourceLite(&timeRecordListRes);
	return result;
}