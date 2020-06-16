local lu = require 'luaunit'

local test_lua = lu.test "lua"

function test_lua:test_stack_overflow_1()
    lu.assertError(function()
        local function a()
            a()
        end
        a()
    end)
end

function test_lua:test_stack_overflow_2()
    lu.assertError(function()
        local t = setmetatable({}, {__index = function(t)
            table.concat(t,'',1,1)
        end})
        print(t[1])
    end)
end

function test_lua:test_next()
    local t = {}
    for i = 1, 26 do
        t[string.char(0x40+i)] = true
    end
    local expected = {
        'Z', 'Y', 'V', 'U', 'X', 'W', 'R', 'Q', 'T', 'S', 'N', 'M', 'P', 'O', 'J', 'I', 'L', 'K', 'F', 'E', 'H', 'G', 'B', 'A', 'D', 'C'
    }
    local function checkOK()
        local key
        for i = 1, 26 do
            key = next(t, key)
            lu.assertEquals(key, expected[i])
        end
    end
    checkOK()
end
