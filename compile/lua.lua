local lm = require "luamake"
lm.rootdir = ".."

require "config"
lm:import "common.lua"

lm:lua_dll "bee" {
    deps = "source_bee",
    windows = {
        export_luaopen = "off"
    },
}

if lm.os == "windows" then
    lm:source_set "lua54" {
        sources = "3rd/lua/utf8_crt.c",
    }
    lm:shared_library "lua54" {
        sources = {
            "3rd/lua/onelua.c",
            "3rd/lua/linit.c",
        },
        defines = {
            "MAKE_LIB",
            "LUA_BUILD_AS_DLL",
        }
    }
    lm:executable "lua" {
        deps = "lua54",
        sources = {
            "3rd/lua/utf8_lua.c",
            "3rd/lua/utf8_crt.c",
        }
    }
    lm:executable "luac" {
        sources = {
            "3rd/lua/onelua.c",
            "3rd/lua/utf8_crt.c",
        },
        defines = {
            "MAKE_LUAC",
        }
    }
    return
end

lm:executable "lua" {
    deps = "source_lua",
    sources = "3rd/lua/lua.c",
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" }
    },
    netbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    freebsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    openbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    android = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" },
    }
}
