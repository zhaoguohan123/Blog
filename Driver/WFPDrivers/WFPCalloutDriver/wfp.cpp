#include <ntddk.h>
#include "wfp.h"

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

HANDLE EngHandle = nullptr;
UINT32 CalloutId = 0;
UINT32 SystemCalloutId = 0; 
UINT64 FilterId = 0;
ULONG nDataLength = 0;

VOID ResolveDnsPacket(void* packet, size_t packetSize , size_t udpHeaderLen){
	PHYSICAL_ADDRESS highestAcceptableWriteBufferAddr;
	highestAcceptableWriteBufferAddr.QuadPart = MAXULONG64;
	if (packetSize < sizeof(DNS_HEADER)) 
	{
		return;
	}
	DNS_HEADER* dnsHeader = (DNS_HEADER*)packet;
	if (dnsHeader->IsResponse) 
	{
		return;
	}
	size_t dnsDataLength = packetSize - sizeof(DNS_HEADER);

	if (dnsDataLength >= packetSize) {
		return;
	}

	char* dnsData = (char*)packet + sizeof(DNS_HEADER) + udpHeaderLen;
	char* domainName = reinterpret_cast<char*>(MmAllocateContiguousMemory(128, highestAcceptableWriteBufferAddr));
	if (domainName == NULL) 
	{
		return;
	}

	memset(domainName, '\0', 128);

	bool isSuccess = TRUE;
	size_t domainNameLength = 0;

	while (dnsDataLength > 0) {
		const char length = *dnsData;
		if (length == 0) {
			break;
		}
		if (length >= packetSize || length > 256)
		{
			break;
		}

		if (domainNameLength + 1 > 128)
		{
			isSuccess = FALSE;
			break;
		}
		char domainNameStr = *(dnsData + 1);
		// 检查第一个字符是否是可读字符
		if (isprint(domainNameStr) == FALSE) {
			isSuccess = FALSE;
			break;
		}
		if (domainNameLength != 0) {
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
) {
	classifyOut->actionType = FWP_ACTION_PERMIT;

	if (inFixedValues->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_IP_PROTOCOL].value.uint8 != IPPROTO_UDP || 
		inFixedValues->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_IP_REMOTE_PORT].value.uint16 != 53 ||
		inFixedValues->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_DIRECTION].value.uint32 != FWP_DIRECTION_OUTBOUND)
	{
		return;
	}
	ULONG UdpHeaderLength = 0;
	BOOL isOutBound = FALSE;
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

	UINT8 direction =inFixedValues->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_DIRECTION].value.uint8;
	if (direction == FWP_DIRECTION_OUTBOUND)
	{
		UdpHeaderLength = inMetaValues->transportHeaderSize;
		if (UdpHeaderLength == 0 || UdpContentLength < UdpHeaderLength) {
			goto end;
		}
		UdpContentLength -= UdpHeaderLength;
		isOutBound = TRUE;
	}
	else
	{
		goto end;
	}

	PVOID UdpContent = ExAllocatePoolWithTag(NonPagedPool, UdpContentLength + UdpHeaderLength, 'Tsnd');
	if (UdpContent == NULL)
	{
		goto end;
	}

	PVOID ndisBuffer = NdisGetDataBuffer(pNetBuffer, UdpContentLength + UdpHeaderLength, UdpContent, 1, 0);
	if (ndisBuffer == nullptr) {
		goto end;
	}
	ResolveDnsPacket(ndisBuffer, UdpContentLength + UdpHeaderLength, UdpHeaderLength);
end:
	return;
}

NTSTATUS NotifyCallback(
	FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter
) {
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	DbgPrint("[*] inside callout driver udp notify logic...\n");

	return STATUS_SUCCESS;
}


NTSTATUS RegisterNetworkFilterUDP(PDEVICE_OBJECT DeviceObject)
{
	
	NTSTATUS status = STATUS_SUCCESS;

	// open filter engine session 
	status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &EngHandle);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[*] failed to open filter engine\n");
		return status;
	}

	// register callout in filter engine 
	FWPS_CALLOUT callout = {};
	callout.calloutKey = EXAMPLE_CALLOUT_UDP_GUID;
	callout.flags = 0;
	callout.classifyFn = ClassifyCallback;
	callout.notifyFn = NotifyCallback;
	callout.flowDeleteFn = nullptr;
	
	status =  FwpsCalloutRegister(DeviceObject, &callout, &CalloutId);
	if (!NT_SUCCESS(status)) 
	{
		DbgPrint("[*] failed to register callout in filter engine\n");
		return status;
	}

	// add callout to the system 
	FWPM_CALLOUT calloutm = { };
	calloutm.flags = 0;							 
	calloutm.displayData.name = L"example callout udp";
	calloutm.displayData.description = L"example PoC callout for udp ";
	calloutm.calloutKey = EXAMPLE_CALLOUT_UDP_GUID;
	calloutm.applicableLayer = FWPM_LAYER_DATAGRAM_DATA_V4;
	

	status =  FwpmCalloutAdd(EngHandle, &calloutm, NULL, &SystemCalloutId);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[*] failed to add callout to the system \n");
		return status;
	}

	// create a sublayer to group filters (not actually required 
	FWPM_SUBLAYER sublayer = {};
	sublayer.displayData.name = L"PoC sublayer example filters";
	sublayer.displayData.name = L"PoC sublayer examle filters";
	sublayer.subLayerKey = EXAMPLE_FILTERS_SUBLAYER_GUID;
	sublayer.weight = 65535;


	status =  FwpmSubLayerAdd(EngHandle, &sublayer, NULL);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[*] failed to create a sublayer\n");
		return status;
	}

	// add a filter that references our callout with no conditions 
	UINT64 weightValue = 0xFFFFFFFFFFFFFFFF;								
	FWP_VALUE weight = {};
	weight.type = FWP_UINT64;
	weight.uint64 = &weightValue;

	// process every packet , no conditions 
	FWPM_FILTER_CONDITION conditions[1] = { 0 };										\

		FWPM_FILTER filter = {};
	filter.displayData.name = L"example filter callout udp";
	filter.displayData.name = L"example filter calout udp";
	filter.layerKey = FWPM_LAYER_DATAGRAM_DATA_V4;
	filter.subLayerKey = EXAMPLE_FILTERS_SUBLAYER_GUID;
	filter.weight = weight;
	filter.numFilterConditions = 0;
	filter.filterCondition = conditions;
	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = EXAMPLE_CALLOUT_UDP_GUID;
	

	return FwpmFilterAdd(EngHandle, &filter, NULL, &FilterId);
}




void UnregisterNetwrokFilterUDP()
{
	if (EngHandle)
	{
		if (FilterId) 
		{
			FwpmFilterDeleteById(EngHandle, FilterId);
			FwpmSubLayerDeleteByKey(EngHandle, &EXAMPLE_FILTERS_SUBLAYER_GUID);
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