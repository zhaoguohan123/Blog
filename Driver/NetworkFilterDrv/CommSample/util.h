#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <Psapi.h>
#include <unordered_map>
#include <TlHelp32.h>

class Util
{
public:
	static std::wstring GetForeWindowsText(OUT HWND& activeWindow);
	static std::wstring GetProcessNameByHwnd(HWND hwnd);
	static std::vector<ULONG> GetIpByHost(const std::string& hostName);
	static UINT32 CharToIP(const std::string& charIp);
	static UINT32 GetProcessRunTime(DWORD processId);
	static std::unordered_map<HANDLE, std::wstring>GetAllProcessNameAndId();
	static std::wstring GetProcessNameByProcessId(HANDLE pid);
};