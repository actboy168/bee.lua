local lt = require "ltest"

local thread = require "bee.thread"
local fs = require "bee.filesystem"
local time = require "bee.time"

local function createThread(script, ...)
    return thread.create(script, ...)
end

local function assertNotThreadError()
    lt.assertEquals(thread.errlog(), nil)
end

local function assertHasThreadError(m)
    local msg = thread.errlog()
    lt.assertIsString(msg)
    lt.assertEquals(not not string.find(msg, m, nil, true), true)
end

local test_thread = lt.test "thread"

function test_thread:test_thread_1()
    local function file_exists(filename)
        local f = io.open(filename, "r")
        if f then
            f:close()
            return true
        end
        return false
    end
    assertNotThreadError()
    fs.remove("temp.txt")
    lt.assertEquals(file_exists("temp.txt"), false)
    local thd = createThread [[
        io.open("temp.txt", "w"):close()
    ]]
    thread.wait(thd)
    lt.assertEquals(file_exists("temp.txt"), true)
    fs.remove("temp.txt")
    assertNotThreadError()
end

function test_thread:test_thread_2()
    assertNotThreadError()
    GLOBAL = true
    THREAD = nil
    lt.assertNotEquals(GLOBAL, nil)
    lt.assertEquals(THREAD, nil)
    local thd = createThread [[
        THREAD = true
        assert(GLOBAL == nil)
    ]]
    thread.wait(thd)
    assertNotThreadError()
    lt.assertNotEquals(GLOBAL, nil)
    lt.assertEquals(THREAD, nil)
    GLOBAL = nil
    THREAD = nil
end

function test_thread:test_thread_3()
    assertNotThreadError()
    local thd = createThread [[
        error "Test thread error."
    ]]
    thread.wait(thd)
    assertHasThreadError("Test thread error.")
    assertNotThreadError()
end

function test_thread:test_thread_initargs()
    assertNotThreadError()
    local thd = createThread([[
        local args = ...
        assert(args == "hello")
    ]], "hello")
    thread.wait(thd)
    assertNotThreadError()
end

function test_thread:test_id_1()
    assertNotThreadError()
    lt.assertEquals(thread.id, 0)
    local thd = createThread [[
        local thread = require "bee.thread"
        assert(thread.id ~= 0)
    ]]
    thread.wait(thd)
    assertNotThreadError()
end

function test_thread:test_id_2()
    assertNotThreadError()
    lt.assertEquals(thread.id, 0)
    local thd = createThread [[
        local thread = require "bee.thread"
        assert(thread.id ~= 0)
    ]]
    thread.wait(thd)
    assertNotThreadError()
end

function test_thread:test_sleep()
    local t1 = time.monotonic()
    thread.sleep(1)
    local t2 = time.monotonic()
    lt.assertEquals(t2 - t1 <= 2, true)
end
