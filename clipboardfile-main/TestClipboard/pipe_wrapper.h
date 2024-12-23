#pragma once
#include <windows.h>
#include <fstream>
#include <system_error>
#include <vector>
#include "logger.h"

class PipeFileTransfer {
public:
    // 管道缓冲区大小
    static const DWORD BUFFER_SIZE = 1024;

    PipeFileTransfer(const std::wstring& pipeName);
    ~PipeFileTransfer();

    // 创建命名管道
    bool HelpCreateNamedPipe();
    // 服务端连接到命名管道
    bool ServerConnect();

    std::vector<BYTE> m_buffer;

    HANDLE m_hPipe;

    struct RetryConfig {
        DWORD maxRetries = 3;           // 最大重试次数
        DWORD retryIntervalMs = 1000;   // 重试间隔(毫秒)
        DWORD connectionTimeoutMs = 5000; // 连接超时时间
    } retryConfig;

    SECURITY_ATTRIBUTES sa = {};
    SECURITY_DESCRIPTOR sd = {};

   
    std::wstring m_pipeName;
    // 清理资源
    void Cleanup();
};