
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

NTSTATUS RegisterNetworkFilterUDP(PDEVICE_OBJECT DeviceObject);
void UnregisterNetwrokFilterUDP();