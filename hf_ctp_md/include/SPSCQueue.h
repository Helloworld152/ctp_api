#pragma once

#include <atomic>
#include <cstdlib>
#include <new>
#include <cassert>

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

template<typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity) : capacity_(capacity) {
        // 手动分配对齐内存
        // C++17 以前用 posix_memalign，C++17 可以用 std::aligned_alloc
        // 这里为了兼容性使用 posix_memalign
        void* ptr = nullptr;
        if (posix_memalign(&ptr, CACHELINE_SIZE, sizeof(T) * (capacity_ + 1)) != 0) {
            throw std::bad_alloc();
        }
        buffer_ = static_cast<T*>(ptr);
        
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    ~SPSCQueue() {
        free(buffer_);
    }

    // 强制内联
    inline bool push(const T& item) __attribute__((always_inline)) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % (capacity_ + 1);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    inline bool pop(T& item) __attribute__((always_inline)) {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer_[current_head];
        head_.store((current_head + 1) % (capacity_ + 1), std::memory_order_release);
        return true;
    }

private:
    T* buffer_; // 裸指针
    size_t capacity_;

    alignas(CACHELINE_SIZE) std::atomic<size_t> tail_;
    alignas(CACHELINE_SIZE) std::atomic<size_t> head_;
    
    char padding_[CACHELINE_SIZE - sizeof(std::atomic<size_t>)]; 
};