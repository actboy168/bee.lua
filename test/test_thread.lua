local thread = require "bee.thread"

local cpath_template = ("package.cpath = [[%s]]\n"):format(package.cpath)

thread.thread(cpath_template .. [[
    local thread = require "bee.thread"
    local err = thread.channel "errlog"
    while true do
        print(errlog:bpop())
    end
]])

local function eq2(a, b)
    local delta = a - b
    return delta < 0.01 and delta > -0.01
end

local function file_exists(name)
    local f = io.open(name, 'r')
    if f then
        f:close()
        return true
    else
        return false
    end
end

-- thread.sleep
local clock = os.clock()
thread.sleep(0.1)
assert(eq2(os.clock() - clock, 0.1))

-- thread.thread
os.remove('temp')
thread.sleep(0.1)
assert(file_exists('temp') == false)
thread.thread(cpath_template .. [[
    io.open('temp', 'w'):close()
]])
thread.sleep(0.1)
assert(file_exists('temp') == true)
os.remove('temp')

-- wait
os.remove('temp')
local thd = thread.thread(cpath_template .. [[
    local thread = require 'bee.thread'
    thread.sleep(0.1)
    io.open('temp', 'w'):close()
]])
assert(file_exists('temp') == false)
local clock = os.clock()
thd:wait()
assert(eq2(os.clock() - clock, 0.1))
assert(file_exists('temp') == true)
os.remove('temp')

-- thread.newchannel
-- thread.channel
local suc, c = pcall(thread.channel, 'test')
assert(suc == false)

thread.newchannel 'test'
local suc, c = pcall(thread.channel, 'test')
assert(suc == true)
assert(c ~= nil)

local suc = pcall(thread.newchannel, 'test')
assert(suc == false)

-- thread.reset
thread.newchannel 'reset'
local suc, c = pcall(thread.channel, 'reset')
assert(suc == true)
assert(c ~= nil)

thread.reset()

local suc, c = pcall(thread.channel, 'reset')
assert(suc == false)

-- push
-- bpop
thread.newchannel 'bpop_request'
thread.newchannel 'bpop_response'
local thd = thread.thread(cpath_template .. [[
    local thread = require "bee.thread"
    local request = thread.channel 'bpop_request'
    local response = thread.channel 'bpop_response'
    local msg = request:bpop()
    if msg == 'request' then
        response:push('response')
    end
]])

local request = thread.channel 'bpop_request'
local response = thread.channel 'bpop_response'
request:push('request')
local msg = response:bpop()
assert(msg == 'response')
thd:wait()

local thd = thread.thread(cpath_template .. [[
    local thread = require "bee.thread"
    local request = thread.channel 'bpop_request'
    local response = thread.channel 'bpop_response'
    local msg = request:bpop()
    if msg == 'request' then
        thread.sleep(0.1)
        response:push('response')
    end
]])

local clock = os.clock()
request:push('request')
local msg = response:bpop()
assert(msg == 'response')
assert(eq2(os.clock() - clock, 0.1))
thd:wait()

local thd = thread.thread(cpath_template .. [[
    local thread = require "bee.thread"
    local request = thread.channel 'bpop_request'
    local response = thread.channel 'bpop_response'
    while true do
        local op, data = request:bpop()
        if op == 'echo' then
            response:push(data)
        elseif op == 'plus' then
            response:push(data[1] + data[2])
        elseif op == 'quit' then
            return
        end
    end
]])

request:push('echo', nil)
local result = response:bpop()
assert(result == nil)

request:push('echo', 5)
local result = response:bpop()
assert(result == 5)

request:push('echo', 'test')
local result = response:bpop()
assert(result == 'test')

request:push('plus', {1, 2})
local result = response:bpop()
assert(result == 3)

request:push('quit')

thd:wait()
thread.reset()

-- pop
thread.newchannel 'pop_request'
thread.newchannel 'pop_response'
local thd = thread.thread(cpath_template .. [[
    local thread = require "bee.thread"
    local request = thread.channel 'pop_request'
    local response = thread.channel 'pop_response'
    while true do
        local who, delay = request:bpop()
        if who == 'quit' then
            return
        end
        thread.sleep(delay)
        response:push('response')
    end
]])

local request = thread.channel 'pop_request'
local response = thread.channel 'pop_response'

request:push('request', 0.1)
local ok, msg = response:pop()
assert(ok == false)
thread.sleep(0.15)
local ok, msg = response:pop()
assert(ok == true)
assert(msg == 'response')

request:push('request', 0.2)
local clock = os.clock()
local ok, msg = response:pop(0.1)
assert(ok == false)
local passed = os.clock() - clock
assert(eq2(passed, 0.1))

local clock = os.clock()
local ok, msg = response:pop(0.2)
local passed = os.clock() - clock
assert(eq2(passed, 0.1))
assert(ok == true)
assert(msg == 'response')

request:push('quit')
thd:wait()

channel.reset()
