#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace bee {
    template <class, class = void>
    struct span_has_size_and_data : std::false_type {};
    template <class C>
    struct span_has_size_and_data<C, std::void_t<decltype(std::size(std::declval<C>())), decltype(std::data(std::declval<C>()))>> : std::true_type {};
    template <class, class, class = void>
    struct span_compatible_element : std::false_type {};
    template <class C, class E>
    struct span_compatible_element<C, E, std::void_t<decltype(std::data(std::declval<C>()))>> : std::is_convertible<std::remove_pointer_t<decltype(std::data(std::declval<C &>()))> (*)[], E (*)[]> {};
    template <class C, class E>
    struct span_compatible_container : std::bool_constant<span_has_size_and_data<C>::value && span_compatible_element<C, E>::value> {};
    template <class T>
    class span {
    public:
        using element_type           = T;
        using value_type             = std::remove_cv_t<T>;
        using reference              = T &;
        using pointer                = T *;
        using const_pointer          = const T *;
        using const_reference        = const T &;
        using size_type              = size_t;
        using iterator               = pointer;
        using const_iterator         = const_pointer;
        using difference_type        = std::ptrdiff_t;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        constexpr span() noexcept
            : data_(nullptr)
            , size_(0) {
        }
        constexpr span(pointer ptr, size_type count) noexcept
            : data_(ptr)
            , size_(count) {
        }
        template <size_t N, std::enable_if_t<std::is_convertible_v<value_type (*)[], element_type (*)[]>, int> = 0>
        constexpr span(element_type (&arr)[N]) noexcept
            : data_(std::addressof(arr[0]))
            , size_(N) {}
        template <class Container, std::enable_if_t<span_compatible_container<Container, element_type>::value, int> = 0>
        constexpr span(Container &cont) noexcept
            : data_(std::data(cont))
            , size_(std::size(cont)) {}
        template <class Container, std::enable_if_t<std::is_const_v<element_type> && span_compatible_container<Container, element_type>::value, int> = 0>
        constexpr span(const Container &cont) noexcept
            : data_(std::data(cont))
            , size_(std::size(cont)) {}
        constexpr span(const span &other) noexcept            = default;
        ~span() noexcept                                      = default;
        constexpr span &operator=(const span &other) noexcept = default;
        constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        }
        constexpr reference operator[](size_type idx) const noexcept {
            assert(idx < size());
            return *(data() + idx);
        }
        constexpr pointer data() const noexcept {
            return data_;
        }
        constexpr iterator begin() const noexcept {
            return { data() };
        }
        constexpr iterator end() const noexcept {
            return { data() + size() };
        }
        constexpr const_iterator cbegin() const noexcept {
            return { data() };
        }
        constexpr const_iterator cend() const noexcept {
            return { data() + size() };
        }
        constexpr reverse_iterator rbegin() const noexcept {
            return reverse_iterator(end());
        }
        constexpr reverse_iterator rend() const noexcept {
            return reverse_iterator(begin());
        }
        constexpr const_reverse_iterator crbegin() const noexcept {
            return const_reverse_iterator(cend());
        }
        constexpr const_reverse_iterator crend() const noexcept {
            return const_reverse_iterator(cbegin());
        }

    private:
        pointer data_;
        size_type size_;
    };
}
