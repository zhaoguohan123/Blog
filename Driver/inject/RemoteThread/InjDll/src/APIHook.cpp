#include "APIHook.h"

#ifdef TESTMESSAGEBOX
    typedef BOOL (WINAPI *pfnMessageBox)(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
    pfnMessageBox g_pfnMessageBox = NULL;
    BOOL WINAPI MyMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
    {
        return g_pfnMessageBox(hWnd, L"Hooked", lpCaption, uType);
    }
#endif

BOOL HookFunction(PVOID* ppPointer, PVOID pDetour, const char* functionName) {
    auto error = DetourAttach(ppPointer, pDetour);
    if ( error != NO_ERROR) 
    {
        //STREAM_ERROR("Failed to hook " << functionName << ", error " << error);
        return FALSE;
    }
    return TRUE;
}
#define HOOKFUNC(n, m) if (!HookFunction(&(PVOID&)n, m, #n)) return;


BOOL UnhookFunction(PVOID* ppPointer, PVOID pDetour, const char* functionName) {
   auto error = DetourDetach(ppPointer, pDetour); 
    if (error != NO_ERROR) {
        //STREAM_ERROR("Failed to unhook " << functionName << ", error " << error);
        return FALSE;
    }
    return TRUE;
}
#define UNHOOKFUNC(n, m) if (!UnhookFunction(&(PVOID&)n, m, #n)) return;


void  InstallApiHook()
{
    #ifdef TESTMESSAGEBOX
        //MessageBoxW
        g_pfnMessageBox = (pfnMessageBox)GetProcAddress(GetModuleHandle(L"user32.dll"), "MessageBoxW");
        if (g_pfnMessageBox == NULL)
        {
            return;
        }
        HOOKFUNC(g_pfnMessageBox, MyMessageBox);
    #endif

}

void  UnInstallApiHook()
{
    #ifdef TESTMESSAGEBOX
    UNHOOKFUNC(g_pfnMessageBox, MyMessageBox);
    #endif
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                        )
{

    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            DetourRestoreAfterWith();
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            InstallApiHook();

            DetourTransactionCommit();
            break;
        }
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            UnInstallApiHook();

            DetourTransactionCommit();
            break;
        }
        default:
            break;
    }
    return TRUE;
}
