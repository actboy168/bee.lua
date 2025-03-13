local lt = require "ltest"

local thread = require "bee.thread"
local channel = require "bee.channel"
local epoll = require "bee.epoll"

local function assertNotThreadError()
    lt.assertEquals(thread.errlog(), nil)
end

local test_channel = lt.test "channel"

function test_channel:test_create()
    lt.assertIsNil(channel.query "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    lt.assertIsUserdata(channel.query "test")
    channel.destroy "test"
    channel.create "test"
    lt.assertErrorMsgEquals("Duplicate channel 'test'", channel.create, "test")
    channel.destroy "test"
end

function test_channel:test_reset_1()
    lt.assertIsNil(channel.query "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    channel.destroy "test"
    lt.assertIsNil(channel.query "test")
    channel.create "test"
    lt.assertIsUserdata(channel.query "test")
    channel.destroy "test"
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
    channel.destroy "test"
end

function test_channel:test_pop_2()
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
    channel.destroy "test"
end

function test_channel:test_pop_3()
    assertNotThreadError()
    local req = channel.create "testReq"
    local res = channel.create "testRes"
    local thd = thread.create [[
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
    channel.destroy "testReq"
    channel.destroy "testRes"
    assertNotThreadError()
end

function test_channel:test_fd()
    assertNotThreadError()
    local req = channel.create "testReq"
    local res = channel.create "testRes"
    local thd = thread.create [[
        local thread = require "bee.thread"
        local channel = require "bee.channel"
        local epoll = require "bee.epoll"
        local req = channel.query "testReq"
        local res = channel.query "testRes"
        local epfd <close> = epoll.create(16)
        epfd:event_add(req:fd(), epoll.EPOLLIN)
        local function dispatch(ok, what, ...)
            if not ok then
                return 1
            end
            if what == "exit" then
                return 0
            end
            res:push(what, ...)
        end
        while true do
            for _, event in epfd:wait() do
                if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
                    assert(false, "unknown error")
                    return
                end
                if event & epoll.EPOLLIN ~= 0 then
                    while true do
                        local r = dispatch(req:pop())
                        if r == 0 then
                            return
                        elseif r == 1 then
                            break
                        end
                    end
                end
            end
        end
    ]]
    local epfd <close> = epoll.create(16)
    epfd:event_add(res:fd(), epoll.EPOLLIN)
    local function test_ok(...)
        req:push(...)
        local expected = table.pack(true, ...)
        while true do
            for _, event in epfd:wait() do
                if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
                    lt.failure("unknown error")
                end
                if event & epoll.EPOLLIN ~= 0 then
                    local actual = table.pack(res:pop())
                    if actual[1] == true then
                        lt.assertEquals(actual, expected)
                        return
                    end
                end
            end
        end
    end
    TestSuit(test_ok)
    req:push "exit"
    thread.wait(thd)
    channel.destroy "testReq"
    channel.destroy "testRes"
    assertNotThreadError()
end
