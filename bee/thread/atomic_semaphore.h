#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__cpp_lib_semaphore)
#    if defined(_MSC_VER)
#        define BEE_USE_BINARY_SEMAPHORE 1
#    elif defined(__GLIBCXX__)
#        if (defined(__glibcxx_atomic_wait) || defined(__cpp_lib_atomic_wait)) && !_GLIBCXX_USE_POSIX_SEMAPHORE
#            define BEE_USE_BINARY_SEMAPHORE 1
#        endif
#    endif
#endif

#if defined(BEE_USE_BINARY_SEMAPHORE)
#    include <semaphore>
namespace bee {
    using atomic_semaphore = ::std::binary_semaphore;
}
#else
#    include <atomic>
#    include <chrono>
#    include <cstdint>

#    if defined(_WIN32)
#        include <Windows.h>
#    elif defined(__linux__)
#        include <linux/futex.h>
#        include <sys/syscall.h>
#        include <unistd.h>

#        include <climits>
#    elif defined(__APPLE__)
#        if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#            if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#                define BEE_USE_ULOCK
#            endif
#        elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#            if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 130000
#                define BEE_USE_ULOCK
#            endif
#        endif
#    else
#        include <bee/thread/simplethread.h>
#        include <bee/thread/spinlock.h>
#    endif

namespace bee {

#    if defined(_WIN32)
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val) {
        WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(T), INFINITE);
    }
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val, int timeout) {
        WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(T), timeout);
    }
    template <typename T>
    void kernel_wake(const T* ptr, bool all) {
        if (all)
            WakeByAddressAll((PVOID)ptr);
        else
            WakeByAddressSingle((PVOID)ptr);
    }
#    elif defined(__linux__)
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val) {
        syscall(SYS_futex, ptr, FUTEX_WAIT_PRIVATE, val, nullptr, 0, 0);
    }
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val, int timeout) {
        struct FutexTimespec {
            long tv_sec;
            long tv_nsec;
        };
        FutexTimespec ts;
        ts.tv_sec  = (long)(timeout / 1000);
        ts.tv_nsec = (long)(timeout % 1000 * 1000);
        syscall(SYS_futex, ptr, FUTEX_WAIT_PRIVATE, val, &ts, 0, 0);
    }
    template <typename T>
    void kernel_wake(const T* ptr, bool all) {
        syscall(SYS_futex, ptr, FUTEX_WAKE_PRIVATE, all ? INT_MAX : 1, 0, 0, 0);
    }
#    elif defined(BEE_USE_ULOCK)
    extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout);
    extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value);
    constexpr uint32_t UL_COMPARE_AND_WAIT = 1;
    constexpr uint32_t ULF_WAKE_ALL        = 0x00000100;
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val) {
        __ulock_wait(UL_COMPARE_AND_WAIT, const_cast<T*>(ptr), val, 0);
    }
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val, int timeout) {
        __ulock_wait(UL_COMPARE_AND_WAIT, const_cast<T*>(ptr), val, timeout * 1000);
    }
    template <typename T>
    void kernel_wake(const T* ptr, bool all) {
        __ulock_wake(UL_COMPARE_AND_WAIT | (all ? ULF_WAKE_ALL : 0), const_cast<T*>(ptr), 0);
    }
#    else
    // TODO *bsd
    template <typename T>
    void kernel_wait(int& ctx, const T* ptr, T val, int timeout = -1) {
        ++ctx;
        if (ctx < 64) {
            cpu_relax();
        }
        else if (ctx < 72) {
            thread_yield();
        }
        else {
            thread_sleep(10);
        }
        template <typename T>
        void kernel_wake(const T* ptr, bool all) {}
#    endif
}

namespace bee {
    class atomic_semaphore {
#    if defined(_WIN32)
        using value_type = uint8_t;
#    elif defined(__linux__)
        using value_type = uint32_t;
#    elif defined(BEE_USE_ULOCK)
        using value_type = uint64_t;
#    else
            using value_type = uint8_t;
#    endif
    public:
        constexpr explicit atomic_semaphore(const ptrdiff_t desired) noexcept
            : counter(static_cast<value_type>(desired)) {
        }

        atomic_semaphore(const atomic_semaphore&)            = delete;
        atomic_semaphore& operator=(const atomic_semaphore&) = delete;

        void release() noexcept {
            counter.store(1);
            kernel_wake((value_type*)&counter, false);
        }

        void acquire() noexcept {
            int ctx = 0;
            for (;;) {
                value_type prev = counter.exchange(0);
                if (prev == 1) {
                    break;
                }
                kernel_wait(ctx, (value_type*)&counter, prev);
            }
        }

        bool try_acquire() noexcept {
            value_type prev = counter.exchange(0);
            return prev == 1;
        }

        template <class Rep, class Period>
        bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
            int ctx       = 0;
            auto abs_time = std::chrono::system_clock::now() + rel_time;
            for (;;) {
                value_type prev = counter.exchange(0);
                if (prev == 1) {
                    return true;
                }
                const unsigned long remaining_timeout = get_remaining_timeout(abs_time);
                if (remaining_timeout == 0) {
                    return false;
                }
                kernel_wait(ctx, (value_type*)&counter, prev, remaining_timeout);
            }
        }

        template <class Clock, class Duration>
        bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
            int ctx = 0;
            for (;;) {
                value_type prev = counter.exchange(0);
                if (prev == 1) {
                    return true;
                }
                const unsigned long remaining_timeout = get_remaining_timeout(abs_time);
                if (remaining_timeout == 0) {
                    return false;
                }
                kernel_wait(ctx, (value_type*)&counter, prev, remaining_timeout);
            }
        }

    private:
        template <class Clock, class Duration>
        unsigned long get_remaining_timeout(const std::chrono::time_point<Clock, Duration>& abs_time) {
            const auto now = Clock::now();
            if (now >= abs_time) {
                return 0;
            }
            const auto rel_time = std::chrono::ceil<std::chrono::milliseconds>(abs_time - now);
            return static_cast<unsigned long>(rel_time.count());
        }

    private:
        std::atomic<value_type> counter;
    };
}

#endif
