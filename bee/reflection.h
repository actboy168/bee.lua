#pragma once

#include <string_view>
#include <utility>
#include <bee/utility/fixed_string.h>

namespace bee::reflection {
    template <typename T>
    constexpr auto name_raw() noexcept {
#if defined(__clang__) || defined(__GNUC__)
        std::string_view name = __PRETTY_FUNCTION__;
        size_t start          = name.find('=') + 2;
        size_t end            = name.size() - 1;
        return std::string_view { name.data() + start, end - start };
#elif defined(_MSC_VER)
        std::string_view name = __FUNCSIG__;
        size_t start          = name.find('<') + 1;
        size_t end            = name.rfind(">(");
        name                  = std::string_view { name.data() + start, end - start };
        start                 = name.find(' ');
        return start == std::string_view::npos ? name : std::string_view { name.data() + start + 1, name.size() - start - 1 };
#else
#    error Unsupported compiler
#endif
    }

    template <typename T>
    constexpr auto name() noexcept {
        constexpr auto name = name_raw<T>();
        return fixed_string<name.size()> { name };
    }
    template <typename T>
    constexpr auto name_v = name<T>();
}
