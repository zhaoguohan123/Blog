#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <psapi.h>
#include <winuser.h>
#include "detours.h"
#include "SingletonApplication.h"


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

std::string GetParentProcessName(HANDLE hProcess);

#pragma region MyRegion 
BOOL (WINAPI * pTerminateProcess)(HANDLE hProcess, UINT uExitCode) = TerminateProcess;
BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode);
BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode)
{
    char proc_name[MAX_PATH] = {0};
    std::string proc_parent_name_;
    if (GetProcessImageFileNameA(hProcess, proc_name, MAX_PATH))
    {
        std::string proc_name_ = proc_name;
        if (proc_name_.find("KLDELearnApp.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            hProcess = NULL;
        }
        if (proc_name_.find("KLDELearnDaemon.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            hProcess = NULL;
        }
        if (proc_name_.find("test_wm_close.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            hProcess = NULL;
        }
    }
    proc_parent_name_ = GetParentProcessName(hProcess);
    if (proc_parent_name_.size() > 0)
    {
        if (proc_parent_name_.find("KLDELearnApp.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            return TRUE;
        }
        if (proc_parent_name_.find("KLDELearnDaemon.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            return TRUE;
        }
        if (proc_parent_name_.find("test_wm_close.exe") != std::string::npos)    // 防止用任务管理器关闭的进程名
        {
            return TRUE;
        }
    }

    return pTerminateProcess(hProcess, uExitCode);
}
#pragma endregion

// 获取进程名的函数
std::string GetProcessNameById(DWORD processId) {
    std::string processName = "";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            char szProcessName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR))) {
                processName = szProcessName;
            }
        }
        CloseHandle(hProcess);
    }
    return processName;
}

std::string GetParentProcessName(HANDLE hProcess) {
    DWORD dwProcessId = GetProcessId(hProcess);
    if (dwProcessId == 0) {
        std::cerr << "Failed to get process ID!" << std::endl;
        return "";
    }

    // 创建进程快照
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create snapshot!" << std::endl;
        return "";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // 遍历进程列表
    if (Process32First(hSnapshot, &pe32)) {
        do {
            // 找到目标进程的进程信息
            if (pe32.th32ProcessID == dwProcessId) {
                // 获取父进程的进程 ID
                DWORD dwParentProcessId = pe32.th32ParentProcessID;

                // 根据父进程的进程 ID 查找父进程名
                HANDLE hParentProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwParentProcessId);
                if (hParentProcess != NULL) {
                    char szParentName[MAX_PATH];
                    DWORD dwSize = MAX_PATH;
                    if (QueryFullProcessImageNameA(hParentProcess, 0, szParentName, &dwSize)) {
                        CloseHandle(hSnapshot);
                        CloseHandle(hParentProcess);
                        return szParentName;
                    }
                    CloseHandle(hParentProcess);
                }
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return "";
}

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

    ATTACH(&(PVOID&)pTerminateProcess, MyTerminateProcess);

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

    DETACH(&(PVOID&)pTerminateProcess, MyTerminateProcess);

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
