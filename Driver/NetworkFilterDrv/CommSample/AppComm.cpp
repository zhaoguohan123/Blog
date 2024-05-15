#include "AppComm.h"
#include <iostream>

#define IOCTL_SET_MAILWEB_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x900,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_SET_RECORDTIME_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x901,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_SET_PROHIBITION_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x902,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_FILTER_SET CTL_CODE(FILE_DEVICE_UNKNOWN,0x903,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_FILTER_DROP CTL_CODE(FILE_DEVICE_UNKNOWN,0x904,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_BLOCK_MAILWEB CTL_CODE(FILE_DEVICE_UNKNOWN,0x905,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_GET_RECORDTIME_INFO CTL_CODE(FILE_DEVICE_UNKNOWN,0x906,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_GET_CURRECORDTIME_INFO CTL_CODE(FILE_DEVICE_UNKNOWN,0x907,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_USER_GET_PROCESSINFO_CONTEX_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN,0x908,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_USER_SET_CONTEX_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN,0x909,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_IP_FILTER_DROP_ALL        CTL_CODE(FILE_DEVICE_UNKNOWN,0x90a,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_BLOCK_ACCESS_DNS          CTL_CODE(FILE_DEVICE_UNKNOWN,0x90b,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_ENABLE_IP_WHITE_LIST      CTL_CODE(FILE_DEVICE_UNKNOWN,0x90c,METHOD_IN_DIRECT,FILE_ANY_ACCESS)


#define COMM_IO_LINK L"\\\\.\\commIolink"
//#define COMM_IO_LINK L"\\\\.\\kl_gse_iolink"

AppComm::AppComm()
: initFlag(false)
, commIoHandle(nullptr)
, hitWebEvent(nullptr)
, recordTimeEvent(nullptr)
, prohibitEvent(nullptr)
{
	
}

bool AppComm::Initialize()
{
    commIoHandle = CreateFileW(COMM_IO_LINK, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    hitWebEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    recordTimeEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    prohibitEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    if (commIoHandle == INVALID_HANDLE_VALUE || hitWebEvent == 0 || recordTimeEvent == 0 || prohibitEvent == 0)
    {
        this->initFlag = false;
        return false;
    }
    else
    {
        this->initFlag = true;
        if (!AppComm::SetMailWebEvent())
        {
            this->initFlag = false;
        }
        if (!AppComm::SetRecordTimeEvent())
        {
            this->initFlag = false;
        }
        if (!AppComm::SetProhibitEvent())
        {
            this->initFlag = false;
        }
    }

    return true;
}

bool AppComm::UnInitlize()
{
    this->initFlag = false;
    if (commIoHandle)
    {
        (void)CloseHandle(commIoHandle);
        commIoHandle = nullptr;
    }
    
    if (hitWebEvent)
    {
        (void)CloseHandle(hitWebEvent);
        hitWebEvent = nullptr;
    }

    if (recordTimeEvent)
    {
        (void)CloseHandle(recordTimeEvent);
        recordTimeEvent = nullptr;
    }

    if (prohibitEvent)
    {
        (void)CloseHandle(prohibitEvent);
        prohibitEvent = nullptr;
    }

    return true;
}

AppComm::~AppComm()
{
	
}

bool AppComm::SetFilter(PFILTER filter, ULONG filterSize)
{
	if (!this->isSuccess()) return false;
	DWORD retd = filterSize;
	return DeviceIoControl(commIoHandle, IOCTL_FILTER_SET, filter, filterSize, 0, 0, &retd, 0);
}

bool AppComm::DropFilter(PFILTER filter, ULONG filterSize)
{
	if (!this->isSuccess()) return false;
	DWORD retd = filterSize;
	return DeviceIoControl(commIoHandle, IOCTL_FILTER_DROP, filter, filterSize, 0, 0, &retd, 0);
}

bool AppComm::DropAllFilter()
{
    if (!this->isSuccess()) return false;
    DWORD retd = 0;
    return DeviceIoControl(commIoHandle, IOCTL_IP_FILTER_DROP_ALL, nullptr, 0, 0, 0, &retd, 0);
}

bool AppComm::BlockAccessDns(BOOLEAN enable)
{
    if (!this->isSuccess()) return false;

    DWORD retd = sizeof(BOOLEAN);
    return DeviceIoControl(commIoHandle, IOCTL_BLOCK_ACCESS_DNS, &enable, sizeof(BOOLEAN), 0, 0, &retd, 0);
}

bool AppComm::EnableIPWhiteList(BOOLEAN enable)
{
    if (!this->isSuccess()) return false;

    DWORD retd = sizeof(BOOLEAN);
    return DeviceIoControl(commIoHandle, IOCTL_ENABLE_IP_WHITE_LIST, &enable, sizeof(BOOLEAN), 0, 0, &retd, 0);
}

bool AppComm::BlockMailWeb(BOOLEAN isBlock)
{
	if (!this->isSuccess()) return false;
	DWORD retd = sizeof(BOOLEAN);
	return DeviceIoControl(commIoHandle, IOCTL_BLOCK_MAILWEB, &isBlock, sizeof(BOOLEAN), 0, 0, &retd, 0);
}

bool AppComm::SetMailWebEvent()
{
	if (!this->isSuccess()) return false;
	DWORD retd = sizeof(PHANDLE);
	return DeviceIoControl(commIoHandle, IOCTL_SET_MAILWEB_EVENT, &(this->hitWebEvent), sizeof(PHANDLE), 0, 0, &retd, 0);
}

bool AppComm::SetRecordTimeEvent()
{
	if (!this->isSuccess()) return false;
	DWORD retd = sizeof(PHANDLE);
	return DeviceIoControl(commIoHandle, IOCTL_SET_RECORDTIME_EVENT, &(this->recordTimeEvent), sizeof(PHANDLE), 0, 0, &retd, 0);
}

bool AppComm::GetRecordTimeInfo(OUT LPVOID buffer, IN LONG buferSize)
{
	if(!this->isSuccess()) return false;
	DWORD retd = buferSize;
	return DeviceIoControl(commIoHandle, IOCTL_GET_RECORDTIME_INFO, 0, 0, buffer, buferSize, &retd, 0);
}

bool AppComm::GetProcessStartCurTime(IN HANDLE pid, OUT LPVOID buffer, IN LONG buferSize)
{
	if (!this->isSuccess()) return false;
	DWORD retd = buferSize;
	return DeviceIoControl(commIoHandle, IOCTL_GET_CURRECORDTIME_INFO, (LPVOID)&pid, sizeof(HANDLE), buffer, buferSize, &retd, 0);
}

bool AppComm::SetProhibitEvent()
{
	if (!this->isSuccess()) return false;
	DWORD retd = sizeof(PHANDLE);
	return DeviceIoControl(commIoHandle, IOCTL_SET_PROHIBITION_EVENT, &(this->prohibitEvent), sizeof(PHANDLE), 0, 0, &retd, 0);
}

bool AppComm::GetCreateProcessInfo(OUT LPVOID buffer, IN LONG buferSize)
{
	if (!this->isSuccess()) return false;
	DWORD retd = buferSize;
	return DeviceIoControl(commIoHandle, IOCTL_USER_GET_PROCESSINFO_CONTEX_CONTROL, 0, 0, buffer, buferSize, &retd, 0);
}

bool AppComm::SetProcessRuntimeStat(PPROCESS_RUNTIME_STAT runTimeStat, LONG statSize)
{
	if (!this->isSuccess()) return false;
	DWORD retd = statSize;
	return DeviceIoControl(commIoHandle, IOCTL_USER_SET_CONTEX_CONTROL, runTimeStat, statSize, 0, 0, &retd, 0);
}