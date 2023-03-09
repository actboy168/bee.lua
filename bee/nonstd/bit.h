#pragma once

#include <version>

#if defined(__has_builtin)
#define BEE_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BEE_HAS_BUILTIN(x) 0
#endif

#if defined(__cpp_lib_bit_cast)
#include <bit>
#elif BEE_HAS_BUILTIN(__builtin_bit_cast) || (defined(_MSC_VER) && _MSC_VER >= 1927)
#include <type_traits>
namespace std {
template <typename To, typename From, typename = std::enable_if_t<
    sizeof(To) == sizeof(From)
    && std::is_trivially_copyable_v<To>
    && std::is_trivially_copyable_v<From>
>>
constexpr To bit_cast(const From& v) noexcept {
    return __builtin_bit_cast(To, v);
}
}
#else
#include <type_traits>
#include <memory>
#include <cstring>
namespace std {
template <typename To, typename From, typename = std::enable_if_t<
    sizeof(To) == sizeof(From)
    && std::is_trivially_copyable_v<To>
    && std::is_trivially_copyable_v<From>
    && std::is_default_constructible_v<To>
>>
#if BEE_HAS_BUILTIN(__builtin_memcpy) || defined(__GNUC__)
constexpr
#endif
To bit_cast(const From& src) noexcept {
    To dst;
#if BEE_HAS_BUILTIN(__builtin_memcpy) || defined(__GNUC__)
    __builtin_memcpy
#else
    std::memcpy
#endif
        (static_cast<void*>(std::addressof(dst)), static_cast<const void*>(std::addressof(src)), sizeof(src));
    return dst;
}
}
#endif
