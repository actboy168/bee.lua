local lm = require "luamake"

lm:required_version "1.6"

lm.compile_commands = "$builddir"

lm.lua = lm.lua or "55"
lm.luadir = lm:path("3rd/lua"..lm.lua)
lm.luasrcdir = lm.luadir

-- 可选链补丁：将官方源码复制到构建目录并应用补丁（git apply）。
-- 复制清单（无需手工维护）：
--   * 补丁涉及的文件——生成期从 git diff 补丁头解析
--   * 编译入口 onelua.c / linit.c / lua.c——onelua.c 是 amalgamation，
--     会 #include 补丁涉及的源文件，必须与补丁版本同目录
--   * 全部头文件——bootstrap/binding 等只 -I 补丁目录；但 lprefix.h
--     相对引用 ../lua-patch/ 下的头文件，编译 lua 源码的目标还需把
--     官方源码目录（lm.luasrcdir）加进 includes
-- inputs 即复制清单对应的源文件 + 补丁 + 脚本：任一变化都会重新打补丁；
-- apply_patch.lua 内容不变不重写文件，配合规则的 restat = 1，产物没变
-- 时不会触发下游重编（增量编译）。
-- 补丁直接重写的文件不能作为 outputs：git apply 每次都会重写它们（即使
-- 内容不变），restat 会误判为产物变化；其消费者（onelua.c 的 #include）
-- 由编译器 depfile 跟踪，内容变化时依然会触发重编。
if lm.optchain then
    local fs = require "bee.filesystem"

    local script = tostring(lm:path("3rd/lua-patch/apply_patch.lua"))
    local srcdir = tostring(lm.luasrcdir)
    local patchfile = tostring(lm:path("3rd/lua-patch/optchain/lua" .. lm.lua .. ".patch"))
    lm.luadir = lm:path("$builddir/patched/lua" .. lm.lua)
    local dstdir = tostring(lm.luadir)

    local files = {}
    local mark = {}
    local patchedmark = {}
    local function add(file)
        if not mark[file] then
            mark[file] = true
            files[#files+1] = file
        end
    end

    -- 从 git diff 补丁头解析补丁涉及的文件
    for line in io.lines(patchfile) do
        local file = line:match "^diff %-%-git a/(%S+) %S+$"
        if file then
            patchedmark[file] = true
            add(file)
        end
    end

    -- 编译入口
    for _, file in ipairs {
        "onelua.c",
        "linit.c",
        "lua.c",
    } do
        add(file)
    end

    -- 全部头文件
    local srcpath = fs.path(lm.workdir) / srcdir
    for file in fs.pairs_r(srcpath) do
        local ext = file:extension()
        if ext == ".h" or ext == ".hpp" then
            add(fs.relative(file, srcpath):string())
        end
    end

    -- prebuilt 模式下 $luamake 是 bootstrap（直接运行脚本），没有 lua 子命令
    local command = { "$luamake" }
    if not lm.prebuilt then
        command[#command+1] = "lua"
    end
    command[#command+1] = script
    command[#command+1] = srcdir
    command[#command+1] = dstdir
    command[#command+1] = patchfile
    for _, file in ipairs(files) do
        command[#command+1] = file
    end

    lm:rule "apply_optchain_patch" {
        args = command,
        description = "Apply optchain patch.",
        restat = 1,
    }

    local inputs = {
        script,
        patchfile,
    }
    for _, file in ipairs(files) do
        inputs[#inputs+1] = srcdir .. "/" .. file
    end
    local outputs = {}
    for _, file in ipairs(files) do
        if not patchedmark[file] then
            outputs[#outputs+1] = dstdir .. "/" .. file
        end
    end

    lm:build "apply_optchain_patch" {
        rule = "apply_optchain_patch",
        deps = lm.prebuilt and "bootstrap",
        inputs = inputs,
        outputs = outputs,
    }
end

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
    objdeps = lm.optchain and "apply_optchain_patch",
    -- optchain 时 lua 源码在补丁目录，但 lprefix.h 相对引用
    -- ../lua-patch/ 下的头文件，需要源码目录作为 -I 解析前缀
    includes = {
        lm.luadir,
        lm.optchain and lm.luasrcdir,
    },
    sources = {
        lm.luadir / "onelua.c",
    },
    defines = {
        "MAKE_LIB",
    },
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
    objdeps = lm.optchain and "apply_optchain_patch",
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
    objdeps = lm.optchain and "apply_optchain_patch",
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
    objdeps = lm.optchain and "apply_optchain_patch",
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
