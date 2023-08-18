#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
#include "DestructWithThreadNotEnd.h"

// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{
    // //1 . 创建服务
    //serv_opt::run_serv(argc, argv);
    
    // 2.遍历所有的内核obj
    //MyWinobj::main();


    //3. 类析构时，类中的线程未退出，且在使用类中对象
    DestructWithThreadNotEnd::main();

    getchar();
    return 0;
}



