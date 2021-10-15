local lm = require 'luamake'

local internal = lm.INTERNAL

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
    defines = "BEE_INLINE",
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
            "bee/**.mm",
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
    defines = {
        "BEE_INLINE",
        internal and "BEE_STATIC",
    },
    sources = "binding/*.cpp",
    windows = {
        defines = "_CRT_SECURE_NO_WARNINGS",
        links = {
            "advapi32",
            "ws2_32",
            "ole32",
            "user32",
            "version",
            "wbemuuid",
            "oleAut32",
            "shell32",
        },
    },
    mingw = {
        links = {
            "uuid",
            "stdc++fs"
        }
    },
    linux = {
        sources = {
            "!binding/lua_unicode.cpp",
        },
        links = {
            "pthread",
        }
    },
    macos = {
        sources = {
            "!binding/lua_unicode.cpp",
        },
        frameworks = {
            "Foundation",
            "CoreFoundation",
            "CoreServices",
        }
    },
    android = {
        sources = {
            "!binding/lua_unicode.cpp",
        }
    }
}

lm:source_set 'source_lua' {
    sources = {
        "3rd/lua/*.c",
        "!3rd/lua/lua.c",
        "!3rd/lua/luac.c",
        "!3rd/lua/utf8_*.c",
    },
    windows = {
        defines = not internal and "LUA_BUILD_AS_DLL",
    },
    macos = {
        defines = "LUA_USE_MACOSX",
        visibility = not internal and "default",
    },
    linux = {
        defines = "LUA_USE_LINUX",
        visibility = not internal and "default",
    },
    android = {
        defines = "LUA_USE_LINUX",
        visibility = not internal and "default",
    },
    msvc = {
        warnings = {
            disable = {
                "4267"
            }
        }
    }
}

