#pragma once

#if defined(_MSC_VER)
#    define BEE_ASSUME(cond) __assume(cond)
#else
#    define BEE_ASSUME(cond) ((cond) ? (void)(0) : __builtin_unreachable())
#endif
