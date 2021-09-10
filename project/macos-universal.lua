local lm = require "luamake"

local BOOTSTRAP = lm.EXE_NAME or "bootstrap"

lm:build(BOOTSTRAP.."-arm64") {
    "$luamake",
    "-notest",
    "-builddir", "build/arm64",
    "-target", "arm64-apple-macos11",
    "-EXE_NAME", BOOTSTRAP,
    pool = "console",
}

lm:build(BOOTSTRAP.."-x86_64") {
    "$luamake",
    "-notest",
    "-builddir", "build/x86_64",
    "-target", "x86_64-apple-macos10.12",
    "-EXE_NAME", BOOTSTRAP,
    pool = "console",
}

lm:build "mkdir" {
    "mkdir", "-p", "$bin"
}

lm:build(BOOTSTRAP.."-universal") {
    deps = {
        BOOTSTRAP.."-arm64",
        BOOTSTRAP.."-x86_64",
        "mkdir"
    },
    "lipo", "-create", "-output", "$bin/"..BOOTSTRAP,
    "build/arm64/bin/"..BOOTSTRAP,
    "build/x86_64/bin/"..BOOTSTRAP,
}

lm:copy "copy_script" {
    input = "@../bootstrap/main.lua",
    output = "$bin/main.lua",
    deps = BOOTSTRAP.."-universal",
}

if not lm.notest then
    lm:build "test" {
        "$bin/"..BOOTSTRAP, "@../test/test.lua",
        deps = "copy_script",
        pool = "console",
    }
end
