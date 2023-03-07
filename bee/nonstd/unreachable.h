#pragma once

#if defined(__cpp_lib_unreachable)
#include <utility>
#else

#if !defined(NDEBUG)
    #include <cstdlib>
#endif

namespace std {
[[noreturn]] inline
#if defined(_MSC_VER)
    __forceinline
#endif
void unreachable() noexcept {
#if defined(_MSC_VER)
    __assume(false);
#else
    __builtin_unreachable();
#endif
#if !defined(NDEBUG)
    std::abort();
#endif
}
}

#endif
