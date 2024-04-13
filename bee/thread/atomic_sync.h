#pragma once

#include <cstdint>

#if defined(__linux__) || defined(__OpenBSD__)
#    define BEE_HAVE_FUTEX 1
#elif defined(__NetBSD__)
#    include <sys/syscall.h>
#    if defined(SYS___futex)
#        define BEE_HAVE_FUTEX 1
#    endif
#elif defined(__APPLE__)
#    if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#            define BEE_HAVE_ULOCK 1
#        endif
#    elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 130000
#            define BEE_HAVE_ULOCK 1
#        endif
#    endif
#endif

namespace bee {
    struct atomic_sync {
#if defined(_WIN32)
        using value_type = uint8_t;
#elif defined(BEE_HAVE_FUTEX)
        using value_type = int;
#elif defined(__FreeBSD__)
        using value_type = unsigned int;
#elif defined(BEE_HAVE_ULOCK)
        using value_type = uint32_t;
#else
        using value_type = uint8_t;
#endif
        static void wait(int& ctx, const value_type* ptr, value_type val) noexcept;
        static void wait(int& ctx, const value_type* ptr, value_type val, int timeout) noexcept;
        static void wake(const value_type* ptr, bool all) noexcept;
    };
}
