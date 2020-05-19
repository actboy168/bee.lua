#pragma once

#if defined(_MSC_VER)
#   if __has_include(<span>) && __cplusplus > 201703L
#       define BEE_ENABLE_SPAN 1
#   endif
#elif defined(__GUNC__)
#   if __has_include(<span>) && __cplusplus > 201703L
#       define BEE_ENABLE_SPAN 1
#   endif
#endif


#if defined(BEE_ENABLE_SPAN)
#   include <span>
#else
#   include <stdexcept>
#   define span_FEATURE_BYTE_SPAN 1
#   include <bee/nonstd/span.h>
namespace std {
    using nonstd::span;
}
#endif
