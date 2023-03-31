#pragma once

#include <atomic>

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__x86_64__) || defined(__i386__)
#    include <immintrin.h>
#endif

namespace bee {
    inline void cpu_relax() noexcept {
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

#if defined(__cpp_lib_atomic_flag_test)  // c++20
    using std::atomic_flag;
#else
    struct atomic_flag {
        bool test(std::memory_order order) const noexcept {
            return storage.load(order);
        }
        bool test(std::memory_order order) const volatile noexcept {
            return storage.load(order);
        }
        bool test_and_set(std::memory_order order) noexcept {
            return storage.exchange(true, order);
        }
        bool test_and_set(std::memory_order order) volatile noexcept {
            return storage.exchange(true, order);
        }
        void clear(std::memory_order order) noexcept {
            storage.store(false, order);
        }
        void clear(std::memory_order order) volatile noexcept {
            storage.store(false, order);
        }
        std::atomic<bool> storage = { false };
    };
#endif
    class spinlock {
    public:
        void lock() noexcept {
            for (;;) {
                if (!l.test_and_set(std::memory_order_acquire)) {
                    return;
                }
                while (l.test(std::memory_order_relaxed)) {
                    cpu_relax();
                }
            }
        }
        void unlock() noexcept {
            l.clear(std::memory_order_release);
        }
        bool try_lock() noexcept {
            return !l.test(std::memory_order_relaxed) && !l.test_and_set(std::memory_order_acquire);
        }

    private:
        atomic_flag l;
    };
}
