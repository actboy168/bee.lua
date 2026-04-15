local subprocess = require "bee.subprocess"
local platform = require "bee.platform"
local fs = require "bee.filesystem"

local luaexe = fs.absolute(platform.os == "windows"
    and "./build/bin/bootstrap.exe"
    or "./build/bin/bootstrap"):string()

local bench = false
local bench_args = {}
local test_args = {}
for i, v in ipairs(arg) do
    if v == "-bench" then
        bench = true
    elseif bench then
        bench_args[#bench_args+1] = v
    else
        test_args[#test_args+1] = v
    end
end

local process
if bench then
    process = assert(subprocess.spawn {
        luaexe,
        "bench.lua", bench_args,
        stdout = io.stdout,
        stderr = "stdout",
        cwd = "benchmark"
    })
else
    process = assert(subprocess.spawn {
        luaexe, "test/test.lua", test_args,
        stdout = io.stdout,
        stderr = "stdout",
    })
end

local code = process:wait()
if code ~= 0 then
    os.exit(code, true)
end
