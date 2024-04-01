#pragma once

#include <bee/lua/cxx_status.h>

#include <type_traits>

namespace bee::lua {
    template <class U>
    cxx::status narrow_pushinteger(lua_State* L, U u) noexcept {
        constexpr const bool is_different_signedness = (std::is_signed<lua_Integer>::value != std::is_signed<U>::value);
        const lua_Integer t                          = static_cast<lua_Integer>(u);
        if (static_cast<U>(t) != u || (is_different_signedness && ((t < lua_Integer {}) != (u < U {})))) {
            lua_pushstring(L, "narrowing_error");
            return cxx::error;
        }
        lua_pushinteger(L, t);
        return 1;
    }
}
