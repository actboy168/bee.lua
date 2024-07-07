#include <bee/thread/atomic_sync.h>

#if defined(_WIN32)

#    include <Windows.h>

static_assert(sizeof(bee::atomic_sync::value_type) == 1);

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val) noexcept {
    ::WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(value_type), INFINITE);
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept {
    ::WaitOnAddress((PVOID)ptr, (PVOID)&val, sizeof(value_type), timeout);
}

void bee::atomic_sync::wake(const value_type* ptr, bool all) noexcept {
    if (all) {
        ::WakeByAddressAll((PVOID)ptr);
    } else {
        ::WakeByAddressSingle((PVOID)ptr);
    }
}

#elif defined(BEE_HAVE_FUTEX)
#    if defined(__linux__)
#        include <linux/futex.h>
#    else
#        include <sys/futex.h>
#    endif
#    include <sys/syscall.h>
#    include <sys/time.h>
#    include <unistd.h>

#    include <climits>

#    if !defined(FUTEX_PRIVATE_FLAG)
#        define FUTEX_PRIVATE_FLAG 0
#    endif

namespace {
    struct FutexTimespec {
        long tv_sec;
        long tv_nsec;
    };
}

static void futex_wait(const int* ptr, int val, const FutexTimespec* timeout) {
#    if defined(__linux__)
    ::syscall(SYS_futex, ptr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, timeout, 0, 0);
#    elif defined(__NetBSD__)
    struct timespec ts;
    ts.tv_sec = timeout->tv_sec;
    ts.tv_nsec = timeout->tv_nsec;
    ::syscall(SYS___futex, ptr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, &ts, 0, 0, 0);
#    elif defined(__OpenBSD__)
    static_assert(sizeof(FutexTimespec) == sizeof(timespec));
    ::futex((uint32_t*)const_cast<int*>(ptr), FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, (const timespec*)timeout, 0);
#    endif
}

static void futex_wake(const int* ptr, bool all) {
#    if defined(__linux__)
    ::syscall(SYS_futex, ptr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, all ? INT_MAX : 1, 0, 0, 0);
#    elif defined(__NetBSD__)
    ::syscall(SYS___futex, ptr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, all ? INT_MAX : 1, 0, 0, 0, 0);
#    elif defined(__OpenBSD__)
    ::futex((uint32_t*)const_cast<int*>(ptr), FUTEX_WAKE | FUTEX_PRIVATE_FLAG, all ? INT_MAX : 1, 0, 0);
#    endif
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val) noexcept {
    futex_wait(ptr, val, nullptr);
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept {
    FutexTimespec ts;
    ts.tv_sec  = (long)(timeout / 1000);
    ts.tv_nsec = (long)(timeout % 1000 * 1000 * 1000);
    futex_wait(ptr, val, &ts);
}

void bee::atomic_sync::wake(const value_type* ptr, bool all) noexcept {
    futex_wake(ptr, all);
}

#elif defined(__FreeBSD__)

#    include <sys/types.h>
#    include <sys/umtx.h>
#    include <time.h>

#    include <climits>

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val) noexcept {
    ::_umtx_op(const_cast<value_type*>(ptr), UMTX_OP_WAIT_UINT_PRIVATE, val, NULL, NULL);
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept {
    // TODO: use UMTX_ABSTIME
    struct _umtx_time ut;
    ut._clockid         = CLOCK_MONOTONIC;
    ut._flags           = 0;
    ut._timeout.tv_sec  = timeout / 1000;
    ut._timeout.tv_nsec = (timeout % 1000) * 1000 * 1000;
    ::_umtx_op(const_cast<value_type*>(ptr), UMTX_OP_WAIT_UINT_PRIVATE, val, (void*)sizeof(struct _umtx_time), (void*)&ut);
}

void bee::atomic_sync::wake(const value_type* ptr, bool all) noexcept {
    ::_umtx_op(const_cast<value_type*>(ptr), UMTX_OP_WAKE_PRIVATE, all ? INT_MAX : 1, NULL, NULL);
}

#elif defined(BEE_HAVE_ULOCK)

#    include <errno.h>

extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value);

constexpr uint32_t UL_COMPARE_AND_WAIT = 1;
constexpr uint32_t ULF_WAKE_ALL        = 0x00000100;
constexpr uint32_t ULF_NO_ERRNO        = 0x01000000;

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val) noexcept {
    for (;;) {
        int rc = ::__ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, const_cast<value_type*>(ptr), val, 0);
        if (rc == -EINTR) {
            continue;
        }
        return;
    }
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept {
    ::__ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, const_cast<value_type*>(ptr), val, timeout * 1000);
}

void bee::atomic_sync::wake(const value_type* ptr, bool all) noexcept {
    ::__ulock_wake(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO | (all ? ULF_WAKE_ALL : 0), const_cast<value_type*>(ptr), 0);
}

#else

#    include <bee/thread/simplethread.h>
#    include <bee/thread/spinlock.h>

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val) noexcept {
    ++ctx;
    if (ctx < 64) {
        cpu_relax();
    } else if (ctx < 72) {
        thread_yield();
    } else {
        thread_sleep(10);
    }
}

void bee::atomic_sync::wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept {
    atomic_sync::wait(ctx, ptr, val);
}

void bee::atomic_sync::wake(const value_type* ptr, bool all) noexcept {
}

#endif
