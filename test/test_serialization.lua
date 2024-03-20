local lt = require "ltest"

local seri = require "bee.serialization"

local function TestEq(...)
    lt.assertEquals(
        table.pack(seri.unpack(seri.pack(...))),
        table.pack(...)
    )
    lt.assertEquals(
        table.pack(seri.unpack(seri.packstring(...))),
        table.pack(...)
    )
end

local function TestErr(msg, ...)
    lt.assertErrorMsgEquals(msg, seri.pack, ...)
    lt.assertErrorMsgEquals(msg, seri.packstring, ...)
end

local test_seri = lt.test "serialization"

function test_seri:test_ok_1()
    TestEq(1)
    TestEq(0.0001)
    TestEq("TEST")
    TestEq(true)
    TestEq(false)
    TestEq({})
    TestEq({ 1, 2 })
    TestEq(1, { 1, 2 })
    TestEq(1, 2, { A = { B = { C = "D" } } })
    TestEq(1, nil, 2)
end

function test_seri:test_err_1()
    TestErr("Only light C function can be serialized", function () end)
    TestErr("Only light C function can be serialized", require)
    TestEq(os.clock)
end

function test_seri:test_err_2()
    TestErr("Unsupport type thread to serialize", coroutine.create(function () end))
end

function test_seri:test_err_3()
    TestErr("Unsupport type userdata to serialize", io.stdout)
end

function test_seri:test_ref()
    local N <const> = 10
    local t = {}
    for i = 1, N do
        t[i] = {}
    end
    for i = 1, N do
        for j = 1, N do
            t[i][j] = t[j]
        end
    end
    local newt = seri.unpack(seri.pack(t))
    for i = 1, N do
        for j = 1, N do
            lt.assertEquals(newt[i][j], newt[j])
        end
    end
end
