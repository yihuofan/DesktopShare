#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

template <typename T>
class ThreadSafeQueue
{
public:
    void push(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    bool wait_and_pop(T &value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]
                   { return !queue_.empty() || stop_flag_; });
        if (stop_flag_ && queue_.empty())
        {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_flag_ = true;
        }
        cond_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cond_;
    bool stop_flag_ = false;
};