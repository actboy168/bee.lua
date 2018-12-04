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
