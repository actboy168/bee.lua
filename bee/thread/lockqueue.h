#pragma once

#include <queue>
#include <mutex>
#include <bee/thread/spinlock.h>

namespace bee {
    template <class T>
    class lockqueue {
    public:
        void push(const T& data) {
            std::unique_lock<spinlock> lk(mutex);
            queue.push(data);
        }
        void push(T&& data) {
            std::unique_lock<spinlock> lk(mutex);
            queue.push(std::forward<T>(data));
        }
        bool pop(T& data) {
            std::unique_lock<spinlock> lk(mutex);
            if (queue.empty()) {
                return false;
            }
            data = queue.front();
            queue.pop();
            return true;
        }
    protected:
        std::queue<T> queue;
        spinlock mutex;
    };
}
