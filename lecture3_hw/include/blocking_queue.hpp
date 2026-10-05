// Provided infrastructure. You do not need to modify this file.

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template <class T>
class BlockingQueue
{
public:
    void push(T value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_)
            {
                return;
            }
            queue_.push(std::move(value));
        }
        ready_.notify_one();
    }

    bool pop(T &value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this]
                    { return closed_ || !queue_.empty(); });
        if (queue_.empty())
        {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool closed_ = false;
};
