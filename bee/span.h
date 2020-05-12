#pragma once

#if defined(_MSC_VER)
#   include <yvals_core.h>
#   if __has_include(<span>) && _HAS_CXX20
#       define BEE_ENABLE_SPAN 1
#   endif
#elif defined(__MINGW32__)
#   if __has_include(<span>) && __cplusplus > 201703L
#       define BEE_ENABLE_SPAN 1
#   endif
#endif


#if defined(BEE_ENABLE_SPAN)
#   include <span>
#else
#   include <stdexcept>
#   include <bee/nonstd/span.h>
namespace std {
    using nonstd::span;
}
#endif
