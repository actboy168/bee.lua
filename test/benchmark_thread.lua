local thread = require "bee.thread"

local cpath_template = ("package.cpath = [[%s]]\n"):format(package.cpath)

thread.thread(cpath_template .. [[
	local thread = require "bee.thread"
	local err = thread.channel "errlog"
	while true do
		print(errlog:bpop())
	end
]])


local consume_bpop_template = [[
	--consume_bpop
]] .. cpath_template .. [[
	local thread = require "bee.thread"
	local c = thread.channel "channel"
	local pool = setmetatable({}, {__index=function(self, k)
		local v = thread.channel(k)
		self[k] = v
		return v
	end})
	while true do
		local who, msg = c:bpop()
		pool[who]:push(msg)
	end
]]

local consume_pop_template = [[
	--consume_pop
]] .. cpath_template .. [[
	local thread = require "bee.thread"
	local c = thread.channel "channel"
	local pool = setmetatable({}, {__index=function(self, k)
		local v = thread.channel(k)
		self[k] = v
		return v
	end})
	while true do
		local ok, who, msg = c:pop()
		if ok then
			pool[who]:push(msg)
		else
			thread.sleep(0)
		end
	end
]]

local consume_timed_pop_template = [[
	--consume_timed_pop
]] .. cpath_template .. [[
	local thread = require "bee.thread"
	local c = thread.channel "channel"
	local pool = setmetatable({}, {__index=function(self, k)
		local v = thread.channel(k)
		self[k] = v
		return v
	end})
	while true do
		local ok, who, msg = c:pop(0.1)
		if ok then
			pool[who]:push(msg)
		else
			thread.sleep(0)
		end
	end
]]

local produce_template =  [[
	--produce
]] .. cpath_template .. [[
	local thread = require "bee.thread"
	local who = "pd" .. thread.id
	thread.newchannel(who)
	local input = thread.channel "channel"
	local output = thread.channel(who)
	for i = 1, 10000 do
		input:push(who, i)
		local r = output:bpop()
		if i ~= r then
			print(who .. " error:", i, r)
		end
	end
	--print(who .. " ok!")
]]

local t = os.clock()

thread.newchannel "channel"

thread.thread(consume_bpop_template)
thread.thread(consume_pop_template)
thread.thread(consume_timed_pop_template)

local N = 12

local thds = {}
for i = 1, N do
	thds[i] = thread.thread(produce_template)
end

for i = 1, N do
	thds[i]:wait()
end

print(os.clock() - t)
