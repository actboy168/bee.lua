#pragma once

#include <atomic>
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
    class atomic_semaphore {
    public:
#if defined(_WIN32)
        using value_type = uint8_t;
#elif defined(__linux__)
        using value_type = uint32_t;
#elif defined(BEE_USE_ULOCK)
        using value_type = uint64_t;
#else
        using value_type = uint8_t;
#endif

        atomic_semaphore() noexcept;
        atomic_semaphore(const atomic_semaphore&)            = delete;
        atomic_semaphore& operator=(const atomic_semaphore&) = delete;
        void release() noexcept;
        void acquire() noexcept;

        static constexpr value_type SEM_FALSE = 0;
        static constexpr value_type SEM_TRUE  = 1;

    private:
        std::atomic<value_type> v;
    };
}
