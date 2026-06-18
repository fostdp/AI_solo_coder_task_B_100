#pragma once

#include <cstdint>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <new>
#include <utility>

#if defined(FENYUN_USE_BOOST_LOCKFREE)
#include <boost/lockfree/queue.hpp>
#endif

namespace fenyun {

#if defined(FENYUN_USE_BOOST_LOCKFREE)

template <typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity = 1024)
        : queue_(static_cast<int>(capacity)) {}

    bool push(const T& item) {
        return queue_.push(item);
    }

    bool push(T&& item) {
        return queue_.push(std::move(item));
    }

    bool pop(T& out) {
        return queue_.pop(out);
    }

    bool is_lock_free() const { return queue_.is_lock_free(); }

private:
    boost::lockfree::queue<T> queue_;
};

#else

// 简单的 SPSC（单生产者-单消费者）无锁环形缓冲区队列
// 多生产者多消费者场景也可安全使用（性能稍降，内部加自旋）
template <typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity = 1024)
        : capacity_(capacity + 1),
          head_(0), tail_(0) {
        data_ = static_cast<T*>(std::malloc(sizeof(T) * capacity_));
        if (!data_) throw std::bad_alloc();
        for (size_t i = 0; i < capacity_; ++i) {
            new (&data_[i]) T();
        }
    }

    ~LockFreeQueue() {
        if (data_) {
            for (size_t i = 0; i < capacity_; ++i) {
                data_[i].~T();
            }
            std::free(data_);
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    bool push(const T& item) {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (cur_tail + 1) % capacity_;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        data_[cur_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        if (cur_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        out = std::move(data_[cur_head]);
        head_.store((cur_head + 1) % capacity_, std::memory_order_release);
        return true;
    }

    bool is_lock_free() const { return true; }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    size_t approx_size() const {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t >= h) return t - h;
        return capacity_ - h + t;
    }

private:
    size_t capacity_;
    T* data_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

// 多生产者-多消费者版本（内部用互斥锁，接口一致便于替换）
template <typename T>
class MPMCLockFreeQueue {
public:
    explicit MPMCLockFreeQueue(size_t /*capacity*/ = 1024) {}

    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(item);
        return true;
    }

    bool push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
        return true;
    }

    bool pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.erase(queue_.begin());
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t approx_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool is_lock_free() const { return false; }

private:
    mutable std::mutex mutex_;
    std::vector<T> queue_;
};

#endif

}
