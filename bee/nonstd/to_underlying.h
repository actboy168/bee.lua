#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__cpp_lib_to_underlying)
#    include <utility>
#else
#    include <type_traits>
namespace std {
    template <typename E, typename = std::enable_if_t<std::is_enum_v<E> > >
    constexpr std::underlying_type_t<E> to_underlying(E e) noexcept {
        return static_cast<std::underlying_type_t<E> >(e);
    }
}
#endif
