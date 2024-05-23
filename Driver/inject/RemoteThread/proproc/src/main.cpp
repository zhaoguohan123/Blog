#include "main.h"

LONG s_nTlsIndent = -1;
LONG s_nTlsThread = -1;
LONG s_nThreadCnt = 0;

static HMODULE  s_hInst = NULL;
static WCHAR    s_wzDllPath[MAX_PATH];


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  Reason,
                       LPVOID lpReserved
                     )
{

    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        //先恢复状态
        DetourRestoreAfterWith();
        OnProcessAttach(hModule);
        break;
    case DLL_PROCESS_DETACH:
        ProcessDetach(hModule);
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
        return TRUE;
}

#pragma region MyRegion 
BOOL (WINAPI * pTerminateProcess)(HANDLE hProcess, UINT uExitCode) = TerminateProcess;
BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode);
BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode)
{
    char proc_name[MAX_PATH] = {0};
    if (GetProcessImageFileNameA(hProcess, proc_name, MAX_PATH))
    {
        std::string proc_name_ = proc_name;
        if (proc_name_.find("RemoteInjector.exe") != std::string::npos) 
        {
            return 0;
        } 
    }

    return pTerminateProcess(hProcess, uExitCode);
}
#pragma endregion

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

LONG AttachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    ATTACH(&(PVOID&)pTerminateProcess, MyTerminateProcess);

    return DetourTransactionCommit();
}


LONG DetachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DETACH(&(PVOID&)pTerminateProcess, MyTerminateProcess);

    return DetourTransactionCommit();
}

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
