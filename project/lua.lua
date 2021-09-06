local lm = require "luamake"

lm:lua_dll "bee" {
    deps = "source_bee",
    linux = {
        crt = "static",
    }
}

if lm.os == "windows" then
    --TODO
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
