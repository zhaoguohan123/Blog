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
} DNS_QUESTION, * PDNS_QUESTION;

typedef struct _DNS_ANSWER {
	USHORT name;
	USHORT type;
	USHORT tclass;
	ULONG ttl;
	USHORT rdlength;
	// The rdata field is variable length and depends on the type field.
	// For A records (type=1), it will be 4 bytes long (IPv4 address).
} DNS_ANSWER, * PDNS_ANSWER;
#pragma pack(pop)

#define DNS_HEADER_LEN 12     // sizeof(DNS_HEADER)
#define UDP_HEADER_LEN 8
#define DNS_MAX_LEN    128
#define MEM_TAG		   'salm'

HANDLE EngHandle = NULL;

UINT32 CalloutId = 0, SystemCalloutId = 0; 
UINT64 FilterId = 0;

UINT32 udp_in_regCalloutId = 0, udp_in_addCallId = 0;
UINT64 udp_in_filterId = 0;

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
		DbgPrint("[salmon] <query>:  %s \n", domainName);
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

	ULONG UdpContentLength = NET_BUFFER_DATA_LENGTH(netBuffer);
	if (UdpContentLength == 0)
	{
		return;
	}

	ULONG UdpHeaderLength = inMetaValues->transportHeaderSize;

	if (UdpHeaderLength == 0 || UdpContentLength < UdpHeaderLength)
	{
		return;
	}

	PVOID UdpContent = ExAllocatePool2(POOL_FLAG_NON_PAGED, UdpContentLength, MEM_TAG);
	if (UdpContent == NULL)
	{
		DbgPrint("salmon:ExAllocatePool2 failed!");
		return;
	}

	PUCHAR ndisBuffer = (PUCHAR)NdisGetDataBuffer(netBuffer, UdpContentLength, UdpContent, 1, 0);
	if (ndisBuffer == NULL)
	{
		return;
	}
	ResolveDnsPacket(ndisBuffer, UdpContentLength);

	if (UdpContent)
	{
		ExFreePoolWithTag(UdpContent, MEM_TAG);
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


NTSTATUS RegisterNetworkFilterUDP_OUT(PDEVICE_OBJECT DeviceObject)
{
	
	NTSTATUS status = STATUS_SUCCESS;

	// open filter engine session 
	if (EngHandle == NULL)
	{
		status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &EngHandle);
		if (!NT_SUCCESS(status)) {
			DbgPrint("[salmon] failed to open filter engine, 0x%08x\n", status);
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
		DbgPrint("[salmon] failed to register callout in filter engine ,0x%08x\n", status);
		return status;
	}

	// add callout to the system 
	FWPM_CALLOUT calloutm = { };						 
	calloutm.displayData.name = L"DNS_OUT callout";
	calloutm.displayData.description = L"DNS_OUT callout for filtering DNS traffic";
	calloutm.calloutKey = CALLOUT_DNS_OUT_GUID;
	calloutm.applicableLayer = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	

	status =  FwpmCalloutAdd(EngHandle, &calloutm, NULL, &SystemCalloutId);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[salmon] failed to add callout to the system ,0x%08x\n", status);
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
		DbgPrint("salmon: failed to create a sublayer,0x%08x\n", status);
		return status;
	}


	FWPM_FILTER_CONDITION conditions[2] = { 0 };
	FWPM_FILTER filter = {};

	conditions[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
	conditions[0].matchType = FWP_MATCH_EQUAL;
	conditions[0].conditionValue.type = FWP_UINT8;
	conditions[0].conditionValue.uint8 = IPPROTO_UDP;

	conditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
	conditions[1].matchType = FWP_MATCH_EQUAL;
	conditions[1].conditionValue.type = FWP_UINT16;
	conditions[1].conditionValue.uint16 = 53;

	filter.numFilterConditions = 2;
	filter.filterCondition = conditions;

	filter.displayData.name = L"DNS_OUT Filter";
	filter.displayData.name = L"Filter for DNS_OUT traffic";
	filter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	filter.subLayerKey = FILTERS_DNS_OUT_SUBLAYER_GUID;
	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_DNS_OUT_GUID;
	

	status = FwpmFilterAdd(EngHandle, &filter, NULL, &FilterId);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[salmon] FwpmFilterAdd filter failed! status = 0x%08x", status);
	}


	return status;
}


void UnregisterNetwrokFilterUDP_OUT()
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
	}
}


PUCHAR ParseDnsName(PUCHAR buffer, PUCHAR current, PCHAR name, ULONG nameLen)
{
	PUCHAR end = buffer + nameLen;
	while (current < end && *current != 0)
	{
		if ((*current & 0xC0) == 0xC0)
		{ // Pointer
			current = buffer + (((*current & 0x3F) << 8) | *(current + 1));
		}
		else {
			UCHAR len = *current++;
			if (nameLen - strlen(name) <= len)
				break; // Buffer overflow protection
			strncat(name, (PCHAR)current, len);
			strcat(name, ".");
			current += len;
		}
	}
	if (strlen(name) > 0) {
		name[strlen(name) - 1] = '\0'; // Remove trailing dot
	}
	return current + 1;
}

VOID ParseDnsPacket(PUCHAR buffer, ULONG bufferSize) {
	PDNS_HEADER dnsHeader = (PDNS_HEADER)buffer;
	PUCHAR current = buffer + DNS_HEADER_LEN;

	CHAR qname[256] = { 0 };
	for (int i = 0; i < RtlUshortByteSwap(dnsHeader->QuestionCount); i++)
	{
		current = ParseDnsName(buffer, current, qname, bufferSize);
		PDNS_QUESTION question = (PDNS_QUESTION)current;
		current += sizeof(DNS_QUESTION);

		DbgPrint("[salmon] <response> iter: %d Question: %s, Type: %u, Class: %u\n", i, qname, RtlUshortByteSwap(question->qtype), RtlUshortByteSwap(question->qclass));
	}

	for (int i = 0; i < RtlUshortByteSwap(dnsHeader->AnswerCount); i++)
	{
		PDNS_ANSWER answer = (PDNS_ANSWER)current;
		current += sizeof(DNS_ANSWER);

		if (RtlUshortByteSwap(answer->type) == 1 && RtlUshortByteSwap(answer->rdlength) == 4)
		{
			ULONG ip = *(PULONG)current;
			current += 4;
			DbgPrint("[salmon] <response> iter:%d IPv4: %d.%d.%d.%d\n", i, ((PUCHAR)&ip)[0], ((PUCHAR)&ip)[1], ((PUCHAR)&ip)[2], ((PUCHAR)&ip)[3]);
		
		}
		else {
			current += RtlUshortByteSwap(answer->rdlength);
		}
	}
}

VOID UdpInClassifyCallback(
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

	classifyOut->actionType = FWP_ACTION_PERMIT;

	if (layerData == NULL)
	{
		goto end;
	}

	ULONG UdpHeaderLength = 0;
	PNET_BUFFER pNetBuffer = NET_BUFFER_LIST_FIRST_NB((PNET_BUFFER_LIST)layerData);
	if (pNetBuffer == NULL)
	{
		goto end;
	}

	ULONG UdpContentLength = NET_BUFFER_DATA_LENGTH(pNetBuffer);
	if (UdpContentLength == 0)
	{
		goto end;
	}
	UdpHeaderLength = inMetaValues->transportHeaderSize;

	if (UdpHeaderLength == 0 || UdpContentLength < UdpHeaderLength)
	{
		goto end;
	}

	PVOID UdpContent = ExAllocatePoolWithTag(NonPagedPool, UdpContentLength, MEM_TAG);
	if (UdpContent == NULL)
	{
		goto end;
	}

	PVOID ndisBuffer = NdisGetDataBuffer(pNetBuffer, UdpContentLength, UdpContent, 1, 0);
	if (ndisBuffer == NULL) {
		goto end;
	}

	// 域名白名单需要放开所有解析到的ip

	ParseDnsPacket((PUCHAR)ndisBuffer, UdpContentLength);
end:
	return;
}

NTSTATUS UdpNotifyCallback(
	FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter
) {
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	return STATUS_SUCCESS;
}

NTSTATUS RegisterNetworkFilterUDP_IN(PDEVICE_OBJECT DeviceObject)
{
	NTSTATUS status = STATUS_SUCCESS;

	if (EngHandle == NULL)
	{
		status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &EngHandle);
		if (!NT_SUCCESS(status)) {
			DbgPrint("salmon:[salmon] failed to open filter engine, 0x%08x\n", status);
			return status;
		}
	}

	FWPS_CALLOUT udp_in_callout = { 0 };
	udp_in_callout.calloutKey = EXAMPLE_CALLOUT_UDP_IN_GUID;
	udp_in_callout.flags = 0;
	udp_in_callout.classifyFn = UdpInClassifyCallback;
	udp_in_callout.notifyFn = UdpNotifyCallback;
	udp_in_callout.flowDeleteFn = NULL;


	status = FwpsCalloutRegister(DeviceObject, &udp_in_callout, &udp_in_regCalloutId);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[salmon] failed to register callout in filter engine ,0x%08x\n", status);
		return status;
	}

	FWPM_CALLOUT calloutm_udp_in = { 0 };
	calloutm_udp_in.flags = 0;
	calloutm_udp_in.displayData.name = L"callout udp in";
	calloutm_udp_in.displayData.description = L"PoC callout for udp in";
	calloutm_udp_in.calloutKey = EXAMPLE_CALLOUT_UDP_IN_GUID;
	calloutm_udp_in.applicableLayer = FWPM_LAYER_INBOUND_TRANSPORT_V4;

	status = FwpmCalloutAdd(EngHandle, &calloutm_udp_in, NULL, &udp_in_addCallId);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[salmon] FwpmCalloutAdd calloutm_udp_in failed! status = 0x%08x", status);
		return status;
	}

	FWPM_SUBLAYER udp_in_sublayer = { 0 };
	udp_in_sublayer.displayData.name = L"PoC sublayer udp in filters";
	udp_in_sublayer.displayData.name = L"PoC sublayer udp in filters";
	udp_in_sublayer.subLayerKey = UDP_IN_SUB_LAYER_GUID;
	udp_in_sublayer.weight = 64400;

	status = FwpmSubLayerAdd(EngHandle, &udp_in_sublayer, NULL);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[salmon] FwpmSubLayerAdd udp_in_sublayer failed! status = 0x%08x", status);
	}

	FWPM_FILTER udp_in_filter = { 0 };
	// udp in
	FWPM_FILTER_CONDITION udp_in_condition[2];
	udp_in_condition[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
	udp_in_condition[0].matchType = FWP_MATCH_EQUAL;
	udp_in_condition[0].conditionValue.type = FWP_UINT8;
	udp_in_condition[0].conditionValue.uint8 = IPPROTO_UDP;

	udp_in_condition[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
	udp_in_condition[1].matchType = FWP_MATCH_EQUAL;
	udp_in_condition[1].conditionValue.type = FWP_UINT16;
	udp_in_condition[1].conditionValue.uint16 = 53;

	udp_in_filter.numFilterConditions = 2;
	udp_in_filter.filterCondition = udp_in_condition;

	udp_in_filter.layerKey = FWPM_LAYER_INBOUND_TRANSPORT_V4;
	udp_in_filter.displayData.name = L"DNS Response Filter";
	udp_in_filter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	udp_in_filter.action.calloutKey = EXAMPLE_CALLOUT_UDP_IN_GUID;

	status = FwpmFilterAdd(EngHandle, &udp_in_filter, NULL, &udp_in_filterId);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[salmon] FwpmFilterAdd udp_in_filter failed! status = 0x%08x", status);
	}

	return status;
}

void UnregisterNetwrokFilterUDP_IN()
{
	if (EngHandle)
	{
		if (udp_in_filterId)
		{
			FwpmFilterDeleteById(EngHandle, udp_in_filterId);
			FwpmSubLayerDeleteByKey(EngHandle, &UDP_IN_SUB_LAYER_GUID);
		}

		if (udp_in_addCallId) {
			FwpmCalloutDeleteById(EngHandle, udp_in_addCallId);
		}

		if (udp_in_regCalloutId) {
			FwpsCalloutUnregisterById(udp_in_regCalloutId);
		}
		FwpmEngineClose(EngHandle);
	}
}
