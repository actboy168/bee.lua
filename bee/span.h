#pragma once

#if defined(_MSC_VER)
#   include <yvals_core.h>
#endif

#if defined(_MSC_VER) && _HAS_CXX20 && __has_include(<span>)
#	include <span>
#else
#   include <stdexcept>
#	include <bee/nonstd/span.h>
namespace std {
    using nonstd::span;
}
#endif
