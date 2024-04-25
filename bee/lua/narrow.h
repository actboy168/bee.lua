#pragma once

#include <limits>
#include <lua.hpp>

namespace bee::lua {
    template <class U>
    int narrow_pushinteger(lua_State* L, U u) noexcept {
        static_assert(sizeof(U) <= sizeof(lua_Integer));
        static_assert(std::numeric_limits<lua_Integer>::is_signed);
        if constexpr ((sizeof(U) < sizeof(lua_Integer)) || std::numeric_limits<U>::is_signed) {
            lua_pushinteger(L, static_cast<lua_Integer>(u));
            return 1;
        } else {
            constexpr U umax = static_cast<U>((std::numeric_limits<lua_Integer>::max)());
            lua_pushinteger(L, u > umax ? umax : static_cast<lua_Integer>(u));
            return 1;
        }
    }
}
