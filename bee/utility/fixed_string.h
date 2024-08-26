#pragma once

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <string_view>
#include <bee/utility/assume.h>

namespace bee {

template <typename CharT, std::size_t N, typename Traits = std::char_traits<CharT>>
class basic_fixed_string {
public:
    CharT data_[N + 1] = {};

    using traits_type            = Traits;
    using value_type             = CharT;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using const_iterator         = const value_type*;
    using iterator               = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator       = const_reverse_iterator;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    constexpr basic_fixed_string(const CharT (&txt)[N + 1]) noexcept {
        BEE_EXPECTS(txt[N] == CharT {});
        for (std::size_t i = 0; i < N; ++i) data_[i] = txt[i];
    }
    constexpr basic_fixed_string(const std::basic_string_view<CharT, Traits> txt) noexcept {
        BEE_EXPECTS(txt.size() == N);
        for (std::size_t i = 0; i < N; ++i) data_[i] = txt[i];
    }

    constexpr basic_fixed_string(const basic_fixed_string&) noexcept            = default;
    constexpr basic_fixed_string& operator=(const basic_fixed_string&) noexcept = default;

    [[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return data() + size(); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    static constexpr std::integral_constant<size_type, N> size {};
    static constexpr std::integral_constant<size_type, N> length {};
    static constexpr std::integral_constant<size_type, N> max_size {};
    static constexpr std::bool_constant<N == 0> empty {};

    [[nodiscard]] constexpr const_reference operator[](size_type pos) const {
        BEE_EXPECTS(pos < N);
        return data()[pos];
    }

    [[nodiscard]] constexpr const_reference front() const {
        BEE_EXPECTS(!empty());
        return (*this)[0];
    }
    [[nodiscard]] constexpr const_reference back() const {
        BEE_EXPECTS(!empty());
        return (*this)[N - 1];
    }

    constexpr void swap(basic_fixed_string& s) noexcept { swap_ranges(begin(), end(), s.begin()); }

    [[nodiscard]] constexpr const_pointer c_str() const noexcept { return data(); }
    [[nodiscard]] constexpr const_pointer data() const noexcept { return static_cast<const_pointer>(data_); }
    [[nodiscard]] constexpr std::basic_string_view<CharT, Traits> view() const noexcept {
        return std::basic_string_view<CharT, Traits>(cbegin(), cend());
    }
    [[nodiscard]] constexpr operator std::basic_string_view<CharT, Traits>() const noexcept {
        return view();
    }

    template <std::size_t N2>
    [[nodiscard]] constexpr friend basic_fixed_string<CharT, N + N2, Traits> operator+(
        const basic_fixed_string& lhs, const basic_fixed_string<CharT, N2, Traits>& rhs
    ) noexcept {
        CharT txt[N + N2];
        CharT* it = txt;
        for (CharT c : lhs) *it++ = c;
        for (CharT c : rhs) *it++ = c;
        return basic_fixed_string<CharT, N + N2, Traits>(txt, it);
    }

    [[nodiscard]] constexpr friend basic_fixed_string<CharT, N + 1, Traits> operator+(const basic_fixed_string& lhs, CharT rhs) noexcept {
        CharT txt[N + 1];
        CharT* it = txt;
        for (CharT c : lhs) *it++ = c;
        *it++ = rhs;
        return basic_fixed_string<CharT, N + 1, Traits>(txt, it);
    }

    [[nodiscard]] constexpr friend basic_fixed_string<CharT, 1 + N, Traits> operator+(
        const CharT lhs, const basic_fixed_string& rhs
    ) noexcept {
        CharT txt[1 + N];
        CharT* it = txt;
        *it++     = lhs;
        for (CharT c : rhs) *it++ = c;
        return basic_fixed_string<CharT, 1 + N, Traits>(txt, it);
    }

    template <std::size_t N2>
    [[nodiscard]] constexpr friend basic_fixed_string<CharT, N + N2 - 1, Traits> operator+(
        const basic_fixed_string& lhs, const CharT (&rhs)[N2]
    ) noexcept {
        BEE_EXPECTS(rhs[N2 - 1] == CharT {});
        CharT txt[N + N2];
        CharT* it = txt;
        for (CharT c : lhs) *it++ = c;
        for (CharT c : rhs) *it++ = c;
        return txt;
    }

    template <std::size_t N1>
    [[nodiscard]] constexpr friend basic_fixed_string<CharT, N1 + N - 1, Traits> operator+(
        const CharT (&lhs)[N1], const basic_fixed_string& rhs
    ) noexcept {
        BEE_EXPECTS(lhs[N1 - 1] == CharT {});
        CharT txt[N1 + N];
        CharT* it = txt;
        for (size_t i = 0; i != N1 - 1; ++i) *it++ = lhs[i];
        for (CharT c : rhs) *it++ = c;
        *it++ = CharT();
        return txt;
    }

    template <size_t N2>
    [[nodiscard]] friend constexpr bool operator==(const basic_fixed_string& lhs, const basic_fixed_string<CharT, N2, Traits>& rhs) {
        return lhs.view() == rhs.view();
    }
    template <size_t N2>
    [[nodiscard]] friend constexpr bool operator==(const basic_fixed_string& lhs, const CharT (&rhs)[N2]) {
        BEE_EXPECTS(rhs[N2 - 1] == CharT {});
        return lhs.view() == std::basic_string_view<CharT, Traits>(std::cbegin(rhs), std::cend(rhs) - 1);
    }
    };

    template <std::size_t N>
    using fixed_string = basic_fixed_string<char, N>;
    template <std::size_t N>
    using fixed_wstring = basic_fixed_string<wchar_t, N>;
}
