// InstallDrv.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <Windows.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <strsafe.h>

void MyDebugPrintA(const char* format, ...)
{
    if (format == NULL)
    {
        return;
    }
    char buffer[1024 * 4] = { "[ZGHTEST]" };

    va_list ap;
    va_start(ap, format);
    (void)StringCchVPrintfA(buffer + 9, _countof(buffer) - 9, format, ap);
    va_end(ap);

    OutputDebugStringA(buffer);
}

enum WND_CONTROL_CODE
{
    WND_INSTALL_DRIVER,                             //安装驱动
    WND_DELETE_DRIVER,                              //卸载驱动
    WND_START_DRIVER,                               //打开驱动
    WND_STOP_DRIVER,                                //关闭驱动
    WND_NOTIFY_INIT,                                //通知与驱动交互的程序初始化
    WND_NOTIFY_CREATE_EVENT,                        //通知驱动创建同步事件
    WND_NOTIFY_DISABLE_CAD,                         //禁用Ctrl+alt+del
    WND_NOTIFY_ENABLE_CAD,                          //启用Ctrl+alt+del
    WND_NOTIFY_DISABLE_KEYBOARD_MOUSE,              //禁用键盘鼠标
    WND_NOTIFY_ENABLE_KEYBOARD_MOUSE,               //启用键盘鼠标
    WND_NOTIFY_ENABLE_BLACK_SCREEN,                 //黑屏
    WND_NOTIFY_DISABLE_BLACK_SCREEN                 //解除黑屏
};

#define NAME_CAD_EVENTA  "Global\\Troila_Kbd_fltr"
#define NAME_CAD_EVENTW  L"Global\\Troila_Kbd_fltr"

HANDLE cadEvent = NULL;
DWORD WaitCadEventThread(LPVOID param)
{
    printf("WaitCadEventThread begin... \n");
    while (TRUE)
    {
        DWORD dwWaitResult = WaitForSingleObject(cadEvent, INFINITE);
        if (dwWaitResult == WAIT_OBJECT_0)
        {
            MyDebugPrintA("------------------------------------------------ ctrl alt del was pressed! ------------------------------------------- \n");
            if (!ResetEvent(cadEvent))
            {
                MyDebugPrintA("reset event failed  %u", GetLastError());
            }
        }
        else
        {
            MyDebugPrintA("Wait failed with error code:%u", GetLastError());
        }

    }
    return 0;
}
int main()
{
    std::string user_input;
    HWND wnd = NULL;

    //创建同步事件
    SECURITY_DESCRIPTOR secutityDese;
    ::InitializeSecurityDescriptor(&secutityDese, SECURITY_DESCRIPTOR_REVISION);
    ::SetSecurityDescriptorDacl(&secutityDese, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES securityAttr;
    securityAttr.nLength = sizeof SECURITY_ATTRIBUTES;
    securityAttr.bInheritHandle = FALSE;
    securityAttr.lpSecurityDescriptor = &secutityDese;

    cadEvent = ::CreateEvent(&securityAttr, TRUE, FALSE, NAME_CAD_EVENTW); // 这个事件是用户层创建的
    if (cadEvent == NULL)
    {
        std::cout << "CreateEvent Troila_Kbd_fltr failed!, err:" << GetLastError() << std::endl;
        return 0;
    }

    // 创建事件等待驱动触发事件
    HANDLE waitCadEventThread_ = CreateThread(NULL, 0, WaitCadEventThread, NULL, 0, NULL);
    if (waitCadEventThread_ == NULL)
    {
        printf("create WaitCadEventThread failed!");
        return 0;
    }

    // 每隔一秒循环查找TroilaKbdWnd窗口
    do
    {
        wnd = FindWindowA("TroilaKbdWnd", NULL);
        if (wnd == NULL)
        {
            printf("wnd is NULL! errcode = %u \n", GetLastError());
        }
        else
        {
            printf("TroilaKbdWnd was found! \n");
            break;
        }
        Sleep(1000);
    } while (TRUE);


    while (true)
    {
        if (wnd == NULL)
        {
            return 0;
        }
        std::cout << "输入命令:" << std::endl;
        std::cout << "install:  安装驱动   delete: 删除驱动     start:  启动驱动   stop : 停止驱动" << std::endl;
        std::cout << "init:   驱动初始化   event: 创建同步事件  discad: 禁用cad    encad : 启动cad     disable : 禁用键盘鼠标 30s后启用鼠标键盘  silence: 黑屏肃静" << std::endl;

        std::cin >> user_input;

        if (user_input == "install")
        {
            LRESULT ret =  SendMessageA(wnd, WM_MOVE, WND_INSTALL_DRIVER, NULL);
            MyDebugPrintA("ret:%u, err %u", ret, GetLastError());
        }

        if (user_input == "delete")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_DELETE_DRIVER, NULL);
        }
        
        if (user_input == "start")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_START_DRIVER, NULL);
        }
        
        if (user_input == "stop")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_STOP_DRIVER, NULL);
        }

        if (user_input == "event")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_CREATE_EVENT, NULL);
        }
        if (user_input == "init")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_INIT, NULL);
        }

        if (user_input == "discad")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_DISABLE_CAD, NULL);
        }

        if (user_input == "encad")
        {
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_ENABLE_CAD, NULL);
        }

        if (user_input == "disable")
        {
            printf("禁用鼠标键盘 30s 后自动解锁 \n");
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_DISABLE_KEYBOARD_MOUSE, NULL);
            Sleep(30000);
            printf("启用鼠标键盘 \n");
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_ENABLE_KEYBOARD_MOUSE, NULL);
        }

        if (user_input == "silence")
        {
            printf("黑屏 30s 后自动恢复 \n");
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_ENABLE_BLACK_SCREEN, NULL);
            Sleep(5000);
            printf("恢复黑屏 \n");
            (void)SendMessageA(wnd, WM_MOVE, WND_NOTIFY_DISABLE_BLACK_SCREEN, NULL);
        }
    }
}

