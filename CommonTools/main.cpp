#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
#include "DestructWithThreadNotEnd.h"

// ����һ�����񣬲��ڷ������������Ĳ���
int main(int argc, TCHAR* argv[])
{
    // //1 . ��������
    //serv_opt::run_serv(argc, argv);
    
    // 2.�������е��ں�obj
    //MyWinobj::main();


    //3. ������ʱ�����е��߳�δ�˳�������ʹ�����ж���
    DestructWithThreadNotEnd::main();

    getchar();
    return 0;
}



