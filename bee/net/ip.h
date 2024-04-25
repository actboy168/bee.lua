#pragma once

#include <bee/nonstd/bit.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>

namespace bee::net::ip {
    template <typename T>
    static constexpr T host_to_network(T v) noexcept {
        static_assert(std::is_integral_v<T> && (sizeof(T) == 2 || sizeof(T) == 4));
        if constexpr (std::endian::native == std::endian::big) {
            return v;
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<unsigned short>((v << 8) | (v >> 8));
        } else if constexpr (sizeof(T) == 4) {
            return (v << 24) | ((v << 8) & 0x00FF0000) | ((v >> 8) & 0x0000FF00) | (v >> 24);
        }
    }

    template <size_t N>
    static constexpr ptrdiff_t rfind(const char (&str)[N], size_t from, char c) noexcept {
        for (ptrdiff_t i = from; i >= 0; i--) {
            if (str[i] == c) {
                return i;
            }
        }
        return -1;
    }

    struct invalid_value_exception : public std::exception {};

    template <size_t N>
    static constexpr uint8_t parse_uint8(const char (&str)[N], size_t s, size_t e) {
        if (e != s && e != s + 1 && e != s + 2) {
            throw invalid_value_exception {};
        }
        if (e != s && str[s] == '0') {
            throw invalid_value_exception {};
        }
        uint16_t res = 0;
        for (size_t i = s; i <= e; ++i) {
            if (str[i] < '0' || str[i] > '9') {
                throw invalid_value_exception {};
            }
            res *= 10;
            res += str[i] - '0';
        }
        if (res > 255) {
            throw invalid_value_exception {};
        }
        return (uint8_t)res;
    }

    template <size_t N>
    static constexpr uint32_t inet_pton_v4(const char (&str)[N]) {
        static_assert(N >= 7);
        if (str[N - 1] != '\0') {
            throw invalid_value_exception {};
        }
        ptrdiff_t sep3 = rfind(str, N - 2, '.');
        if (sep3 < 1) {
            throw invalid_value_exception {};
        }
        ptrdiff_t sep2 = rfind(str, sep3 - 1, '.');
        if (sep2 < 1) {
            throw invalid_value_exception {};
        }
        ptrdiff_t sep1 = rfind(str, sep2 - 1, '.');
        if (sep1 < 1) {
            throw invalid_value_exception {};
        }
        ptrdiff_t sep0 = rfind(str, sep1 - 1, '.');
        if (sep0 != -1) {
            throw invalid_value_exception {};
        }
        uint8_t c1 = parse_uint8(str, 0, sep1 - 1);
        uint8_t c2 = parse_uint8(str, sep1 + 1, sep2 - 1);
        uint8_t c3 = parse_uint8(str, sep2 + 1, sep3 - 1);
        uint8_t c4 = parse_uint8(str, sep3 + 1, N - 2);
        return host_to_network((static_cast<uint32_t>(c1) << 24) | (static_cast<uint32_t>(c2) << 16) | (static_cast<uint32_t>(c3) << 8) | static_cast<uint32_t>(c4));
    }
}
