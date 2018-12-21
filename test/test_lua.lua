local lu = require 'luaunit'

test_lua = {}

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
            local y = table.concat(t,'',1,1)
        end})
        print(t[1])
    end)
end
