local lm = require 'luamake'
lm:required_version "1.2"

lm.cxx = 'c++17'

if lm.sanitize then
    lm:config "sanitize" {
        mode = "debug",
        flags = "-fsanitize=address",
        gcc = {
            ldflags = "-fsanitize=address"
        },
        clang = {
            ldflags = "-fsanitize=address"
        },
        msvc = {
            defines = "_DISABLE_STRING_ANNOTATION",
            flags = lm.mode == "release" and {
                "/wd5072",
            },
            ldflags = lm.mode == "release" and {
                "/ignore:4302",
            }
        }
    }
end

lm:config "common" {
    warnings = "error",
    rtti = "off",
    windows = {
        defines = "_WIN32_WINNT=0x0601",
    },
    msvc = lm.mode == "debug" and lm.arch == "x86_64" and {
        ldflags = "/STACK:" .. 0x160000
    },
    macos = {
        flags = "-Wunguarded-availability",
        sys = "macos10.15",
    },
    linux = {
        flags = "-fPIC",
    },
}

lm.configs = {
    "common",
    lm.sanitize and "sanitize"
}
