local lt = require "ltest"

local subprocess = require "bee.subprocess"
local thread = require "bee.thread"
local platform = require "bee.platform"
local fs = require "bee.filesystem"
local shell = require "shell"

local function safe_exit(process, code)
    lt.assertEquals(process:wait(), code or 0)
    lt.assertEquals(process:detach(), true)
end

local function testArgs(...)
    local args = table.pack(...)
    local s = {}
    for _, v in ipairs(args) do
        s[#s+1] = ("%q"):format(v)
    end
    local process = shell:runlua(([[
        local function eq(a, b, offset)
            assert(#a == #b + offset)
            for i, v in ipairs(b) do
                assert(v == a[i+offset], a[i])
            end
        end
        local t = {%s}
        eq(arg, t, 0)
    ]]):format(table.concat(s, ",")), { "_", args, stderr = true })

    lt.assertIsUserdata(process)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process.stderr:read "a", "")
    safe_exit(process)
end

local test_subprocess = lt.test "subprocess"

function test_subprocess:test_spawn()
    local process = shell:runlua " "
    safe_exit(process)

    local process = shell:runlua(" ", { stdin = true })
    lt.assertIsUserdata(process)
    lt.assertIsUserdata(process.stdin)
    lt.assertEquals(process.stdout, nil)
    lt.assertEquals(process.stderr, nil)
    safe_exit(process)

    local process = shell:runlua(" ", { stdout = true })
    lt.assertIsUserdata(process)
    lt.assertEquals(process.stdin, nil)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process.stderr, nil)
    safe_exit(process)

    local process = shell:runlua(" ", { stderr = true })
    lt.assertIsUserdata(process)
    lt.assertEquals(process.stdin, nil)
    lt.assertEquals(process.stdout, nil)
    lt.assertIsUserdata(process.stderr)
    safe_exit(process)
end

function test_subprocess:test_wait()
    local process = shell:runlua " "
    safe_exit(process)

    local process = shell:runlua "os.exit(true)"
    safe_exit(process)

    local process = shell:runlua "os.exit(false)"
    safe_exit(process, 1)

    local process = shell:runlua "os.exit(197)"
    safe_exit(process, 197)
end

function test_subprocess:test_is_running()
    local process = shell:runlua([[io.read "a"]], { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertIsUserdata(process.stdin)
    process.stdin:close()
    safe_exit(process)
    lt.assertEquals(process:is_running(), false)
end

function test_subprocess:test_is_running_2()
    local process = shell:runlua([[io.read "a";os.exit(13)]], { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertIsUserdata(process.stdin)
    process.stdin:close()
    while process:is_running() do
        thread.sleep(10)
    end
    lt.assertEquals(process:is_running(), false)
    safe_exit(process, 13)
end

function test_subprocess:test_kill()
    local process = shell:runlua([[io.read "a"]], { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertEquals(process:kill(), true)
    lt.assertEquals(process:wait(), 0x0F00)
    lt.assertEquals(process:is_running(), false)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[io.read "a"]], { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertEquals(process:kill(0), true)
    lt.assertEquals(process:is_running(), true)
    lt.assertIsUserdata(process.stdin)
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process:is_running(), false)
    lt.assertEquals(process:kill(0), false)
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_stdio_1()
    local process = shell:runlua([[io.stdout:write("ok")]], { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(process.stdout:read(2), nil)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[io.stderr:write("ok")]], { stderr = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process.stderr:read(2), "ok")
    lt.assertEquals(process.stderr:read(2), nil)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[io.write(io.read "a")]], { stdin = true, stdout = true, stderr = true })
    lt.assertIsUserdata(process.stdin)
    lt.assertIsUserdata(process.stdout)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process:is_running(), true)
    process.stdin:write "ok"
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(process.stdout:read(2), nil)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[error "Test subprocess error."]], { stderr = true })
    lt.assertEquals(process:wait(), 1)
    local found = not not string.find(process.stderr:read "a", "Test subprocess error.", nil, true)
    lt.assertEquals(found, true)
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_stdio_2()
    fs.remove "temp.txt"
    local f = lt.assertIsUserdata(io.open("temp.txt", "w"))
    ---@cast f file*
    local process = shell:runlua([[io.write "ok"]], { stdout = f })
    lt.assertEquals(process.stdout, f)
    f:close()
    safe_exit(process)
    local f = lt.assertIsUserdata(io.open("temp.txt", "r"))
    ---@cast f file*
    lt.assertEquals(f:read "a", "ok")
    f:close()

    fs.remove "temp.txt"
    local wr = io.open("temp.txt", "w")
    local rd = io.open("temp.txt", "r")
    lt.assertIsUserdata(wr)
    lt.assertIsUserdata(rd)
    ---@cast wr file*
    ---@cast rd file*
    local process = shell:runlua([[io.write "ok"]], { stdout = wr })
    wr:close()
    lt.assertEquals(process.stdout, wr)
    safe_exit(process)
    lt.assertEquals(rd:read "a", "ok")
    rd:close()
    fs.remove "temp.txt"

    local process1 = shell:runlua([[io.write "ok"]], { stdout = true })
    local process2 = shell:runlua([[io.write(io.read "a")]], { stdin = process1.stdout, stdout = true })
    safe_exit(process1)
    lt.assertEquals(process2:wait(), 0)
    lt.assertEquals(process2.stdout:read "a", "ok")
    lt.assertEquals(process2:detach(), true)
    lt.assertEquals(tostring(process2.stdout), "file (closed)")
end

function test_subprocess:test_stdio_3()
    local process = shell:runlua([[
        io.stdout:write "[stdout]"; io.stdout:flush()
        io.stderr:write "[stderr]"; io.stderr:flush()
    ]], { stdout = true, stderr = "stdout" })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process.stdout, process.stderr)
    lt.assertEquals(process.stdout:read "a", "[stdout][stderr]")
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_stdio_4()
    local process = shell:runlua([[
        io.stdout:setvbuf "no"
        while true do
            local what = io.read(4)
            if what ~= "PING" then
                break
            end
            io.write "PONG"
        end
    ]], { stdin = true, stdout = true })
    lt.assertIsUserdata(process.stdin)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process:is_running(), true)

    process.stdin:setvbuf "no"

    for _ = 1, 10 do
        process.stdin:write "PING"
        lt.assertEquals(process.stdout:read(4), "PONG")
    end
    process.stdin:write "EXIT"
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(4), nil)
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_peek()
    local process = shell:runlua([[io.write "ok"]], { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(subprocess.peek(process.stdout), nil)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[io.write "ok"]], { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    process.stdout:close()
    lt.assertError("attempt to use a closed file", process.stdout.read, process.stdout, 2)
    lt.assertEquals(subprocess.peek(process.stdout), nil)
    lt.assertEquals(process:detach(), true)

    local process = shell:runlua([[io.stdout:setvbuf "no" io.write "start" io.write(io.read "a")]], { stdin = true, stdout = true })
    lt.assertIsUserdata(process.stdin)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process:is_running(), true)
    while true do
        local n = subprocess.peek(process.stdout)
        if n >= 5 then
            process.stdout:read(5)
            break
        end
        thread.sleep(0)
    end
    process.stdin:write "ok"
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(process.stdout:read(2), nil)
    lt.assertEquals(process:detach(), true)
end

if platform.os == "windows" then
    function test_subprocess:test_filemode()
        local process = shell:runlua([[
            assert(io.read "a" == "\n")
        ]], { stdin = true, stderr = true })
        process.stdin:write "\r\n"
        process.stdin:close()
        lt.assertEquals(process:wait(), 0)
        lt.assertEquals(process.stderr:read "a", "")
        lt.assertEquals(process:detach(), true)
        local process = shell:runlua([[
            local windows = require "bee.windows"
            windows.filemode(io.stdin, "b")
            assert(io.read "a" == "\r\n")
        ]], { stdin = true, stderr = true })
        process.stdin:write "\r\n"
        process.stdin:close()
        lt.assertEquals(process:wait(), 0)
        lt.assertEquals(process.stderr:read "a", "")
        lt.assertEquals(process:detach(), true)
    end
end

function test_subprocess:test_env()
    local function test_env(cond, env)
        local script = ("assert(%s)"):format(cond)
        local process = shell:runlua(
            script,
            { env = env }
        )
        safe_exit(process)
    end
    test_env([[os.getenv "BEE_TEST" == nil]], {})
    test_env([[os.getenv "BEE_TEST" == "ok"]], { BEE_TEST = "ok" })

    subprocess.setenv("BEE_TEST_ENV_1", "OK")
    lt.assertEquals(os.getenv "BEE_TEST_ENV_1", "OK")
    test_env([[os.getenv "BEE_TEST_ENV_1" == "OK"]], {})
    test_env([[os.getenv "BEE_TEST_ENV_1" == nil]], { BEE_TEST_ENV_1 = false })
end

function test_subprocess:test_args()
    testArgs("A")
    testArgs("A", "B")
    testArgs("A B")
    testArgs("A", "B C")
    testArgs([["A"]])
    testArgs([["A]])
    testArgs([["A\]])
    testArgs([["A\"]])
    testArgs([[A" B]])
end

function test_subprocess:test_setenv()
    lt.assertEquals(os.getenv "TEST_ENV", nil)
    local process = shell:runlua([[
        assert(os.getenv "TEST_ENV" == nil)
    ]], { stderr = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stderr:read "a", "")
    lt.assertEquals(process:detach(), true)

    subprocess.setenv("TEST_ENV", "OK")

    lt.assertEquals(os.getenv "TEST_ENV", "OK")
    local process = shell:runlua([[
        assert(os.getenv "TEST_ENV" == "OK")
    ]], { stderr = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stderr:read "a", "")
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_encoding()
    local process = shell:runlua([[
        print "中文"
    ]], { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(6), "中文")
    lt.assertEquals(process:detach(), true)
end

function test_subprocess:test_cwd()
    local path = fs.absolute "test_cwd"
    fs.create_directories(path)
    local process = shell:runlua([[
        local fs = require "bee.filesystem"
        assert(fs.path(arg[1]) == fs.current_path())
    ]], { "_", path:string(), cwd = path, stdout = true, stderr = "stdout" })
    lt.assertIsUserdata(process)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process.stdout:read "a", "")
    safe_exit(process)
    fs.remove_all(path)
end

function test_subprocess:test_select()
    local progs = {}
    for i = 1, 10 do
        progs[i] = shell:runlua [[
        ]]
        lt.assertIsUserdata(progs[i])
    end
    while #progs > 0 do
        local ok = subprocess.select(progs)
        lt.assertEquals(ok, true)
        local i = 1
        while i <= #progs do
            local prog = progs[i]
            if prog:is_running() then
                i = i + 1
            else
                local exitcode = prog:wait()
                lt.assertEquals(exitcode, 0)
                prog:detach()
                table.remove(progs, i)
            end
        end
    end
end
