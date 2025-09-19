local platform = require "bee.platform"
local lt = require "ltest"
local shell = require "shell"

local SKIP <const> = lt.skip

SKIP "lua.test_stack_overflow_1"
SKIP "lua.test_stack_overflow_2"

if platform.os == "macos" then
    SKIP "thread.test_sleep"
end

if platform.os == "freebsd" then
    SKIP "filewatch"
    SKIP "filesystem.test_symlink"
end

if platform.os == "openbsd" then
    SKIP "filewatch"
    SKIP "thread.test_sleep"
end

if platform.os == "netbsd" then
    SKIP "filewatch"
    SKIP "thread.test_sleep"
end

if shell.is_luamake then
    SKIP "subprocess.test_args"
    SKIP "subprocess.test_cwd"
end
