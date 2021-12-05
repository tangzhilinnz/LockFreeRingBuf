/*
  Copyright (c) 2021 wujiaxu <void00@foxmail.com>
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __RING_QUEUE_H
#define __RING_QUEUE_H

#include <stdint.h>
#include <string.h>
#include <atomic>
#include <new>
#include <utility>
#include <chrono>
#include <thread>

#define __CHECK_POWER_OF_2(x) ((x) > 0 && ((x) & ((x) - 1)) == 0)

namespace { // not for user
    template <typename T, unsigned int capacity>
    class __spsc_queue;
}

// replace boost/lockfree/spsc_queue.hpp
// The spsc_queue class provides a single-producer/single-consumer fifo queue
// pushing and popping is wait-free
template <typename T, unsigned int capacity>
class spsc_queue
{
public:
    spsc_queue() { }
    ~spsc_queue() { }
    spsc_queue(const spsc_queue&) = delete;
    spsc_queue(spsc_queue&&) = delete;
    spsc_queue& operator=(const spsc_queue&) = delete;
    spsc_queue& operator=(spsc_queue&&) = delete;

public:
    int read_available() const;

    bool try_push(const T& t);
    bool try_push(T&& t);
    void wait_push(const T& t);
    void wait_push(T&& t);
    bool pop(T& ret);

    // Attempt to reserve contiguous space for the producer without making 
    // it visible to the consumer. The caller should invoke finish() before
    // invoking reserve() again to make the bytes reserved visible to the 
    // consumer.
    // This mechanism is in place to allow the producer to initialize the 
    // contents of the reservation before exposing it to the consumer.This
    // function will block behind the consumer if there's not enough space.
    T* reserve(int n);
    void finish(int n);
    // =====================================================

    void wait_push(T* ret, int n);

    int push(const T* ret, int n);
    int push(T* ret, int n);
    int pop(T* ret, int n);

private:
    __spsc_queue<T, capacity> queue_;
};


////
// template inl, not for user
template <typename T, unsigned int capacity>
int spsc_queue<T, capacity>::read_available() const
{
    return queue_.read_available();
}

template <typename T, unsigned int capacity>
bool spsc_queue<T, capacity>::try_push(const T& t)
{
    return queue_.try_push(t);
}

template <typename T, unsigned int capacity>
bool spsc_queue<T, capacity>::try_push(T&& t)
{
    return queue_.try_push(std::move(t));
}

template <typename T, unsigned int capacity>
void spsc_queue<T, capacity>::wait_push(const T& t)
{
    queue_.wait_push(t);
}

template <typename T, unsigned int capacity>
void spsc_queue<T, capacity>::wait_push(T&& t)
{
    queue_.wait_push(std::move(t));
}

template <typename T, unsigned int capacity>
bool spsc_queue<T, capacity>::pop(T& t)
{
    return queue_.pop(t);
}

template <typename T, unsigned int capacity>
T* spsc_queue<T, capacity>::reserve(int n)
{
    return queue_.reserve(n);
}

template <typename T, unsigned int capacity>
void spsc_queue<T, capacity>::finish(int n)
{
    queue_.finish(n);
}

template <typename T, unsigned int capacity>
void spsc_queue<T, capacity>::wait_push(T* ret, int n)
{
    queue_.wait_push(ret, n);
}

template <typename T, unsigned int capacity>
int spsc_queue<T, capacity>::push(const T* ret, int n)
{
    return queue_.push(ret, n);
}

template <typename T, unsigned int capacity>
int spsc_queue<T, capacity>::push(T* ret, int n)
{
    return queue_.push(ret, n);
}

template <typename T, unsigned int capacity>
int spsc_queue<T, capacity>::pop(T* ret, int n)
{
    return queue_.pop(ret, n);
}


// Number of bytes in a cache-line in our x86 machines.
static const uint32_t BYTES_PER_CACHE_LINE = 64;


namespace {
    struct __fifo
    {
        unsigned int in;
        // An extra cache-line to separate the variables that are primarily
        // updated/read by the producer (above) from the ones by the
        // consumer(below)
        char cacheLineSpacer[1 * BYTES_PER_CACHE_LINE];
        unsigned int mask;
        unsigned int size;
        unsigned int out;
        void* buffer;
    };

    template <typename T, bool is_trivial = std::is_trivial<T>::value>
    class __spsc_worker;

    template <typename T, unsigned int capacity>
    class __spsc_queue
    {
    public:
        __spsc_queue();
        ~__spsc_queue() { }
        __spsc_queue(const __spsc_queue&) = delete;
        __spsc_queue(__spsc_queue&&) = delete;
        __spsc_queue& operator=(const __spsc_queue&) = delete;
        __spsc_queue& operator=(__spsc_queue&&) = delete;

    public:
        int read_available() const;

        bool try_push(const T& t);
        bool try_push(T&& t);
        void wait_push(const T& t);
        void wait_push(T&& t);
        bool pop(T& ret);

        T* reserve(int n);
        void finish(int n);

        void wait_push(T* ret, int n);
        int push(const T* ret, int n);
        int push(T* ret, int n);
        int pop(T* ret, int n);

    private:
        //unsigned int minFreeSpace_;
        __fifo fifo_;
        T arr_[capacity];

        using WORKER = __spsc_worker<T, std::is_trivial<T>::value>;
        static_assert(__CHECK_POWER_OF_2(capacity), "Capacity MUST power of 2");
    };

    template <typename T, unsigned int capacity>
    __spsc_queue<T, capacity>::__spsc_queue()
    {
        fifo_.in = 0;
        fifo_.out = 0;
        fifo_.mask = capacity - 1;
        fifo_.size = capacity;
        fifo_.buffer = &arr_;
        //minFreeSpace_ = capacity;
    }

    template <typename T, unsigned int capacity>
    int __spsc_queue<T, capacity>::read_available() const
    {
        return fifo_.in - fifo_.out;
    }

    template <typename T, unsigned int capacity>
    bool __spsc_queue<T, capacity>::try_push(const T& t)
    {
        // if (minFreeSpace_ > 0) {
        //    arr_[fifo_.in & (capacity - 1)] = t;
        //    --minFreeSpace_;
        //    asm volatile("sfence" ::: "memory");
        //    ++fifo_.in;
        //    return true;
        // }
        // else {
        //    if (capacity - fifo_.in + fifo_.out == 0)
        //        return false;
        //    minFreeSpace_ = capacity - fifo_.in + fifo_.out;
        //    arr_[fifo_.in & (capacity - 1)] = t;
        //    --minFreeSpace_;
        //    asm volatile("sfence" ::: "memory");
        //    //Fence::sfence();
        //    ++fifo_.in;
        //    return true;
        // }

        if (capacity - fifo_.in + fifo_.out == 0)
            return false;

        arr_[fifo_.in & (capacity - 1)] = t;

        asm volatile("sfence" ::: "memory");
        //Fence::sfence();

        ++fifo_.in;

        return true;
    }

    template <typename T, unsigned int capacity>
    bool __spsc_queue<T, capacity>::try_push(T&& t)
    {
        // if (minFreeSpace_ > 0) {
        //     arr_[fifo_.in & (capacity - 1)] = t;
        //     --minFreeSpace_;
        //     asm volatile("sfence" ::: "memory");
        //     ++fifo_.in;
        //     return true;
        // }
        // else {
        //     if (capacity - fifo_.in + fifo_.out == 0)
        //         return false;
        //     minFreeSpace_ = capacity - fifo_.in + fifo_.out;
        //     arr_[fifo_.in & (capacity - 1)] = std::move(t);
        //     --minFreeSpace_;
        //     asm volatile("sfence" ::: "memory");
        //     ++fifo_.in;
        //     return true;
        // }

        if (capacity - fifo_.in + fifo_.out == 0)
            return false;

        arr_[fifo_.in & (capacity - 1)] = std::move(t);

        asm volatile("sfence" ::: "memory");
        //Fence::sfence();

        ++fifo_.in;

        return true;
    }

    template <typename T, unsigned int capacity>
    void __spsc_queue<T, capacity>::wait_push(const T& t)
    {
        while (capacity - fifo_.in + fifo_.out == 0) {
            //std::this_thread::sleep_for(std::chrono::microseconds(1));
            std::cout << "";
        }

        arr_[fifo_.in & (capacity - 1)] = t;

        asm volatile("sfence" ::: "memory");
        //Fence::sfence();

        ++fifo_.in;
    }

    template <typename T, unsigned int capacity>
    void __spsc_queue<T, capacity>::wait_push(T&& t)
    {
        while (capacity - fifo_.in + fifo_.out == 0) {
            //std::this_thread::sleep_for(std::chrono::microseconds(1));
            std::cout << "";
        }

        arr_[fifo_.in & (capacity - 1)] = std::move(t);

        asm volatile("sfence" ::: "memory");
        //Fence::sfence();

        ++fifo_.in;
    }

    template <typename T, unsigned int capacity>
    bool __spsc_queue<T, capacity>::pop(T& t)
    {
        if (fifo_.in - fifo_.out == 0)
            return false;

        t = std::move(arr_[fifo_.out & (capacity - 1)]);

        asm volatile("sfence" ::: "memory");
        //Fence::sfence();
        //Fence::lfence();

        ++fifo_.out;

        return true;
    }

    template <typename T, unsigned int capacity>
    T* __spsc_queue<T, capacity>::reserve(int n) 
    {
        while (capacity - fifo_.in + fifo_.out < n) {
            std::cout << "";
        }

        return arr_ + fifo_.in & fifo_.mask;
    }

    template <typename T, unsigned int capacity>
    void __spsc_queue<T, capacity>::finish(int n)
    {
        // Ensures producer finishes writes before bump
        asm volatile("sfence" ::: "memory");
        fifo_.in += n;
    }

    template <typename T, unsigned int capacity>
    void __spsc_queue<T, capacity>::wait_push(T* ret, int n)
    {
        WORKER::wait_push(&fifo_, ret, n);
    }

    template <typename T, unsigned int capacity>
    int __spsc_queue<T, capacity>::push(const T* ret, int n)
    {
        return WORKER::push(&fifo_, ret, n);
    }

    template <typename T, unsigned int capacity>
    int __spsc_queue<T, capacity>::push(T* ret, int n)
    {
        return WORKER::push(&fifo_, ret, n);
    }

    template <typename T, unsigned int capacity>
    int __spsc_queue<T, capacity>::pop(T* ret, int n)
    {
        return WORKER::pop(&fifo_, ret, n);
    }



    static inline unsigned int _min(unsigned int a, unsigned int b)
    {
        return (a < b) ? a : b;
    }

    template <typename T>
    class __spsc_worker<T, true>
    {
    public:
  
        static void wait_push(__fifo* fifo, T* ret, int n)
        {
            if (n == 0)
                return;

            while (fifo->size - fifo->in + fifo->out < (unsigned int)n) {
                std::cout << "";
            }

            unsigned int idx_in = fifo->in & fifo->mask;
            unsigned int l = _min(n, fifo->size - idx_in);
            T* arr = (T*)fifo->buffer;

            memcpy(arr + idx_in, ret, l * sizeof(T));
            memcpy(arr, ret + l, (n - l) * sizeof(T));

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();

            fifo->in += n;
        }

        static int push(__fifo* fifo, const T* ret, int n)
        {
            unsigned int len = _min(n, fifo->size - fifo->in + fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_in = fifo->in & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_in);
            T* arr = (T*)fifo->buffer;

            memcpy(arr + idx_in, ret, l * sizeof(T));
            memcpy(arr, ret + l, (len - l) * sizeof(T));

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();

            fifo->in += len;

            return len;
        }

        static int push(__fifo* fifo, T* ret, int n)
        {
            unsigned int len = _min(n, fifo->size - fifo->in + fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_in = fifo->in & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_in);
            T* arr = (T*)fifo->buffer;

            memcpy(arr + idx_in, ret, l * sizeof(T));
            memcpy(arr, ret + l, (len - l) * sizeof(T));

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();

            fifo->in += len;

            return len;
        }

        static int pop(__fifo* fifo, T* ret, int n)
        {
            unsigned int len = _min(n, fifo->in - fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_out = fifo->out & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_out);
            T* arr = (T*)fifo->buffer;

            memcpy(ret, arr + idx_out, l * sizeof(T));
            memcpy(ret + l, arr, (len - l) * sizeof(T));

            //std::cout << "test" << std::endl;

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();
            //Fence::lfence();

            fifo->out += len;

            return len;
        }
    };

    template <typename T>
    class __spsc_worker<T, false>
    {
    public:
        static int push(__fifo* fifo, const T* ret, int n)
        {
            unsigned int len = _min(n, fifo->size - fifo->in + fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_in = fifo->in & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_in);
            T* arr = (T*)fifo->buffer;

            for (unsigned int i = 0; i < l; i++)
                arr[idx_in + i] = /*std::move*/(ret[i]);

            for (unsigned int i = 0; i < len - l; i++)
                arr[i] = /*std::move*/(ret[l + i]);

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();

            fifo->in += len;

            return len;
        }

        static int push(__fifo* fifo, T* ret, int n)
        {
            unsigned int len = _min(n, fifo->size - fifo->in + fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_in = fifo->in & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_in);
            T* arr = (T*)fifo->buffer;

            for (unsigned int i = 0; i < l; i++)
                arr[idx_in + i] = std::move(ret[i]);

            for (unsigned int i = 0; i < len - l; i++)
                arr[i] = std::move(ret[l + i]);

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();

            fifo->in += len;

            return len;
        }

        static int pop(__fifo* fifo, T* ret, int n)
        {
            unsigned int len = _min(n, fifo->in - fifo->out);
            if (len == 0)
                return 0;

            unsigned int idx_out = fifo->out & fifo->mask;
            unsigned int l = _min(len, fifo->size - idx_out);
            T* arr = (T*)fifo->buffer;

            for (unsigned int i = 0; i < l; i++)
                ret[i] = std::move(arr[idx_out + i]);

            for (unsigned int i = 0; i < len - l; i++)
                ret[l + i] = std::move(arr[i]);

            asm volatile("sfence" ::: "memory");
            //Fence::sfence();
            //Fence::lfence();

            fifo->out += len;

            return len;
        }
    };

}


#endif  // __RING_QUEUE_H