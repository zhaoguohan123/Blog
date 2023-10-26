#include "Common.h"
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
std::shared_ptr<ControlDrv> g_controlDrv;

#define MAX_LOADSTRING 100 // 当前实例
WCHAR szTitle[MAX_LOADSTRING] = TEXT("TroilaKbdWnd");                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING] = TEXT("TroilaKbdWnd");            // 主窗口类名

//   窗口回调.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_MOVE:
        {
        MyDebugPrintA("WM_CUSTOM_MESSAGE");
            switch(DWORD(wParam))
            {
                case WND_INSTALL_DRIVER:
                    g_controlDrv->InstallDriver();
                    MyDebugPrintA("recv WND_INSTALL_DRIVER ");
                    break;

                case WND_DELETE_DRIVER:
                    g_controlDrv->DeleteDriver();
                    MyDebugPrintA("recv WND_DELETE_DRIVER ");
                    break;

                case WND_START_DRIVER:
                    g_controlDrv->StartDriver();
                    MyDebugPrintA("recv WND_START_DRIVER ");
                    break;

                case WND_STOP_DRIVER:
                    g_controlDrv->StopDriver();
                    MyDebugPrintA("recv WND_STOP_DRIVER ");
                    break;

                case WND_NOTIFY_INIT:
                    g_controlDrv->Init();
                    MyDebugPrintA("recv WND_NOTIFY_INIT ");
                    break;
                case WND_NOTIFY_CREATE_EVENT:
                    MyDebugPrintA("recv WND_NOTIFY_CREATE_EVENT ");
                    g_controlDrv->SendCmdToDrv(IO_CREATE_CAD_EVENT);
                    break;
                case WND_NOTIFY_DISABLE_CAD:
                    g_controlDrv->SendCmdToDrv(IO_DISABLE_CAD);
                    MyDebugPrintA("recv WND_NOTIFY_DISABLE_CAD ");
                    break;

                case WND_NOTIFY_ENABLE_CAD:
                    g_controlDrv->SendCmdToDrv(IO_ENABLE_CAD);
                    MyDebugPrintA("recv WND_NOTIFY_ENABLE_CAD ");
                    break;

                case WND_NOTIFY_DISABLE_KEYBOARD_MOUSE:
                    g_controlDrv->SendCmdToDrv(IO_DISABLE_KEYBOARD_MOUSE);
                    MyDebugPrintA("recv WND_NOTIFY_DISABLE_KEYBOARD_MOUSE");
                    break;

                case WND_NOTIFY_ENABLE_KEYBOARD_MOUSE:
                    g_controlDrv->SendCmdToDrv(IO_ENABLE_KEYBOARD_MOUSE);
                    MyDebugPrintA("recv WND_NOTIFY_ENABLE_KEYBOARD_MOUSE ");
                    break;
                case WND_NOTIFY_ENABLE_BLACK_SCREEN:
                    SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2); 
                    MyDebugPrintA("recv WND_NOTIFY_ENABLE_BLACK_SCREEN ");
                    break;
                    
                case WND_NOTIFY_DISABLE_BLACK_SCREEN:
                    SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, -1);
                    mouse_event(MOUSEEVENTF_MOVE, 0, 1, 0, NULL);
                    Sleep(40);
                    mouse_event(MOUSEEVENTF_MOVE, 0, -1, 0, NULL);
                    MyDebugPrintA("recv WND_NOTIFY_DISABLE_BLACK_SCREEN ");
                    break;
                default:
                    break;
            }
            break;
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

enum class WindowsVersion
{
    XP,
    VISTA,
    WIN7,
    WIN8,
    WIN8_1,
    WIN10,
    OTHER
};

WindowsVersion GetWindowsVersion()
{
    typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
    HINSTANCE hinst = LoadLibrary(TEXT("ntdll.dll"));
    if (!hinst)
    {
        printf("LoadLibrary ntdll.dll failed,err_code=%u", GetLastError());
        return WindowsVersion::OTHER;
    }

    NTPROC GetNtVersionNumbers = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
    DWORD major = 0;
    DWORD minor = 0;
    DWORD build_number = 0;
    GetNtVersionNumbers(&major, &minor, &build_number);

    WindowsVersion version = WindowsVersion::OTHER;
    if (major == 10 && minor == 0)
    {
        version = WindowsVersion::WIN10;
    }
    else if (major == 6 && minor == 3)
    {
        version = WindowsVersion::WIN8_1;
    }
    else if (major == 6 && minor == 2)
    {
        version = WindowsVersion::WIN8;
    }
    else if (major == 6 && minor == 1)
    {
        version = WindowsVersion::WIN7;
    }
    else if (major == 6 && minor == 0)
    {
        version = WindowsVersion::VISTA;
    }
    else if (major == 5 && minor == 1)
    {
        version = WindowsVersion::XP;
    }

    return version;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    char buffer[MAX_PATH];
    DWORD result = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (result == 0) {
        MyDebugPrintA("Failed to retrieve the executable path. errcode = %u", GetLastError());
        return 1;
    }
    
    std::string executable_path(buffer);

    // 使用 std::experimental::filesystem 获取可执行程序所在的目录
    std::experimental::filesystem::path path(executable_path);
    std::experimental::filesystem::path executable_directory = path.parent_path();

    LPCTSTR lpszDriverPath;
    WindowsVersion version = GetWindowsVersion();

    std::string sys_drv = executable_directory.string();

    if (WindowsVersion::WIN10 == version)
    {
        sys_drv = sys_drv + "\\poc\\win10\\Poc.sys";
    }
    else if(WindowsVersion::WIN7 == version)
    {
        sys_drv = sys_drv + "\\poc\\win7\\Poc.sys";
    }
    else
    {
        printf("not support version:%d", version);
        return 0;
    }
    MyDebugPrintA("sys path is %s", sys_drv.c_str());

    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring w_sys_drv = converter.from_bytes(sys_drv);
    lpszDriverPath = w_sys_drv.c_str();

    LPCTSTR lpszDriverName = TEXT("Troila_kbdflt_serv");
    LPCTSTR lpszAltitude = TEXT("389992");
    g_controlDrv = std::make_shared<ControlDrv>(lpszDriverName, lpszDriverPath, lpszAltitude);

    WNDCLASS wcex = { 0 };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;                          //设置回调
    wcex.hInstance = hInstance;                         //设置模块实例句柄
    wcex.lpszClassName = szWindowClass;                //设置类名

    //2.注册窗口类
    BOOL bRet = RegisterClass(&wcex);
    if (bRet == FALSE)
    {
        MyDebugPrintA("RegisterClass falied! errcode = %u", GetLastError());
        return 0;
    }

    //3.创建窗口 并且显示跟更新窗口
    HWND hWnd = CreateWindowW(
        szWindowClass,                 //类名
        szTitle,                       //窗口名称
        WS_OVERLAPPEDWINDOW,           //窗口的创建样式
        CW_USEDEFAULT,
        0,
        CW_USEDEFAULT,
        0,
        nullptr,
        nullptr,
        hInstance,                    //实例句柄
        nullptr);
    if (!hWnd)
    {
        return FALSE;
    }

    //4.消息循环.
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}