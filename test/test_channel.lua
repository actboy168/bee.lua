local lt = require "ltest"

local thread = require "bee.thread"
local channel = require "bee.channel"
local epoll = require "bee.epoll"

local function assertNotThreadError()
    lt.assertEquals(thread.errlog(), false)
end

local test_channel = lt.test "channel"

function test_channel:test_channel_create()
    channel.reset()
    lt.assertErrorMsgEquals("Can't query channel 'test'", channel.query, "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    lt.assertIsUserdata(channel.query "test")
    channel.reset()
    channel.create "test"
    lt.assertErrorMsgEquals("Duplicate channel 'test'", channel.create, "test")
    channel.reset()
end

function test_channel:test_reset_1()
    channel.reset()
    lt.assertErrorMsgEquals("Can't query channel 'test'", channel.query, "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    channel.reset()
    lt.assertErrorMsgEquals("Can't query channel 'test'", channel.query, "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    channel.reset()
end

local function TestSuit(f)
    f(1)
    f(0.0001)
    f("TEST")
    f(true)
    f(false)
    f({})
    f({ 1, 2 })
    f(1, { 1, 2 })
    f(1, 2, { A = { B = { C = "D" } } })
    f(1, nil, 2)
end

function test_channel:test_pop_1()
    channel.reset()
    local chan = channel.create "test"
    local function pack_pop(ok, ...)
        lt.assertEquals(ok, true)
        return table.pack(...)
    end
    local function test_ok(...)
        chan:push(...)
        lt.assertEquals(pack_pop(chan:pop()), table.pack(...))
    end
    TestSuit(test_ok)
    -- 基本和serialization的测试重复，所以failed就不测了
end

function test_channel:test_pop_2()
    channel.reset()
    local chan = channel.create "test"

    local function assertIs(expected)
        local ok, v = chan:pop()
        lt.assertEquals(ok, true)
        lt.assertEquals(v, expected)
    end
    local function assertEmpty()
        local ok, v = chan:pop()
        lt.assertEquals(ok, false)
        lt.assertEquals(v, nil)
    end

    assertEmpty()

    chan:push(1024)
    assertIs(1024)
    assertEmpty()

    chan:push(1024)
    chan:push(1025)
    chan:push(1026)
    assertIs(1024)
    assertIs(1025)
    assertIs(1026)
    assertEmpty()

    chan:push(1024)
    chan:push(1025)
    assertIs(1024)
    chan:push(1026)
    assertIs(1025)
    assertIs(1026)
    assertEmpty()

    channel.reset()
end

function test_channel:test_pop_3()
    channel.reset()
    assertNotThreadError()
    thread.reset()
    local req = channel.create "testReq"
    local res = channel.create "testRes"
    local thd = thread.thread [[
        local thread = require "bee.thread"
        local channel = require "bee.channel"
        local req = channel.query "testReq"
        local res = channel.query "testRes"
        local function dispatch(ok, what, ...)
            if not ok then
                return
            end
            if what == "exit" then
                return true
            end
            res:push(what, ...)
        end
        while not dispatch(req:pop()) do
            thread.sleep(0)
        end
    ]]
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
        lt.assertEquals(t, table.pack(...))
    end
    TestSuit(test_ok)
    req:push "exit"
    thread.wait(thd)
    assertNotThreadError()
end

function test_channel:test_fd()
    channel.reset()
    assertNotThreadError()
    thread.reset()
    local req = channel.create "testReq"
    local res = channel.create "testRes"
    local thd = thread.thread [[
        local thread = require "bee.thread"
        local channel = require "bee.channel"
        local epoll = require "bee.epoll"
        local req = channel.query "testReq"
        local res = channel.query "testRes"
        local epfd <close> = epoll.create(16)
        epfd:event_add(req:fd(), epoll.EPOLLIN)
        local function dispatch(ok, what, ...)
            if not ok then
                return true
            end
            if what == "exit" then
                os.exit()
                return
            end
            res:push(what, ...)
        end
        for _, event in epfd:wait() do
            if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
                assert(false, "unknown error")
                return
            end
            if event & epoll.EPOLLIN ~= 0 then
                while not dispatch(req:pop()) do
                end
            end
        end
    ]]
    local epfd <close> = epoll.create(16)
    epfd:event_add(res:fd(), epoll.EPOLLIN)
    local function test_ok(...)
        req:push(...)
        for _, event in epfd:wait() do
            if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
                lt.failure("unknown error")
            end
            if event & epoll.EPOLLIN ~= 0 then
                local r = table.pack(res:pop())
                if r[1] == true then
                    lt.assertEquals(r, table.pack(true, ...))
                    break
                end
            end
        end
    end
    TestSuit(test_ok)
    req:push "exit"
    thread.wait(thd)
    assertNotThreadError()
end
