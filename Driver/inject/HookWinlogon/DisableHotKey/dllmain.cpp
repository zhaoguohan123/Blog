// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "..\..\..\..\ComLib\detours\x86\detours.h"
#include <WtsApi32.h>
#include <string>
#include "rpc.h"

#pragma comment(lib, "detours.lib")
#pragma comment(lib, "WtsApi32.lib")
#pragma comment(lib, "Rpcrt4.lib")

PVOID fpNdr64AsyncServerCallAll = NULL;
void StartHookingFunction();
void UnmappHookedFunction();
BOOL SvcMessageBox(LPSTR lpCap, LPSTR lpMsg, DWORD style, BOOL bWait, DWORD& result);

#define __RPC_FAR
#define RPC_MGR_EPV void
#define  RPC_ENTRY __stdcall

typedef void* LI_RPC_HANDLE;
typedef LI_RPC_HANDLE LRPC_BINDING_HANDLE;

typedef struct _LRPC_VERSION {
    unsigned short MajorVersion;
    unsigned short MinorVersion;
} LRPC_VERSION;

typedef struct _LRPC_SYNTAX_IDENTIFIER {
    GUID SyntaxGUID;
    LRPC_VERSION SyntaxVersion;
} LRPC_SYNTAX_IDENTIFIER, __RPC_FAR* LPRPC_SYNTAX_IDENTIFIER;

typedef struct _LRPC_MESSAGE
{
    LRPC_BINDING_HANDLE Handle;
    unsigned long DataRepresentation;// %lu
    void __RPC_FAR* Buffer;
    unsigned int BufferLength;
    unsigned int ProcNum;
    LPRPC_SYNTAX_IDENTIFIER TransferSyntax;
    RPC_SERVER_INTERFACE* RpcInterfaceInformation; // void __RPC_FAR*
    void __RPC_FAR* ReservedForRuntime;
    RPC_MGR_EPV __RPC_FAR* ManagerEpv;
    void __RPC_FAR* ImportContext;
    unsigned long RpcFlags;
} LRPC_MESSAGE, __RPC_FAR* LPRPC_MESSAGE;


//--------------------------------------------------
typedef void (RPC_ENTRY* __Ndr64AsyncServerCallAll)(
    LPRPC_MESSAGE pRpcMsg
    );

void RPC_ENTRY HookedNdr64AsyncServerCallAll(
    LPRPC_MESSAGE pRpcMsg
);

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    DisableThreadLibraryCalls(hModule);
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        StartHookingFunction();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        UnmappHookedFunction();
        break;
    }
    return TRUE;
}


// -------------------------------------------

BOOL SvcMessageBox(LPSTR lpCap, LPSTR lpMsg, DWORD style, BOOL bWait, DWORD& result)
{
    if (NULL == lpMsg || NULL == lpCap)
        return FALSE;
    result = 0;
    DWORD sessionXId = WTSGetActiveConsoleSessionId();
    return WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, sessionXId,
        lpCap, (DWORD)strlen(lpCap) * sizeof(DWORD),
        lpMsg, (DWORD)strlen(lpMsg) * sizeof(DWORD),
        style, 0, &result, bWait);
}

void StartHookingFunction()
{
    //开始事务
    DetourTransactionBegin();
    //更新线程信息  
    DetourUpdateThread(GetCurrentThread());


    fpNdr64AsyncServerCallAll =
        DetourFindFunction(
            "rpcrt4.dll",
            "Ndr64AsyncServerCallAll");

    //将拦截的函数附加到原函数的地址上,这里可以拦截多个函数。

    DetourAttach(&(PVOID&)fpNdr64AsyncServerCallAll,
        HookedNdr64AsyncServerCallAll);

    //结束事务
    DetourTransactionCommit();
}

void UnmappHookedFunction()
{
    //开始事务
    DetourTransactionBegin();
    //更新线程信息 
    DetourUpdateThread(GetCurrentThread());

    //将拦截的函数从原函数的地址上解除，这里可以解除多个函数。

    DetourDetach(&(PVOID&)fpNdr64AsyncServerCallAll,
        HookedNdr64AsyncServerCallAll);

    //结束事务
    DetourTransactionCommit();
}


void RPC_ENTRY HookedNdr64AsyncServerCallAll(
    LPRPC_MESSAGE pRpcMsg
)
{
    CHAR lpMsgCap[] = "Windows LogonManager";

    int nCode = -1; // 消息的解释器编码（对 MsgInterStr 字符串的索引）
    DWORD dwMsgResult = 0;     // WTSSendMessage 的返回值
    char bufferMask[4] = { 0 }; // 用于存储特征码
    const char MsgInterStr[23][45] = {
        "有程序请求以管理员身份启动",    // 0
        "检测到资源管理器重启",          // 1
        "已经处理权限提升的请求",        // 2
        "正在请求注销这台计算机",        // 3
        "正在请求重启这台计算机",        // 4
        "正在请求关闭这台计算机",        // 5
        "Ctrl+Shift+Esc 快捷键被按下",   // 6
        "Win+L 快捷键被按下",            // 7
        "已经从自动睡眠状态唤醒",        // 8
        "Ctrl+Alt+Del 快捷键被按下",     // 9
        "已经从深度睡眠状态唤醒",        // 10
        "正在进行切换用户操作 1",        // 11
        "正在进入自动睡眠状态",          // 12
        "正在进行切换用户操作 2",        // 13
        "已经从深度睡眠状态唤醒",        // 14
        "正在进入深度睡眠状态 1",        // 15
        "请求的操作已经完成",            // 16
        "正在进入深度睡眠状态 2",        // 17
        "正在进行切换用户操作 3",        // 18
        "检测到 DWM 重启",               // 19
        "正在向 DWM 发送状态请求",       // 20
        "",            // 21
        "目前未知的处理操作码"           // 22
    }; // 存储消息的描述字符串

    BOOL bWaitForCloseBox = FALSE; // 标识是否等待消息对话框的返回

    // 基址
    uint64_t iBaseAddress = reinterpret_cast<uintptr_t>(pRpcMsg->Buffer);

    // 根据缓冲区特征判断是否符合要求，长度不是12字节的调用不进行判断(2024.04.17 修改)
    if (pRpcMsg->BufferLength != 12 || pRpcMsg->Buffer == nullptr)
    {
        ((__Ndr64AsyncServerCallAll)fpNdr64AsyncServerCallAll)(pRpcMsg);
        return;
    }

    // 从内存中复制特征码（低位 + 高位）
    memcpy(&bufferMask[0], reinterpret_cast<PVOID>(iBaseAddress), sizeof(char));
    memcpy(&bufferMask[1], reinterpret_cast<PVOID>(iBaseAddress + 1), sizeof(char));
    memcpy(&bufferMask[2], reinterpret_cast<PVOID>(iBaseAddress + 4), sizeof(char));

    switch (bufferMask[0]) {
    case 0:
        // 请求以管理员身份启动
        bWaitForCloseBox = TRUE;
        nCode = 0;
        break;
    case 1:
        switch (bufferMask[1]) {
        case 4:
            // 重启资源管理器
            bWaitForCloseBox = TRUE;
            nCode = 1;
            break;
        case 5:
            // 已经处理权限提升的请求
            bWaitForCloseBox = FALSE;
            nCode = 2;
            break;
        case 0:
            switch (bufferMask[2]) {
            case 0:
                // 注销计算机
                bWaitForCloseBox = TRUE;
                nCode = 3;
                break;
            case 3:
                // 重启计算机
                bWaitForCloseBox = TRUE;
                nCode = 4;
                break;
            case 9:
                // 关闭计算机
                bWaitForCloseBox = TRUE;
                nCode = 5;
                break;
            default: /* 处理未知操作通知 */
                bWaitForCloseBox = FALSE;
                nCode = -1001;
                break;
            }
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1000;
            break; // 结束分支
        }
        break;
    case 4:
        switch (bufferMask[2]) {
        case 4:
            // Ctrl + Shift + Esc 快捷键被按下
            bWaitForCloseBox = TRUE;
            nCode = 6;
            break;
        case 5:
            // Win + L 快捷键被按下
            bWaitForCloseBox = TRUE;
            nCode = 7;
            break;
        case 0:
            switch (bufferMask[1]) {
            case 1:
                // 从自动睡眠唤醒（实验性）
                bWaitForCloseBox = FALSE;
                nCode = 8;
                break;
            case 4:
                // Ctrl + Alt + Del 快捷键被按下
                bWaitForCloseBox = TRUE;
                nCode = 9;
                break;
            default: /* 处理未知操作通知 */
                bWaitForCloseBox = FALSE;
                nCode = -1003;
                break;
            }
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1002;
            break; // 结束分支
        }
        break;
    case 3:
        switch (bufferMask[1]) {
        case 1:
            // 从深度睡眠中唤醒（L1，实验性）
            bWaitForCloseBox = FALSE;
            nCode = 10;
            break;
        case 4:
            // 注销用户凭证后（K1，实验性）
            bWaitForCloseBox = FALSE;
            nCode = 11;
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1004;
            break; // 结束分支
        }
        break;
    case 5:
        switch (bufferMask[1]) {
        case 1:
            // 自动睡眠
            bWaitForCloseBox = FALSE;
            nCode = 12;
            break;
        case 2:
            // 切换用户
            bWaitForCloseBox = FALSE;
            nCode = 13;
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1006;
            break; // 结束分支
        }
        break;
    case 6:
        switch (bufferMask[2]) {
        case 0:
            // 从深度睡眠中唤醒（L2，实验性）
            bWaitForCloseBox = FALSE;
            nCode = 14;
            break;
        case 2:
            // 进入深度睡眠（L3，实验性）
            bWaitForCloseBox = FALSE;
            nCode = 15;
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1008;
            break; // 结束分支
        }
        break;
    case 7:
        switch (bufferMask[1]) {
        case 4:
            // 请求的操作已经完成。（除了提升权限的其他操作，完成处理时都会发生该消息）
            bWaitForCloseBox = FALSE;  // TODO: Patch(20240111001) -- 将该值设置为 TRUE，导致注销时进程退出失败，若设置为 FALSE ，弹窗未能显示，但进程正常退出（判断为多线程处理问题）
            nCode = 16;
            break;
        case 1:
            if (bufferMask[2] == 2) // 进入深度睡眠（L4，实验性）
            {
                bWaitForCloseBox = FALSE;
                nCode = 17;
            }
            else /* 处理未知操作通知 */
            {
                bWaitForCloseBox = FALSE;
                nCode = -1011;
            }
            break;
        default: /* 处理未知操作通知 */
            bWaitForCloseBox = FALSE;
            nCode = -1010;
            break; // 结束分支
        }
        break;
    case 2:
        if (bufferMask[1] == 2 && bufferMask[2] == 0) // 注销用户凭证后（K2，实验性）
        {
            bWaitForCloseBox = FALSE;
            nCode = 18;
        }
        else  /* 处理未知操作通知 */
        {
            bWaitForCloseBox = FALSE;
            nCode = -1012;
        }
        break;
    case 12:
        if (bufferMask[1] == 4) // DWM 崩溃重启
        {
            bWaitForCloseBox = TRUE;
            nCode = 19;
        }
        else  /* 处理未知操作通知 */
        {
            bWaitForCloseBox = FALSE;
            nCode = -1014;
        }
        break;
    case 13:
        if (bufferMask[1] == 4 && bufferMask[2] == 0) // 注销用户凭证后 DWM 处理（K2，实验性）
        {
            bWaitForCloseBox = FALSE;
            nCode = 20;
        }
        else  /* 处理未知操作通知 */
        {
            bWaitForCloseBox = FALSE;
            nCode = -1016;
        }
        break;
        /* case -1: / 0xFFFFFFFF /
             if (bufferMask[1] == -1 && bufferMask[2] == -1) // 切换用户
             {
                 bWaitForCloseBox = FALSE;
                 nCode = 21;
             }
             else  / 处理未知操作通知 /
             {
                 bWaitForCloseBox = FALSE;
                 nCode = -1018;
             }
             break; */  // 2024.04.17: 排除了操作不是 12 字节的缓冲区
    default:
        /* 处理未知操作通知 */
        bWaitForCloseBox = FALSE;
        nCode = -1020;
        break;
    }

    // 弹窗显示结果
    char lpMsgStr[45] = { 0 };
    if (nCode >= 0) strcpy_s(lpMsgStr, MsgInterStr[nCode]);
    else sprintf_s(lpMsgStr, "%s:%d,%d,%d\n", MsgInterStr[22],
        bufferMask[0], bufferMask[1], bufferMask[2]);

    DWORD MsgStyle = MB_APPLMODAL | MB_ICONINFORMATION |
        (bWaitForCloseBox ? MB_YESNO : MB_OK);

    // 弹窗处理
    if (SvcMessageBox(lpMsgCap, lpMsgStr,
        MsgStyle,
        bWaitForCloseBox, dwMsgResult))
    {
        switch (dwMsgResult) {
        case IDASYNC:
        case IDYES:
            if (nCode == 2 || nCode == 16) Sleep(300); // 等待 UAC 窗口关闭
            break;
        default:
            /*
            * 修改 12 字节提权信息，导致提权失败，
            * 否则会一直等待提权，导致调用方死锁
            * 经过测试，任何调用，前 12 字节
            * 每个字节修改为 0xff 或者 0x0f 都可以阻止调用
            * 不建议直接将 BufferLength 设置为 0，
            * 那样的话和其他调试程序兼容性较差。
            * 2022.01.12
            */
            memset(pRpcMsg->Buffer, 0xff, 12);
            break;
        }
        // 调用原函数
        return ((__Ndr64AsyncServerCallAll)fpNdr64AsyncServerCallAll)(pRpcMsg);
    }
}