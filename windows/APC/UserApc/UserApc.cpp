#include <iostream>
#include <Windows.h>
/* 在用户层向某一个线程插入apc
*  1. 先定义一个apc回调
*  2. 获取目标线程的句柄
*  3. 调用QueueUserAPC将APC插件队列
* 
*  注意点： 
*        1. 标线程必须处于"可警告"状态才能执行APC,比如调用了SleepEx、WaitForSingleObjectEx等函数
*        2. APC函数在目标线程的上下文中执行
*        3. 当目标线程结束时,未处理的APC会被丢弃
*/
VOID CALLBACK APCProc(ULONG_PTR dwParam) 
{
    printf("APCProc---------------------- 0x%x\n", dwParam);
}

DWORD WINAPI ThreadProc(LPVOID p)
{
    for (size_t i = 0; i < 100; i++)
    {
        SleepEx(1000, TRUE);
        printf("running .... \n");
    }
    return 1;
}

int main()
{
    
    HANDLE hThread = CreateThread(NULL, NULL, ThreadProc, NULL, NULL,NULL);
    Sleep(4000);
    QueueUserAPC(APCProc, hThread, 16);
    
    system("PAUSE");
}

