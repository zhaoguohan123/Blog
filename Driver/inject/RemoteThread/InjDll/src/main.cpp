#include "common.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                        )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            // //1. ��ʼ��,�����߳�
            // DetourTransactionBegin();
            // DetourUpdateThread(GetCurrentThread());

            // //2. �滻windows��API
            // DetourAttach(&(PVOID&)OldMsgBoxW, MyMessageBoxW);

            // //3. �ָ����滻��API�����߳�ת������̬
            // DetourTransactionCommit();
            break;
        }
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            //UnInstallApiHook();
            break;
        }
        default:
            break;
    }
    return TRUE;
}
