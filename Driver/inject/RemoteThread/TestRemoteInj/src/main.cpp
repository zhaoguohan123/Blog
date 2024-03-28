#include "common.h"

//判断某模块(dll)是否在相应的进程中
//dwPID         进程的PID
//szDllPath     查询的dll的完整路径
BOOL CheckDllInProcess(DWORD dwPID, LPCTSTR szDllPath);

//向指定的进程注入相应的模块
//dwPID         目标进程的PID
//szDllPath     被注入的dll的完整路径
BOOL InjectDll(DWORD dwPID, LPCTSTR szDllPath);

//让指定的进程卸载相应的模块
//dwPID         目标进程的PID
//szDllPath     被注入的dll的完整路径,注意：路径不要用“/”来代替“\\”
BOOL EjectDll(DWORD dwPID, LPCTSTR szDllPath);


int main()
{
    InjectDll(21092, L"C:\\Users\\zgh\\Desktop\\work\\Blog\\Driver\\inject\\RemoteThread\\bin\\InjDll.dll");
    EjectDll(21092, L"C:\\Users\\zgh\\Desktop\\work\\Blog\\Driver\\inject\\RemoteThread\\bin\\InjDll.dll");
    system("pause");

    return 0;
}