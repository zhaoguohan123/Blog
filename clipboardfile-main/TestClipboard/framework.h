// header.h: 标准系统包含文件的包含文件，
// 或特定于项目的包含文件
//

#pragma once

#define WINVER 0x0600          // Windows Vista 或更高版本
#define _WIN32_WINNT 0x0600    // Windows Vista 或更高版本


#include "targetver.h"

#include <windows.h>
#include <tchar.h>
#include <winerror.h>
#include <strsafe.h>
#include <DbgHelp.h>

#include <initguid.h>
#include <Guiddef.h>
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>
#include <comdef.h>

#include <atlbase.h>
#include <atlcom.h>

#include <Shldisp.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <knownfolders.h> 

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <mutex>

#include "logger.h"
#include "cjson.h"

// 8. 添加需要的库
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Dbghelp.lib")

DEFINE_GUID(CLSID_ProgressDialog, 0xf8383852, 0xfcd3, 0x11d1, 0xa6, 0xb9, 0x0, 0x60, 0x97, 0xdf, 0x5b, 0xd4);

#define WND_MSG_SET_CLIPBOARD WM_USER+0x1

#define CLIP_SHARE_BUF_SIZE                     10240
#define GLOBAL_CLIP_SHARE_MEMORY                L"Global\\global_clip_share_MEMORY"
#define GLOBAL_CLIP_MUTEX                       L"Global\\global_clip_mutex"
#define GLOBAL_CLIP_TRANS_FILE_INFO             L"Global\\global_clip_trans_file_info"
#define GLOBAL_CLIP_TRANS_FILE_DATA_BEGAIN       L"Global\\global_clip_trans_file_data_begain"
#define GLOBAL_CLIP_TRANS_FILE_DATA_END         L"Global\\global_clip_trans_file_data_end"

template <typename T>
using shared_ptr_t = std::shared_ptr<std::remove_pointer_t<T>>;

template <typename T>
auto make_shared_t(T ptr) -> shared_ptr_t<T>
{
    return shared_ptr_t<T>(ptr, ReleaseResources<T>());
}

template <typename T>
class ReleaseResources
{
public:
    void operator()(T* res) const
    {
        delete res;
        res = NULL;
    }
};

template <>
class ReleaseResources<HANDLE>
{
public:
    void operator()(HANDLE res) const
    {
        if (res && res != INVALID_HANDLE_VALUE)
        {
            CloseHandle(res);
            res = NULL;
        }
    }
};
extern shared_ptr_t<HANDLE> g_clip_share_memery;
extern shared_ptr_t<HANDLE> g_clip_mutex;
extern shared_ptr_t<HANDLE> g_clip_trans_file_info;
extern shared_ptr_t<HANDLE> g_clip_trans_file_data_begain;
extern shared_ptr_t<HANDLE> g_clip_trans_file_data_end;

#define OLE_DEBUG