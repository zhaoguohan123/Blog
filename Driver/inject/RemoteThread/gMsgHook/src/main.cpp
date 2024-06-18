#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <tchar.h>
#include <psapi.h>
#include <winuser.h>
#include "detours.h"
#include "SingletonApplication.h"

#define HIDE_PROCESS_NAME_1 L"KLDELearnApp.exe"
#define HIDE_PROCESS_NAME_2 L"KLDELearnDaemon.exe"
VOID DetAttach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourAttach(ppbReal, pbMine);
    if (l != 0)
    {
    }
}

VOID DetDetach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourDetach(ppbReal, pbMine);
    if (l != 0)
    {
    }
}

#define ATTACH(x,y)   DetAttach(x,y,#x)
#define DETACH(x,y)   DetDetach(x,y,#x)

// 指定全局变量
HHOOK global_Hook;
BOOL  g_flage = FALSE;
LONG s_nTlsIndent = -1;
LONG s_nTlsThread = -1;
LONG s_nThreadCnt = 0;
static HMODULE  s_hInst = NULL;
static WCHAR    s_wzDllPath[MAX_PATH];
CSingletonApplication g_SingletonApplication;

#pragma region MyRegion 
typedef struct _VM_COUNTERS
{
    SIZE_T        PeakVirtualSize;
    SIZE_T        VirtualSize;
    ULONG         PageFaultCount;
    SIZE_T        PeakWorkingSetSize;
    SIZE_T        WorkingSetSize;
    SIZE_T        QuotaPeakPagedPoolUsage;
    SIZE_T        QuotaPagedPoolUsage;
    SIZE_T        QuotaPeakNonPagedPoolUsage;
    SIZE_T        QuotaNonPagedPoolUsage;
    SIZE_T        PagefileUsage;
    SIZE_T        PeakPagefileUsage;
} VM_COUNTERS;


typedef NTSTATUS(WINAPI* __NtQuerySystemInformation)(
    _In_       SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _In_ _Out_ PVOID SystemInformation,
    _In_       ULONG SystemInformationLength,
    _Out_opt_  PULONG ReturnLength
    );

typedef struct _MY_UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} MY_UNICODE_STRING, * PMY_UNICODE_STRING;

typedef struct _MY_SYSTEM_PROCESS_INFORMATION
{
    ULONG            NextEntryOffset; // 指向下一个结构体的指针
    ULONG            ThreadCount; // 本进程的总线程数
    ULONG            Reserved1[6]; // 保留
    LARGE_INTEGER    CreateTime; // 进程的创建时间
    LARGE_INTEGER    UserTime; // 在用户层的使用时间
    LARGE_INTEGER    KernelTime; // 在内核层的使用时间
    MY_UNICODE_STRING   ImageName; // 进程名
    KPRIORITY        BasePriority; // 
    ULONG            ProcessId; // 进程ID
    ULONG            InheritedFromProcessId;
    ULONG            HandleCount; // 进程的句柄总数
    ULONG            Reserved2[2]; // 保留
    VM_COUNTERS      VmCounters;
    IO_COUNTERS      IoCounters;
    SYSTEM_THREAD_INFORMATION Threads[5]; // 子线程信息数组
}MY_SYSTEM_PROCESS_INFORMATION, * PMY_SYSTEM_PROCESS_INFORMATION;



PVOID pNtQuerySystemInformation = DetourFindFunction("ntdll.dll","NtQuerySystemInformation");
NTSTATUS WINAPI MyNtQuerySystemInformation(
    _In_       SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _In_ _Out_ PVOID SystemInformation,
    _In_       ULONG SystemInformationLength,
    _Out_opt_  PULONG ReturnLength
) 
{
    NTSTATUS Result;
    PSYSTEM_PROCESS_INFORMATION pSystemProcess;
    PSYSTEM_PROCESS_INFORMATION pNextSystemProcess;

    Result = ((__NtQuerySystemInformation)pNtQuerySystemInformation)(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

    if (NT_SUCCESS(Result) && SystemInformationClass == SystemProcessInformation)
    {
        pSystemProcess = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;
        pNextSystemProcess = (PSYSTEM_PROCESS_INFORMATION)((PBYTE)pSystemProcess + pSystemProcess->NextEntryOffset);

        while (pNextSystemProcess->NextEntryOffset != 0)
        {
            if (lstrcmpW((&pNextSystemProcess->ImageName)->Buffer, HIDE_PROCESS_NAME_1) == 0) 
            {
                pSystemProcess->NextEntryOffset += pNextSystemProcess->NextEntryOffset;
            }
            if (lstrcmpW((&pNextSystemProcess->ImageName)->Buffer, HIDE_PROCESS_NAME_2) == 0) 
            {
                pSystemProcess->NextEntryOffset += pNextSystemProcess->NextEntryOffset;
            }
            pSystemProcess = pNextSystemProcess;
            pNextSystemProcess = (PSYSTEM_PROCESS_INFORMATION)((PBYTE)pSystemProcess + pSystemProcess->NextEntryOffset);
        }
    }

    return Result;
}
#pragma endregion

// 判断是否是需要注入的进程
BOOL GetFristModuleName(DWORD Pid, LPCTSTR ExeName)
{
  MODULEENTRY32 me32 = { 0 };
  me32.dwSize = sizeof(MODULEENTRY32);
  HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, Pid);

  if (INVALID_HANDLE_VALUE != hModuleSnap)
  {
    // 先拿到自身进程名称
    BOOL bRet = Module32First(hModuleSnap, &me32);

    if (!_tcsicmp(ExeName, (LPCTSTR)me32.szModule))
    {
      CloseHandle(hModuleSnap);
      return TRUE;
    }
    CloseHandle(hModuleSnap);
    return FALSE;
  }
  CloseHandle(hModuleSnap);
  return FALSE;
}

// 设置全局消息回调函数
LRESULT CALLBACK MyProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  return CallNextHookEx(global_Hook, nCode, wParam, lParam);
}

// 安装全局钩子
extern "C" __declspec(dllexport) void SetHook()
{
    if (!g_SingletonApplication.HasPreviousInstance())
    {
        g_SingletonApplication.RegisterCurrentInstance();
        global_Hook = SetWindowsHookEx(WH_CBT, MyProc, GetModuleHandleA("gMsgHook.dll"), 0);
    }
}

// 卸载全局钩子
extern "C" __declspec(dllexport) void UnHook()
{
  if (global_Hook)
  {
    UnhookWindowsHookEx(global_Hook);
  }
}

LONG AttachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    ATTACH(&(PVOID&)pNtQuerySystemInformation, MyNtQuerySystemInformation);
    return DetourTransactionCommit();
}

BOOL ThreadAttach(HMODULE hDll)
{
    (void)hDll;

    if (s_nTlsIndent >= 0)
    {
        TlsSetValue(s_nTlsIndent, (PVOID)0);
    }
    if (s_nTlsThread >= 0)
    {
        LONG nThread = InterlockedIncrement(&s_nThreadCnt);
        TlsSetValue(s_nTlsThread, (PVOID)(LONG_PTR)nThread);
    }
    return TRUE;
}

BOOL OnProcessAttach(HMODULE ModuleHandle)
{
    s_nTlsIndent = TlsAlloc();
    s_nTlsThread = TlsAlloc();

    WCHAR wzExeName[MAX_PATH];
    s_hInst = ModuleHandle;

    GetModuleFileNameW(ModuleHandle, s_wzDllPath, ARRAYSIZE(s_wzDllPath));
    GetModuleFileNameW(NULL, wzExeName, ARRAYSIZE(wzExeName));

    //加载所有拦截函数内容
    LONG error = AttachDetours();
    if (error != NO_ERROR)
    {
        return FALSE;
    }

    ThreadAttach(ModuleHandle);

    return TRUE;
}

BOOL ThreadDetach(HMODULE hDll)
{
    (void)hDll;

    if (s_nTlsIndent >= 0)
    {
        TlsSetValue(s_nTlsIndent, (PVOID)0);
    }
    if (s_nTlsThread >= 0)
    {
        TlsSetValue(s_nTlsThread, (PVOID)0);
    }
    return TRUE;
}

LONG DetachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DETACH(&(PVOID&)pNtQuerySystemInformation, MyNtQuerySystemInformation);
    return DetourTransactionCommit();
}

BOOL ProcessDetach(HMODULE hDll)
{
    ThreadDetach(hDll);

    LONG error = DetachDetours();
    if (error != NO_ERROR)
    {
    }

    if (s_nTlsIndent >= 0)
    {
        TlsFree(s_nTlsIndent);
    }
    if (s_nTlsThread >= 0)
    {
        TlsFree(s_nTlsThread);
    }
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
    {
      // 只hook任务管理中的API
      g_flage = GetFristModuleName(GetCurrentProcessId(), TEXT("Taskmgr.exe"));
      if (g_flage == TRUE)
      {
        DetourRestoreAfterWith();
        OnProcessAttach(hModule);
      }
      break;
    }
    case DLL_THREAD_ATTACH:
    {
      break;
    }
    case DLL_THREAD_DETACH:
    {
      break;
    }
    case DLL_PROCESS_DETACH:
    {
      // DLL卸载时自动清理
      if (g_flage == TRUE)
      {
        ProcessDetach(hModule);
      }
      UnHook();
      break;
    }
    default:
      break;
  }
  return TRUE;
}
