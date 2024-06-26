
#pragma once
#define NDIS630

#include <initguid.h>
#include <fwpsk.h>
#include <fwpmk.h>



DEFINE_GUID(
	CALLOUT_DNS_OUT_GUID,
	0x265a65e1, 0xf9c3, 0x4f97, 0x82, 0xe1, 0xe7, 0x53, 0x0, 0xb3, 0x3e, 0x6);

DEFINE_GUID(
	FILTERS_DNS_OUT_SUBLAYER_GUID,
	0x622b619a, 0x41b5, 0x4854, 0x9b, 0x6, 0x61, 0x91, 0xe9, 0x5a, 0xf3, 0x36);

NTSTATUS RegisterNetworkFilterUDP_OUT(PDEVICE_OBJECT DeviceObject);
void	 UnregisterNetwrokFilterUDP_OUT();



DEFINE_GUID(
	EXAMPLE_CALLOUT_UDP_IN_GUID, 
	0xbc3fea14, 0x7c76, 0x4d96, 0x97, 0xaa, 0xbc, 0x96, 0x24, 0xa8, 0x1b, 0x27);

DEFINE_GUID(
	UDP_IN_SUB_LAYER_GUID,
	0x1389c308, 0x7989, 0x4812, 0xba, 0xaa, 0x8e, 0xa7, 0x7f, 0xee, 0x15, 0xce);


NTSTATUS RegisterNetworkFilterUDP_IN(PDEVICE_OBJECT DeviceObject);
void	 UnregisterNetwrokFilterUDP_IN();