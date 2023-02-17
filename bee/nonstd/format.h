#pragma once

#if defined(__cpp_lib_format)
#include <format>
#else
#include <bee/nonstd/3rd/fmt/format.h>
#include <bee/nonstd/3rd/fmt/xchar.h>
namespace std {
    using ::fmt::format;
}
#endif
