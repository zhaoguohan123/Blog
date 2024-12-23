// TestClipboard.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "TestClipboard.h"
#include "HelpCoInitialize.h"
#include "DataObject.h"
#include "VirtualFileSrcStream.h"


#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#endif

#define MAX_LOADSTRING 100
using clipboard::FileInfoData;
using clipboard::CFileInfo;
// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HWND g_hWnd;

std::wstring user_down_path;
CHelpCoInitialize ole_init;
std::shared_ptr<CFileInfo> file_info;


LPVOID pClipShareBuf;

std::once_flag flag;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void SetDataFromClip(std::shared_ptr<CFileInfo>& file_infos);

BOOL IsoTimeToFileTime(const char* isoTime, FILETIME* ft);

void CreateDumpFile(const std::string& dmp_file_path, PEXCEPTION_POINTERS excep,
    MINIDUMP_TYPE dmp_type)
{
    auto free = [](HANDLE handle)
    {
        if (handle)
            CloseHandle(handle);
    };

    HANDLE dump_file =
        ::CreateFileA(dmp_file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    std::unique_ptr<void, decltype(free)> hhh(dump_file, free);
    DWORD err_code = 0;
    if (INVALID_HANDLE_VALUE == dump_file)
    {
        err_code = GetLastError();
        LOGGER_ERROR("file_name=%s,errcode=%u", dmp_file_path.c_str(), err_code);
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exp_info = { 0 };
    exp_info.ThreadId = GetCurrentThreadId();
    exp_info.ExceptionPointers = excep;
    exp_info.ClientPointers = TRUE;
    HANDLE process = GetCurrentProcess();
    DWORD process_id = GetCurrentProcessId();
    if (!MiniDumpWriteDump(process, process_id, dump_file, dmp_type,
        &exp_info, nullptr, nullptr))
    {
        LOGGER_ERROR("MiniDumpWriteDump() failed,file_name=%s,errcode=%u",
            dmp_file_path.c_str(), err_code);
        return;
    }
}

LONG WINAPI UnhandledExceptionFilterFunc(struct _EXCEPTION_POINTERS* exception_info)
{
    std::string dmp_file_path = "c:\\windows\\temp\\ClipBoard";
    SYSTEMTIME time = { 0 };
    GetLocalTime(&time);
    char time_str[MAX_PATH] = { 0 };
    sprintf_s(time_str, "%d-%d-%d %d %d %d %d",
        time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    dmp_file_path += time_str;
    dmp_file_path += ".dmp";
    CreateDumpFile(dmp_file_path, exception_info, MiniDumpWithFullMemory);
    return EXCEPTION_EXECUTE_HANDLER;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    // TODO: 在此处放置代码。
    init_logger("OleClip.txt");
    ::SetUnhandledExceptionFilter(UnhandledExceptionFilterFunc);
    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TESTCLIPBOARD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    
    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TESTCLIPBOARD));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTCLIPBOARD));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TESTCLIPBOARD);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

VOID parse_data_and_set_to_clip(char* pData)
{
    cJSON* root = NULL;
    if (user_down_path.empty())
    {
        LOGGER_ERROR("user_down_path is NULL");

        return;
    }
    do
    {
        file_info->reset();

        root = cJSON_Parse(pData);

        if (!root)
        {
            LOGGER_ERROR("parse json root error {}!", cJSON_GetErrorPtr());
            break;
        }
        
        DWORD size = cJSON_GetArraySize(root);
        for (int ind =0; ind < size ; ind++)
        {
            cJSON* item = cJSON_GetArrayItem(root, ind);
            if (!item)
            {
                LOGGER_ERROR("parse json item error {}!", cJSON_GetErrorPtr());
                break;
            }
            FileInfoData file;

            // 解析每个属性
            cJSON* name = cJSON_GetObjectItem(item, "name");
            if (name == NULL) {
                LOGGER_ERROR("parse json name error {}!", cJSON_GetErrorPtr());
                break;
            }
            file.name = A2U(name->valuestring);
            file.full_path = user_down_path + L"\\" + file.name;

            cJSON* relativePath = cJSON_GetObjectItem(item, "relativePath");
            if (name == NULL) {
                LOGGER_ERROR("parse json relativePath error {}!", cJSON_GetErrorPtr());
                break;
            }
            file.relative_path = A2U(relativePath->valuestring);

            cJSON* attributes = cJSON_GetObjectItem(item, "attributes");
            if (attributes == NULL) 
            {
                LOGGER_ERROR("parse attributes error {}!", cJSON_GetErrorPtr());
                break;
            }
            file.attributes = attributes->valueint;


            cJSON* isDirectory = cJSON_GetObjectItem(item, "isDirectory");
            if (isDirectory == NULL)
            {
                LOGGER_ERROR("parse isDirectory error {}!", cJSON_GetErrorPtr());
                break;
            } 
            file.is_directory = isDirectory->valueint?TRUE:FALSE;


            cJSON* created = cJSON_GetObjectItem(item, "created");
            if (created == NULL) {
                LOGGER_ERROR("parse created error {}!", cJSON_GetErrorPtr());
            }
            else
            {
                IsoTimeToFileTime(created->valuestring, &file.creation_time);
            }
            

            cJSON* modified = cJSON_GetObjectItem(item, "modified");
            if (modified == NULL){
                LOGGER_ERROR("parse modified error {}!", cJSON_GetErrorPtr());
            }
            else
            {
                IsoTimeToFileTime(modified->valuestring, &file.last_write_time);
            }
            
            file.last_write_time = file.last_write_time;


            cJSON* size_item = cJSON_GetObjectItem(item, "size");
            if (size) file.file_size.QuadPart = _strtoui64(size_item->valuestring, nullptr, 10);

            file_info->add_file_data_entry(file);
#ifdef OLE_DEBUG
            LOGGER_INFO("file name {}", U2A(file.name).c_str());
            LOGGER_INFO("file attributes {}", file.attributes);
            LOGGER_INFO("file created {} {}", file.creation_time.dwHighDateTime, file.creation_time.dwLowDateTime);
            LOGGER_INFO("file last_access_time {}  {}", file.last_access_time.dwHighDateTime, file.last_access_time.dwLowDateTime);
            LOGGER_INFO("file last_write_time {}  {}", file.last_write_time.dwHighDateTime, file.last_write_time.dwLowDateTime);
            LOGGER_INFO("file file_size {}", file.file_size.QuadPart);
#endif // OLE_DEBUG
            SendMessage(g_hWnd, WND_MSG_SET_CLIPBOARD, NULL, NULL);
        }
    } while (FALSE);


}

VOID set_files_info() 
{
    do 
    {
        DWORD bRet = WaitForSingleObject(g_clip_trans_file_info.get(), INFINITE);
        if (bRet == WAIT_OBJECT_0)
        {
            if (nullptr == pClipShareBuf) {
                LOGGER_ERROR("pClipShareBuf is null");
                Sleep(1000);
                continue;
            }
            char _clip_share_buf[CLIP_SHARE_BUF_SIZE] = { 0 };

            memcpy_s(_clip_share_buf, CLIP_SHARE_BUF_SIZE, pClipShareBuf, CLIP_SHARE_BUF_SIZE);
#ifdef OLE_DEBUG
            LOGGER_INFO("files_json_data is {}", _clip_share_buf);
#endif
            parse_data_and_set_to_clip(_clip_share_buf);

        }
        else
        {
            LOGGER_ERROR("WaitForSingleObject falied:{}", GetLastError());
           
        }
        if (g_clip_trans_file_info.get() == NULL || g_clip_trans_file_info.get() == INVALID_HANDLE_VALUE)
        {
            g_clip_trans_file_info.reset(OpenEvent(EVENT_ALL_ACCESS, FALSE, GLOBAL_CLIP_TRANS_FILE_INFO));
            if (g_clip_trans_file_info.get() == NULL || g_clip_trans_file_info.get() == INVALID_HANDLE_VALUE)
            {
                LOGGER_ERROR("OpenEvent g_clip_trans_file_info failed! errcode:{}", GetLastError());
            }
            Sleep(1000);
        }
    } while (TRUE);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!g_hWnd)
   {
      return FALSE;
   }

   g_clip_trans_file_info = make_shared_t<HANDLE>(OpenEvent(EVENT_ALL_ACCESS, FALSE, GLOBAL_CLIP_TRANS_FILE_INFO));
   if (g_clip_trans_file_info.get() == NULL || g_clip_trans_file_info.get() == INVALID_HANDLE_VALUE)
   {
       LOGGER_ERROR("OpenEvent g_clip_trans_file_info failed! errcode:{}", GetLastError());
   }

   g_clip_trans_file_data_begain = make_shared_t<HANDLE>(OpenEvent(EVENT_ALL_ACCESS, FALSE, GLOBAL_CLIP_TRANS_FILE_DATA_BEGAIN));
   if (g_clip_trans_file_data_begain.get() == NULL || g_clip_trans_file_data_begain.get() == INVALID_HANDLE_VALUE)
   {
       LOGGER_ERROR("OpenEvent g_clip_trans_file_data_begain failed! errcode:{}", GetLastError());
   }

   g_clip_trans_file_data_end = make_shared_t<HANDLE>(OpenEvent(EVENT_ALL_ACCESS, FALSE, GLOBAL_CLIP_TRANS_FILE_DATA_END));
   if (g_clip_trans_file_data_end.get() == NULL || g_clip_trans_file_data_end.get() == INVALID_HANDLE_VALUE)
   {
       LOGGER_ERROR("OpenEvent g_clip_trans_file_data_end failed! errcode:{}", GetLastError());
   }
   //g_clip_mutex = make_shared_t<HANDLE>(
   //    OpenMutex(MUTEX_ALL_ACCESS, FALSE, GLOBAL_CLIP_MUTEX)
   //); 

   //if (g_clip_mutex.get() == NULL || g_clip_mutex.get() == INVALID_HANDLE_VALUE)
   //{
   //    MyDebugPrintA("OpenMutex g_clip_mutex failed! errcode:%u ", GetLastError());
   //}

   g_clip_share_memery = make_shared_t<HANDLE>(
       OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, GLOBAL_CLIP_SHARE_MEMORY)
   );

   if (g_clip_share_memery.get() == NULL || g_clip_share_memery.get() == INVALID_HANDLE_VALUE)
       LOGGER_ERROR("OpenFileMapping g_clip_share_memery failed! errcode:{} ", GetLastError());

   pClipShareBuf = MapViewOfFile(
       g_clip_share_memery.get(),   // handle to map object
       FILE_MAP_ALL_ACCESS,         // read/write permission
       0,
       0,
       0);

   if (pClipShareBuf == NULL)
       LOGGER_ERROR("MapViewOfFile() failed: {}", GetLastError());

   // 创建线程异步等待被复制的文件信息
   std::call_once(flag, []() { std::thread(set_files_info).detach(); });

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);
   file_info  = std::make_shared<CFileInfo>();

   user_down_path = GetDownloadFolder();
   if (user_down_path.empty())
   {
       LOGGER_ERROR("get user_down_path failed. errcode is %u",GetLastError());
   }
   return TRUE;
}



VOID GetDataFromClip(std::shared_ptr<CFileInfo> & file_infos)
{   
    if (!OpenClipboard(NULL))
        return;

    HANDLE hDrop = GetClipboardData(CF_HDROP);
    if (hDrop == NULL)
    {
        CloseClipboard();
        return;
    }

    UINT fileCount = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < fileCount; i++)
    {
        FileInfoData fileInfo;
        WCHAR filePath[MAX_PATH];

        if (DragQueryFile((HDROP)hDrop, i, filePath, MAX_PATH))
        {
            fileInfo.full_path = filePath;
            fileInfo.name = PathFindFileName(filePath);
            
            WIN32_FILE_ATTRIBUTE_DATA fileData ;
            if (GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileData))
            {
                fileInfo.relative_path = fileInfo.name;
                fileInfo.attributes = fileData.dwFileAttributes;
                fileInfo.creation_time = fileData.ftCreationTime;
                fileInfo.last_access_time = fileData.ftLastAccessTime;
                fileInfo.last_write_time = fileData.ftLastWriteTime;
                fileInfo.file_size.LowPart = fileData.nFileSizeLow;
                fileInfo.file_size.HighPart = fileData.nFileSizeHigh;
                fileInfo.is_directory = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }
            //fixme: 这个要放前面, 目录本身也是一个文件（解决空目录的拷贝），目录文件拷贝要优先于目录中文件的拷贝，否则拷贝过程中会提示已存在该目录
            file_infos->add_file_data_entry(fileInfo);  

            if (fileInfo.is_directory)
            {
                file_infos->count_file_info_with_folder_path(filePath, fileInfo.relative_path);
            }
        }
    }

    CloseClipboard();
}

void SetDataFromClip(std::shared_ptr<CFileInfo>& file_infos)
{
    CComPtr<IDataObject> data_obj;

    HRESULT hr = clipboard::VirtualFileSrcStream_CreateInstance(IID_IDataObject, (void**)&data_obj,  file_infos);
    if (SUCCEEDED(hr)) {
        ::OleSetClipboard(data_obj);
    }

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static RECT s_rc = { 100, 50, 400, 70 };
	static bool s_pushdown = false;
    //FileInfoData fileInfo1;
    FILETIME file1;

    FileInfoData fileInfo2;

    FileInfoData fileInfo3;

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case ID_GETDATA:                // 获取剪切板中文件信息
                file_info->reset();
                GetDataFromClip(file_info);
                break;
            case ID_SETDATA:               //  先清空剪切板，然后将获取的数据设置到剪切板中
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    CloseClipboard();
                }
                else {
                    LOGGER_ERROR("Failed to open clipboard!errocode {}", GetLastError());
                }
                
                file1.dwLowDateTime = 4131053751;
                file1.dwHighDateTime = 31136851;
                //fileInfo1.full_path = L"C:\\Users\\admin\\Downloads\\1111测试递归文件夹";
                //fileInfo1.name = L"1111测试递归文件夹";
                //fileInfo1.relative_path = L"1111测试递归文件夹";
                //fileInfo1.attributes = 16;
                //fileInfo1.creation_time = file1;
                //fileInfo1.last_access_time = file1;
                //fileInfo1.last_write_time = file1;
                //fileInfo1.file_size.LowPart = 0;
                //fileInfo1.file_size.HighPart = 0;

                fileInfo2.full_path = L"C:\\Users\\admin\\Downloads\\[P.Y.G]IDA Pro 7.5 SP3(x86, x64, ARM, ARM64, PPC, PPC64, MIPS)+SDK+DOC.7z";
                fileInfo2.name = L"[P.Y.G]IDA Pro 7.5 SP3(x86, x64, ARM, ARM64, PPC, PPC64, MIPS)+SDK+DOC.7z";
                fileInfo2.relative_path = L"[P.Y.G]IDA Pro 7.5 SP3(x86, x64, ARM, ARM64, PPC, PPC64, MIPS)+SDK+DOC.7z";
                fileInfo2.attributes = 32;
                fileInfo2.creation_time = file1;
                fileInfo2.last_access_time = file1;
                fileInfo2.last_write_time = file1;
                fileInfo2.file_size.LowPart = 389947330;
                fileInfo2.file_size.HighPart = 0;
                file_info->add_file_data_entry(fileInfo2);

                fileInfo3.full_path = L"C:\\Users\\admin\\Downloads\\Tools.7z";
                fileInfo3.name = L"Tools.7z";
                fileInfo3.relative_path = L"Tools.7z";
                fileInfo3.attributes = 32;
                fileInfo3.creation_time = file1;
                fileInfo3.last_access_time = file1;
                fileInfo3.last_write_time = file1;
                fileInfo3.file_size.LowPart = 24980065;
                fileInfo3.file_size.HighPart = 0;

               
                file_info->add_file_data_entry(fileInfo3);

                SetDataFromClip(file_info);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WND_MSG_SET_CLIPBOARD:
        SetDataFromClip(file_info);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


// 将ISO 8601格式的时间字符串转换为FILETIME
BOOL IsoTimeToFileTime(const char* isoTime, FILETIME* ft) {
    SYSTEMTIME st = { 0 };
    int year, month, day, hour, minute, second;

    if (sscanf_s(isoTime, "%d-%d-%dT%d:%d:%d",
        &year, &month, &day, &hour, &minute, &second) != 6) {
        return FALSE;
    }

    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    st.wHour = (WORD)hour;
    st.wMinute = (WORD)minute;
    st.wSecond = (WORD)second;
    st.wMilliseconds = 0;

    // 先转换为SystemTime，再转换为FileTime
    FILETIME localft;
    if (!SystemTimeToFileTime(&st, &localft)) {
        return FALSE;
    }

    // 转换为UTC时间
    return LocalFileTimeToFileTime(&localft, ft);
}


std::wstring GetDownloadFolder() {
    wchar_t* path = nullptr;
    std::wstring result;

    // 获取下载文件夹路径
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        result = path;
        CoTaskMemFree(path);  // 释放内存
    }
    else
    {
        LOGGER_ERROR("SHGetKnownFolderPath failed {}!", GetLastError());
    }

    return result;
}