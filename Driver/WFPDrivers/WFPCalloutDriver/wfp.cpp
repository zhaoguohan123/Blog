#include <ntddk.h>
#include "wfp.h"
#pragma pack(push, 1)
typedef struct _DNS_HEADER
{
	WORD    Xid;

	BYTE    RecursionDesired : 1;
	BYTE    Truncation : 1;
	BYTE    Authoritative : 1;
	BYTE    Opcode : 4;
	BYTE    IsResponse : 1;

	BYTE    ResponseCode : 4;
	BYTE    CheckingDisabled : 1;
	BYTE    AuthenticatedData : 1;
	BYTE    Reserved : 1;
	BYTE    RecursionAvailable : 1;

	WORD    QuestionCount;
	WORD    AnswerCount;
	WORD    NameServerCount;
	WORD    AdditionalCount;
}
DNS_HEADER, * PDNS_HEADER;

typedef struct {
	WORD qtype; // Query type
	WORD qclass; // Query class
} DNS_QUESTION;
#pragma pack(pop)

#define DNS_HEADER_LEN 12     // sizeof(DNS_HEADER)
#define UDP_HEADER_LEN 8
#define DNS_MAX_LEN    128
#define MEM_TAG		   'salm'

HANDLE EngHandle = NULL;
UINT32 CalloutId = 0;
UINT32 SystemCalloutId = 0; 
UINT64 FilterId = 0;
ULONG nDataLength = 0;

VOID ResolveDnsPacket(void* packet, size_t packetSize){
	PHYSICAL_ADDRESS highestAcceptableWriteBufferAddr;
	highestAcceptableWriteBufferAddr.QuadPart = MAXULONG64;
	if (packetSize < DNS_HEADER_LEN)
	{
		return;
	}
	DNS_HEADER* dnsHeader = (DNS_HEADER*)packet;
	if (dnsHeader->IsResponse) 
	{
		return;
	}
	size_t dnsDataLength = packetSize - DNS_HEADER_LEN - UDP_HEADER_LEN;

	if (dnsDataLength >= packetSize) {
		return;
	}

	char* dnsData = (char*)packet + DNS_HEADER_LEN + UDP_HEADER_LEN;
	char* domainName = reinterpret_cast<char*>(MmAllocateContiguousMemory(DNS_MAX_LEN, highestAcceptableWriteBufferAddr));
	if (domainName == NULL) 
	{
		return;
	}

	memset(domainName, '\0', DNS_MAX_LEN);

	bool isSuccess = TRUE;
	DWORD domainNameLength = 0;

	while (dnsDataLength > 0) {
		const char length = *dnsData;
		if (length == 0) {
			break;
		}
		if (length >= packetSize || length > 256)
		{
			break;
		}

		if (domainNameLength + 1 > DNS_MAX_LEN)
		{
			isSuccess = FALSE;
			break;
		}
		char domainNameStr = *(dnsData + 1);
		// 检查第一个字符是否是可读字符
		if (isprint(domainNameStr) == FALSE) 
		{
			isSuccess = FALSE;
			break;
		}
		if (domainNameLength != 0) 
		{
			domainName[domainNameLength] = '.';
			domainNameLength++;
		}
		memcpy(domainName + domainNameLength, dnsData + 1, length);
		domainNameLength += length;
		dnsDataLength -= *dnsData + 1;
		dnsData += *dnsData + 1;
	}
	if (isSuccess) 
	{
		// TODO: OnDnsQueryEvent;
		DbgPrint("ResolverDnsPacket: %s \n", domainName);
	}
	MmFreeContiguousMemory(domainName);
}

VOID ClassifyCallback(
	const FWPS_INCOMING_VALUES* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT* classifyOut
)
{
	UNREFERENCED_PARAMETER(inFixedValues);
	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(flowContext);
	UNREFERENCED_PARAMETER(classifyOut);

	// Layer data contains the packet
	if (layerData == NULL)
	{
		return;
	}
	DbgPrint("layerId :%d, protocol:%d, port:%d",
		inFixedValues->layerId,
		inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_PROTOCOL].value.uint8,
		inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_PORT].value.uint16);
	if (inFixedValues->layerId != FWPS_LAYER_OUTBOUND_TRANSPORT_V4)
	{
		return;
	}
	if (inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_PROTOCOL].value.uint8 != IPPROTO_UDP)
	{
		return;
	}
	if(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_PORT].value.uint16 != 53)
	{
		
		return;
	}

	NET_BUFFER_LIST* netBufferList = (NET_BUFFER_LIST*)layerData;
	if (netBufferList == NULL)
	{
		return;
	}
	NET_BUFFER* netBuffer = NET_BUFFER_LIST_FIRST_NB(netBufferList);
	if (netBuffer == NULL)
	{
		return;
	}

	ULONG dataLength = NET_BUFFER_DATA_LENGTH(netBuffer);
	if (dataLength < DNS_HEADER_LEN)
	{
		return;
	}
	PVOID pBuffer = ExAllocatePoolZero(NonPagedPoolNx, dataLength, MEM_TAG);
	if (pBuffer)
	{
		RtlZeroMemory(pBuffer, dataLength);
	}
	else 
	{
		return;
	}

	PUCHAR packetData = (PUCHAR)NdisGetDataBuffer(netBuffer, dataLength, pBuffer, 1, 0);
	if (packetData == NULL)
	{
		return;
	}
	ResolveDnsPacket(pBuffer, dataLength);

	if (pBuffer)
	{
		ExFreePoolWithTag(pBuffer, MEM_TAG);
	}
}

NTSTATUS NotifyCallback(
	FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter
) {
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	return STATUS_SUCCESS;
}


NTSTATUS RegisterNetworkFilterUDP(PDEVICE_OBJECT DeviceObject)
{
	
	NTSTATUS status = STATUS_SUCCESS;

	// open filter engine session 
	if (EngHandle == NULL)
	{
		status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &EngHandle);
		if (!NT_SUCCESS(status)) {
			DbgPrint("[salmon] failed to open filter engine\n");
			return status;
		}
	}

	// register callout in filter engine 
	FWPS_CALLOUT callout = {0};
	callout.calloutKey = CALLOUT_DNS_OUT_GUID;
	callout.classifyFn = ClassifyCallback;
	callout.notifyFn = NotifyCallback;
	callout.flowDeleteFn = NULL;
	
	status =  FwpsCalloutRegister(DeviceObject, &callout, &CalloutId);
	if (!NT_SUCCESS(status)) 
	{
		DbgPrint("[salmon] failed to register callout in filter engine\n");
		return status;
	}

	// add callout to the system 
	FWPM_CALLOUT calloutm = { };
	//calloutm.flags = 0;							 
	calloutm.displayData.name = L"DNS_OUT callout";
	calloutm.displayData.description = L"DNS_OUT callout for filtering DNS traffic";
	calloutm.calloutKey = CALLOUT_DNS_OUT_GUID;
	calloutm.applicableLayer = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	

	status =  FwpmCalloutAdd(EngHandle, &calloutm, NULL, &SystemCalloutId);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[salmon] failed to add callout to the system \n");
		return status;
	}

	// create a sublayer to group filters (not actually required 
	FWPM_SUBLAYER sublayer = {};
	sublayer.displayData.name = L"DNS_OUT sublayer";
	sublayer.displayData.name = L"DNS_OUT sublayer for filters";
	sublayer.subLayerKey = FILTERS_DNS_OUT_SUBLAYER_GUID;
	sublayer.weight = 65535;


	status =  FwpmSubLayerAdd(EngHandle, &sublayer, NULL);
	if (!NT_SUCCESS(status)) 
	{
		DbgPrint("[*] failed to create a sublayer\n");
		return status;
	}

	// add a filter that references our callout with no conditions 
	UINT64 weightValue = 0xFFFFFFFFFFFFFFFF;								
	FWP_VALUE weight = {};
	weight.type = FWP_UINT64;
	weight.uint64 = &weightValue;

	// process every packet , no conditions 
	FWPM_FILTER_CONDITION conditions[1] = { 0 };

	FWPM_FILTER filter = {};
	filter.displayData.name = L"DNS_OUT Filter";
	filter.displayData.name = L"Filter for DNS_OUT traffic";
	filter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	filter.subLayerKey = FILTERS_DNS_OUT_SUBLAYER_GUID;
	filter.weight = weight;
	filter.numFilterConditions = 0;
	filter.filterCondition = conditions;
	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_DNS_OUT_GUID;
	

	return FwpmFilterAdd(EngHandle, &filter, NULL, &FilterId);
}


void UnregisterNetwrokFilterUDP()
{
	if (EngHandle)
	{
		if (FilterId) 
		{
			FwpmFilterDeleteById(EngHandle, FilterId);
			FwpmSubLayerDeleteByKey(EngHandle, &FILTERS_DNS_OUT_SUBLAYER_GUID);
		}

		// Remove the callout from the FWPM_LAYER_INBOUND_TRANSPORT_V4 layer
		if (SystemCalloutId) {
			FwpmCalloutDeleteById(EngHandle, SystemCalloutId);
		}

		// Unregister the callout
		if (CalloutId) {
			FwpsCalloutUnregisterById(CalloutId);
		}
		FwpmEngineClose(EngHandle);
	}
}