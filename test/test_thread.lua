local lu = require 'ltest'

local thread = require "bee.thread"
local err = thread.channel "errlog"

local cpath_template = ("package.cpath = [[%s]]\n"):format(package.cpath)

local function createThread(script)
    return thread.thread(cpath_template .. script)
end

local function assertNotThreadError()
    lu.assertEquals(err:pop(), false)
end

local function assertHasThreadError(m)
    local ok, msg = err:pop()
    lu.assertEquals(ok, true)
    lu.assertEquals(not not string.find(msg, m, nil, true), true)
end

local test_thread = lu.test "thread"

function test_thread:test_thread_1()
    local function file_exists(filename)
        local f = io.open(filename, 'r')
        if f then
            f:close()
            return true
        end
        return false
    end
    assertNotThreadError()
    os.remove('temp.txt')
    lu.assertEquals(file_exists('temp.txt'), false)
    local thd = createThread [[
        io.open('temp.txt', 'w'):close()
    ]]
    thread.wait(thd)
    lu.assertEquals(file_exists('temp.txt'), true)
    os.remove('temp.txt')
    assertNotThreadError()
end

function test_thread:test_thread_2()
    assertNotThreadError()
    GLOBAL = true
    THREAD = nil
    lu.assertNotEquals(GLOBAL, nil)
    lu.assertEquals(THREAD, nil)
    local thd = createThread [[
        THREAD = true
        assert(GLOBAL == nil)
    ]]
    thread.wait(thd)
    assertNotThreadError()
    lu.assertNotEquals(GLOBAL, nil)
    lu.assertEquals(THREAD, nil)
    GLOBAL = nil
    THREAD = nil
end

function test_thread:test_thread_3()
    assertNotThreadError()
    local thd = createThread [[
        error 'Test thread error.'
    ]]
    thread.wait(thd)
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
    thread.wait(thd)
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
    thread.wait(thd)
    assertNotThreadError()
end

function test_thread:test_id_3()
    thread.reset()
    assertNotThreadError()
    thread.newchannel 'test'
    local thd = createThread [[
        local thread = require "bee.thread"
        local channel = thread.channel 'test'
        assert(thread.id == channel:bpop())
    ]]
    local channel = thread.channel 'test'
    channel:push(thread.getid(thd))
    thread.wait(thd)
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
    thread.wait(thd)
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
        lu.assertEquals(ok, true)
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
        lu.assertEquals(ok, true)
        lu.assertEquals(v, expected)
    end
    local function assertEmpty()
        local ok, v = channel:pop()
        lu.assertEquals(ok, false)
        lu.assertEquals(v, nil)
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
    thread.wait(thd)
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
    thread.wait(thd)
    assertNotThreadError()
end
