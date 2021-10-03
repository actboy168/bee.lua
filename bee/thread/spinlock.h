#pragma once

#include <atomic>

#if defined(_WIN32)
    #include <windows.h>
    inline void cpu_relax() { YieldProcessor(); }
#elif defined(__x86_64__)
    #include <immintrin.h>
    inline void cpu_relax() { _mm_pause(); }
#else
    inline void cpu_relax() {}
#endif

namespace bee {
    class spinlock {
    public:
        void lock() {
            for (;;) {
                if (!l.exchange(true, std::memory_order_acquire)) {
                    return;
                }
                while (l.load(std::memory_order_relaxed)) {
                    cpu_relax();
                }
            }
        }
        void unlock() {
            l.store(false, std::memory_order_release);
        }
        bool try_lock() {
            return !l.load(std::memory_order_relaxed) && !l.exchange(true, std::memory_order_acquire);
        }
    private:
        std::atomic<bool> l = {0};
    };
}
