#include <bee/thread/atomic_semaphore.h>
#include <bee/thread/simplethread.h>
#include <bee/thread/spinlock.h>

#include <type_traits>

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__linux__)
#    include <linux/futex.h>
#    include <sys/syscall.h>
#    include <unistd.h>

#    include <climits>
#endif

namespace bee {

#if defined(_WIN32)
    template <typename T, std::enable_if_t<sizeof(T) <= 8, int> = 0>
    void kernel_wait(const T* ptr, T val) {
        WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(T), INFINITE);
    }
    template <typename T, std::enable_if_t<sizeof(T) <= 8, int> = 0>
    void kernel_wake(const T* ptr, bool all) {
        if (all)
            WakeByAddressAll((PVOID)ptr);
        else
            WakeByAddressSingle((PVOID)ptr);
    }
#elif defined(__linux__)
    template <typename T, std::enable_if_t<std::is_same_v<T, uint32_t>, int> = 0>
    void kernel_wait(const T* ptr, T val) {
        syscall(SYS_futex, ptr, FUTEX_WAIT_PRIVATE, val, nullptr, 0, 0);
    }
    template <typename T, std::enable_if_t<std::is_same_v<T, uint32_t>, int> = 0>
    void kernel_wake(const T* ptr, bool all) {
        syscall(SYS_futex, ptr, FUTEX_WAKE_PRIVATE, all ? INT_MAX : 1, 0, 0, 0);
    }
#elif defined(BEE_USE_ULOCK)
    extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout);
    extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value);
    constexpr uint32_t UL_COMPARE_AND_WAIT = 1;
    constexpr uint32_t ULF_WAKE_ALL        = 0x00000100;
    template <typename T, std::enable_if_t<std::is_same_v<T, uint64_t>, int> = 0>
    void kernel_wait(const T* ptr, T val) {
        __ulock_wait(UL_COMPARE_AND_WAIT, const_cast<T*>(ptr), val, 0);
    }
    template <typename T, std::enable_if_t<std::is_same_v<T, uint64_t>, int> = 0>
    void kernel_wake(const T* ptr, bool all) {
        __ulock_wake(UL_COMPARE_AND_WAIT | (all ? ULF_WAKE_ALL : 0), const_cast<T*>(ptr), 0);
    }
#else
    // TODO *bsd
#    define BEE_NO_KERNEL_WAIT
#endif

    atomic_semaphore::atomic_semaphore() noexcept
        : v(SEM_FALSE) {
    }

    void atomic_semaphore::release() noexcept {
#if !defined(BEE_NO_KERNEL_WAIT)
        const auto* ptr = &v;
        v.store(SEM_TRUE);
        kernel_wake((const value_type*)ptr, false);
#else
        v.store(SEM_TRUE);
#endif
    }

    void atomic_semaphore::acquire() noexcept {
#if !defined(BEE_NO_KERNEL_WAIT)
        for (;;) {
            if (v.exchange(SEM_FALSE) == SEM_TRUE)
                return;
            kernel_wait((const value_type*)&v, SEM_FALSE);
        }
#else
        for (int i = 0; i < 64; ++i) {
            if (v.exchange(SEM_FALSE) == SEM_TRUE)
                return;
            cpu_relax();
        }
        for (int i = 0; i < 8; ++i) {
            if (v.exchange(SEM_FALSE) == SEM_TRUE)
                return;
            thread_yield();
        }
        for (;;) {
            if (v.exchange(SEM_FALSE) == SEM_TRUE)
                return;
            thread_sleep(10);
        }
#endif
    }
}
