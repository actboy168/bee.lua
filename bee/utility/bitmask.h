#pragma once

#include <type_traits>

#define BEE_BITMASK_OPERATORS(bitmask)                                         \
    [[nodiscard]] constexpr bitmask operator~(bitmask a) noexcept {            \
        return static_cast<bitmask>(                                           \
            ~static_cast<std::underlying_type_t<bitmask>>(a)                   \
        );                                                                     \
    }                                                                          \
    [[nodiscard]] constexpr bitmask operator|(bitmask a, bitmask b) noexcept { \
        return static_cast<bitmask>(                                           \
            static_cast<std::underlying_type_t<bitmask>>(a) |                  \
            static_cast<std::underlying_type_t<bitmask>>(b)                    \
        );                                                                     \
    }                                                                          \
    [[nodiscard]] constexpr bitmask operator&(bitmask a, bitmask b) noexcept { \
        return static_cast<bitmask>(                                           \
            static_cast<std::underlying_type_t<bitmask>>(a) &                  \
            static_cast<std::underlying_type_t<bitmask>>(b)                    \
        );                                                                     \
    }                                                                          \
    [[nodiscard]] constexpr bitmask operator^(bitmask a, bitmask b) noexcept { \
        return static_cast<bitmask>(                                           \
            static_cast<std::underlying_type_t<bitmask>>(a) ^                  \
            static_cast<std::underlying_type_t<bitmask>>(b)                    \
        );                                                                     \
    }                                                                          \
    constexpr bitmask& operator|=(bitmask& a, bitmask b) noexcept {            \
        return a = a | b;                                                      \
    }                                                                          \
    constexpr bitmask& operator&=(bitmask& a, bitmask b) noexcept {            \
        return a = a & b;                                                      \
    }                                                                          \
    constexpr bitmask& operator^=(bitmask& a, bitmask b) noexcept {            \
        return a = a ^ b;                                                      \
    }

namespace bee {
    template <class Bitmask>
    [[nodiscard]] constexpr bool bitmask_has(Bitmask a, Bitmask b) noexcept {
        return (a & b) != Bitmask {};
    }
}
