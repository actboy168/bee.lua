#pragma once

#if defined(_MSC_VER)
#    define ASSUME(cond) __assume(cond)
#else
#    define ASSUME(cond) ((cond) ? static_cast<void>(0) : __builtin_unreachable())
#endif
