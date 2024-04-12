#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__linux__)
#    include <linux/futex.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#elif defined(__APPLE__)
#    if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#            define BEE_USE_ULOCK
#        endif
#    elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 130000
#            define BEE_USE_ULOCK
#        endif
#    endif
#else
#    include <bee/thread/simplethread.h>
#    include <bee/thread/spinlock.h>
#endif

#if defined(BEE_USE_ULOCK)
extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value);
#endif

namespace bee {
    struct atomic_sync {
#if defined(_WIN32)
        using value_type = uint8_t;
        static inline void wait(int& ctx, const value_type* ptr, value_type val) {
            ::WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(value_type), INFINITE);
        }
        static inline void wait(int& ctx, const value_type* ptr, value_type val, int timeout) {
            ::WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(value_type), timeout);
        }
        static inline void wake(const value_type* ptr, bool all) {
            if (all) {
                ::WakeByAddressAll((PVOID)ptr);
            }
            else {
                ::WakeByAddressSingle((PVOID)ptr);
            }
        }
#elif defined(__linux__)
        using value_type = int;
        static inline void wait(int& ctx, const value_type* ptr, value_type val) {
            ::syscall(SYS_futex, ptr, FUTEX_WAIT_PRIVATE, val, nullptr, 0, 0);
        }
        static inline void wait(int& ctx, const value_type* ptr, value_type val, int timeout) {
            struct FutexTimespec {
                long tv_sec;
                long tv_nsec;
            };
            FutexTimespec ts;
            ts.tv_sec  = (long)(timeout / 1000);
            ts.tv_nsec = (long)(timeout % 1000 * 1000 * 1000);
            ::syscall(SYS_futex, ptr, FUTEX_WAIT_PRIVATE, val, &ts, 0, 0);
        }
        static inline void wake(const value_type* ptr, bool all) {
            ::syscall(SYS_futex, ptr, FUTEX_WAKE_PRIVATE, all ? INT_MAX : 1, 0, 0, 0);
        }
#elif defined(BEE_USE_ULOCK)
        using value_type                              = uint32_t;
        static constexpr uint32_t UL_COMPARE_AND_WAIT = 1;
        static constexpr uint32_t ULF_WAKE_ALL        = 0x00000100;
        static constexpr uint32_t ULF_NO_ERRNO        = 0x01000000;
        static inline void wait(int& ctx, const value_type* ptr, value_type val) {
            for (;;) {
                int rc = ::__ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, const_cast<value_type*>(ptr), val, 0);
                if (rc == -EINTR) {
                    continue;
                }
                return;
            }
        }
        static inline void wait(int& ctx, const value_type* ptr, value_type val, int timeout) {
            ::__ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, const_cast<value_type*>(ptr), val, timeout * 1000);
        }
        static inline void wake(const value_type* ptr, bool all) {
            ::__ulock_wake(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO | (all ? ULF_WAKE_ALL : 0), const_cast<value_type*>(ptr), 0);
        }
#else
        using value_type = uint8_t;
        // TODO *bsd
        static inline void wait(int& ctx, const value_type* ptr, value_type val, int timeout = -1) {
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
        }
        static inline void wake(const value_type* ptr, bool all) {}
#endif
    };
}
