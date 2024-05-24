#include <iostream>
#include <windows.h>

int main(int argc, char* argv[])
{
    HMODULE hMod = LoadLibrary(TEXT("C:\\Users\\zgh\\Desktop\\work\\Blog\\Driver\\inject\\RemoteThread\\bin\\gMsgHook.dll"));  // 这里是dll的绝对地址
    if (hMod == NULL)
    {
        std::cout << "LoadLibrary() failed: " << GetLastError() <<std::endl;
        return 0;
    }
    
    // 安装钩子
    typedef void(*pSetHook)(void);
    pSetHook SetHook = (pSetHook)GetProcAddress(hMod, "SetHook");
    if (SetHook == NULL)
    {
        std::cout << "GetProcAddress() failed: " << GetLastError() <<std::endl;
        FreeLibrary(hMod);
        return 0;
    }
    
    SetHook();

    getchar();

    // 卸载钩子
    typedef BOOL(*pUnSetHook)(HHOOK);
    pUnSetHook UnsetHook = (pUnSetHook)GetProcAddress(hMod, "UnHook");
    if (UnsetHook == NULL)
    {
        std::cout << "GetProcAddress() failed: " << GetLastError() <<std::endl;
        FreeLibrary(hMod);
        return 0;
    }
    
    pUnSetHook();
    if (hMod != NULL)
    {
        (void)FreeLibrary(hMod);
        hMod = NULL;
    }
   
    return 0;
}