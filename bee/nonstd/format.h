#pragma once

#if defined(__cpp_lib_print)
#include <format>
#elif defined(__cpp_lib_format) && (__cpp_lib_format >= 202200L) // see https://wg21.link/P2508R1
#include <format>
#include <3rd/fmt/fmt/format.h>
#include <3rd/fmt/fmt/xchar.h>
namespace std {
    using ::fmt::print;
    using ::fmt::println;
}
#else
#include <3rd/fmt/fmt/format.h>
#include <3rd/fmt/fmt/xchar.h>
namespace std {
    using ::fmt::format;
    using ::fmt::format_string;
    using ::fmt::print;
    using ::fmt::println;
}
#endif
