#include "RemoteThreadInject.h"
#include "../../../../../common/logger.cpp"

int wmain(int argc, LPWSTR * argv)
{
    // usage Hooker.exe <processname.exe> <C:\\Path\\To\\DLL.dll>
    
    init_logger("logs.log");

    WCHAR DllPath[MAX_PATH] = L"DisableHotKey.dll";
    WCHAR ProcessName[MAX_PATH] = L"winlogon.exe";

    std::shared_ptr<RemoteThreadInject> obj = std::make_shared<RemoteThreadInject>(ProcessName, DllPath);
    if  (FALSE == obj->Initialize())
    {
        return 0;
    }
    obj->InjectDll();

    getchar();
    obj->EjectDll();
    return 0;
}