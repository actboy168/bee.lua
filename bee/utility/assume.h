#pragma once

#if defined(__has_cpp_attribute) && __has_cpp_attribute(assume) >= 202207
#    define BEE_ASSUME(cond) [[assume(cond)]]
#else
#    if defined(_MSC_VER)
#        define BEE_ASSUME(cond) __assume(cond)
#    else
#        define BEE_ASSUME(cond) ((cond) ? (void)(0) : __builtin_unreachable())
#    endif
#endif
