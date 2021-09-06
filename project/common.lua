local lm = require 'luamake'

lm:source_set "source_bee" {
    includes = {
        "3rd/lua",
        "3rd/lua-seri",
    },
    sources = {
        "3rd/lua-seri/*.c",
    },
    linux = {
        flags = "-fPIC"
    }
}

lm:source_set "source_bee" {
    includes = {
        "bee/nonstd",
        "."
    },
    defines = {
        "BEE_INLINE",
    },
    sources = {
        "bee/**.cpp",
        "bee/nonstd/fmt/*.cc",
    },
    windows = {
        sources = {
            "!bee/**_osx.cpp",
            "!bee/**_linux.cpp",
            "!bee/**_posix.cpp",
        }
    },
    macos = {
        sources = {
            "!bee/**_win.cpp",
            "!bee/**_linux.cpp",
        }
    },
    linux = {
        flags = "-fPIC",
        sources = {
            "!bee/**_win.cpp",
            "!bee/**_osx.cpp",
        }
    },
    android = {
        sources = {
            "!bee/**_win.cpp",
            "!bee/**_osx.cpp",
        }
    }
}

lm:source_set "source_bee" {
    includes = {
        "3rd/lua",
        "3rd/lua-seri",
        "bee/nonstd",
        "."
    },
    defines = "BEE_INLINE",
    sources = "binding/*.cpp",
    windows = {
        deps = "lua54",
        defines = "_CRT_SECURE_NO_WARNINGS",
        links = {
            "advapi32",
            "ws2_32",
            "ole32",
            "user32",
            "version",
            "wbemuuid",
            "oleAut32",
        },
    },
    mingw = {
        links = "stdc++fs"
    },
    linux = {
        crt = "static",
        sources = {
            "!binding/lua_unicode.cpp",
            "!binding/lua_registry.cpp",
            "!binding/lua_wmi.cpp",
        },
        links = {
            "pthread",
        }
    },
    macos = {
        sources = {
            "!binding/lua_unicode.cpp",
            "!binding/lua_registry.cpp",
            "!binding/lua_wmi.cpp",
        },
        frameworks = {
            "CoreFoundation",
            "CoreServices",
        }
    },
    android = {
        sources = {
            "!binding/lua_unicode.cpp",
            "!binding/lua_registry.cpp",
            "!binding/lua_wmi.cpp",
        }
    }
}

lm:shared_library "bee" {
    deps = "source_bee"
}

lm:executable 'bootstrap' {
    deps = "source_bee",
    includes = "3rd/lua",
    sources = "bootstrap/*.cpp",
    windows = {
        deps = "lua54",
        sources = {
            "3rd/lua/utf8_crt.c",
            lm.EXE_RESOURCE,
        },
    },
    macos = {
        deps = "source_lua",
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        deps = "source_lua",
        defines = "LUA_USE_LINUX",
        ldflags = "-Wl,-E",
        links = {
            "m", "dl",
            -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
            "pthread",
        }
    },
    android = {
        deps = "source_lua",
        defines = "LUA_USE_LINUX",
        ldflags = "-Wl,-E",
        links = { "m", "dl" }
    }
}
