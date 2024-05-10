local fs       = require "bee.filesystem"
local sys      = require "bee.sys"
local lt       = require "ltest"
local shell    = require "shell"

local test_sys = lt.test "sys"

function test_sys:test_filelock_1()
    local lock = "temp.lock"
    local f1 = lt.assertIsUserdata(sys.filelock(lock))
    lt.assertEquals(sys.filelock(lock), nil)
    f1:close()
    local f2 = lt.assertIsUserdata(sys.filelock(lock))
    f2:close()
    fs.remove "temp.lock"
end

function test_sys:test_filelock_2()
    local process = shell:runlua([[
    local sys = require "bee.sys"
    sys.filelock "temp.lock"
    io.stdout:write "ok"
    io.stdout:flush()
    io.read "a"
]], { stdin = true, stdout = true, stderr = true })
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(sys.filelock "temp.lock", nil)
    process.stdin:close()
    lt.assertEquals(process.stderr:read "a", "")
    lt.assertEquals(process:wait(), 0)
    local f = lt.assertIsUserdata(sys.filelock "temp.lock")
    f:close()
    fs.remove "temp.lock"
end
