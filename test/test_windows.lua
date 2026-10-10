local lt = require "ltest"

local test_windows = lt.test "windows"

function test_windows:test_wtf8()
    local str = "ᜄȺy𐞲:𞢢𘴇𐀀'¥3̞[<i$"
    io.open(str..".txt", "wb"):close()
    os.remove(str..".txt")
end
