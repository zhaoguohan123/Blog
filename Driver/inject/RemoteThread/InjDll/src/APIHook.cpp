#include "APIHook.h"

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
        ThreadAttach(hModule);
        break;
    case DLL_THREAD_DETACH:
        ThreadDetach(hModule);
        break;
    }
        return TRUE;
}

void TestMessageBox()
{
    MessageBoxA(NULL, "Inject", "Inject ok!", MB_OK);
}


#pragma region MyRegion 
BOOL (WINAPI *pTrueSystemParametersInfoW)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) = SystemParametersInfoW;
BOOL WINAPI MySystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
BOOL WINAPI MySystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni)
{
    if (uiAction == SPI_SETDRAGFULLWINDOWS)
    {
        uiParam = 0;
    }
    return pTrueSystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
}

BOOL (WINAPI *pTrueSystemParametersInfoA)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) = SystemParametersInfoA;
BOOL WINAPI MySystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
BOOL WINAPI MySystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni)
{
    if (uiAction == SPI_SETDRAGFULLWINDOWS) 
    {
        uiParam = 0;
    }
    return pTrueSystemParametersInfoA(uiAction, uiParam, pvParam, fWinIni);
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

    }

    ThreadAttach(ModuleHandle);

    return TRUE;
}

LONG AttachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    ATTACH(&(PVOID&)pTrueSystemParametersInfoW, MySystemParametersInfoW);
    ATTACH(&(PVOID&)pTrueSystemParametersInfoA, MySystemParametersInfoA);

    return DetourTransactionCommit();
}


LONG DetachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DETACH(&(PVOID&)pTrueSystemParametersInfoW, MySystemParametersInfoW);
    DETACH(&(PVOID&)pTrueSystemParametersInfoA, MySystemParametersInfoA);

    return DetourTransactionCommit();
}

VOID DetAttach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourAttach(ppbReal, pbMine);
    if (l != 0)
    {
        //Syelog(SYELOG_SEVERITY_NOTICE,               "Attach failed: `%s': error %d\n", DetRealName(psz), l);
    }
}

VOID DetDetach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourDetach(ppbReal, pbMine);
    if (l != 0)
    {
        //Syelog(SYELOG_SEVERITY_NOTICE,               "Detach failed: `%s': error %d\n", DetRealName(psz), l);
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
