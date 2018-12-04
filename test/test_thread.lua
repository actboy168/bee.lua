local thread = require "bee.thread"
local fs = require "bee.filesystem"

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

-- thread.sleep
local clock = os.clock()
thread.sleep(1.0)
assert(eq2(os.clock() - clock, 1.0))

-- thread.thread
fs.remove(fs.path 'temp')
thread.thread(cpath_template .. [[
    io.open('temp', 'w'):close()
]])
thread.sleep(0.1)
assert(fs.exists(fs.path 'temp'))
fs.remove(fs.path 'temp')

-- wait
fs.remove(fs.path 'temp')
local thd = thread.thread(cpath_template .. [[
    local thread = require 'bee.thread'
    thread.sleep(0.1)
    io.open('temp', 'w'):close()
]])
assert(fs.exists(fs.path 'temp') == false)
local clock = os.clock()
thd:wait()
assert(eq2(os.clock() - clock, 0.1))
assert(fs.exists(fs.path 'temp') == true)
fs.remove(fs.path 'temp')
