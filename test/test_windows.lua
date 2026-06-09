local lt = require "ltest"

local test_windows = lt.test "windows"

function test_windows:test_wtf8()
    local str = "ᜄȺy𐞲:𞢢𘴇𐀀'¥3̞[<i$"
    io.open(str..".txt", "wb"):close()
    os.remove(str..".txt")
end

function test_windows:test_find_file_holders()
    local windows = require "bee.windows"
    local subprocess = require "bee.subprocess"
    local my_pid = subprocess.get_id()
    local f = io.open("holders_test.txt", "w")
    f:write("test")
    f:flush()
    local pids = windows.find_file_holders("holders_test.txt")
    local found = false
    for _, pid in ipairs(pids) do
        if pid == my_pid then found = true end
    end
    lt.assertIsTable(pids)
    f:close()
    os.remove("holders_test.txt")
    if not found then
        lt.failure("should find current process")
    end
end

function test_windows:test_find_file_holders_nonexistent()
    local windows = require "bee.windows"
    local pids = windows.find_file_holders("X:\\nonexistent\\path\\file.txt")
    lt.assertEquals(#pids, 0)
end

function test_windows:test_find_file_holders_unrelated()
    local windows = require "bee.windows"
    local subprocess = require "bee.subprocess"
    -- os.tmpname() generates a unique path; no process should hold this file
    local pids = windows.find_file_holders(os.tmpname())
    local my_pid = subprocess.get_id()
    for _, pid in ipairs(pids) do
        lt.assertNotEquals(pid, my_pid)
    end
end
