// #include <iostream>
// #include<Windows.h>
// #include<TlHelp32.h>

// using namespace std;

// void ShowError(const TCHAR* pszText)
// {
//     TCHAR szError[MAX_PATH] = { 0 };
//     ::wsprintf(szError, L"%s Error[%d]\n", pszText, ::GetLastError());
//     ::MessageBox(NULL, szError, L"ERROR", MB_OK);
// }

// //列出指定进程的所有线程
// BOOL GetProcessThreadList(DWORD th32ProcessID, DWORD** ppThreadIdList, LPDWORD pThreadIdListLength)
// {
//     // 申请空间
//     DWORD dwThreadIdListLength = 0;
//     DWORD dwThreadIdListMaxCount = 2000;
//     LPDWORD pThreadIdList = NULL;
//     HANDLE hThreadSnap = INVALID_HANDLE_VALUE;

//     pThreadIdList = (LPDWORD)VirtualAlloc(NULL, dwThreadIdListMaxCount * sizeof(DWORD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//     if (pThreadIdList == NULL)
//     {
//         return FALSE;
//     }
//     RtlZeroMemory(pThreadIdList, dwThreadIdListMaxCount * sizeof(DWORD));
//     THREADENTRY32 th32 = { 0 };
//     // 拍摄快照
//     hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, th32ProcessID);
//     if (hThreadSnap == INVALID_HANDLE_VALUE)
//     {
//         return FALSE;
//     }

//     // 结构的大小
//     th32.dwSize = sizeof(THREADENTRY32);

//     //遍历所有THREADENTRY32结构, 按顺序填入数组
//     BOOL bRet = Thread32First(hThreadSnap, &th32);
//     while (bRet)
//     {
//         if (th32.th32OwnerProcessID == th32ProcessID)
//         {
//             if (dwThreadIdListLength >= dwThreadIdListMaxCount)
//             {
//                 break;
//             }
//             pThreadIdList[dwThreadIdListLength++] = th32.th32ThreadID;
//         }
//         bRet = Thread32Next(hThreadSnap, &th32);
//     }

//     *pThreadIdListLength = dwThreadIdListLength;
//     *ppThreadIdList = pThreadIdList;

//     return TRUE;
// }

// BOOL APCInject(HANDLE hProcess, CHAR* wzDllFullPath, LPDWORD pThreadIdList, DWORD dwThreadIdListLength)
// {
//     // 申请内存
//     PVOID lpAddr = NULL;
//     SIZE_T page_size = 4096;

//     lpAddr = ::VirtualAllocEx(hProcess, nullptr, page_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
//     if (lpAddr == NULL)
//     {
//         ShowError(L"VirtualAllocEx - Error\n\n");
//         VirtualFreeEx(hProcess, lpAddr, page_size, MEM_DECOMMIT);
//         CloseHandle(hProcess);
//         return FALSE;
//     }
//     // 把Dll的路径复制到内存中
//     if (FALSE == ::WriteProcessMemory(hProcess, lpAddr, wzDllFullPath, (strlen(wzDllFullPath) + 1) * sizeof(wzDllFullPath), nullptr))
//     {
//         ShowError(L"WriteProcessMemory - Error\n\n");
//         VirtualFreeEx(hProcess, lpAddr, page_size, MEM_DECOMMIT);
//         CloseHandle(hProcess);
//         return FALSE;
//     }


//     // 获得LoadLibraryA的地址
//     PVOID loadLibraryAddress = ::GetProcAddress(::GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
//     if (loadLibraryAddress == NULL)
//     {
//         ShowError(L"GetProcAddress - Error\n\n");
//         VirtualFreeEx(hProcess, lpAddr, page_size, MEM_DECOMMIT);
//         CloseHandle(hProcess);
//         return FALSE;
//     }
    

//     // 遍历线程, 插入APC
//     float fail = 0;
//     for (int i = dwThreadIdListLength - 1; i >= 0; i--)
//     {
//         // 打开线程
//         HANDLE hThread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, pThreadIdList[i]);
//         if (hThread)
//         {
//             // 插入APC
//             if (!::QueueUserAPC((PAPCFUNC)loadLibraryAddress, hThread, (ULONG_PTR)lpAddr))
//             {
//                 fail++;
//             }
//             // 关闭线程句柄
//             ::CloseHandle(hThread);
//             hThread = NULL;
//         }
//     }


//     printf("Total Thread: %d\n", dwThreadIdListLength);
//     printf("Total Failed: %d\n", (int)fail);

//     if ((int)fail == 0 || dwThreadIdListLength / fail > 0.5)
//     {
//         printf("Success to Inject APC\n");
//         return TRUE;
//     }
//     else
//     {
//         printf("Inject may be failed\n");
//         return FALSE;
//     }
// }

// int main()
// {
//     ULONG32 ulProcessID = 0;
//     printf("Input the Process ID:");
//     cin >> ulProcessID;
//     CHAR wzDllFullPath[MAX_PATH] = { 0 };
//     LPDWORD pThreadIdList = NULL;
//     DWORD dwThreadIdListLength = 0;

// #ifndef _WIN64
//     strcpy_s(wzDllFullPath, "加载要注入的dll的路径");
// #else // _WIN64
//     strcpy_s(wzDllFullPath, "C:\\Users\\zgh\\Desktop\\TestDll.dll");
// #endif
//     if (!GetProcessThreadList(ulProcessID, &pThreadIdList, &dwThreadIdListLength))
//     {
//         printf("Can not list the threads\n");
//         goto end;
//     }
//     //打开句柄
//     HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, ulProcessID);


//     if (hProcess == NULL)
//     {
//         printf("Failed to open Process\n");
//          goto end;
//     }


//     //注入
//     if (!APCInject(hProcess, wzDllFullPath, pThreadIdList, dwThreadIdListLength))
//     {
//         printf("Failed to inject DLL\n");
//         goto end;
//     }

// end:
//     system("pause");
//     return 0;
// }

#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <TlHelp32.h>

DWORD ProcessInfoo(LPWSTR PName)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 Process;
	Process.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First( snap , &Process))
	{
		while (Process32Next(snap , &Process))
		{
			if (wcsncmp(PName, Process.szExeFile, lstrlen(PName)) == 0)
			{
				std::wcout << TEXT("Found!") << std::endl;
				CloseHandle(snap);
				return Process.th32ProcessID;
			}
		}
	}
	CloseHandle(snap);
	return (EXIT_FAILURE);
}

DWORD GetThreadInfoo(DWORD PID)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	THREADENTRY32 thread;
	thread.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(snap, &thread))
	{
		while (Thread32Next(snap, &thread))
		{
			if (thread.th32OwnerProcessID == PID)
			{
				CloseHandle(snap);
				return thread.th32ThreadID;
			}
		}
	}

	CloseHandle(snap);
	return (EXIT_FAILURE);
}

int wmain(int argc, LPWSTR * argv)
{
	// usage Hooker.exe <processname.exe> <C:\\Path\\To\\DLL.dll>

	if (argc != 3)
	{
		std::wcout << TEXT("USAGE: Hooker.exe <processname.exe> <C:\\Path\\To\\DLL.dll>") << std::endl;
		ExitProcess(1);
	}

	WCHAR * DllPath = argv[2];
	
	DWORD dwSize = ( lstrlenW(DllPath) + 1 ) * sizeof(wchar_t);
	DWORD PID = ProcessInfoo(argv[1]);
	DWORD TID = GetThreadInfoo(PID);

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, TID);
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, PID);

	if (hThread != NULL)
	{
		LPVOID FAddr = VirtualAllocEx(hProcess, NULL, 1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (FAddr != NULL)
		{
			if (WriteProcessMemory(hProcess, FAddr, DllPath, dwSize, NULL))
			{
				LPTHREAD_START_ROUTINE ProcH = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "LoadLibraryW");
				if (ProcH != NULL)
				{
					QueueUserAPC((PAPCFUNC)ProcH, hThread,(ULONG_PTR)FAddr);
				}
				std::cout << "Funcionou!" << std::endl;
			}
		}
	}
	else
	{
		std::cout << "Nao Funcionou!" << std::endl;
	}

	CloseHandle(hThread);
	CloseHandle(hProcess);
	return (EXIT_SUCCESS);
}