#include "CommonHead.h"
#include "service_opt.h"


// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{
    serv_opt::run_serv(argc, argv);
    
    return 0;
}



