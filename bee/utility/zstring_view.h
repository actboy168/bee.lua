#pragma once

#include <cassert>
#include <string>
#include <string_view>

namespace bee {
    template <class CharT, class Traits = std::char_traits<CharT>>
    class basic_zstring_view : private std::basic_string_view<CharT, Traits> {
    private:
        using string_view_base = std::basic_string_view<CharT, Traits>;

    public:
        using string_view_base::npos;
        using typename string_view_base::const_iterator;
        using typename string_view_base::const_pointer;
        using typename string_view_base::const_reference;
        using typename string_view_base::const_reverse_iterator;
        using typename string_view_base::difference_type;
        using typename string_view_base::iterator;
        using typename string_view_base::pointer;
        using typename string_view_base::reference;
        using typename string_view_base::reverse_iterator;
        using typename string_view_base::size_type;
        using typename string_view_base::traits_type;
        using typename string_view_base::value_type;

    public:
        constexpr basic_zstring_view() noexcept                                     = default;
        constexpr basic_zstring_view(const basic_zstring_view&) noexcept            = default;
        constexpr basic_zstring_view& operator=(const basic_zstring_view&) noexcept = default;
        constexpr basic_zstring_view(basic_zstring_view&&) noexcept                 = default;
        constexpr basic_zstring_view& operator=(basic_zstring_view&&) noexcept      = default;
        basic_zstring_view(const CharT* s) noexcept
            : string_view_base { s } {}
        basic_zstring_view(const CharT* s, size_type sz) noexcept
            : string_view_base { s, sz } { assert(s[sz] == 0); }
        basic_zstring_view(const std::basic_string<CharT, Traits>& s) noexcept
            : string_view_base { s } {}

    public:
        using string_view_base::begin;
        using string_view_base::cbegin;
        using string_view_base::cend;
        using string_view_base::crbegin;
        using string_view_base::crend;
        using string_view_base::end;
        using string_view_base::rbegin;
        using string_view_base::rend;
        using string_view_base::operator[];
        using string_view_base::at;
        using string_view_base::back;
        using string_view_base::compare;
        using string_view_base::data;
        using string_view_base::empty;
        using string_view_base::find;
        using string_view_base::find_first_not_of;
        using string_view_base::find_first_of;
        using string_view_base::find_last_not_of;
        using string_view_base::find_last_of;
        using string_view_base::front;
        using string_view_base::length;
        using string_view_base::max_size;
        using string_view_base::rfind;
        using string_view_base::size;
    };
    template <class CharT, class Traits>
    constexpr bool operator==(basic_zstring_view<CharT, Traits> lhs, basic_zstring_view<CharT, Traits> rhs) noexcept {
        return lhs.size() == rhs.size() && Traits::compare(lhs.data(), rhs.data(), lhs.size()) == 0;
    }
    template <class CharT, class Traits, size_t N>
    constexpr bool operator==(basic_zstring_view<CharT, Traits> lhs, const CharT (&rhs)[N]) noexcept {
        static_assert(N > 0);
        return lhs.size() == (N - 1) && Traits::compare(lhs.data(), rhs, (N - 1)) == 0;
    }
    using zstring_view  = basic_zstring_view<char>;
    using wzstring_view = basic_zstring_view<wchar_t>;
}
