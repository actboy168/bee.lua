local lm = require "luamake"

lm:required_version "1.6"

lm.compile_commands = "$builddir"

lm.lua = lm.lua or "55"
lm.luadir = lm:path("$builddir/patched/lua"..lm.lua)

-- 通用 Lua 补丁基础设施：构建期把官方源码整树复制到
-- $builddir/patched/lua<ver>/，再按注册表顺序应用启用的补丁（git apply）。
-- 无启用补丁时退化为纯复制；整树复制使 include 始终解析到补丁目录内的
-- 文件，新增补丁无需改动构建代码。
local fs = require "bee.filesystem"

-- 补丁注册表：补丁文件约定为 3rd/lua-patch/<dir>/lua<ver>.patch；
-- flag 是可选的命令行开关（luamake -<flag>），省略则始终启用。
local lua_patches <const> = {
    { flag = "optchain", dir = "optchain" },
}

local srcdir = "3rd/lua"..lm.lua
local dstdir = tostring(lm.luadir)

local args = { srcdir, dstdir }
local inputs = {}
for _, p in ipairs(lua_patches) do
    if not p.flag or lm[p.flag] then
        local patchfile = ("3rd/lua-patch/%s/lua%s.patch"):format(p.dir, lm.lua)
        assert(fs.exists(fs.path(lm.workdir) / patchfile), "patch not found: " .. patchfile)
        args[#args+1] = patchfile
        inputs[#inputs+1] = patchfile
    end
end

local outputs = {}
local srcpath = fs.path(lm.workdir) / srcdir
for file in fs.pairs_r(srcpath) do
    if fs.is_regular_file(file) then
        local rel = fs.relative(file, srcpath):string()
        inputs[#inputs+1] = srcdir .. "/" .. rel
        outputs[#outputs+1] = dstdir .. "/" .. rel
    end
end

lm:runlua "apply_lua_patch" {
    script = "compile/apply_patch.lua",
    args = args,
    inputs = inputs,
    outputs = outputs,
    restat = true,
}

local function macos_version()
    local cxx = lm.cxx or "c++17"
    local version = cxx:match "^c%+%+(.+)$"
    if version == "17" then
        return "macos10.15"
    else
        return "macos13.3"
    end
end

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
        sys = macos_version(),
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

lm:source_set "source_lua" {
    objdeps = "apply_lua_patch",
    includes = {
        lm.luadir,
        "3rd/lua-patch",
    },
    sources = {
        lm.luadir / "onelua.c",
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
    msvc = lm.fast_setjmp ~= "off" and {
        defines = "BEE_FAST_SETJMP",
        sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch),
    }
}

lm:source_set "source_bee" {
    objdeps = "apply_lua_patch",
    includes = lm.luadir,
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
    objdeps = "apply_lua_patch",
    includes = {
        ".",
        lm.luadir,
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
            },
            lm.async_backend == "kqueue" and {
                "!bee/async/async_osx.cpp",
                "bee/async/async_bsd.cpp"
            },
        },
        defines = lm.async_backend == "kqueue" and "BEE_ASYNC_BACKEND_KQUEUE",
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
        sources = {
            "!bee/crash/linux/**/",
            need {
                "linux",
                "posix",
            },
            lm.async_backend == "epoll" and {
                "!bee/async/async_uring_linux.cpp",
            },
        },
        defines = lm.async_backend == "epoll" and "BEE_ASYNC_BACKEND_EPOLL",
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

lm:source_set "source_bee" {
    objdeps = "apply_lua_patch",
    includes = {
        ".",
        lm.luadir,
    },
    defines = {
        lm.EXE ~= "lua" and "BEE_STATIC",
    },
    sources = {
        "binding/*.cpp",
        "3rd/lua-patch/bee_newstate.c",
    },
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
            "advapi32",
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
            --"unwind",
            --"bfd",
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

if lm.os == "windows" then
    lm:source_set "bee_utf8_crt" {
        includes = ".",
        sources = "3rd/lua-patch/bee_utf8_crt.cpp",
    }
end
