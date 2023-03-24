#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__cpp_lib_print)
#    include <print>
#else
#    if defined(_MSC_VER)
#        pragma warning(push)
#        pragma warning(disable : 6239)
#    endif
#    include <3rd/fmt/fmt/format.h>
#    include <3rd/fmt/fmt/xchar.h>
#    if defined(_MSC_VER)
#        pragma warning(pop)
#    endif
namespace std {
    using ::fmt::print;
    using ::fmt::println;
}
#endif
