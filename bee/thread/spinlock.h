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
#if defined(__cpp_lib_atomic_flag_test) //c++20
    using std::atomic_flag;
#else
    struct atomic_flag {
        bool test(std::memory_order order) const noexcept {
            return storage.load(order) != 0;
        }
        bool test(std::memory_order order) const volatile noexcept {
            return storage.load(order) != 0;
        }
        bool test_and_set(std::memory_order order) noexcept {
            return storage.exchange(true, order) != 0;
        }
        bool test_and_set(std::memory_order order) volatile noexcept {
            return storage.exchange(true, order) != 0;
        }
        void clear(std::memory_order order) noexcept {
            storage.store(false, order);
        }
        void clear(std::memory_order order) volatile noexcept {
            storage.store(false, order);
        }
        std::atomic<bool> storage = {false};
    };
#endif
    class spinlock {
    public:
        void lock() {
            for (;;) {
                if (!l.test_and_set(std::memory_order_acquire)) {
                    return;
                }
                while (l.test(std::memory_order_relaxed)) {
                    cpu_relax();
                }
            }
        }
        void unlock() {
            l.clear(std::memory_order_release);
        }
        bool try_lock() {
            return !l.test(std::memory_order_relaxed) && !l.test_and_set(std::memory_order_acquire);
        }
    private:
        atomic_flag l;
    };
}
