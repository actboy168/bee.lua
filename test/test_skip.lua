local platform = require "bee.platform"
local lt = require "ltest"

if platform.os == "freebsd" then
    lt.skip "filewatch"
    lt.skip "filesystem.test_symlink"
end

if platform.os == "openbsd" then
    lt.skip "filewatch"
end
