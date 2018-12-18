local lu = require 'luaunit'

test_lua = {}

function test_lua:test_stack_overflow()
    lu.assertError(function()
        local function a()
            a()
        end
        a()
    end)
end
