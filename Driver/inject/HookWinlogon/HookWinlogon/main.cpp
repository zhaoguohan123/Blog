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
    printf("LoadLibraryTool v.1.0 ͨ�� DLL ע�빤�ߣ�@Rio-CTH\n");
    if (argc != 3)
    {
        std::cout << "���󣺲������Ϸ���" << std::endl;
        std::cin.get();
        return -1;
    }
    const size_t exepthlen =
        wcslen(argv[1]) * sizeof(wchar_t);
    const size_t dllpthlen =
        wcslen(argv[2]) * sizeof(wchar_t);

    if (exepthlen < 5 || dllpthlen < 1)
    {
        std::cout << "�����ļ�·������" << std::endl;
        std::cin.get();
        return -1;
    }
    wchar_t* exepth = new wchar_t[exepthlen];
    wchar_t* dllpth = new wchar_t[dllpthlen];

    wcscpy_s(exepth, exepthlen, argv[1]);
    wcscpy_s(dllpth, dllpthlen, argv[2]);

    if (!wcsstr(exepth, L".exe"))
    {
        std::cout << "����Ŀ��������������.exe��׺��" << std::endl;
        std::cin.get();
        return -1;
    }

    BOOL dllextflag = PathFileExistsW(dllpth);
    if (FALSE == dllextflag)
    {
        std::cout << "�����ļ������ڻ����޷����ʣ�" << std::endl;
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
            std::cout << "�����ļ������ڻ����޷����ʣ�" << std::endl;
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
            std::cout << "���棺PID Ϊ " << pids[i] << " �Ľ����Ѿ�����Ŀ�� DLL��" << std::endl;
            continue;
        }

        if (ZwCreateThreadExInjectDll(pids[i], dllpth) == FALSE)
        {
            std::cout << "����ע�� PID Ϊ " << pids[i] << " �Ľ���ʱʧ�ܡ�" << std::endl;
            std::cout << "ԭ��" << GetLastError() << std::endl;
        }
    }

    if (pids.size() == 0)
    {
        std::wcout << "����δ�ҵ�Ŀ����̣�" << exepth << std::endl;
        std::cin.get();
        EnableDebugPrivilege(FALSE);
        return -1;
    }
    // ���ٹ���
    pids.clear();
    pids.shrink_to_fit();
    std::cout << "�����Ѿ���ɡ�" << std::endl;
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
    * ����ΪTH32CS_SNAPMODULE �� TH32CS_SNAPMODULE32ʱ,�������ʧ�ܲ�����ERROR_BAD_LENGTH�������Ըú���ֱ���ɹ�
    * ���̴���δ��ʼ�����ʱ��CreateToolhelp32Snapshot�᷵��error 299������������²���
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
    mi.dwSize = sizeof(MODULEENTRY32W); //��һ��ʹ�ñ����ʼ����Ա
    BOOL bRet = Module32FirstW(hSnapshot, &mi);
    while (bRet) {
        // mi.szModule�Ƕ�·��
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
    // 1.��Ŀ�����
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS, // ��Ȩ��
        FALSE, // �Ƿ�̳�
        dwProcessId); // ����PID
    if (NULL == hProcess)
    {
        printf("���󣺴�Ŀ�����ʧ�ܣ�\n");
        return FALSE;
    }
    // 2.��Ŀ�����������ռ�
    LPVOID lpPathAddr = VirtualAllocEx(
        hProcess, // Ŀ����̾��
        0, // ָ�������ַ
        pathSize, // ����ռ��С
        MEM_RESERVE | MEM_COMMIT, // �ڴ��״̬
        PAGE_READWRITE); // �ڴ�����
    if (NULL == lpPathAddr)
    {
        printf("������Ŀ�����������ռ�ʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    // 3.��Ŀ�������д��Dll·��
    if (FALSE == WriteProcessMemory(
        hProcess, // Ŀ����̾��
        lpPathAddr, // Ŀ����̵�ַ
        pszDllFileName, // д��Ļ�����
        pathSize, // ��������С
        NULL)) // ʵ��д���С
    {
        printf("����Ŀ�������д��Dll·��ʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //4.����ntdll.dll
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (NULL == hNtdll)
    {
        printf("���󣺼���ntdll.dllʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //5.��ȡLoadLibraryA�ĺ�����ַ
    //FARPROC��������Ӧ32λ��64λ
    FARPROC pFuncProcAddr = GetProcAddress(GetModuleHandle(L"Kernel32.dll"),
        "LoadLibraryW");
    if (NULL == pFuncProcAddr)
    {
        printf("���󣺻�ȡLoadLibrary������ַʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    //6.��ȡZwCreateThreadEx������ַ,�ú�����32λ��64λ��ԭ�Ͳ�ͬ
    //_WIN64�����жϱ��뻷�� ��_WIN32�����ж��Ƿ���Windowsϵͳ
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
        printf("���󣺻�ȡZwCreateThreadEx������ַʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    //7.��Ŀ������д���Զ�߳�
    HANDLE hRemoteThread = NULL;
    DWORD lpExitCode = 0;
    DWORD dwStatus = ZwCreateThreadEx(&hRemoteThread, PROCESS_ALL_ACCESS, NULL,
        hProcess,
        (LPTHREAD_START_ROUTINE)pFuncProcAddr, lpPathAddr, 0, 0, 0, 0, NULL);
    if (NULL == hRemoteThread)
    {
        printf("����Ŀ������д����߳�ʧ�ܣ�\n");
        CloseHandle(hProcess);
        return FALSE;
    }

    // 8.�ȴ��߳̽���
    WaitForSingleObject(hRemoteThread, -1);
    GetExitCodeThread(hRemoteThread, &lpExitCode);
    if (lpExitCode == 0)
    {
        printf("����Ŀ�������ע�� DLL ʧ�ܣ������ṩ�� DLL �Ƿ���Ч��\n");
        CloseHandle(hProcess);
        return FALSE;
    }
    // 9.������
    VirtualFreeEx(hProcess, lpPathAddr, 0, MEM_RELEASE);
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);
    FreeLibrary(hNtdll);
    return TRUE;
}