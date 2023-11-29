#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
//#include "DestructWithThreadNotEnd.h"
#include "Hide_Tray_icon.h"
#include "utils.h"

// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{   
    init_logger("logs.log");
    LOGGER_INFO("main start 221");

    //1 . 创建服务
    //serv_opt::run_serv(argc, argv);
    
    // 2.遍历所有的内核obj
    //MyWinobj::main();


    //3. 类析构时，类中的线程未退出，且在使用类中对象
    //DestructWithThreadNotEnd::main();
    
    //HIDE_TRAY_ICON::DeleteTrayIcon();
    char j = 'A';
    for (auto i = L'A';  i <= L'Z'; i++)
    {
        if (utils::IsDriveExists(i))
        {
            LOGGER_INFO("{}: is exists", j);
        }
        j++;
    }
    HWND hwnd = FindWindow(L"Shell_TrayWnd", nullptr);
    if (hwnd != nullptr) {
        SendMessage(hwnd, WM_DEVICECHANGE, 0x8000, 0); // 发送 WM_DEVICECHANGE 消息
    }else
    {
        LOGGER_ERROR("hwnd is nullptr");
    }
    return 0;
}



