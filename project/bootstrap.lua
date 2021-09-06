local lm = require "luamake"

local BOOTSTRAP = lm.EXE_NAME or "bootstrap"

lm:executable (BOOTSTRAP) {
    deps = { "source_bee", "source_lua" },
    includes = "3rd/lua",
    sources = "bootstrap/*.cpp",
    windows = {
        sources = {
            "3rd/lua/utf8_crt.c",
            lm.EXE_RESOURCE,
        },
        ldflags = {
            "/IMPLIB:$obj/$name/$name.lib"
        }
    },
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        crt = "static",
        defines = "LUA_USE_LINUX",
        ldflags = "-Wl,-E",
        links = {
            "m", "dl",
            -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
            "pthread",
        }
    },
    android = {
        defines = "LUA_USE_LINUX",
        ldflags = "-Wl,-E",
        links = { "m", "dl" }
    }
}

if lm.os == "windows" then
    lm:build "forward_lua" {
        "$luamake", "lua", "@bootstrap/forward_lua.lua","@3rd/lua/", "@bootstrap/forward_lua.def", BOOTSTRAP..".exe"
    }
    lm:shared_library "lua54" {
        includes = "3rd/lua",
        sources = {
            "bootstrap/forward_lua.c",
        },
        ldflags = {
            "/def:bootstrap/forward_lua.def",
            "$obj/"..BOOTSTRAP.."/"..BOOTSTRAP..".lib",
        },
        links= "user32",
        deps = {
            "forward_lua",
            BOOTSTRAP,
        }
    }
end
