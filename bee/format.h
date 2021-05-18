#pragma once

#if defined(__cpp_lib_format)
#include <format>
#else
#include <fmt/format.h>
namespace std {
    using ::fmt::format;
}
#endif
