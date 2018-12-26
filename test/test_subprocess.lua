local lu = require 'luaunit'

local subprocess = require 'bee.subprocess'
local socket = require 'bee.socket'
local thread = require 'bee.thread'
local platform = require 'bee.platform'

local function getexe()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
end

local function createLua(script, option)
    local init = ("package.cpath = [[%s]]"):format(package.cpath)
    option = option or {}
    option[1] = {
        getexe(),
        '-e', init,
        '-e', script,
    }
    return subprocess.spawn(option)
end


local function createTestArgsFile(tbl)
    local s = {}
    for _, v in ipairs(tbl) do
        s[#s+1] = ('%q'):format(v)
    end
    local f = assert(io.open('test/temp.lua', 'wb'))
    f:write(([=[
        package.cpath = [[%s]]
        local function eq(a, b)
            assert(#a == #b)
            for i, v in ipairs(a) do
                assert(v == b[i], b[i])
            end
        end
        local t = {%s}
        eq(arg, t)
    ]=]):format(package.cpath, table.concat(s, ',')))
    f:close()
end

local function testArrayArgs(...)
    local args = table.pack(...)
    createTestArgsFile(args)
    local option = {}
    option.stderr = true
    option.argsStyle = 'array'
    option[1] = getexe()
    option[2] = 'test/temp.lua'
    table.move(args, 1, args.n, 3, option)
    local process = subprocess.spawn(option)
    lu.assertUserdata(process)
    lu.assertUserdata(process.stderr)
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
end

local function testStringArgs(args, ...)
    createTestArgsFile(table.pack(...))
    local option = {}
    option.stderr = true
    option.argsStyle = 'string'
    option[1] = getexe()
    option[2] = 'test/temp.lua ' .. args
    local process = subprocess.spawn(option)
    lu.assertUserdata(process)
    lu.assertUserdata(process.stderr)
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
end

test_subprocess = {}

function test_subprocess:test_spawn()
    lu.assertUserdata(createLua ' ')

    local process = createLua(' ', { stdin = true })
    lu.assertUserdata(process)
    lu.assertUserdata(process.stdin)
    lu.assertIsNil(process.stdout)
    lu.assertIsNil(process.stderr)

    local process = createLua(' ', { stdout = true })
    lu.assertUserdata(process)
    lu.assertIsNil(process.stdin)
    lu.assertUserdata(process.stdout)
    lu.assertIsNil(process.stderr)

    local process = createLua(' ', { stderr = true })
    lu.assertUserdata(process)
    lu.assertIsNil(process.stdin)
    lu.assertIsNil(process.stdout)
    lu.assertUserdata(process.stderr)
end

function test_subprocess:test_wait()
    local process = createLua ' '
    lu.assertEquals(process:wait(), 0)

    local process = createLua 'os.exit(true)'
    lu.assertEquals(process:wait(), 0)

    local process = createLua 'os.exit(false)'
    lu.assertEquals(process:wait(), 1)

    local process = createLua 'os.exit(197)'
    lu.assertEquals(process:wait(), 197)
end

function test_subprocess:test_is_running()
    local process = createLua('io.read "a"', { stdin = true })
    lu.assertIsTrue(process:is_running())
    lu.assertUserdata(process.stdin)
    process.stdin:close()
    lu.assertEquals(process:wait(), 0)
    lu.assertIsFalse(process:is_running())
end

function test_subprocess:test_kill()
    local process = createLua('io.read "a"', { stdin = true })
    lu.assertIsTrue(process:is_running())
    lu.assertIsTrue(process:kill())
    lu.assertIsFalse(process:is_running())
    lu.assertEquals(process:wait(), 0xF00)

    local process = createLua('io.read "a"', { stdin = true })
    lu.assertIsTrue(process:is_running())
    lu.assertIsTrue(process:kill(0))
    lu.assertIsTrue(process:is_running())
    lu.assertUserdata(process.stdin)
    process.stdin:close()
    lu.assertEquals(process:wait(), 0)
    lu.assertIsFalse(process:is_running())
    lu.assertIsFalse(process:kill(0))
end

function test_subprocess:test_stdio_1()
    local process = createLua('io.stdout:write("ok")', { stdout = true })
    lu.assertEquals(process:wait(), 0)
    lu.assertUserdata(process.stdout)
    lu.assertEquals(process.stdout:read(2), "ok")
    lu.assertIsNil(process.stdout:read(2))

    local process = createLua('io.stderr:write("ok")', { stderr = true })
    lu.assertEquals(process:wait(), 0)
    lu.assertUserdata(process.stderr)
    lu.assertEquals(process.stderr:read(2), "ok")
    lu.assertIsNil(process.stderr:read(2))

    local process = createLua('io.write(io.read "a")', { stdin = true, stdout = true, stderr = true })
    lu.assertUserdata(process.stdin)
    lu.assertUserdata(process.stdout)
    lu.assertUserdata(process.stderr)
    lu.assertIsTrue(process:is_running())
    process.stdin:write "ok"
    process.stdin:close()
    lu.assertEquals(process:wait(), 0)
    lu.assertEquals(process.stdout:read(2), "ok")
    lu.assertIsNil(process.stdout:read(2))

    local process = createLua('error "Test subprocess error."', { stderr = true })
    lu.assertEquals(process:wait(), 1)
    lu.assertErrorMsgContains("Test subprocess error.", error, process.stderr:read "a")
end

function test_subprocess:test_stdio_2()
    os.remove 'temp.txt'
    local f = io.open('temp.txt', 'w')
    lu.assertUserdata(f)
    local process = createLua('io.write "ok"', { stdout = f })
    lu.assertEquals(process.stdout, f)
    f:close()
    lu.assertEquals(process:wait(), 0)
    local f = io.open('temp.txt', 'r')
    lu.assertUserdata(f)
    lu.assertEquals(f:read 'a', "ok")
    f:close()

    os.remove 'temp.txt'
    local wr = io.open('temp.txt', 'w')
    local rd = io.open('temp.txt', 'r')
    lu.assertUserdata(wr)
    lu.assertUserdata(rd)
    local process = createLua('io.write "ok"', { stdout = wr })
    wr:close()
    lu.assertEquals(process.stdout, wr)
    lu.assertEquals(process:wait(), 0)
    lu.assertEquals(rd:read 'a', "ok")
    rd:close()
    os.remove 'test.txt'

    local process1 = createLua('io.write "ok"', { stdout = true })
    local process2 = createLua('io.write(io.read "a")', { stdin = process1.stdout, stdout = true })
    lu.assertEquals(process1:wait(), 0)
    lu.assertEquals(process2:wait(), 0)
    lu.assertEquals(process2.stdout:read 'a', "ok")
end

function test_subprocess:test_peek()
    local process = createLua('io.write "ok"', { stdout = true })
    lu.assertEquals(process:wait(), 0)
    lu.assertUserdata(process.stdout)
    lu.assertEquals(subprocess.peek(process.stdout), 2)
    lu.assertEquals(subprocess.peek(process.stdout), 2)
    lu.assertEquals(process.stdout:read(2), "ok")
    lu.assertIsNil(subprocess.peek(process.stdout))

    local process = createLua('io.write "ok"', { stdout = true })
    lu.assertEquals(process:wait(), 0)
    lu.assertUserdata(process.stdout)
    lu.assertEquals(subprocess.peek(process.stdout), 2)
    process.stdout:close()
    lu.assertError("attempt to use a closed file", process.stdout.read, process.stdout, 2)
    lu.assertIsNil(subprocess.peek(process.stdout))

    local process = createLua('io.stdout:setvbuf "no" io.write "start" io.write(io.read "a")', { stdin = true, stdout = true })
    lu.assertUserdata(process.stdin)
    lu.assertUserdata(process.stdout)
    lu.assertIsTrue(process:is_running())
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
    lu.assertEquals(process:wait(), 0)
    lu.assertEquals(process.stdout:read(2), "ok")
    lu.assertIsNil(process.stdout:read(2))
end

function test_subprocess:test_filemode()
    if platform.OS == 'Windows' then
        local process = createLua([[
            assert(io.read "a" == "\n")
        ]], { stdin = true, stderr = true })
        process.stdin:write "\r\n"
        process.stdin:close()
        lu.assertEquals(process:wait(), 0)
        lu.assertEquals(process.stderr:read "a", "")
    end
    local process = createLua([[
        local sp = require "bee.subprocess"
        sp.filemode(io.stdin, 'b')
        assert(io.read "a" == "\r\n")
    ]], { stdin = true, stderr = true })
    process.stdin:write "\r\n"
    process.stdin:close()
    lu.assertEquals(process:wait(), 0)
    lu.assertEquals(process.stderr:read "a", "")
end

function test_subprocess:test_env()
    local function test_env(cond, env)
        local script = ('assert(%s)'):format(cond)
        lu.assertEquals(
            createLua(
                script,
                {env = env}
            ):wait(),
            0
        )
    end
    test_env('os.getenv "BEE_TEST" == nil', {})
    test_env('os.getenv "BEE_TEST" == "ok"', { BEE_TEST = 'ok' })

    lu.assertNotNil(os.getenv 'PATH')
    test_env('os.getenv "PATH" ~= nil', {})
    test_env('os.getenv "PATH" == nil', {PATH=false})
end

function test_subprocess:test_sockets()
    local sfd, cfd = assert(socket.pair())
    local process = createLua([[
        local subprocess = require 'bee.subprocess'
        local socket = require 'bee.socket'
        local thread = require 'bee.thread'
        assert(type(subprocess.sockets) == 'table')
        assert(type(subprocess.sockets[1]) == 'userdata')
        local cfd = subprocess.sockets[1]
        socket.select({cfd})
        assert(cfd:recv(1) == 'A')
        socket.select({cfd})
        assert(cfd:recv(1) == 'B')
        socket.select({cfd})
        assert(cfd:recv(1) == 'C')
        socket.select({cfd})
        assert(cfd:recv() == nil)
        cfd:close()
    ]], { sockets = {cfd}, stderr = true })
    cfd:close()
    socket.select(nil, {sfd})
    sfd:send 'A'
    socket.select(nil, {sfd})
    sfd:send 'B'
    socket.select(nil, {sfd})
    sfd:send 'C'
    sfd:close()
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
end

function test_subprocess:test_args()
    testArrayArgs('A')
    testArrayArgs('A', 'B')
    testArrayArgs('A B')
    testArrayArgs('A', 'B C')
    testArrayArgs([["A"]])
    testArrayArgs([["A]])
    testArrayArgs([["A\]])
    testArrayArgs([["A\"]])
    testArrayArgs([[A" B]])
    testStringArgs([[A]], 'A')
    testStringArgs([[A B]], 'A', 'B')
    testStringArgs([["A B"]], 'A B')
    testStringArgs([[A "B C"]], 'A', 'B C')
    testStringArgs([[\"A\"]], [["A"]])
    testStringArgs([[\"A]], [["A]])
    testStringArgs([[\"A\]], [["A\]])
    testStringArgs([[\"A\\\"]], [["A\"]])
    testStringArgs([["A\" B"]], [[A" B]])
end

local function quote_arg(s)
    if #s == 0 then
        return '""'
    end
    if not s:find(' \t\"', 1, true) then
        return s
    end
    if not s:find('\"\\', 1, true) then
        return '"'..s..'"'
    end
    local quote_hit = true
    local t = {}
    t[#t+1] = '"'
    for i = #s, 1, -1 do
        local c = s:sub(i,i)
        t[#t+1] = c
        if quote_hit and c == '\\' then
            t[#t+1] = '\\'
        elseif c == '"' then
            quote_hit = true
            t[#t+1] = '\\'
        else
            quote_hit = false
        end
    end
    t[#t+1] = '"'
    for i = 1, #t // 2 do
        local tmp = t[i]
        t[i] = t[#t-i+1]
        t[#t-i+1] = tmp
    end
    return table.concat(t)
end

if platform.OS == 'macOS' then
    return
end

require 'bee'

function test_subprocess:test_shell()
    local process, err = subprocess.shell {'echo', 'ok', stdout = true, stderr = true}
    lu.assertUserdata(process, err)
    lu.assertUserdata(process.stdout)
    lu.assertUserdata(process.stderr)
    if platform.OS == 'Windows' then
        lu.assertEquals(process.stdout:read 'a', 'ok\r\n')
    else
        lu.assertEquals(process.stdout:read 'a', 'ok\n')
    end
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
end

function test_subprocess:test_fork()
    os.remove('./test/test_fork.lua')
    local f = assert(io.open('./test/test_fork.lua', 'wb'))
    f:write('io.write "ok"')
    f:close()
    local process, err = subprocess.fork { 'test_fork', stdout = true, stderr = true }
    lu.assertUserdata(process, err)
    lu.assertUserdata(process.stdout)
    lu.assertUserdata(process.stderr)
    lu.assertEquals(process.stdout:read 'a', 'ok')
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
    os.remove('./test/test_fork.lua')
end
