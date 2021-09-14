local lm = require "luamake"
lm.rootdir = ".."

require 'config'
lm:import 'common.lua'

lm:lua_dll "bee" {
    deps = "source_bee",
    windows = {
        export_luaopen = "off"
    },
    linux = {
        crt = "static",
    }
}

if lm.os == "windows" then
    lm:shared_library 'lua54' {
        sources = {
            "3rd/lua/*.c",
            "!3rd/lua/lua.c",
            "!3rd/lua/luac.c",
            "!3rd/lua/utf8_lua.c",
        },
        defines = {
            "LUA_BUILD_AS_DLL",
        }
    }
    lm:executable 'lua' {
        deps = "lua54",
        sources = {
            "3rd/lua/utf8_lua.c",
            "3rd/lua/utf8_crt.c",
            lm.EXE_RESOURCE,
        }
    }
    return
end

lm:executable 'lua' {
    deps = "source_lua",
    sources = "3rd/lua/lua.c",
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        ldflags = "-Wl,-E",
        defines = "LUA_USE_LINUX",
        links = {
            "m", "dl",
            -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
            "pthread",
        }
    },
    android = {
        ldflags = "-Wl,-E",
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" },
    }
}
