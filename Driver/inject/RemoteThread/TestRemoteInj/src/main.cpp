#include "RemoteThreadInject.h"
#include "../../../../../common/logger.cpp"

int wmain(int argc, LPWSTR * argv)
{
    // usage Hooker.exe <processname.exe> <C:\\Path\\To\\DLL.dll>
    init_logger("logs.log");

    if (argc != 3)
    {
        std::wcout << TEXT("USAGE: Hooker.exe <processname.exe> <C:\\Path\\To\\DLL.dll>") << std::endl;
        return 0;
    }

    WCHAR * DllPath = argv[2];
    WCHAR * ProcessName = argv[1];

    std::shared_ptr<RemoteThreadInject> obj = std::make_shared<RemoteThreadInject>(ProcessName, DllPath);
    obj->Initialize();
    obj->InjectDll();

    getchar();
    //obj->EjectDll();

    return 0;
}