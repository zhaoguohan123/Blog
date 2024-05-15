#include "util.h"
#pragma comment(lib, "ws2_32.lib")
#define LOG_INFO(_x_)  printf(_x_);

using namespace std;

// 得到活动窗口的标题、句柄
std::wstring Util::GetForeWindowsText(OUT HWND& activeWindow)
{
	activeWindow = GetForegroundWindow();
	if (activeWindow != nullptr)
	{
		const int bufferSize = 256;
		wchar_t windowTitle[bufferSize];
		GetWindowText(activeWindow, windowTitle, bufferSize);
		if (wcslen(windowTitle) != 0)
		{
			return wstring(windowTitle);
		}
	}
	return wstring(L"");
}

// 通过窗口句柄得到进程名
std::wstring Util::GetProcessNameByHwnd(HWND hwnd)
{
	wstring processPath(L"");
	wstring processName(L"");
	if (hwnd == NULL) return processName;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != 0)
	{
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess != NULL)
		{
			wchar_t tmpPath[MAX_PATH];
			GetModuleFileNameEx(hProcess, NULL, tmpPath, MAX_PATH);
			processPath = tmpPath;
			CloseHandle(hProcess);
		}
	}
	processName = processPath.substr(processPath.find_last_of('\\') + 1, processPath.length() - processPath.find_last_of('\\'));
	return processName;
}

// 通过域名得到IP(小端序)
std::vector<ULONG> Util::GetIpByHost(const std::string& hostName)
{
	vector<ULONG> ipVec;
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		LOG_INFO(("WSAStartup failed\n"));
		return ipVec;
	}

	struct addrinfo hints;
	struct addrinfo* res, * tmp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	int ret = getaddrinfo(hostName.c_str(), NULL, &hints, &res);
	if (ret != 0)
	{
		//LOG_INFO(("getaddrinfo: %s \n", gai_strerrorA(ret)));
		//LOG_INFO(("\n"));

		WSACleanup();
		return ipVec;
	}

	for (tmp = res; tmp != NULL; tmp = tmp->ai_next)
	{
		if (tmp->ai_family == AF_INET)
		{
			sockaddr_in* addr = (sockaddr_in*)tmp->ai_addr;
			ULONG tmpIp = addr->sin_addr.S_un.S_addr;

			// 大端转小端
			ULONG ip = ((tmpIp & 0xFF) << 24) + (((tmpIp >> 8) & 0xFF) << 16) + (((tmpIp >> 16) & 0xFF) << 8) + (((tmpIp >> 24) & 0xFF));
			// printf("%d.%d.%d.%d\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF);
			ipVec.push_back(ip);
		}
	}

	freeaddrinfo(res);
	WSACleanup();
	return ipVec;
}

// IP地址字符串转小端序
UINT32 Util::CharToIP(const std::string& charIp)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		LOG_INFO(("WSAStartup failed\n"));
		return 0;
	}

	struct addrinfo hints;
	struct addrinfo* res;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	int ret = getaddrinfo(charIp.c_str(), NULL, &hints, &res);
	if (ret != 0)
	{
		LOG_INFO(("getaddrinfo: %s\n", gai_strerrorA(ret)));
		WSACleanup();
		return 0;
	}
	sockaddr_in* addr = (sockaddr_in*)res->ai_addr;
	ULONG tmpIp = addr->sin_addr.S_un.S_addr;
	ULONG ip = ((tmpIp & 0xFF) << 24) + (((tmpIp >> 8) & 0xFF) << 16) + (((tmpIp >> 16) & 0xFF) << 8) + (((tmpIp >> 24) & 0xFF));

	freeaddrinfo(res);
	WSACleanup();
	return ip;
}

// 得到进程运行时间
UINT32 Util::GetProcessRunTime(DWORD processId)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
	if (hProcess == NULL)
	{
		return 0;
	}

	UINT32 runTime = 0;
	FILETIME createTime, exitTime, kernelTime, userTime;
	if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime))
	{
		// 计算运行时间
		ULARGE_INTEGER createTimeUL;
		ULARGE_INTEGER nowUL;
		ULARGE_INTEGER runTimeUL;
		createTimeUL.LowPart = createTime.dwLowDateTime;
		createTimeUL.HighPart = createTime.dwHighDateTime;
		GetSystemTimeAsFileTime((LPFILETIME)&nowUL);

		runTimeUL.QuadPart = nowUL.QuadPart - createTimeUL.QuadPart;

		// 转换为毫秒
		runTime = static_cast<DWORD>(runTimeUL.QuadPart / 10000);
	}

	CloseHandle(hProcess);
	return runTime;
}

// 获取当前系统进程名与PID
std::unordered_map<HANDLE, wstring> Util::GetAllProcessNameAndId()
{
	std::unordered_map<HANDLE, wstring> processNameAndId;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
	{
		return processNameAndId;
	}

	PROCESSENTRY32 pro_entry{ sizeof(PROCESSENTRY32) };
	if (Process32First(hSnap, &pro_entry))
	{
		do
		{
			processNameAndId.insert({ (HANDLE)pro_entry.th32ProcessID,pro_entry.szExeFile });
		} while (Process32Next(hSnap, &pro_entry));
	}

	CloseHandle(hSnap);
	return processNameAndId;
}

// 获取当前进程名
wstring Util::GetProcessNameByProcessId(HANDLE pid)
{
	auto tmpMap = Util::GetAllProcessNameAndId();
	if (tmpMap.find(pid) != tmpMap.end())
	{
		return tmpMap.find(pid)->second;
	}
	return wstring(L"");
}
