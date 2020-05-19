#pragma once

#if __cplusplus > 201703L && __has_include(<span>)
#   include <span>
#else
#   include <stdexcept>
#   define span_FEATURE_BYTE_SPAN 1
#   include <bee/nonstd/span.h>
namespace std {
    using nonstd::span;
}
#endif
