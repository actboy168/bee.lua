local lu = require 'luaunit'

local subprocess = require 'bee.subprocess'

local function getexe()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
end

local function createLua(script, option)
    option = option or {}
    option[1] = {
        getexe(),
        '-e', script,
    }
    return subprocess.spawn(option)
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

function test_subprocess:test_stdio()
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
end
