#ifndef _JOB_QUEUE_HEADER_
#define _JOB_QUEUE_HEADER_
#include "CommonHead.h"

class AppInstall
{
public:
    int type_;
    std::string  appUuid_;

    AppInstall(const int& type, const std::string& appUuid) 
    {
        LOGGER_INFO("type is {}, appUuid is {}", type, appUuid);
        type_ = type;
        appUuid_ = appUuid;
    };
    ~AppInstall() { };

public:
    enum Result
    {
        DOWNLOADING = 0,
        DOWNLOAD_SUCCESSED,
        ERROR_DOWNLOAD,

        INSTALLING,
        INSTALL_SUCCESSED,
        ERROR_INSTALL,

        ERROR_MD5,
    };


    void Deal(std::function<void(int, std::string)> onResult) { 
        if (type_ % 2 ==0)
        {
            Sleep(500 * type_);
        }
        onResult((Result::DOWNLOADING + type_%7), "Downloading...");
    };
};

typedef struct stJobData {
    int type;
    std::string appUuid;
    std::function<void(int, std::string)> onResult;
} JobData;

class JobQueue
{
private:
    JobQueue();
    ~JobQueue();
public:
    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;
    static JobQueue* getInstance();
    void Start();
    void Stop();
    bool CreateJob(int type, std::string appUuid,
        std::function<void(int, std::string)> onResult);
private:
    void Run();
private:
    std::atomic_bool running_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<JobData> queue_;
};

#endif