#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__GLIBCXX__)
#    if __GLIBCXX__ >= 20230320 && __cplusplus >= 202002L
#        if defined(__cpp_lib_format)
#            undef __cpp_lib_format
#        endif
#        define __cpp_lib_format 202207L
#    endif
#endif

#if defined(__cpp_lib_format) && (__cpp_lib_format >= 202200L)  // see https://wg21.link/P2508R1
#    include <format>
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
    using ::fmt::format;
    using ::fmt::format_string;
}
#endif
