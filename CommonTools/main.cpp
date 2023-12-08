#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
//#include "DestructWithThreadNotEnd.h"
#include "Hide_Tray_icon.h"
#include "utils.h"
#include ".\GetProcInfo\GetProcInfo.h"
#include ".\GetTaskMsgProc\GetTaskMsgProcData.h"


// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{
    init_logger("logs.log");

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

     GetTaskMsgProcData * a = new GetTaskMsgProcData();
    // std::shared_ptr<std::map<std::string, ROCESSINFOA>> proc_info_vec = a->GetProcInfo();

    // std::map<std::string, ROCESSINFOA>::iterator iter = proc_info_vec->begin();
    // for (; iter != proc_info_vec->end(); ++iter)
    // {
    //     LOGGER_INFO("proc_name:{} proc_version:{} proc_running_time:{}", iter->first, iter->second.proc_version, iter->second.proc_running_time);
    // }
    
    getchar();
    return 0;
}



