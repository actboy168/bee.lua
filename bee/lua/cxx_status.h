#pragma once

#include <lua.hpp>

namespace bee::lua::cxx {
    struct status {
        int n;
        bool err;
        status(int n) noexcept
            : n(n)
            , err(false) {}
        status() noexcept
            : n(0)
            , err(true) {}
        explicit operator bool() const noexcept {
            return !err;
        }
    };
    static inline status error = {};
    using func                 = status (*)(lua_State* L);
    template <func f>
    static int cfunc(lua_State* L) {
        status s = f(L);
        if (!s.err) {
            return s.n;
        }
        return lua_error(L);
    }
}
