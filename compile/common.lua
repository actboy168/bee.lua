local lm = require "luamake"

lm:required_version "1.6"

lm.compile_commands = "$builddir"

lm:conf {
    c = "c11",
    cxx = "c++17",
    rtti = "off",
    windows = {
        defines = "_WIN32_WINNT=0x0602",
    },
    msvc = {
        flags = "/utf-8",
        ldflags = lm.mode == "debug" and lm.arch == "x86_64" and {
            "/STACK:"..0x160000
        },
    },
    macos = {
        flags = "-Wunguarded-availability",
        sys = "macos10.15",
    },
    linux = {
        crt = "static",
        flags = "-fPIC",
        ldflags = {
            "-Wl,-E",
            "-static-libgcc",
        },
    },
    netbsd = {
        crt = "static",
        ldflags = "-Wl,-E",
    },
    freebsd = {
        crt = "static",
        ldflags = "-Wl,-E",
    },
    openbsd = {
        crt = "static",
        ldflags = "-Wl,-E",
    },
    android = {
        ldflags = "-Wl,-E",
    },
}

if lm.sanitize then
    lm:conf {
        mode = "debug",
        flags = "-fsanitize=address",
        clang_cl = {
            mode = "release",
        },
        gcc = {
            ldflags = "-fsanitize=address"
        },
        clang = {
            ldflags = "-fsanitize=address"
        }
    }
    lm:msvc_copydll "sanitize-dll" {
        type = "asan",
        outputs = "$bin"
    }
end

lm:lua_src "source_bee" {
    sources = "3rd/lua-seri/lua-seri.cpp",
    msvc = {
        flags = "/wd4244"
    }
}

lm:source_set "source_bee" {
    sources = "3rd/fmt/format.cc",
}

local OS = {
    "win",
    "posix",
    "osx",
    "linux",
    "bsd",
}

local function need(lst)
    local map = {}
    if type(lst) == "table" then
        for _, v in ipairs(lst) do
            map[v] = true
        end
    else
        map[lst] = true
    end
    local t = {}
    for _, v in ipairs(OS) do
        if not map[v] then
            t[#t+1] = "!bee/**/*_"..v..".cpp"
            t[#t+1] = "!bee/"..v.."/**/*.cpp"
        end
    end
    return t
end

lm:source_set "source_bee" {
    includes = {
        ".",
        "3rd/lua/",
    },
    sources = "bee/**/*.cpp",
    msvc = lm.analyze and {
        flags = "/analyze",
    },
    gcc = lm.analyze and {
        flags = {
            "-fanalyzer",
            "-Wno-analyzer-use-of-uninitialized-value"
        },
    },
    windows = {
        sources = need "win"
    },
    macos = {
        sources = {
            need {
                "osx",
                "posix",
            }
        }
    },
    ios = {
        sources = {
            "!bee/filewatch/**/",
            need {
                "osx",
                "posix",
            }
        }
    },
    linux = {
        sources = need {
            "linux",
            "posix",
        }
    },
    android = {
        sources = need {
            "linux",
            "posix",
        }
    },
    netbsd = {
        sysincludes = "/usr/pkg/include",
        sources = need {
            "bsd",
            "posix",
        }
    },
    freebsd = {
        sysincludes = "/usr/local/include",
        sources = need {
            "bsd",
            "posix",
        }
    },
    openbsd = {
        sysincludes = "/usr/local/include/inotify",
        sources = need {
            "bsd",
            "posix",
        }
    }
}

lm:lua_src "source_bee" {
    includes = ".",
    defines = {
        lm.EXE ~= "lua" and "BEE_STATIC",
    },
    sources = "binding/*.cpp",
    msvc = lm.analyze and {
        flags = "/analyze",
    },
    gcc = lm.analyze and {
        flags = {
            "-fanalyzer",
            "-Wno-analyzer-use-of-uninitialized-value"
        },
    },
    windows = {
        defines = "_CRT_SECURE_NO_WARNINGS",
        sources = {
            "binding/port/lua_windows.cpp",
        },
        links = {
            "ntdll",
            "ws2_32",
            "ole32",
            "user32",
            "version",
            "synchronization",
            lm.arch == "x86" and "dbghelp",
        },
    },
    mingw = {
        links = {
            "uuid",
            "stdc++fs"
        }
    },
    linux = {
        ldflags = "-pthread",
        links = {
            "stdc++fs",
            "unwind",
            "bfd",
        }
    },
    macos = {
        frameworks = {
            "Foundation",
            "CoreFoundation",
            "CoreServices",
        }
    },
    ios = {
        sources = {
            "!binding/lua_filewatch.cpp",
        },
        frameworks = "Foundation",
    },
    netbsd = {
        links = ":libinotify.a",
        linkdirs = "/usr/pkg/lib",
        ldflags = "-pthread"
    },
    freebsd = {
        links = "inotify",
        linkdirs = "/usr/local/lib",
        ldflags = "-pthread"
    },
    openbsd = {
        links = ":libinotify.a",
        linkdirs = "/usr/local/lib/inotify",
        ldflags = "-pthread"
    },
}

lm:source_set "source_lua" {
    sources = {
        "3rd/lua/onelua.c",
    },
    defines = "MAKE_LIB",
    visibility = "default",
    windows = {
        defines = "LUA_BUILD_AS_DLL",
    },
    macos = {
        defines = "LUA_USE_MACOSX",
    },
    linux = {
        defines = "LUA_USE_LINUX",
    },
    netbsd = {
        defines = "LUA_USE_LINUX",
    },
    freebsd = {
        defines = "LUA_USE_LINUX",
    },
    openbsd = {
        defines = "LUA_USE_LINUX",
    },
    android = {
        defines = "LUA_USE_LINUX",
    },
    msvc = {
        sources = ("3rd/lua/fast_setjmp_%s.s"):format(lm.arch)
    },
}

if lm.os == "windows" then
    lm:source_set "bee_utf8_crt" {
        includes = ".",
        sources = "3rd/lua/bee_utf8_crt.cpp",
    }
end
