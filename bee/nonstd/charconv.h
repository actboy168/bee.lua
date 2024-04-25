#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if !defined(__cpp_lib_to_chars) && (__cplusplus >= 202000)
#    if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#        if defined(__MAC_14_4) && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 130300)
#            define __cpp_lib_to_chars 201611L
#        endif
#    elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#        if defined(__IPHONE_17_4) && (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 160300)
#            define __cpp_lib_to_chars 201611L
#        endif
#    endif
#endif

#if defined(__cpp_lib_to_chars)
#    include <charconv>
#else
#    include <cstring>
#    include <limits>
#    include <system_error>
#    include <type_traits>

namespace std {

    struct from_chars_result {
        const char* ptr;
        std::errc ec;
    };

    struct to_chars_result {
        char* ptr;
        std::errc ec;
    };

    template <class T>
    [[nodiscard]] to_chars_result to_chars(char* first, char* last, const T value) noexcept {
        static_assert(sizeof(T) <= sizeof(size_t));
        static_assert(std::is_integral_v<T>);
        static_assert(std::is_unsigned_v<T>);
        if (first > last) {
            return { first, std::errc::invalid_argument };
        }
        constexpr size_t size = std::numeric_limits<T>::digits10 + 1;
        char buf[size];
        char* next       = buf + size;
        T unsolved_value = value;
        do {
            *--next = static_cast<char>('0' + unsolved_value % 10);
            unsolved_value /= 10;
        } while (unsolved_value != 0);
        const ptrdiff_t written = buf + size - next;
        if (last - first < written) {
            return { last, std::errc::value_too_large };
        }
        std::memcpy(first, next, static_cast<size_t>(written));
        return { first + written, std::errc {} };
    }

    [[nodiscard]] inline unsigned char charconv_from_char(char c) noexcept {
        static constexpr unsigned char from_char_table[] = {
            // clang-format off
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            // clang-format on
        };
        static_assert(std::size(from_char_table) == 256);
        return from_char_table[static_cast<unsigned char>(c)];
    }

    template <class T>
    [[nodiscard]] from_chars_result from_chars(const char* first, const char* last, T& value) noexcept {
        static_assert(std::is_integral_v<T>);
        static_assert(std::is_unsigned_v<T>);
        if (first > last) {
            return { first, std::errc::invalid_argument };
        }
        constexpr T max_div_10 = static_cast<T>((std::numeric_limits<T>::max)() / 10);
        constexpr T max_mod_10 = static_cast<T>((std::numeric_limits<T>::max)() % 10);
        T unsolved_value       = 0;
        bool overflowed        = false;
        const char* next       = first;
        for (; next != last; ++next) {
            const unsigned char digit = charconv_from_char(*next);
            if (digit >= 10) {
                break;
            }
            if (unsolved_value < max_div_10 || (unsolved_value == max_div_10 && digit <= max_mod_10)) {
                unsolved_value = static_cast<T>(unsolved_value * 10 + digit);
            } else {
                overflowed = true;
            }
        }
        if (next == first) {
            return { first, std::errc::invalid_argument };
        }
        if (overflowed) {
            return { next, std::errc::result_out_of_range };
        }
        value = unsolved_value;
        return { next, std::errc {} };
    }

}

#endif
