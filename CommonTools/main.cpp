#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"

// ����һ�����񣬲��ڷ������������Ĳ���
int main(int argc, TCHAR* argv[])
{
    // //1 . ��������
    //serv_opt::run_serv(argc, argv);
    
    // 2.�������е��ں�obj
    list_sysbol_link_obj();
    getchar();
    return 0;
}



