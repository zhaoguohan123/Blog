#pragma once

#include "FastMutex.h"

#define DEVICE_NAME L"RegProtectorDrv"
#define DRIVER_TAG 'HAN'
#define IOCTL_REGKEY_PROTECT_ADD	CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REGKEY_PROTECT_REMOVE	CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REGKEY_PROTECT_CLEAR	CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

const int MaxRegKeyCount = 100;

typedef struct _Globals
{
	LIST_ENTRY ItemsHead;
	int ItemCount;
	FastMutex Mutex;
	LARGE_INTEGER RegCookie;

} Globals, * PGlobals;


const int MaxRegNameSize = 300;
struct RegKeyProtectInfo {
	WCHAR KeyName[MaxRegNameSize]{};
};

template <typename T>
struct FullItem
{
	LIST_ENTRY Entry;
	T Data;
};

UNICODE_STRING init_protect_path[] = {
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\vservice"),
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Services\\vservice"),
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\vdservice"),
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Services\\vdservice"),
};
