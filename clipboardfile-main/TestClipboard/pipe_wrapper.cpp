#include "pipe_wrapper.h"

PipeFileTransfer::PipeFileTransfer(const std::wstring& pipeName) : m_hPipe(INVALID_HANDLE_VALUE), m_pipeName(pipeName)
{
    m_buffer.resize(BUFFER_SIZE);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = &sd;
}

PipeFileTransfer::~PipeFileTransfer() {
    LOGGER_INFO("~PipeFileTransfer");
    Cleanup();
}

bool PipeFileTransfer::HelpCreateNamedPipe() {


    m_hPipe = ::CreateNamedPipe(
        m_pipeName.c_str(),
        PIPE_ACCESS_DUPLEX, // | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, //| PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,                  // 最大实例数
        BUFFER_SIZE,       // 输出缓冲区大小
        BUFFER_SIZE,       // 输入缓冲区大小
        0,                 // 默认超时
        &sa            // 默认安全属性
    );

    if (m_hPipe == INVALID_HANDLE_VALUE) {
        LOGGER_ERROR("CreateNamedPipe falied {}", GetLastError());
        return false;
    }
    return true;
}

void PipeFileTransfer::Cleanup() {
    LOGGER_INFO("pipe Cleanup");
    
    if (m_hPipe == INVALID_HANDLE_VALUE) {
        return;
    }

    if (!DisconnectNamedPipe(m_hPipe)) {
        LOGGER_ERROR("DisconnectNamedPipe failed!!");
    }

    CloseHandle(m_hPipe);
    m_hPipe = INVALID_HANDLE_VALUE;
}

bool PipeFileTransfer::ServerConnect() {

    // 尝试连接命名管道，设置重试次数和等待时间
    DWORD retryCount = 0;
    bool connected = false;
    DWORD lastError = 0;

    while (retryCount <= retryConfig.maxRetries && !connected) {

        BOOL result = ConnectNamedPipe(m_hPipe, nullptr);
        lastError = GetLastError();

        if (result) {
            connected = true;
            break;
        }
        else {
            switch (lastError) {
            case ERROR_PIPE_CONNECTED:
                connected = true;
                break;

            case ERROR_IO_PENDING:
                LOGGER_ERROR("Connection attempt timed out");
                break;

            default:
                LOGGER_ERROR("ConnectNamedPipe failed with error: {}", lastError);
                break;
            }
        }

        if (!connected) {
            retryCount++;
            LOGGER_INFO("Retry {} of {}", retryCount, retryConfig.maxRetries);
        }
    }

    if (!connected) {
        LOGGER_ERROR("Failed to connect after {} retries. Last error: {}",
            retryConfig.maxRetries, lastError);
    }

    return connected;
}
