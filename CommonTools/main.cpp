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

    // 测试设置扬声器
    //TEST::test_set_win_sound();

    //TEST::test_job_queue();

    TEST::test_CBT_hook_task_list_protect_proc_kill();

    getchar();
    return 0;
}