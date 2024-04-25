#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__cpp_lib_expected)
#    include <expected>

namespace bee {
    using std::expected;
    using std::unexpected;
}
#else

#    include <utility>

namespace bee {
    struct in_place_t {};
    struct unexpect_t {};
    inline unexpect_t unexpect {};

    template <typename E>
    class unexpected {
    public:
        template <typename... Args>
        unexpected(Args&&... args) {
            new (&val) E(std::forward<Args>(args)...);
        }
        template <typename U, typename... Args>
        unexpected(std::initializer_list<U> il, Args&&... args) {
            new (&val) E(il, std::forward<Args>(args)...);
        }
        unexpected(E&& e) {
            new (&val) E(std::move(e));
        }

        unexpected(const unexpected&)            = default;
        unexpected(unexpected&&)                 = default;
        unexpected& operator=(const unexpected&) = default;
        unexpected& operator=(unexpected&&)      = default;

        const E& value() const& noexcept {
            return val;
        }
        E& value() & noexcept {
            return val;
        }
        const E&& value() const&& noexcept {
            return val;
        }
        E&& value() && noexcept {
            return val;
        }

    private:
        E val;
    };

    template <typename T, typename E>
    class expected {
    public:
        using value_type = T;
        using error_type = E;
        ~expected() {
            if (has_val) {
                val.~value_type();
            } else {
                unex.~error_type();
            }
        }
        expected()
            : has_val(true) {
            new (&val) value_type();
        }
        expected(const value_type& e)
            : has_val(true) {
            new (&val) value_type(e);
        }
        expected(value_type&& e)
            : has_val(true) {
            new (&val) value_type(std::move(e));
        }
        expected(const error_type& e)
            : has_val(false) {
            new (&unex) error_type(e);
        }
        expected(error_type&& e)
            : has_val(false) {
            new (&unex) error_type(std::move(e));
        }
        expected(const unexpected<error_type>& e)
            : has_val(false) {
            new (&unex) error_type(e.value());
        }
        expected(unexpected<error_type>&& e)
            : has_val(false) {
            new (&unex) error_type(std::move(e.value()));
        }
        template <class... Args>
        expected(in_place_t, Args&&... args)
            : has_val(true) {
            new (&val) value_type(std::forward<Args>(args)...);
        }
        template <class U, class... Args>
        expected(in_place_t, std::initializer_list<U> il, Args&&... args)
            : has_val(true) {
            new (&val) value_type(il, std::forward<Args>(args)...);
        }
        template <class... Args>
        expected(unexpect_t, Args&&... args)
            : has_val(false) {
            new (&unex) error_type(std::forward<Args>(args)...);
        }
        template <class U, class... Args>
        expected(unexpect_t, std::initializer_list<U> il, Args&&... args)
            : has_val(false) {
            new (&unex) error_type(il, std::forward<Args>(args)...);
        }
        template <typename Err = E>
        expected(const unexpected<Err>& other)
            : has_val(false) {
            new (&unex) error_type(other.value());
        }
        template <typename Err = E>
        expected(unexpected<Err>&& other)
            : has_val(false) {
            new (&unex) error_type(std::move(other.value()));
        }
        const value_type& value() const& {
            return val;
        }
        value_type& value() & {
            return val;
        }
        const value_type&& value() const&& {
            return std::move(val);
        }
        value_type&& value() && {
            return std::move(val);
        }
        const value_type* value_ptr() const {
            return &val;
        }
        value_type* value_ptr() {
            return &val;
        }
        const error_type& error() const& {
            return unex;
        }
        error_type& error() & {
            return unex;
        }
        const error_type&& error() const&& {
            return std::move(unex);
        }
        error_type&& error() && {
            return std::move(unex);
        }
        operator bool() const noexcept {
            return has_val;
        }
        bool has_value() const noexcept {
            return has_val;
        }

    private:
        bool has_val;
        union {
            value_type val;
            error_type unex;
        };
    };
}

#endif
