#pragma once

#if defined(__cpp_lib_print)
#include <format>
#elif defined(__cpp_lib_format)
#include <format>
#include <bee/nonstd/3rd/fmt/format.h>
#include <bee/nonstd/3rd/fmt/xchar.h>
namespace std {
    using ::fmt::print;
    using ::fmt::println;
}
#else
#include <bee/nonstd/3rd/fmt/format.h>
#include <bee/nonstd/3rd/fmt/xchar.h>
namespace std {
    using ::fmt::format;
    using ::fmt::print;
    using ::fmt::println;
}
#endif
