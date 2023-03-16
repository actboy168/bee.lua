#pragma once

#if defined(_MSC_VER)
#    define ASSUME(cond) __assume(cond)
#elif defined(__clang__)
#    define ASSUME(cond) __builtin_assume(cond)
#else
#    define ASSUME(cond)             \
        if (cond) {                  \
        }                            \
        else {                       \
            __builtin_unreachable(); \
        }
#endif
