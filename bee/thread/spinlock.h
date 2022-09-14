#pragma once

#include <atomic>

#if defined(_WIN32)
    #include <windows.h>
    namespace bee { inline void cpu_relax() { YieldProcessor(); }}
#elif defined(__x86_64__) || defined(__i386__)
    #include <immintrin.h>
    namespace bee { inline void cpu_relax() { _mm_pause(); }}
#elif defined(__aarch64__)
    namespace bee { inline void cpu_relax() { asm volatile("yield" ::: "memory"); }}
#elif defined(__arm__)
    namespace bee { inline void cpu_relax() { asm volatile("":::"memory"); }}
#elif defined(__riscv)
    namespace bee { inline void cpu_relax() {
        int dummy;
        asm volatile ("div %0, %0, zero" : "=r" (dummy));
        asm volatile ("" ::: "memory");
    }}
#elif defined(__powerpc__)
    namespace bee { inline void cpu_relax() { asm volatile("ori 0,0,0" ::: "memory"); }}
#else
    #error unsupport platform
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
