local wmi = require "bee.wmi"
local lu = require "luaunit"

local test_wmi = lu.test "wmi"

function test_wmi:test_query()
    local function test(sql)
        for _, res in ipairs(wmi.query(sql)) do
            for _ in pairs(res) do
                --do nothing
            end
        end
    end
    test "SELECT * FROM Win32_OperatingSystem"
    test "SELECT * FROM Meta_Class"
end
