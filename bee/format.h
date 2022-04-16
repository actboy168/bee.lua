#pragma once

#if defined(__cpp_lib_format)
#include <format>
#else
#include <bee/nonstd/fmt/format.h>
#include <bee/nonstd/fmt/xchar.h>
namespace std {
    using ::fmt::format;
}
#endif
