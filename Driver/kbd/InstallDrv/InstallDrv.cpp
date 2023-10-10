// InstallDrv.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <Windows.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>

BOOL InstallDriver(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude)
{
    HKEY    hKey = NULL;
    DWORD   dwData = 0;
    BOOL    bRet = FALSE;
    wchar_t szTempStr[MAX_PATH] = { 0 };
    wchar_t szDriverImagePath[MAX_PATH] = { 0 };



    GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

    SC_HANDLE hServiceMgr = NULL;
    SC_HANDLE hService = NULL;

    do
    {
        hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hServiceMgr == NULL)
        {
            // OpenSCManager fail
            printf("OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        //create service
        hService = CreateService(hServiceMgr,
            lpszDriverName,                         // 驱动程序的在注册表中的名字
            lpszDriverName,                         // 注册表驱动程序的DisplayName 值
            SERVICE_ALL_ACCESS,                     // 加载驱动程序的访问权限
            SERVICE_KERNEL_DRIVER,                  // 表示加载的服务是文件系统驱动程序
            SERVICE_DEMAND_START,                   // 注册表驱动程序的Start 值 ,按需启动
            SERVICE_ERROR_IGNORE,                   // 注册表驱动程序的ErrorControl 值
            szDriverImagePath,                      // 注册表驱动程序的ImagePath 值
            TEXT("FSFilter Activity Monitor"),      // 注册表驱动程序的Group 值
            NULL,
            NULL,                   // 注册表驱动程序的 DependOnService 值
            NULL,
            NULL);

        if (hService == NULL)
        {
            DWORD dwRtn = GetLastError();
            if (dwRtn != ERROR_SERVICE_EXISTS && dwRtn != ERROR_IO_PENDING)
            {
                printf("kbd_Driver_Service Create Faile!, GetLastError:%u \n", GetLastError());
                break;
            }
            printf("kbd_Driver_Service Create fail!, service exists!,GetLastError:%u \n", GetLastError());
            break;
        }

        wcscpy_s(szTempStr, _countof(szTempStr), TEXT("SYSTEM\\CurrentControlSet\\Services\\"));
        wcscat_s(szTempStr, _countof(szTempStr), lpszDriverName);
        wcscat_s(szTempStr, _countof(szTempStr), TEXT("\\Instances"));

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, (LPWSTR)TEXT(""), TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            printf("RegCreateKeyEx 1 falied!errcode = %u \n",GetLastError());
            break;
        }

        // 注册表驱动程序的DefaultInstance 值 
        wcscpy_s(szTempStr, _countof(szTempStr), lpszDriverName);
        wcscpy_s(szTempStr, _countof(szTempStr), TEXT(" Instance"));
        if (RegSetValueEx(hKey, TEXT("DefaultInstance"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr)*sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            printf("RegSetValueEx 1 falied! errcode = %u \n", GetLastError());
            break;
        }

        (void)RegFlushKey(hKey);

        wcscpy_s(szTempStr, TEXT("SYSTEM\\CurrentControlSet\\Services\\"));
        wcscat_s(szTempStr, lpszDriverName);
        wcscat_s(szTempStr, TEXT("\\Instances\\"));
        wcscat_s(szTempStr, lpszDriverName);
        wcscat_s(szTempStr, TEXT(" Instance"));

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, (LPWSTR)TEXT(""), TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            printf("RegCreateKeyEx 2 falied! errcode = %u \n", GetLastError());
            break;
        }

        // 注册表驱动程序的Altitude 值
        wcscpy_s(szTempStr, lpszAltitude);
        if (RegSetValueEx(hKey, TEXT("Altitude"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr)*sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            printf("RegSetValueEx 2 falied! errcode = %u \n", GetLastError());
            break;
        }

        // 注册表驱动程序的Flags 值
        dwData = 0x0;
        if (RegSetValueEx(hKey, TEXT("Flags"), 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            printf("RegSetValueEx 3 falied! errcode = %u \n", GetLastError());
            break;
        }

        (void)RegFlushKey(hKey);

        bRet = TRUE;
    } while (FALSE);

    if (hKey)
    {
        (void)RegCloseKey(hKey);
        hKey = NULL;
    }
    if (hServiceMgr)
    {
        (void)CloseServiceHandle(hServiceMgr);
        hServiceMgr = NULL;
    }
    if (hService)
    {
        (void)CloseServiceHandle(hService);
        hService = NULL;
    }
    return bRet;
}

BOOL StartDriver(LPCTSTR lpszDriverName)
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager  = NULL;
    SC_HANDLE        schService  = NULL;

    do
    {
        if (NULL == lpszDriverName)
        {
            printf("StartDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            printf("StartDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            printf("StartDriver OpenService failed! errcode = %u", GetLastError());
            break;
        }

        if (!StartService(schService, 0, NULL))
        {
            DWORD dwRtn = GetLastError();
            if (dwRtn == ERROR_SERVICE_ALREADY_RUNNING)
            {
                // 服务已经开启
                break;
            }
            printf("StartDriver StartService failed! errcode = %u", GetLastError());
            break;
        }
        bRet = TRUE;
    } while (FALSE);

    if (schManager)
    {
        (void)CloseServiceHandle(schManager);
        schManager = NULL;
    }

    if (schService)
    {
        (void)CloseServiceHandle(schService);
        schService = NULL;
    }

    return bRet;
}


BOOL StopDriver(LPCTSTR lpszDriverName)
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager = NULL;
    SC_HANDLE        schService = NULL;
    SERVICE_STATUS   svcStatus;

    do
    {
        if (NULL == lpszDriverName)
        {
            printf("StopDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            printf("StopDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            printf("StopDriver OpenService failed! errcode = %u", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
        {
            printf("StopDriver ControlService failed! errcode = %u", GetLastError());
            break;
        }

        bRet = TRUE;
    } while (FALSE);

    if (schManager)
    {
        (void)CloseServiceHandle(schManager);
        schManager = NULL;
    }

    if (schService)
    {
        (void)CloseServiceHandle(schService);
        schService = NULL;
    }

    return bRet;
}


BOOL DeleteDriver(LPCTSTR lpszDriverName)
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager = NULL;
    SC_HANDLE        schService = NULL;
    SERVICE_STATUS   svcStatus;

    do
    {
        if (NULL == lpszDriverName)
        {
            printf("DeleteDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            printf("DeleteDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            printf("DeleteDriver OpenService failed! errcode = %u", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus))
        {
            printf("DeleteDriver ControlService failed! errcode = %u", GetLastError());
        }

        // 如果服务处于关闭状态才能删除
        if (svcStatus.dwCurrentState != SERVICE_STOPPED)
        {
            printf("DeleteDriver serv current status is %d", svcStatus.dwCurrentState);
            break;
        }

        if (!DeleteService(schService))
        {
            printf("DeleteDriver DeleteService failed! errcode = %u", GetLastError());
            break;
        }

    } while (FALSE);

    if (schManager)
    {
        (void)CloseServiceHandle(schManager);
        schManager = NULL;
    }

    if (schService)
    {
        (void)CloseServiceHandle(schService);
        schService = NULL;
    }

    return bRet;
}

int main()
{
    std::string user_input;
    LPCTSTR lpszDriverName = TEXT("kbdflt_serv");
    LPCTSTR lpszDriverPath = TEXT("C:\\Users\\zgh\\Desktop\\Poc.sys");
    LPCTSTR lpszAltitude = TEXT("389992");

    while (true)
    {
        std::cout << "输入命令, install:创建服务安装驱动 delete:删除驱动 start:启动服务加载驱动 stop:停止服务:" << std::endl;
        std::cin >> user_input;

        if (user_input == "install")
        {
            if (InstallDriver(lpszDriverName, lpszDriverPath, lpszAltitude) == FALSE)
            {
                std::cout << "安装驱动失败"<<std::endl;
            }
        }

        //(注意：删除之前，一定要先把CreateFile打开驱动的句柄关闭，才能正确删除)
        if (user_input == "delete")
        {
            if (DeleteDriver(lpszDriverName) == FALSE)
            {
                std::cout << "删除服务失败" << std::endl;
            }
        }
        
        if (user_input == "start")
        {
            if (StartDriver(lpszDriverName) == FALSE)
            {
                std::cout << "启动服务失败" << std::endl;
            }
        }

        //(注意：停止之前，一定要先把CreateFile打开驱动的句柄关闭，才能正确停止)
        if (user_input == "stop")
        {
            if (StopDriver(lpszDriverName) == FALSE)
            {
                std::cout << "停止服务失败" << std::endl;
            }
        }
    }
    
}

