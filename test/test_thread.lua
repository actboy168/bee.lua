local lu = require 'luaunit'

local thread = require "bee.thread"
local fs = require "bee.filesystem"
local err = thread.channel "errlog"

local cpath_template = ("package.cpath = [[%s]]\n"):format(package.cpath)

local function createThread(script)
    return thread.thread(cpath_template .. script)
end

local function assertNotThreadError()
    lu.assertIsFalse(err:pop())
end

local function assertHasThreadError(m)
    local ok, msg = err:pop()
    lu.assertIsTrue(ok)
    lu.assertErrorMsgContains(m, error, msg)
end

test_thread = {}

function test_thread:test_thread_1()
    assertNotThreadError()
    fs.remove(fs.path('temp.txt'))
    lu.assertIsFalse(fs.exists(fs.path('temp.txt')))
    local thd = createThread [[
        io.open('temp.txt', 'w'):close()
    ]]
    thd:wait()
    lu.assertIsTrue(fs.exists(fs.path('temp.txt')))
    fs.remove(fs.path('temp.txt'))
    assertNotThreadError()
end

function test_thread:test_thread_2()
    assertNotThreadError()
    GLOBAL = true
    THREAD = nil
    lu.assertNotNil(GLOBAL)
    lu.assertIsNil(THREAD)
    local thd = createThread [[
        THREAD = true
        assert(GLOBAL == nil)
    ]]
    thd:wait()
    assertNotThreadError()
    lu.assertNotNil(GLOBAL)
    lu.assertIsNil(THREAD)
    GLOBAL = nil
    THREAD = nil
end

function test_thread:test_thread_3()
    assertNotThreadError()
    local thd = createThread [[
        error 'Test thread error.'
    ]]
    thd:wait()
    assertHasThreadError('Test thread error.')
    assertNotThreadError()
end

function test_thread:test_channel_1()
    thread.reset()
    lu.assertErrorMsgEquals("Can't query channel 'test'", thread.channel, 'test')
    thread.newchannel 'test'
    lu.assertIsUserdata(thread.channel 'test')
    lu.assertIsUserdata(thread.channel 'test')
    thread.reset()
end

function test_thread:test_channel_2()
    thread.reset()
    thread.newchannel 'test'
    lu.assertErrorMsgEquals("Duplicate channel 'test'", thread.newchannel, 'test')
    thread.reset()
end

function test_thread:test_id_1()
    assertNotThreadError()
    lu.assertEquals(thread.id, 0)
    local thd = createThread [[
        local thread = require "bee.thread"
        assert(thread.id ~= 0)
    ]]
    thd:wait()
    assertNotThreadError()
end

function test_thread:test_id_2()
    assertNotThreadError()
    lu.assertEquals(thread.id, 0)
    thread.reset()
    lu.assertEquals(thread.id, 0)
    local thd = createThread [[
        local thread = require "bee.thread"
        assert(thread.id ~= 0)
    ]]
    thd:wait()
    assertNotThreadError()
end

function test_thread:test_reset_1()
    thread.reset()
    lu.assertErrorMsgEquals("Can't query channel 'test'", thread.channel, 'test')
    thread.newchannel 'test'
    lu.assertIsUserdata(thread.channel 'test')
    thread.reset()
    lu.assertErrorMsgEquals("Can't query channel 'test'", thread.channel, 'test')
    thread.newchannel 'test'
    lu.assertIsUserdata(thread.channel 'test')
    thread.reset()
end

function test_thread:test_reset_2()
    assertNotThreadError()
    local thd = createThread [[
        local thread = require "bee.thread"
        thread.reset()
    ]]
    thd:wait()
    assertHasThreadError('reset must call from main thread')
    assertNotThreadError()
end

local function TestSuit(f)
    f(1)
    f(0.0001)
    f('TEST')
    f(true)
    f(false)
    f({})
    f({1, 2})
    f(1, {1, 2})
    f(1, 2, {A={B={C='D'}}})
    f(1, nil, 2)
end

function test_thread:test_pop_1()
    thread.reset()
    thread.newchannel 'test'
    local channel = thread.channel 'test'
    local function pack_pop(ok, ...)
        lu.assertIsTrue(ok)
        return table.pack(...)
    end
    local function test_ok(...)
        channel:push(...)
        lu.assertEquals(pack_pop(channel:pop()), table.pack(...))
        channel:push(...)
        lu.assertEquals(table.pack(channel:bpop()), table.pack(...))
    end
    TestSuit(test_ok)
    -- 基本和serialization的测试重复，所以failed就不测了
end

function test_thread:test_pop_2()
    thread.reset()
    thread.newchannel 'test'
    local channel = thread.channel 'test'

    local function assertIs(expected)
        local ok, v = channel:pop()
        lu.assertIsTrue(ok)
        lu.assertEquals(v, expected)
    end
    local function assertEmpty()
        local ok, v = channel:pop()
        lu.assertIsFalse(ok)
        lu.assertIsNil(v)
    end

    assertEmpty()

    channel:push(1024)
    assertIs(1024)
    assertEmpty()

    channel:push(1024)
    channel:push(1025)
    channel:push(1026)
    assertIs(1024)
    assertIs(1025)
    assertIs(1026)
    assertEmpty()

    channel:push(1024)
    channel:push(1025)
    assertIs(1024)
    channel:push(1026)
    assertIs(1025)
    assertIs(1026)
    assertEmpty()

    thread.reset()
end

function test_thread:test_thread_bpop()
    assertNotThreadError()
    thread.reset()
    thread.newchannel 'testReq'
    thread.newchannel 'testRes'
    local thd = createThread [[
        local thread = require "bee.thread"
        local req = thread.channel 'testReq'
        local res = thread.channel 'testRes'
        local function dispatch(what, ...)
            if what == 'exit' then
                return true
            end
            res:push(what, ...)
        end
        while not dispatch(req:bpop()) do
        end
    ]]
    local req = thread.channel 'testReq'
    local res = thread.channel 'testRes'
    local function test_ok(...)
        req:push(...)
        lu.assertEquals(table.pack(res:bpop()), table.pack(...))
    end
    TestSuit(test_ok)
    req:push 'exit'
    thd:wait()
    assertNotThreadError()
end

function test_thread:test_thread_pop()
    assertNotThreadError()
    thread.reset()
    thread.newchannel 'testReq'
    thread.newchannel 'testRes'
    local thd = createThread [[
        local thread = require "bee.thread"
        local req = thread.channel 'testReq'
        local res = thread.channel 'testRes'
        local function dispatch(ok, what, ...)
            if not ok then
                thread.sleep(0)
                return
            end
            if what == 'exit' then
                return true
            end
            res:push(what, ...)
        end
        while not dispatch(req:pop()) do
        end
    ]]
    local req = thread.channel 'testReq'
    local res = thread.channel 'testRes'
    local function pack_pop(ok, ...)
        if not ok then
            return
        end
        return table.pack(...)
    end
    local function test_ok(...)
        req:push(...)
        local t
        while true do
            t = pack_pop(res:pop())
            if t then
                break
            end
            thread.sleep(0)
        end
        lu.assertEquals(t, table.pack(...))
    end
    TestSuit(test_ok)
    req:push 'exit'
    thd:wait()
    assertNotThreadError()
end
