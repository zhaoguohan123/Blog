#include <iostream>
#include <Windows.h>
#define IOCTL_CODE_TO_CREATE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CODE_TO_ENABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CODE_TO_DISABLE_CAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main()
{
    HANDLE cad_event = NULL;
    HANDLE kbd_device = NULL;
    DWORD uRetBytes = 0;
    std::string user_input;

    // 创建同步事件
    SECURITY_DESCRIPTOR secutityDese;
    ::InitializeSecurityDescriptor(&secutityDese, SECURITY_DESCRIPTOR_REVISION);
    ::SetSecurityDescriptorDacl(&secutityDese, TRUE, NULL, FALSE);


    SECURITY_ATTRIBUTES securityAttr;
    securityAttr.nLength = sizeof SECURITY_ATTRIBUTES;
    securityAttr.bInheritHandle = FALSE;
    securityAttr.lpSecurityDescriptor = &secutityDese;

    cad_event = ::CreateEvent(&securityAttr, TRUE, FALSE, TEXT("Global\\Kbd_fltr")); // 这个事件是用户层创建的
    if (cad_event == NULL)
    {
        std::cout << "CreateEvent Kbd_fltr failed!, err:" << GetLastError() << std::endl;
        goto exit;
    }

    ////////////////////////////////////// 用户层向驱动发送ioctl消息 //////////////////
            // 打开键盘驱动
    kbd_device = CreateFile(L"\\\\.\\kbd_syb",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_DEVICE_KEYBOARD,
        0);

    if (kbd_device == INVALID_HANDLE_VALUE) {
        std::cerr << "无法打开设备：" << GetLastError() << std::endl;
        goto exit;
    }


    while (TRUE)
    {
        std::cout << "输入命令, 1:通知驱动创建通知事件 2:开启cad屏蔽功能 3:关闭cad屏蔽功能 " << std::endl;
        std::cin >> user_input;

        if (user_input == "1")
        {
            std::cout << "通知驱动创建事件" << std::endl;
            if (0 == DeviceIoControl(
                kbd_device,
                IOCTL_CODE_TO_CREATE_EVENT,
                &cad_event,
                sizeof(HANDLE),
                NULL,
                0,
                &uRetBytes,
                NULL))
            {
                std::cerr << "DeviceIoControl IOCTL_CODE_TO_CREATE_EVENT falied：" << GetLastError() << std::endl;
                continue;
            }
        }

        if (user_input == "2")
        {
            // 通知驱动开启屏蔽ctrl+alt+del的功能
            std::cout << "开启cad屏蔽功能" << std::endl;
            if (0 == DeviceIoControl(
                kbd_device,
                IOCTL_CODE_TO_DISABLE_CAD,
                &cad_event,
                sizeof(HANDLE),
                NULL,
                0,
                &uRetBytes,
                NULL))
            {
                std::cerr << "DeviceIoControl IOCTL_CODE_TO_DISABLE_CAD falied：" << GetLastError() << std::endl;
                continue;
            }

            do
            {
                DWORD ret = WaitForSingleObject(cad_event, INFINITE);

                if (ret == WAIT_OBJECT_0) 
                {
                    std::cout << "事件已触发,已捕获到ctrl+alt+del" << std::endl;
                    if (!ResetEvent(cad_event))
                    {
                        std::cout << "reset event failed  " << GetLastError() << std::endl;
                    }
                }
                else 
                {
                    std::cerr << "等待事件时发生错误：" << GetLastError() << std::endl;
                }
                std::cout << "输入命令，是否继续等待事件? 1：是  2：否" << std::endl;
                std::cin >> user_input;
                if (user_input == "2")
                {
                    break;
                }
            } while (TRUE);
        }

        if (user_input == "3")
        {
            std::cout << "关闭屏蔽功能" << std::endl;
            // 通知驱动关闭屏蔽ctrl+alt+del的功能
            if (0 == DeviceIoControl(
                kbd_device,
                IOCTL_CODE_TO_ENABLE_CAD,
                &cad_event,
                sizeof(HANDLE),
                NULL,
                0,
                &uRetBytes,
                NULL))
            {
                std::cerr << "DeviceIoControl IOCTL_CODE_TO_ENABLE_CAD falied：" << GetLastError() << std::endl;
                continue;
            }
        }
    }
exit:
    if (cad_event)
    {
        CloseHandle(cad_event);
        cad_event = NULL;
    }

    if (kbd_device)
    {
        CloseHandle(kbd_device);
        kbd_device = NULL;
    }

    (void)getchar();
}
