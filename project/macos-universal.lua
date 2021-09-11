local lm = require "luamake"

local BOOTSTRAP = lm.EXE_NAME or "bootstrap"
local EXE_DIR = lm.EXE_DIR or "$bin"

lm:build(BOOTSTRAP.."-arm64") {
    "$luamake",
    "-notest",
    "-builddir", "$builddir/arm64",
    "-target", "arm64-apple-macos11",
    "-EXE_NAME", BOOTSTRAP,
    "-f", "@../make.lua",
    pool = "console",
    output = "$builddir/arm64/bin/"..BOOTSTRAP,
}

lm:build(BOOTSTRAP.."-x86_64") {
    "$luamake",
    "-notest",
    "-builddir", "$builddir/x86_64",
    "-target", "x86_64-apple-macos10.12",
    "-EXE_NAME", BOOTSTRAP,
    "-f", "@../make.lua",
    pool = "console",
    output = "$builddir/x86_64/bin/"..BOOTSTRAP,
}

lm:build "mkdir" {
    "mkdir", "-p", EXE_DIR
}

lm:build(BOOTSTRAP.."-universal") {
    "lipo", "-create", "-output", EXE_DIR.."/"..BOOTSTRAP, "$in",
    deps = "mkdir",
    input = {
        "$builddir/arm64/bin/"..BOOTSTRAP,
        "$builddir/x86_64/bin/"..BOOTSTRAP,
    }
}

lm:copy "copy_script" {
    input = "@../bootstrap/main.lua",
    output = EXE_DIR.."/main.lua",
    deps = BOOTSTRAP.."-universal",
}

if not lm.notest then
    lm:build "test" {
        EXE_DIR.."/"..BOOTSTRAP, "@../test/test.lua",
        deps = "copy_script",
        pool = "console",
    }
end
