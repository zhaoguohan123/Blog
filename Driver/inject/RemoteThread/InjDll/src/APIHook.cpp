#include "APIHook.h"


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                        )
{

    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            MessageBoxA(NULL, "Hello, World!", "Test", MB_OK);
            break;
        }
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            MessageBoxA(NULL, "Hello, World!xxxxxxxxxx", "Test", MB_OK);
            break;
        }
        default:
            break;
    }
    return TRUE;
}
