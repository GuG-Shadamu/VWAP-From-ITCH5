/*
 * @Author: Tairan Gao
 * @Date:   2024-02-10 12:26:11
 * @Last Modified by:   Tairan Gao
 * @Last Modified time: 2024-02-11 15:12:29
 */

#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

namespace GtrTrex {

    template<typename T>
    class ThreadSafeQueue {
    private:
        mutable std::mutex mutex_;
        std::condition_variable cond_var_;
        std::queue<T> queue_;
        bool finished_ = false;

    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
            cond_var_.notify_one();
        }

        bool pop(T& value) {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this] { return !queue_.empty() || finished_; });
            if (queue_.empty()) return false;
            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void finish() {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_ = true;
            cond_var_.notify_all();
        }

        bool isFinished() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return finished_;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        template<typename... Args>
        void emplace(Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.emplace(std::forward<Args>(args)...);
            cond_var_.notify_one();
        }
    };

    bool bigEndian() {
        if constexpr (std::endian::native == std::endian::big) {
            return true;
        }
        return false;
    }
}

#endif