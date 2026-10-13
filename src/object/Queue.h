
/**
 * @file Queue.h
 * @brief 线程安全队列，用于 IOCP 网络层发送/接收缓冲。
 */

#pragma once

#include <cstdint>
#include <queue>
#include <mutex>
#include <memory>
#include <windows.h>

class CWHQueue {
private:
    std::queue<uint8_t*> queue_;
    mutable std::mutex mutex_;

public:
    CWHQueue() = default;
    ~CWHQueue() { clear_all(); }

    // 禁止拷贝
    CWHQueue(const CWHQueue&)            = delete;
    CWHQueue& operator=(const CWHQueue&) = delete;

    bool push_q(uint8_t* lpbtQ)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(lpbtQ);
        return true;
    }

    uint8_t* pop_q()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr;
        }
        uint8_t* data = queue_.front();
        queue_.pop();
        return data;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear_all()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
};