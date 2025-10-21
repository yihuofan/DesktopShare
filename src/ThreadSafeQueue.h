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
        // 等待直到队列非空或者收到停止信号
        cond_.wait(lock, [this]
                   { return !queue_.empty() || stop_flag_; });
        // 如果是因为停止信号被唤醒，并且队列为空，则返回 false
        if (stop_flag_ && queue_.empty())
        {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 唤醒可能在 wait_and_pop 中等待的线程
    void wake()
    {
        cond_.notify_one();
    }

    // 设置停止标志并唤醒所有等待的线程
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_flag_ = true;
        }
        cond_.notify_all(); // 确保所有等待的线程都能被唤醒并检查 stop_flag_
    }

    // 检查队列是否为空 (线程安全)
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cond_;
    bool stop_flag_ = false; // 停止标志
};