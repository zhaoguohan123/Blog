#include "ControlDrv.h"

ControlDrv::ControlDrv(LPCTSTR lpszDriverName, LPCTSTR lpszDriverPath, LPCTSTR lpszAltitude) :
    lpszDriverName_(lpszDriverName),
    lpszDriverPath_(lpszDriverPath),
    lpszAltitude_(lpszAltitude),
    KbdDevice_(NULL)
{

}

ControlDrv::~ControlDrv()
{
    if (KbdDevice_)
    {
        (void)CloseHandle(KbdDevice_);
        KbdDevice_ = NULL;
    }
}

BOOL ControlDrv::Init()
{
    BOOL bRet = FALSE;
    do
    {
        // 打开设备
        KbdDevice_ = CreateFile(L"\\\\.\\troila_kbd_syb",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_DEVICE_KEYBOARD,
            0);

        if (KbdDevice_ == INVALID_HANDLE_VALUE) {
            MyDebugPrintA("无法打开设备：%u", GetLastError());
            break;
        }
        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL ControlDrv::InstallDriver()
{
    HKEY    hKey = NULL;
    DWORD   dwData = 0;
    BOOL    bRet = FALSE;
    wchar_t szTempStr[MAX_PATH] = { 0 };
    wchar_t szDriverImagePath[MAX_PATH] = { 0 };

    GetFullPathName(lpszDriverPath_, MAX_PATH, szDriverImagePath, NULL);

    MyDebugPrintA("szDriverImagePath is %S", szDriverImagePath);
    SC_HANDLE hServiceMgr = NULL;
    SC_HANDLE hService = NULL;

    do
    {
        hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hServiceMgr == NULL)
        {
            // OpenSCManager fail
            MyDebugPrintA("OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        //create service
        hService = CreateService(hServiceMgr,
            lpszDriverName_,                         // 驱动程序的在注册表中的名字
            lpszDriverName_,                         // 注册表驱动程序的DisplayName 值
            SERVICE_ALL_ACCESS,                     // 加载驱动程序的访问权限
            SERVICE_KERNEL_DRIVER,                  // 表示加载的服务是文件系统驱动程序
            SERVICE_DEMAND_START,                   // 注册表驱动程序的Start 值 ,按需启动
            SERVICE_ERROR_IGNORE,                   // 注册表驱动程序的ErrorControl 值
            szDriverImagePath,                      // 注册表驱动程序的ImagePath 值
            TEXT("FSFilter Activity Monitor"),      // 注册表驱动程序的Group 值
            NULL,
            NULL,                                   // 注册表驱动程序的 DependOnService 值
            NULL,
            NULL);

        if (hService == NULL)
        {
            DWORD dwRtn = GetLastError();
            if (dwRtn != ERROR_SERVICE_EXISTS && dwRtn != ERROR_IO_PENDING)
            {
                MyDebugPrintA("kbd_Driver_Service Create Faile!, GetLastError:%u \n", GetLastError());
                break;
            }
            MyDebugPrintA("kbd_Driver_Service Create fail!, service exists!,GetLastError:%u \n", GetLastError());
        }

        wcscpy_s(szTempStr, _countof(szTempStr), TEXT("SYSTEM\\CurrentControlSet\\Services\\"));
        wcscat_s(szTempStr, _countof(szTempStr), lpszDriverName_);
        wcscat_s(szTempStr, _countof(szTempStr), TEXT("\\Instances"));

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, (LPWSTR)TEXT(""), TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            MyDebugPrintA("RegCreateKeyEx 1 falied!errcode = %u \n", GetLastError());
            break;
        }

        // 注册表驱动程序的DefaultInstance 值 
        wcscpy_s(szTempStr, _countof(szTempStr), lpszDriverName_);
        wcscpy_s(szTempStr, _countof(szTempStr), TEXT(" Instance"));
        if (RegSetValueEx(hKey, TEXT("DefaultInstance"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            MyDebugPrintA("RegSetValueEx 1 falied! errcode = %u \n", GetLastError());
            break;
        }

        (void)RegFlushKey(hKey);

        wcscpy_s(szTempStr, TEXT("SYSTEM\\CurrentControlSet\\Services\\"));
        wcscat_s(szTempStr, lpszDriverName_);
        wcscat_s(szTempStr, TEXT("\\Instances\\"));
        wcscat_s(szTempStr, lpszDriverName_);
        wcscat_s(szTempStr, TEXT(" Instance"));

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, (LPWSTR)TEXT(""), TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            MyDebugPrintA("RegCreateKeyEx 2 falied! errcode = %u \n", GetLastError());
            break;
        }

        // 注册表驱动程序的Altitude 值
        wcscpy_s(szTempStr, lpszAltitude_);
        if (RegSetValueEx(hKey, TEXT("Altitude"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            MyDebugPrintA("RegSetValueEx 2 falied! errcode = %u \n", GetLastError());
            break;
        }

        // 注册表驱动程序的Flags 值
        dwData = 0x0;
        if (RegSetValueEx(hKey, TEXT("Flags"), 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            MyDebugPrintA("RegSetValueEx 3 falied! errcode = %u \n", GetLastError());
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

BOOL ControlDrv::StartDriver()
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager = NULL;
    SC_HANDLE        schService = NULL;

    do
    {
        if (NULL == lpszDriverName_)
        {
            MyDebugPrintA("StartDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            MyDebugPrintA("StartDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            MyDebugPrintA("StartDriver OpenService failed! errcode = %u", GetLastError());
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
            MyDebugPrintA("StartDriver StartService failed! errcode = %u", GetLastError());
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

BOOL ControlDrv::StopDriver()
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager = NULL;
    SC_HANDLE        schService = NULL;
    SERVICE_STATUS   svcStatus;

    if (KbdDevice_)
    {
        (void)CloseHandle(KbdDevice_);
        KbdDevice_ = NULL;
    }

    do
    {
        if (NULL == lpszDriverName_)
        {
            MyDebugPrintA("StopDriver lpszDriverName_ is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            MyDebugPrintA("StopDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            MyDebugPrintA("StopDriver OpenService failed! errcode = %u", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
        {
            MyDebugPrintA("StopDriver ControlService failed! errcode = %u", GetLastError());
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

BOOL ControlDrv::DeleteDriver()
{
    BOOL bRet = FALSE;
    SC_HANDLE        schManager = NULL;
    SC_HANDLE        schService = NULL;
    SERVICE_STATUS   svcStatus;

    if (KbdDevice_)
    {
        (void)CloseHandle(KbdDevice_);
        KbdDevice_ = NULL;
    }

    do
    {
        if (NULL == lpszDriverName_)
        {
            MyDebugPrintA("DeleteDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            MyDebugPrintA("DeleteDriver OpenSCManager failed! errcode = %u", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            MyDebugPrintA("DeleteDriver OpenService failed! errcode = %u", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus))
        {
            MyDebugPrintA("DeleteDriver ControlService failed! errcode = %u", GetLastError());
        }

        // 如果服务处于关闭状态才能删除
        if (svcStatus.dwCurrentState != SERVICE_STOPPED)
        {
            MyDebugPrintA("DeleteDriver serv current status is %d", svcStatus.dwCurrentState);
            break;
        }

        if (!DeleteService(schService))
        {
            MyDebugPrintA("DeleteDriver DeleteService failed! errcode = %u", GetLastError());
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

VOID ControlDrv::SendCmdToDrv(DWORD dwCode)
{
    MyDebugPrintA("SendCmdToDrv cmd is %d", dwCode);


    DWORD uRetBytes = 0;

    switch (dwCode)
    {
    case IO_CREATE_CAD_EVENT:
        if (0 == DeviceIoControl(
            KbdDevice_,
            IOCTL_CODE_TO_CREATE_EVENT,
            NULL,
            sizeof(HANDLE),
            NULL,
            0,
            &uRetBytes,
            NULL))
        {
            MyDebugPrintA("DeviceIoControl IOCTL_CODE_TO_CREATE_EVENT falied：%u", GetLastError());
        }
        break;

    case IO_DISABLE_CAD:
        if (0 == DeviceIoControl(
            KbdDevice_,
            IOCTL_CODE_TO_DISABLE_CAD,
            NULL,
            sizeof(HANDLE),
            NULL,
            0,
            &uRetBytes,
            NULL))
        {
            MyDebugPrintA("DeviceIoControl IOCTL_CODE_TO_DISABLE_CAD falied：%u", GetLastError());
        }
        break;

    case IO_ENABLE_CAD:
        if (0 == DeviceIoControl(
            KbdDevice_,
            IOCTL_CODE_TO_ENABLE_CAD,
            NULL,
            sizeof(HANDLE),
            NULL,
            0,
            &uRetBytes,
            NULL))
        {
            MyDebugPrintA("DeviceIoControl IOCTL_CODE_TO_ENABLE_CAD falied：%u", GetLastError());
            break;
        }
        break;
    default:
        MyDebugPrintA("unsupport contorl code!");
        break;
    }
}


