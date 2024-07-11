#pragma once

#if defined(__has_cpp_attribute)
#    if __has_cpp_attribute(assume) >= 202207
#        define BEE_ASSUME(cond) [[assume(cond)]]
#    endif
#endif

#if !defined(BEE_ASSUME)
#    if defined(_MSC_VER)
#        define BEE_ASSUME(cond) __assume(cond)
#    else
#        define BEE_ASSUME(cond) ((cond) ? (void)(0) : __builtin_unreachable())
#    endif
#endif

#if defined(NDEBUG)
#    define BEE_EXPECTS(cond) static_cast<void>(0)
#else
#    define BEE_EXPECTS(cond) ((cond) ? static_cast<void>(0) : std::terminate())
#endif
