local lu = require 'luaunit'
local ls = require 'bee.socket'
local platform = require 'bee.platform'
local thread = require 'bee.thread'
local err = thread.channel "errlog"

local function assertNotThreadError()
    lu.assertIsFalse(err:pop())
end

local function file_exists(filename)
    local f, _, errno = io.open(filename, 'r')
    if f then
        f:close()
        return true
    end
    return errno ~= 2
end

test_socket = {}

function test_socket:test_bind()
    os.remove('test.unixsock')
    local function assert_ok(fd)
        lu.assertUserdata(fd)
        fd:close()
    end
    assert_ok(ls.bind('tcp', '127.0.0.1', 0))
    assert_ok(ls.bind('udp', '127.0.0.1', 0))
    lu.assertErrorMsgEquals('invalid protocol `icmp`.', ls.bind, 'icmp', '127.0.0.1', 0)

    local fd = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(fd)
    lu.assertIsTrue(file_exists('test.unixsock'))
    fd:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

function test_socket:test_tcp_connect()
    local server = ls.bind('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(server)
    local client = ls.connect('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(client)
    client:close()
    server:close()
end

function test_socket:test_unix_connect()
    os.remove('test.unixsock')
    lu.assertIsNil(ls.connect('unix', 'test.unixsock'))
    if platform.OS == 'Windows' then
        -- Windows的bug？
        os.remove('test.unixsock')
    end
    lu.assertIsFalse(file_exists('test.unixsock'))

    local server = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(server)
    lu.assertIsTrue(file_exists('test.unixsock'))
    local client = ls.connect('unix', 'test.unixsock')
    lu.assertUserdata(client)
    client:close()
    server:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

function test_socket:test_tcp_accept()
    local server = ls.bind('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(server)
    local client = ls.connect('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(client)
    local rd, _ = ls.select({server}, nil)
    local _, wr = ls.select(nil, {client})
    lu.assertTable(rd)
    lu.assertTable(wr)
    lu.assertEquals(rd[1], server)
    lu.assertEquals(wr[1], client)
    local session = server:accept()
    lu.assertUserdata(session)
    lu.assertIsTrue(client:status())
    lu.assertIsTrue(session:status())
    session:close()
    client:close()
    server:close()
end

function test_socket:test_unix_accept()
    os.remove('test.unixsock')
    local server = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(server)
    lu.assertIsTrue(file_exists('test.unixsock'))
    local client = ls.connect('unix', 'test.unixsock')
    lu.assertUserdata(client)
    local rd, _ = ls.select({server}, nil)
    local _, wr = ls.select(nil, {client})
    lu.assertTable(rd)
    lu.assertTable(wr)
    lu.assertEquals(rd[1], server)
    lu.assertEquals(wr[1], client)
    local session = server:accept()
    lu.assertUserdata(session)
    lu.assertIsTrue(client:status())
    lu.assertIsTrue(session:status())
    session:close()
    client:close()
    server:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

function test_socket:test_pair()
    local server, client = assert(ls.pair())
    lu.assertUserdata(server)
    lu.assertUserdata(client)
    client:close()
    server:close()
end

local function createEchoThread(name, address)
return thread.thread(([=[
    -- %s
    package.cpath = [[%s]]
    local ls = require 'bee.socket'
    local client = assert(ls.connect(%s))
    local _, wr = ls.select(nil, {client})
    assert(wr[1] == client)
    assert(client:status())
    local queue = ''
    while true do
        local rd, wr = ls.select({client}, {client})
        if rd and rd[1] then
            local data = client:recv()
            if data == nil then
                client:close()
                break
            elseif data == false then
            else
                queue = queue .. data
            end
        end
        if wr and wr[1] then
            local n = client:send(queue)
            if n == nil then
                client:close()
                break
            else
                queue = queue:sub(n + 1)
            end
        end
    end
]=]):format(name, package.cpath, address))

end

local function createTcpEchoTest(name, f)
    local server, errmsg = ls.bind('tcp', '127.0.0.1', 0)
    lu.assertUserdata(server, errmsg)
    local _, port = server:info('socket')
    lu.assertNumber(port)
    local client = createEchoThread(name, ([['tcp', '127.0.0.1', %d]]):format(port))
    local rd, _ = ls.select({server}, nil)
    assert(rd[1], server)
    local session = server:accept()
    assert(session and session:status())

    f(session)

    session:close()
    server:close()
    client:wait()
    assertNotThreadError()
end

local function createUnixEchoTest(name, f)
    os.remove('test.unixsock')
    local server, errmsg = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(server, errmsg)
    local client = createEchoThread(name, [['unix', 'test.unixsock']])
    local rd, _ = ls.select({server}, nil)
    assert(rd[1], server)
    local session = server:accept()
    assert(session and session:status())

    f(session)

    session:close()
    server:close()
    client:wait()
    assertNotThreadError()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

local function syncSend(fd, data)
    while true do
        ls.select(nil, {fd})
        local n = fd:send(data)
        if not n then
            return n, data
        else
            data = data:sub(n + 1)
            if data == '' then
                return true
            end
        end
    end
end

local function syncRecv(fd, n)
    local res = ''
    while true do
        ls.select({fd}, nil)
        local data = fd:recv(n)
        if data == nil then
            return nil, n
        elseif data == false then
        else
            n = n - #data
            res = res .. data
            if n <= 0 then
                return res
            end
        end
    end
end

local function testEcho1()
end

local function testEcho2(session)
    lu.assertIsTrue(syncSend(session, "ok"))
    lu.assertEquals(syncRecv(session, 2), "ok")

    lu.assertIsTrue(syncSend(session, "ok(1)"))
    lu.assertIsTrue(syncSend(session, "ok(2)"))
    lu.assertEquals(syncRecv(session, 10), "ok(1)ok(2)")

    lu.assertIsTrue(syncSend(session, "1234567890"))
    lu.assertEquals(syncRecv(session, 2), "12")
    lu.assertEquals(syncRecv(session, 2), "34")
    lu.assertEquals(syncRecv(session, 2), "56")
    lu.assertEquals(syncRecv(session, 2), "78")
    lu.assertEquals(syncRecv(session, 2), "90")
end

local function testEcho3(session)
    local t = {}
    for _ = 1, 10000 do
        t[#t+1] = tostring(math.random(1, 100000))
    end
    local s = table.concat(t, ",")
    lu.assertIsTrue(syncSend(session, s))
    lu.assertEquals(syncRecv(session, #s), s)
end

function test_socket:test_tcp_echo_1()
    createTcpEchoTest('tcp_echo_1', testEcho1)
end

function test_socket:test_unix_echo_1()
    createUnixEchoTest('unix_echo_1', testEcho1)
end

function test_socket:test_tcp_echo_2()
    createTcpEchoTest('tcp_echo_2', testEcho2)
end

function test_socket:test_unix_echo_2()
    createUnixEchoTest('unix_echo_2', testEcho2)
end

function test_socket:test_tcp_echo_3()
    createTcpEchoTest('tcp_echo_3', testEcho3)
end

function test_socket:test_unix_echo_3()
    createUnixEchoTest('unix_echo_3', testEcho3)
end
