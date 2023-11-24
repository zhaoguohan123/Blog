#include "ControlDrv.h"
#include "..\..\..\Common\logger.h"

#pragma comment(lib, "..\\..\\ComLib\\spdlog\\x64\\lib\\spdlog.lib")

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
        // ���豸
        KbdDevice_ = CreateFile(L"\\\\.\\troila_kbd_syb",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_DEVICE_KEYBOARD,
            0);

        if (KbdDevice_ == INVALID_HANDLE_VALUE) {
            LOGGER_INFO("�޷����豸��{}", GetLastError());
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

  //  LOGGER_INFO("szDriverImagePath is {}", szDriverImagePath);
    SC_HANDLE hServiceMgr = NULL;
    SC_HANDLE hService = NULL;

    do
    {
        hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hServiceMgr == NULL)
        {
            // OpenSCManager fail
            LOGGER_INFO("OpenSCManager failed! errcode = {}", GetLastError());
            break;
        }

        //create service
        hService = CreateService(hServiceMgr,
            lpszDriverName_,                         // �����������ע����е�����
            lpszDriverName_,                         // ע������������DisplayName ֵ
            SERVICE_ALL_ACCESS,                     // ������������ķ���Ȩ��
            SERVICE_KERNEL_DRIVER,                  // ��ʾ���صķ������ļ�ϵͳ��������
            SERVICE_DEMAND_START,                   // ע������������Start ֵ ,��������
            SERVICE_ERROR_IGNORE,                   // ע������������ErrorControl ֵ
            szDriverImagePath,                      // ע������������ImagePath ֵ
            TEXT("FSFilter Activity Monitor"),      // ע������������Group ֵ
            NULL,
            NULL,                                   // ע������������ DependOnService ֵ
            NULL,
            NULL);

        if (hService == NULL)
        {
            DWORD dwRtn = GetLastError();
            if (dwRtn != ERROR_SERVICE_EXISTS && dwRtn != ERROR_IO_PENDING)
            {
                LOGGER_INFO("kbd_Driver_Service Create Faile!, GetLastError:{} \n", GetLastError());
                break;
            }
            LOGGER_INFO("kbd_Driver_Service Create fail!, service exists!,GetLastError:{} \n", GetLastError());
        }

        wcscpy_s(szTempStr, _countof(szTempStr), TEXT("SYSTEM\\CurrentControlSet\\Services\\"));
        wcscat_s(szTempStr, _countof(szTempStr), lpszDriverName_);
        wcscat_s(szTempStr, _countof(szTempStr), TEXT("\\Instances"));

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, (LPWSTR)TEXT(""), TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            LOGGER_INFO("RegCreateKeyEx 1 falied!errcode = {} \n", GetLastError());
            break;
        }

        // ע������������DefaultInstance ֵ 
        wcscpy_s(szTempStr, _countof(szTempStr), lpszDriverName_);
        wcscpy_s(szTempStr, _countof(szTempStr), TEXT(" Instance"));
        if (RegSetValueEx(hKey, TEXT("DefaultInstance"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            LOGGER_INFO("RegSetValueEx 1 falied! errcode = {} \n", GetLastError());
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
            LOGGER_INFO("RegCreateKeyEx 2 falied! errcode = {} \n", GetLastError());
            break;
        }

        // ע������������Altitude ֵ
        wcscpy_s(szTempStr, lpszAltitude_);
        if (RegSetValueEx(hKey, TEXT("Altitude"), 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)lstrlen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            LOGGER_INFO("RegSetValueEx 2 falied! errcode = {} \n", GetLastError());
            break;
        }

        // ע������������Flags ֵ
        dwData = 0x0;
        if (RegSetValueEx(hKey, TEXT("Flags"), 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            LOGGER_INFO("RegSetValueEx 3 falied! errcode = {} \n", GetLastError());
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
            LOGGER_INFO("StartDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            LOGGER_INFO("StartDriver OpenSCManager failed! errcode = {}", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            LOGGER_INFO("StartDriver OpenService failed! errcode = {}", GetLastError());
            break;
        }

        if (!StartService(schService, 0, NULL))
        {
            DWORD dwRtn = GetLastError();
            if (dwRtn == ERROR_SERVICE_ALREADY_RUNNING)
            {
                // �����Ѿ�����
                break;
            }
            LOGGER_INFO("StartDriver StartService failed! errcode = {}", GetLastError());
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
            LOGGER_INFO("StopDriver lpszDriverName_ is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            LOGGER_INFO("StopDriver OpenSCManager failed! errcode = {}", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            LOGGER_INFO("StopDriver OpenService failed! errcode = {}", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
        {
            LOGGER_INFO("StopDriver ControlService failed! errcode = {}", GetLastError());
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
            LOGGER_INFO("DeleteDriver lpszDriverName is NULL!\n");
            break;
        }

        schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            LOGGER_INFO("DeleteDriver OpenSCManager failed! errcode = {}", GetLastError());
            break;
        }

        schService = OpenService(schManager, lpszDriverName_, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            LOGGER_INFO("DeleteDriver OpenService failed! errcode = {}", GetLastError());
            break;
        }

        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus))
        {
            LOGGER_INFO("DeleteDriver ControlService failed! errcode = {}", GetLastError());
        }

        // ��������ڹر�״̬����ɾ��
        if (svcStatus.dwCurrentState != SERVICE_STOPPED)
        {
            LOGGER_INFO("DeleteDriver serv current status is {}", svcStatus.dwCurrentState);
            break;
        }

        if (!DeleteService(schService))
        {
            LOGGER_INFO("DeleteDriver DeleteService failed! errcode = {}", GetLastError());
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
    LOGGER_INFO("SendCmdToDrv cmd is {}", dwCode);


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
            LOGGER_INFO("DeviceIoControl IOCTL_CODE_TO_CREATE_EVENT falied��{}", GetLastError());
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
            LOGGER_INFO("DeviceIoControl IOCTL_CODE_TO_DISABLE_CAD falied��{}", GetLastError());
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
            LOGGER_INFO("DeviceIoControl IOCTL_CODE_TO_ENABLE_CAD falied��{}", GetLastError());
            break;
        }
        break;
    default:
        LOGGER_INFO("unsupport contorl code!");
        break;
    }
}


