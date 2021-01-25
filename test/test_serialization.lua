local lu = require 'ltest'

local seri = require 'bee.serialization'

local function TestEq(...)
    lu.assertEquals(
        table.pack(seri.unpack(seri.pack(...))),
        table.pack(...)
    )
    lu.assertEquals(
        table.pack(seri.unpack(seri.packstring(...))),
        table.pack(...)
    )
end

local function TestErr(msg, ...)
    lu.assertErrorMsgEquals(msg, seri.pack, ...)
    lu.assertErrorMsgEquals(msg, seri.packstring, ...)
end

local test_seri = lu.test "serialization"

function test_seri:test_ok_1()
    TestEq(1)
    TestEq(0.0001)
    TestEq('TEST')
    TestEq(true)
    TestEq(false)
    TestEq({})
    TestEq({1, 2})
    TestEq(1, {1, 2})
    TestEq(1, 2, {A={B={C='D'}}})
    TestEq(1, nil, 2)
end

function test_seri:test_err_1()
    TestErr("Unsupport type function to serialize", function() end)
end

function test_seri:test_err_2()
    TestErr("Unsupport type thread to serialize", coroutine.create(function() end))
end

function test_seri:test_err_3()
    TestErr("Unsupport type userdata to serialize", io.stdout)
end

function test_seri:test_ref()
    local t = {}
    t.a = t
    local newt = seri.unpack(seri.pack(t))
    lu.assertEquals(newt, newt.a)
end

function test_seri:test_lightuserdata()
    lu.assertError(seri.lightuserdata, "")
    lu.assertError(seri.lightuserdata, 1.1)
    lu.assertEquals(seri.lightuserdata(1), seri.lightuserdata(1))
    lu.assertNotEquals(seri.lightuserdata(1), seri.lightuserdata(2))
    lu.assertEquals(type(seri.lightuserdata(1)), "userdata")
    lu.assertEquals(seri.lightuserdata((seri.lightuserdata(10086))), 10086)

    local t = {[seri.lightuserdata(0)]=seri.lightuserdata(1)}
    local newt = seri.unpack(seri.pack(t))
    lu.assertEquals(t, newt)
end
