#include "testdll.h"
#include <windows.h>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  Reason,
                       LPVOID lpReserved
                     )
{

    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        MessageBoxA(NULL, "process inject Hook", "Hook", MB_OK);
        break;	
    case DLL_PROCESS_DETACH:
        MessageBoxA(NULL, "process de-inject Hook", "Hook", MB_OK);
        break;
    case DLL_THREAD_ATTACH:
        MessageBoxA(NULL, "thread inject Hook", "Hook", MB_OK);
        break;
    case DLL_THREAD_DETACH:
        MessageBoxA(NULL, "thread de-inject Hook", "Hook", MB_OK);
        break;
    }
        return TRUE;
}

void TestMessageBox()
{
    MessageBoxA(NULL, "Inject", "Inject ok!", MB_OK);
}