#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__has_builtin)
#    define BEE_HAS_BUILTIN(x) __has_builtin(x)
#else
#    define BEE_HAS_BUILTIN(x) 0
#endif

#if defined(__cpp_lib_bit_cast)
#    include <bit>
#elif BEE_HAS_BUILTIN(__builtin_bit_cast) || (defined(_MSC_VER) && _MSC_VER >= 1927)
#    include <type_traits>
namespace std {
    template <typename To, typename From, typename = std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From> > >
    constexpr To bit_cast(const From& v) noexcept {
        return __builtin_bit_cast(To, v);
    }
}
#else
#    include <cstring>
#    include <memory>
#    include <type_traits>
namespace std {
    template <typename To, typename From, typename = std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From> && std::is_default_constructible_v<To> > >
    To bit_cast(const From& src) noexcept {
        To dst;
        std::memcpy(static_cast<void*>(std::addressof(dst)), static_cast<const void*>(std::addressof(src)), sizeof(src));
        return dst;
    }
}
#endif

#if defined(__cpp_lib_endian)
#    include <bit>
#else
namespace std {
    enum class endian {
#    if defined(_MSC_VER) && !defined(__clang__)
        little = 0,
        big    = 1,
        native = little,
#    else
        little = __ORDER_LITTLE_ENDIAN__,
        big    = __ORDER_BIG_ENDIAN__,
        native = __BYTE_ORDER__,
#    endif
    };
}
#endif
