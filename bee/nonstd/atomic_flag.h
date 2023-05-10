#pragma once

#include <atomic>

namespace bee {
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
}
