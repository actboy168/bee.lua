#pragma once

#include <atomic>

namespace bee {
    class atomic_semaphore {
    public:
        atomic_semaphore() noexcept;
        atomic_semaphore(const atomic_semaphore&)            = delete;
        atomic_semaphore& operator=(const atomic_semaphore&) = delete;
        void release() noexcept;
        void acquire() noexcept;

    private:
        std::atomic<uint32_t> v;
    };
}
