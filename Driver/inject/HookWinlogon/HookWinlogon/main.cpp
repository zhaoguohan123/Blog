#include <iostream>
#include <windows.h>
#include <vector>
#include <tlhelp32.h>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
BOOL ProcessHasLoadDll(
    DWORD pid,
    const TCHAR* dll
);

BOOL ZwCreateThreadExInjectDll(
    DWORD dwProcessId,
    const wchar_t* pszDllFileName
);

BOOL EnableDebugPrivilege(
    BOOL bEnablePrivilege
);

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleTitleW(L"LoadLibraryTool v.1.0");
    printf("LoadLibraryTool v.1.0 通用 DLL 注入工具；@Rio-CTH\n");
    if (argc != 3)
    {
        std::cout << "错误：参数不合法！" << std::endl;
        std::cin.get();
        return -1;
    }
    const size_t exepthlen =
        wcslen(argv[1]) * sizeof(wchar_t);
    const size_t dllpthlen =
        wcslen(argv[2]) * sizeof(wchar_t);

    if (exepthlen < 5 || dllpthlen < 1)
    {
        std::cout << "错误：文件路径错误！" << std::endl;
        std::cin.get();
        return -1;
    }
    wchar_t* exepth = new wchar_t[exepthlen];
    wchar_t* dllpth = new wchar_t[dllpthlen];

    wcscpy_s(exepth, exepthlen, argv[1]);
    wcscpy_s(dllpth, dllpthlen, argv[2]);

    if (!wcsstr(exepth, L".exe"))
    {
        std::cout << "错误：目标进程名必须包含.exe后缀！" << std::endl;
        std::cin.get();
        return -1;
    }

    BOOL dllextflag = PathFileExistsW(dllpth);
    if (FALSE == dllextflag)
    {
        std::cout << "错误：文件不存在或者无法访问！" << std::endl;
        std::cin.get();
        return -1;
    }

    if (PathGetDriveNumberW(dllpth) == -1)
    {
        TCHAR szPath[_MAX_PATH] = { 0 };
        TCHAR szDrive[_MAX_DRIVE] = { 0 };
        TCHAR szDir[_MAX_DIR] = { 0 };
        TCHAR szFname[_MAX_FNAME] = { 0 };
        TCHAR szExt[_MAX_EXT] = { 0 };
        //TCHAR DataBinPath[MAX_PATH] = { 0 };
        TCHAR CurBinPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, szPath, sizeof(szPath) / sizeof(TCHAR));
        ZeroMemory(CurBinPath, sizeof(CurBinPath));
        _wsplitpath_s(szPath, szDrive, szDir, szFname, szExt);
        wsprintf(CurBinPath, L"%s%s", szDrive, szDir);
        //wcscpy_s(DataBinPath, CurBinPath);
        wcscat_s(CurBinPath, dllpth);
        //wprintf(L"[WorkstationDirectory]: %s\n", DataBinPath);
        dllextflag = PathFileExistsW(CurBinPath);
        if (FALSE == dllextflag)
        {
            std::cout << "错误：文件不存在或者无法访问！" << std::endl;
            std::cin.get();
            return -1;
        }
        dllpth = CurBinPath;
    }

    EnableDebugPrivilege(TRUE);

    std::vector<DWORD> pids;
    HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { 0 };
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hp, &pe)) {
        do {
            if (!wcscmp(pe.szExeFile, exepth) && pe.cntThreads >= 1) {
                pids.push_back(pe.th32ProcessID);
            }
        } while (Process32NextW(hp, &pe));
    }
    CloseHandle(hp);

    for (int i = 0; i < pids.size(); i++) {
        if (ProcessHasLoadDll(pids[i], dllpth) == TRUE)
        {
            std::cout << "警告：PID 为 " << pids[i] << " 的进程已经包含目标 DLL。" << std::endl;
            continue;
        }

        if (ZwCreateThreadExInjectDll(pids[i], dllpth) == FALSE)
        {
            std::cout << "错误：注入 PID 为 " << pids[i] << " 的进程时失败。" << std::endl;
            std::cout << "原因：" << GetLastError() << std::endl;
        }
    }

    if (pids.size() == 0)
    {
        std::wcout << "错误：未找到目标进程：" << exepth << std::endl;
        std::cin.get();
        EnableDebugPrivilege(FALSE);
        return -1;
    }
    // 销毁过程
    pids.clear();
    pids.shrink_to_fit();
    std::cout << "操作已经完成。" << std::endl;
    std::cin.get();
    EnableDebugPrivilege(FALSE);
    return 0;
}

BOOL EnableDebugPrivilege(
    BOOL bEnablePrivilege   // to enable or disable privilege
)
{
    HANDLE hProcess = NULL;
    TOKEN_PRIVILEGES tp{};
    LUID luid;
    hProcess = GetCurrentProcess();
    HANDLE hToken = NULL;
    OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken);

    if (!LookupPrivilegeValueW(
        NULL,            // lookup privilege on local system
        SE_DEBUG_NAME,   // privilege to lookup 
        &luid))        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)NULL,
        (PDWORD)NULL))
    {
        printf("AdjustTokenPrivileges error: %u\n", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
        printf("The token does not have the specified privilege. \n");
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);
    return TRUE;
}


BOOL ProcessHasLoadDll(DWORD pid, const TCHAR* dll) {
    /*
    * 参数为TH32CS_SNAPMODULE 或 TH32CS_SNAPMODULE32时,如果函数失败并返回ERROR_BAD_LENGTH，则重试该函数直至成功
    * 进程创建未初始化完成时，CreateToolhelp32Snapshot会返回error 299，但其它情况下不会
    */
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    while (INVALID_HANDLE_VALUE == hSnapshot) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_BAD_LENGTH) {
            hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            continue;
        }
        else {
            //if (dwError == ERROR_PARTIAL_COPY) {
                //printf("CreateToolhelp32Snapshot failed: %d\ncurrentProcessId: %d \t targetProcessId:\n",
                    //dwError, GetCurrentProcessId(), pid);
            //}
            //else {
            printf("CreateToolhelp32Snapshot failed: %d\ncurrentProcessId: %d \t targetProcessId:%d\n",
                dwError, GetCurrentProcessId(), pid);
            //}
            return FALSE;
        }
    }
    MODULEENTRY32W mi{};
    mi.dwSize = sizeof(MODULEENTRY32W); //第一次使用必须初始化成员
    BOOL bRet = Module32FirstW(hSnapshot, &mi);
    while (bRet) {
        // mi.szModule是短路径
        if (wcsstr(dll, mi.szModule) || wcsstr(mi.szModule, dll)) {
            //TCHAR* dllWithFullPath = new TCHAR[MAX_MODULE_NAME32 + 1];
            //lstrcpyW(dllWithFullPath, mi.szModule);
            if (hSnapshot != NULL) CloseHandle(hSnapshot);
            return TRUE;
        }
        mi.dwSize = sizeof(MODULEENTRY32W);
        bRet = Module32NextW(hSnapshot, &mi);
    }
    if (hSnapshot != NULL) CloseHandle(hSnapshot);
    return FALSE;
}

BOOL ZwCreateThreadExInjectDll(
    DWORD dwProcessId,
    const wchar_t* pszDllFileName
)
{
    size_t pathSize = (wcslen(pszDllFileName) + 1) * sizeof(wchar_t);
    // 1.打开目标进程
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS, // 打开权限
        FALSE, // 是否继承
        dwProcessId); // 进程PID
    if (NULL == hProcess)
    {
        printf("错误：打开目标进程失败！\n");
        return FALSE;
    }
    // 2.在目标进程中申请空间
    LPVOID lpPathAddr = VirtualAllocEx(
        hProcess, // 目标进程句柄
        0, // 指定申请地址
        pathSize, // 申请空间大小
        MEM_RESERVE | MEM_COMMIT, // 内存的状态
        PAGE_READWRITE); // 内存属性
    if (NULL == lpPathAddr)
    {
        printf("错误：在目标进程中申请空间失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    // 3.在目标进程中写入Dll路径
    if (FALSE == WriteProcessMemory(
        hProcess, // 目标进程句柄
        lpPathAddr, // 目标进程地址
        pszDllFileName, // 写入的缓冲区
        pathSize, // 缓冲区大小
        NULL)) // 实际写入大小
    {
        printf("错误：目标进程中写入Dll路径失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //4.加载ntdll.dll
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (NULL == hNtdll)
    {
        printf("错误：加载ntdll.dll失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //5.获取LoadLibraryA的函数地址
    //FARPROC可以自适应32位与64位
    FARPROC pFuncProcAddr = GetProcAddress(GetModuleHandle(L"Kernel32.dll"),
        "LoadLibraryW");
    if (NULL == pFuncProcAddr)
    {
        printf("错误：获取LoadLibrary函数地址失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //6.获取ZwCreateThreadEx函数地址,该函数在32位与64位下原型不同
    //_WIN64用来判断编译环境 ，_WIN32用来判断是否是Windows系统
#ifdef _WIN64
    typedef DWORD(WINAPI* typedef_ZwCreateThreadEx)(
        PHANDLE ThreadHandle,
        ACCESS_MASK DesiredAccess,
        LPVOID ObjectAttributes,
        HANDLE ProcessHandle,
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        ULONG CreateThreadFlags,
        SIZE_T ZeroBits,
        SIZE_T StackSize,
        SIZE_T MaximumStackSize,
        LPVOID pUnkown
        );
#else
    typedef DWORD(WINAPI* typedef_ZwCreateThreadEx)(
        PHANDLE ThreadHandle,
        ACCESS_MASK DesiredAccess,
        LPVOID ObjectAttributes,
        HANDLE ProcessHandle,
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        BOOL CreateSuspended,
        DWORD dwStackSize,
        DWORD dw1,
        DWORD dw2,
        LPVOID pUnkown
        );
#endif 
    typedef_ZwCreateThreadEx ZwCreateThreadEx =
        (typedef_ZwCreateThreadEx)GetProcAddress(hNtdll, "ZwCreateThreadEx");
    if (NULL == ZwCreateThreadEx)
    {
        printf("错误：获取ZwCreateThreadEx函数地址失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    //7.在目标进程中创建远线程
    HANDLE hRemoteThread = NULL;
    DWORD lpExitCode = 0;
    DWORD dwStatus = ZwCreateThreadEx(&hRemoteThread, PROCESS_ALL_ACCESS, NULL,
        hProcess,
        (LPTHREAD_START_ROUTINE)pFuncProcAddr, lpPathAddr, 0, 0, 0, 0, NULL);
    if (NULL == hRemoteThread)
    {
        printf("错误：目标进程中创建线程失败！\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    // 8.等待线程结束
    WaitForSingleObject(hRemoteThread, -1);
    GetExitCodeThread(hRemoteThread, &lpExitCode);
    if (lpExitCode == 0)
    {
        printf("错误：目标进程中注入 DLL 失败，请检查提供的 DLL 是否有效！\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    // 9.清理环境
    VirtualFreeEx(hProcess, lpPathAddr, 0, MEM_RELEASE);
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);
    FreeLibrary(hNtdll);
    return TRUE;
}