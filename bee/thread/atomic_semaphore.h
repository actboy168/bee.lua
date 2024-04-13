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
#    include <bee/thread/atomic_sync.h>

#    include <atomic>
#    include <chrono>
#    include <cstdint>

namespace bee {
    class atomic_semaphore {
    public:
        constexpr explicit atomic_semaphore(const ptrdiff_t desired) noexcept
            : counter(static_cast<atomic_sync::value_type>(desired)) {
        }

        atomic_semaphore(const atomic_semaphore&)            = delete;
        atomic_semaphore& operator=(const atomic_semaphore&) = delete;

        void release() noexcept {
            counter.store(1);
            atomic_sync::wake((atomic_sync::value_type*)&counter, false);
        }

        void acquire() noexcept {
            int ctx = 0;
            for (;;) {
                auto prev = counter.exchange(0);
                if (prev == 1) {
                    break;
                }
                atomic_sync::wait(ctx, (atomic_sync::value_type*)&counter, prev);
            }
        }

        bool try_acquire() noexcept {
            auto prev = counter.exchange(0);
            return prev == 1;
        }

        template <class Rep, class Period>
        bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
            int ctx       = 0;
            auto abs_time = std::chrono::steady_clock::now() + rel_time;
            for (;;) {
                auto prev = counter.exchange(0);
                if (prev == 1) {
                    return true;
                }
                const unsigned long remaining_timeout = get_remaining_timeout(abs_time);
                if (remaining_timeout == 0) {
                    return false;
                }
                atomic_sync::wait(ctx, (atomic_sync::value_type*)&counter, prev, remaining_timeout);
            }
        }

        template <class Clock, class Duration>
        bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
            int ctx = 0;
            for (;;) {
                auto prev = counter.exchange(0);
                if (prev == 1) {
                    return true;
                }
                const unsigned long remaining_timeout = get_remaining_timeout(abs_time);
                if (remaining_timeout == 0) {
                    return false;
                }
                atomic_sync::wait(ctx, (atomic_sync::value_type*)&counter, prev, remaining_timeout);
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
        std::atomic<atomic_sync::value_type> counter;
    };
}

#endif

static_assert(std::is_trivially_destructible_v<::bee::atomic_semaphore>);
