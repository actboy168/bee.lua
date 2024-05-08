#pragma once

#include <string_view>
#include <utility>

namespace bee::reflection {
    template <unsigned short N>
    struct cstring {
        constexpr explicit cstring(std::string_view str) noexcept
            : cstring { str, std::make_integer_sequence<unsigned short, N> {} } {}
        constexpr const char* data() const noexcept { return chars_; }
        constexpr unsigned short size() const noexcept { return N; }
        constexpr operator std::string_view() const noexcept { return { data(), size() }; }
        template <unsigned short... I>
        constexpr cstring(std::string_view str, std::integer_sequence<unsigned short, I...>) noexcept
            : chars_ { str[I]..., '\0' } {}
        char chars_[static_cast<size_t>(N) + 1];
    };
    template <>
    struct cstring<0> {
        constexpr explicit cstring(std::string_view) noexcept {}
        constexpr const char* data() const noexcept { return nullptr; }
        constexpr unsigned short size() const noexcept { return 0; }
        constexpr operator std::string_view() const noexcept { return {}; }
    };
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
        return cstring<name.size()> { name };
    }
    template <typename T>
    constexpr auto name_v = name<T>();
}
