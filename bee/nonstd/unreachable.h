#pragma once

#if defined(__cpp_lib_unreachable)
#include <utility>
#else

#include <cstdlib>

namespace bee {
[[noreturn]] inline void unreachable() noexcept {
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

namespace std {
    using bee::unreachable;
}

#endif
