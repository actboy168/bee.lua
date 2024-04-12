#pragma once

#include <cstdint>

#if defined(__APPLE__)
#    if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#            define BEE_USE_ULOCK
#        endif
#    elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#        if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 130000
#            define BEE_USE_ULOCK
#        endif
#    endif
#endif

namespace bee {
    struct atomic_sync {
#if defined(_WIN32)
        using value_type = uint8_t;
#elif defined(__linux__)
        using value_type = int;
#elif defined(BEE_USE_ULOCK)
        using value_type = uint32_t;
#else
        // TODO *bsd
        using value_type = uint8_t;
#endif
        static void wait(int& ctx, const value_type* ptr, value_type val);
        static void wait(int& ctx, const value_type* ptr, value_type val, int timeout);
        static void wake(const value_type* ptr, bool all);
    };
}
