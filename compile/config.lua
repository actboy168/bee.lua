local lm = require "luamake"
lm:required_version "1.2"

lm.c = "c11"
lm.cxx = "c++17"
lm.rtti = "off"

if lm.sanitize then
    lm:config "sanitize" {
        mode = "debug",
        flags = "-fsanitize=address",
        gcc = {
            ldflags = "-fsanitize=address"
        },
        clang = {
            ldflags = "-fsanitize=address"
        }
    }
    lm:msvc_copydll "sanitize-dll" {
        type = "asan",
        output = "$bin"
    }
end

lm:config "test" {
    msvc = lm.mode == "debug" and lm.arch == "x86_64" and {
        ldflags = "/STACK:"..0x160000
    },
}

lm:config "prebuilt" {
    windows = {
        defines = "_WIN32_WINNT=0x0601",
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

lm.configs = {
    "test",
    "prebuilt",
    lm.sanitize and "sanitize"
}
