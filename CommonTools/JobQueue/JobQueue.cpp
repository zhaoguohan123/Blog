#include "JobQueue.h"

JobQueue* JobQueue::getInstance()
{
    static JobQueue instance;
    return &instance;
}

JobQueue::JobQueue()
{
    Start();
}

JobQueue::~JobQueue()
{
    Stop();
}

void JobQueue::Start()
{
    if (!running_)
    {
        running_ = true;
        thread_ = std::thread(&JobQueue::Run, this);
    }
}

void JobQueue::Stop()
{
    if (running_)
    {
        running_ = false;
        cv_.notify_one();
        thread_.join();
    }
}

bool JobQueue::CreateJob(
    int type,
    std::string appUuid, 
    std::function<void(int, std::string)> onResult)
{
    std::unique_lock<std::mutex> lk(mtx_);
    queue_.push({type, appUuid, onResult });
    LOGGER_INFO("push type:{}", type);
    lk.unlock();
    cv_.notify_one();
    return true;
}

void JobQueue::Run()
{
    while (running_)
    {
        bool has_job = false;
        JobData job;

        {  // scope for lock
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.size() > 0)
            {
                job = queue_.front();
                queue_.pop();
                has_job = true;
            }
        }

        if (has_job)
        {
            AppInstall appi(job.type, job.appUuid);
            appi.Deal(job.onResult);
        }
        else
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return !running_ || queue_.size() > 0; });
        }
    }
}