#pragma once

#include <cstdlib>
#include <utility>

namespace bee {
[[noreturn]] inline void unreachable() noexcept {
#if defined(__cpp_lib_unreachable)
    std::unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#else
    __builtin_unreachable();
#endif
#if !defined(NDEBUG)
    std::abort();
#endif
}
}