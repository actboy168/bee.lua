local platform = require "bee.platform"
local lt = require "ltest"

local SKIP <const> = lt.skip

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
