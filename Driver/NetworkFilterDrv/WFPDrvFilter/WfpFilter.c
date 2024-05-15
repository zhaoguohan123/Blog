//#define NDIS620
#include "WfpFilter.h"
#include "Comm.h"
#include "FilterList.h"
#include "Unrevealed.h"
//#pragma comment(lib, "fwpkclnt.lib")

DEFINE_GUID(WFP_SAMPLE_ESTABLISHED_V4_GUID, 0xc50cd3e4, 0x3712, 0x409b, 0xb6, 0xd7, 0x7f, 0x35, 0x84, 0xef, 0x41, 0x97);
DEFINE_GUID(WFP_SAMPLE_SUB_LAYER_GUID, 0x18ea047d, 0xf0ec, 0x4d70, 0x95, 0x9e, 0x6, 0xa4, 0x18, 0xb2, 0xd6, 0xa7);

PDEVICE_OBJECT wfpDevObject = NULL;
HANDLE engineHandle;
UINT32 regCalloutId = 0, addCallId = 0;
UINT64 filterId = 0;

// �������ܺ��� ͨ������·���õ�������λ��
ULONG GetProcessNamePos(const PWSTR processPath, UINT32 size)
{
	for (int i = size / sizeof(WCHAR) - 1; i >= 0; i--)
	{
		if (processPath[i] == L'\\')
		{
			return i + 1;
		}
	}
	return 0;
}

// �ص�����
NTSTATUS NotifyCallback(FWPS_CALLOUT_NOTIFY_TYPE type, const GUID* filterKey, const FWPS_FILTER* filter)
{
	return STATUS_SUCCESS;
}
VOID FlowDeleteCallback(UINT16 layerId, UINT32 callOutId, UINT64 flowContext)
{

}
VOID FilterCallback(const FWPS_INCOMING_VALUES* values, const FWPS_INCOMING_METADATA_VALUES* metaData, PVOID layerData, const void* contest, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classIfyout)
{
	if (KeGetCurrentIrql() > DISPATCH_LEVEL)	 // try�쳣������DISPATCH_LEVEL��������
	{
		KdPrint(("[error]:WfpFilter_FilterCallback -- greater DISPATCH_LEVEL\r\n"));
		return;
	}

    UINT16 remotePort = 0, localport = 0;
	ULONG localIp = 0, remoteIp = 0;
	if (!(classIfyout->rights & FWPS_RIGHT_ACTION_WRITE))
	{
		classIfyout->actionType = FWP_ACTION_PERMIT;
		goto end;
	}

    localport = values->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_PORT].value.int16;
	remotePort = values->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_PORT].value.int16;
	localIp = values->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_ADDRESS].value.uint32;
	remoteIp = values->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_ADDRESS].value.uint32;

	if (remoteIp == 0x7F000001)	// 127.0.0.1
	{
		classIfyout->actionType = FWP_ACTION_PERMIT;
		goto end;
	}

	DbgPrint("<test1><test2>����ͨ�� -- localip is %u.%u.%u.%u:%u,remote ip is %u.%u.%u.%u:%u\r\n",
		(localIp >> 24) & 0xFF, (localIp >> 16) & 0xFF, (localIp >> 8) & 0xFF, (localIp) & 0xFF, localport,
		(remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF, (remoteIp) & 0xFF, remotePort);

	// ��ǰ����ͨ����Ϣ��ʼ��  ��Ƿ�ҳ�ڴ�
	PFILTER pFilters = ExAllocatePool(NonPagedPool, sizeof(FILTER) * FILTER_LIST_SIZE);
	if (pFilters == NULL)
	{
		KdPrint(("[error]:WfpFilter_FilterCallback -- �����ڴ�ʧ��\r\n"));
		goto end;
	}
	RtlZeroMemory(pFilters, sizeof(FILTER) * FILTER_LIST_SIZE);

	pFilters[IP_BLOCK].ip = remoteIp;
	pFilters[IP_BLOCK].filterIndex = IP_BLOCK;

	pFilters[WEB_BLOCK].ip = remoteIp;
	pFilters[WEB_BLOCK].filterIndex = WEB_BLOCK;

	if (FWPS_IS_METADATA_FIELD_PRESENT(metaData, FWPS_METADATA_FIELD_PROCESS_PATH) && MmIsAddressValid(metaData->processPath))
	{
		// ��·���õ�������λ��
		PWSTR path = metaData->processPath->data;
		ULONG position = GetProcessNamePos(path, metaData->processPath->size);
		pFilters[PROCESS_BLOCK].ip = 0;
		memcpy(pFilters[PROCESS_BLOCK].processName, path + position, metaData->processPath->size - position);
		pFilters[PROCESS_BLOCK].filterIndex = PROCESS_BLOCK;
	}

#if 0
	if (RtlQueryFilterToList(&pFilters[WEB_BLOCK], TRUE))
	{
		KdPrint(("[info]:WfpFilter_FilterCallback -- ������ַ IP is %u.%u.%u.%u \r\n",
			(remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF, (remoteIp) & 0xFF));
		classIfyout->actionType = FWP_ACTION_BLOCK;
	}
	else if (!RtlQueryFilterToList(&pFilters[IP_BLOCK], TRUE))
	{
		DbgPrint("<test>[info]:WfpFilter_FilterCallback -- ����IP���� IP is %u.%u.%u.%u \r\n",
			(remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF, (remoteIp) & 0xFF);
		classIfyout->actionType = FWP_ACTION_BLOCK;
	}
	else if (RtlQueryFilterToList(&pFilters[PROCESS_BLOCK], TRUE))
	{
		KdPrint(("[info]:WfpFilter_FilterCallback -- ���ؽ�������\r\n"));
		classIfyout->actionType = FWP_ACTION_BLOCK;
	}
#endif

    if (IsBlockAccessDns() && remotePort == 53)
    {
        DbgPrint("<test3>����DNS���� -- localip is %u.%u.%u.%u:%u,remote ip is %u.%u.%u.%u:%u\r\n",
            (localIp >> 24) & 0xFF, (localIp >> 16) & 0xFF, (localIp >> 8) & 0xFF, (localIp) & 0xFF, localport,
            (remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF, (remoteIp) & 0xFF, remotePort);
        classIfyout->actionType = FWP_ACTION_BLOCK;
    }
    else if (IsEnalbeIPWhiteList() && !RtlQueryFilterToList(&pFilters[IP_BLOCK], TRUE))
    {
        DbgPrint("<test4>����IP���� -- localip is %u.%u.%u.%u:%u,remote ip is %u.%u.%u.%u:%u\r\n",
            (localIp >> 24) & 0xFF, (localIp >> 16) & 0xFF, (localIp >> 8) & 0xFF, (localIp) & 0xFF, localport,
            (remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF, (remoteIp) & 0xFF, remotePort);
        classIfyout->actionType = FWP_ACTION_BLOCK;
    }
	else
	{
		classIfyout->actionType = FWP_ACTION_PERMIT;
	}

	ExFreePool(pFilters);

end:
	if ((classIfyout->actionType == FWP_ACTION_BLOCK) || filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
	{
		classIfyout->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	}
}

// ע�����
NTSTATUS WfpRegisterCallout()
{
	// reg Ϊwpfs
	FWPS_CALLOUT callOut = { 0 };
	callOut.calloutKey = WFP_SAMPLE_ESTABLISHED_V4_GUID;
	callOut.flags = 0;
	callOut.classifyFn = FilterCallback;
	callOut.notifyFn = NotifyCallback;
	callOut.flowDeleteFn = FlowDeleteCallback;
	
	return FwpsCalloutRegister(wfpDevObject, &callOut, &regCalloutId);
}

// ��ӵ���
NTSTATUS WfpAddCallout()
{
	// add Ϊwpfm
	FWPM_CALLOUT callOut = { 0 };
	callOut.flags = 0;
	callOut.displayData.name = L"EstablishedCalloutName";
	callOut.displayData.description = L"EstablishedCalloutName";
	callOut.calloutKey = WFP_SAMPLE_ESTABLISHED_V4_GUID;
	callOut.applicableLayer = FWPM_LAYER_ALE_AUTH_CONNECT_V4;	//FWPM_LAYER_STREAM_V4

	return FwpmCalloutAdd(engineHandle, &callOut, NULL, &addCallId);
}

// ����Ӳ�
NTSTATUS WpfAddSubLayer()
{
	FWPM_SUBLAYER subLayer = { 0 };
	subLayer.displayData.name = L"EstablishedSubLayerName";
	subLayer.displayData.description = L"EstablishedSubLayerName";
	subLayer.subLayerKey = WFP_SAMPLE_SUB_LAYER_GUID;
	subLayer.weight = 65500;

	return FwpmSubLayerAdd(engineHandle, &subLayer, NULL);
}

// ��ӹ�����
NTSTATUS WpfAddFilter()
{
	FWPM_FILTER filter = { 0 };
	FWPM_FILTER_CONDITION condition[1] = { 0 };
	FWP_V4_ADDR_AND_MASK AddrandMask = { 0 };

	filter.displayData.name = L"FilterCalloutName";
	filter.displayData.description = L"FilterCalloutName";
	filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;			//FWPM_LAYER_STREAM_V4
	filter.subLayerKey = WFP_SAMPLE_SUB_LAYER_GUID;
	filter.weight.type = FWP_EMPTY;
	filter.numFilterConditions = 1;
	filter.filterCondition = condition;

	filter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	filter.action.calloutKey = WFP_SAMPLE_ESTABLISHED_V4_GUID;
	condition[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;	//FWPM_CONDITION_IP_LOCAL_PORT
	condition[0].matchType = FWP_MATCH_EQUAL;					//FWP_MATCH_LESS_OR_EQUAL
	condition[0].conditionValue.type = FWP_V4_ADDR_MASK;		//FWP_UINT16;
	condition[0].conditionValue.v4AddrMask = &AddrandMask;
	//condition[0].conditionValue.uint16 = 65000;				// �������������ض˿ڣ�ʹ������������֣����������������С���������

	return FwpmFilterAdd(engineHandle, &filter, NULL, &filterId);
}

// �򿪹�������
NTSTATUS WfpOpenEngine()
{
	return FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &engineHandle);
}

// --------------------------------------------------------------------------------------

VOID UnInitWfp()
{
	if (wfpDevObject != NULL)
	{
		IoDeleteDevice(wfpDevObject);
	}
	if (engineHandle != NULL)
	{
		if (filterId != 0)
		{
			FwpmFilterDeleteById(engineHandle, filterId);
			FwpmSubLayerDeleteByKey(engineHandle, &WFP_SAMPLE_SUB_LAYER_GUID);
		}
		if (addCallId != 0)
		{
			FwpmCalloutDeleteById(engineHandle, addCallId);
		}
		if (regCalloutId != 0)
		{
			FwpsCalloutUnregisterById(regCalloutId);
		}
		FwpmEngineClose(engineHandle);
	}
}

VOID WpfClearContext()
{
	UnInitWfp();
}

NTSTATUS InitializeWfp(PDRIVER_OBJECT pDriver)
{
	UNICODE_STRING aiyou;
	RtlInitUnicodeString(&aiyou, L"\\DEVICE\\aiyou");//��ʼ���豸����

	// �����豸
	NTSTATUS status = STATUS_SUCCESS;
	IoCreateDevice(pDriver, sizeof(pDriver->DriverExtension), &aiyou, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &wfpDevObject);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	// �򿪹�������
	status = WfpOpenEngine();
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	// ע�����
	status = WfpRegisterCallout();
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	// ��ӵ���
	status = WfpAddCallout();
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	// ����Ӳ�
	status = WpfAddSubLayer();
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	// ��ӹ�����
	status = WpfAddFilter();
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	return STATUS_SUCCESS;

end:
	// ��ʼ��ʧ�ܺ�����������
	WpfClearContext();
	return STATUS_UNSUCCESSFUL;
}