#include <bee/thread/spinlock.h>

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__x86_64__) || defined(__i386__)
#    include <immintrin.h>
#endif

namespace bee {
    void cpu_relax() noexcept {
#if defined(_WIN32)
        YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("isb");
#elif defined(__riscv)
        int dummy;
        asm volatile("div %0, %0, zero"
                     : "=r"(dummy));
        asm volatile("" ::
                         : "memory");
#elif defined(__powerpc__)
        asm volatile("ori 0,0,0" ::
                         : "memory");
#else
        std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    }

    void spinlock::lock() noexcept {
        for (;;) {
            if (!l.test_and_set(std::memory_order_acquire)) {
                return;
            }
            while (l.test(std::memory_order_relaxed)) {
                cpu_relax();
            }
        }
    }
    void spinlock::unlock() noexcept {
        l.clear(std::memory_order_release);
    }
    bool spinlock::try_lock() noexcept {
        return !l.test(std::memory_order_relaxed) && !l.test_and_set(std::memory_order_acquire);
    }
}
