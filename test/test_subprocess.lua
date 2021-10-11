local lt = require 'ltest'

local subprocess = require 'bee.subprocess'
local thread = require 'bee.thread'
local platform = require 'bee.platform'
local shell = require 'shell'

local function testArgs(...)
    local args = table.pack(...)
    local s = {}
    for _, v in ipairs(args) do
        s[#s+1] = ('%q'):format(v)
    end
    local process = shell:runlua(([[
        local function eq(a, b, offset)
            assert(#a == #b + offset)
            for i, v in ipairs(b) do
                assert(v == a[i+offset], a[i])
            end
        end
        local t = {%s}
        eq(arg, t, 2)
    ]]):format(table.concat(s, ',')), {'', args, stderr = true})

    lt.assertIsUserdata(process)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process.stderr:read 'a', '')
    lt.assertEquals(process:wait(), 0)
end

local test_subprocess = lt.test "subprocess"

function test_subprocess:test_spawn()
    lt.assertIsUserdata(shell:runlua ' ')

    local process = shell:runlua(' ', { stdin = true })
    lt.assertIsUserdata(process)
    lt.assertIsUserdata(process.stdin)
    lt.assertEquals(process.stdout, nil)
    lt.assertEquals(process.stderr, nil)

    local process = shell:runlua(' ', { stdout = true })
    lt.assertIsUserdata(process)
    lt.assertEquals(process.stdin, nil)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process.stderr, nil)

    local process = shell:runlua(' ', { stderr = true })
    lt.assertIsUserdata(process)
    lt.assertEquals(process.stdin, nil)
    lt.assertEquals(process.stdout, nil)
    lt.assertIsUserdata(process.stderr)
end

function test_subprocess:test_wait()
    local process = shell:runlua ' '
    lt.assertEquals(process:wait(), 0)

    local process = shell:runlua 'os.exit(true)'
    lt.assertEquals(process:wait(), 0)

    local process = shell:runlua 'os.exit(false)'
    lt.assertEquals(process:wait(), 1)

    local process = shell:runlua 'os.exit(197)'
    lt.assertEquals(process:wait(), 197)
end

function test_subprocess:test_is_running()
    local process = shell:runlua('io.read "a"', { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertIsUserdata(process.stdin)
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process:is_running(), false)
end

function test_subprocess:test_kill()
    local process = shell:runlua('io.read "a"', { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertEquals(process:kill(), true)
    lt.assertEquals(process:is_running(), false)
    lt.assertEquals(process:wait() & 0xFF, 0)

    local process = shell:runlua('io.read "a"', { stdin = true })
    lt.assertEquals(process:is_running(), true)
    lt.assertEquals(process:kill(0), true)
    lt.assertEquals(process:is_running(), true)
    lt.assertIsUserdata(process.stdin)
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process:is_running(), false)
    lt.assertEquals(process:kill(0), false)
end

function test_subprocess:test_stdio_1()
    local process = shell:runlua('io.stdout:write("ok")', { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(process.stdout:read(2), nil)

    local process = shell:runlua('io.stderr:write("ok")', { stderr = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process.stderr:read(2), "ok")
    lt.assertEquals(process.stderr:read(2), nil)

    local process = shell:runlua('io.write(io.read "a")', { stdin = true, stdout = true, stderr = true })
    lt.assertIsUserdata(process.stdin)
    lt.assertIsUserdata(process.stdout)
    lt.assertIsUserdata(process.stderr)
    lt.assertEquals(process:is_running(), true)
    process.stdin:write "ok"
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(process.stdout:read(2), nil)

    local process = shell:runlua('error "Test subprocess error."', { stderr = true })
    lt.assertEquals(process:wait(), 1)
    local found = not not string.find(process.stderr:read "a", "Test subprocess error.", nil, true)
    lt.assertEquals(found, true)
end

function test_subprocess:test_stdio_2()
    os.remove 'temp.txt'
    local f = io.open('temp.txt', 'w')
    lt.assertIsUserdata(f)
    local process = shell:runlua('io.write "ok"', { stdout = f })
    lt.assertEquals(process.stdout, f)
    f:close()
    lt.assertEquals(process:wait(), 0)
    local f = io.open('temp.txt', 'r')
    lt.assertIsUserdata(f)
    lt.assertEquals(f:read 'a', "ok")
    f:close()

    os.remove 'temp.txt'
    local wr = io.open('temp.txt', 'w')
    local rd = io.open('temp.txt', 'r')
    lt.assertIsUserdata(wr)
    lt.assertIsUserdata(rd)
    local process = shell:runlua('io.write "ok"', { stdout = wr })
    wr:close()
    lt.assertEquals(process.stdout, wr)
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(rd:read 'a', "ok")
    rd:close()
    os.remove 'temp.txt'

    local process1 = shell:runlua('io.write "ok"', { stdout = true })
    local process2 = shell:runlua('io.write(io.read "a")', { stdin = process1.stdout, stdout = true })
    lt.assertEquals(process1:wait(), 0)
    lt.assertEquals(process2:wait(), 0)
    lt.assertEquals(process2.stdout:read 'a', "ok")
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
end

function test_subprocess:test_peek()
    local process = shell:runlua('io.write "ok"', { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    lt.assertEquals(process.stdout:read(2), "ok")
    lt.assertEquals(subprocess.peek(process.stdout), nil)

    local process = shell:runlua('io.write "ok"', { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertIsUserdata(process.stdout)
    lt.assertEquals(subprocess.peek(process.stdout), 2)
    process.stdout:close()
    lt.assertError("attempt to use a closed file", process.stdout.read, process.stdout, 2)
    lt.assertEquals(subprocess.peek(process.stdout), nil)

    local process = shell:runlua('io.stdout:setvbuf "no" io.write "start" io.write(io.read "a")', { stdin = true, stdout = true })
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
end

function test_subprocess:test_filemode()
    if platform.OS == 'Windows' then
        local process = shell:runlua([[
            assert(io.read "a" == "\n")
        ]], { stdin = true, stderr = true })
        process.stdin:write "\r\n"
        process.stdin:close()
        lt.assertEquals(process:wait(), 0)
        lt.assertEquals(process.stderr:read "a", "")
    end
    local process = shell:runlua([[
        local sp = require "bee.subprocess"
        sp.filemode(io.stdin, 'b')
        assert(io.read "a" == "\r\n")
    ]], { stdin = true, stderr = true })
    process.stdin:write "\r\n"
    process.stdin:close()
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stderr:read "a", "")
end

function test_subprocess:test_env()
    local function test_env(cond, env)
        local script = ('assert(%s)'):format(cond)
        lt.assertEquals(
            shell:runlua(
                script,
                {env = env}
            ):wait(),
            0
        )
    end
    test_env('os.getenv "BEE_TEST" == nil', {})
    test_env('os.getenv "BEE_TEST" == "ok"', { BEE_TEST = 'ok' })

    subprocess.setenv("BEE_TEST_ENV_1", "OK")
    lt.assertEquals(os.getenv 'BEE_TEST_ENV_1', "OK")
    test_env('os.getenv "BEE_TEST_ENV_1" == "OK"', {})
    test_env('os.getenv "BEE_TEST_ENV_1" == nil', {BEE_TEST_ENV_1=false})
end

function test_subprocess:test_args()
    testArgs('A')
    testArgs('A', 'B')
    testArgs('A B')
    testArgs('A', 'B C')
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

    subprocess.setenv("TEST_ENV", "OK")

    lt.assertEquals(os.getenv "TEST_ENV", "OK")
    local process = shell:runlua([[
        assert(os.getenv "TEST_ENV" == "OK")
    ]], { stderr = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stderr:read "a", "")
end

function test_subprocess:test_encoding()
    local process = shell:runlua([[
        print "中文"
    ]], { stdout = true })
    lt.assertEquals(process:wait(), 0)
    lt.assertEquals(process.stdout:read(6), "中文")
end
