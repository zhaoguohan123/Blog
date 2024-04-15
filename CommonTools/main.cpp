// #include "CommonHead.h"
// #include "service_opt.h"
// #include "list_kernel_obj.h"
// //#include "DestructWithThreadNotEnd.h"
// #include "Hide_Tray_icon.h"
// #include "utils.h"
// #include "list_ini.h"
// #include ".\GetProcInfo\GetProcInfo.h"
// #include ".\AssistCollectProc\AssistCollectProc.h"

#include "test.h"


// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{
    init_logger("logs.log");
	//TEST::test_list_ini();

 
    TEST::test_set_win_sound();

    //1 . 创建服务
    //serv_opt::run_serv(argc, argv);
    
    // 2.遍历所有的内核obj
    //MyWinobj::main();


    //3. 类析构时，类中的线程未退出，且在使用类中对象
    //DestructWithThreadNotEnd::main();
    
    //HIDE_TRAY_ICON::DeleteTrayIcon();
    // GetProcInfo a;
    // a.GetAllProcInfo();
    // a.ExcludeSysProc();

    // std::string str = "启动收集";
    // AssistCollectProc a;
    // a.Initialize();

    // if (str == "启动收集" )
    // {
        
    //     a.Run();
    // }
    // else
    // {
    //     a.Stop();
    // }

    getchar();
    return 0;
}