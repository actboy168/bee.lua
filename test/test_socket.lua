local lt = require 'ltest'
local socket = require 'bee.socket'
local thread = require 'bee.thread'
local errlog = thread.channel "errlog"

local function assertNotThreadError()
    lt.assertEquals(errlog:pop(), false)
end

local function file_exists(filename)
    local f, _, errno = io.open(filename, 'r')
    if f then
        f:close()
        return true
    end
    return errno ~= 2
end

local function syncSend(fd, data)
    while true do
        local _, wr = socket.select(nil, {fd})
        if not wr or wr[1] ~= fd then
            break
        end
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
        local rd = socket.select({fd}, nil)
        if not rd or rd[1] ~= fd then
            break
        end
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

local function syncEcho(from, to, msg)
    lt.assertEquals(syncSend(from, msg), true)
    lt.assertEquals(syncRecv(to, #msg), msg)
end

local test_socket = lt.test "socket"

local TestUnixSock = 'test.unixsock'

local supportAutoUnlink = false
local function detectAutoUnlink()
    if supportAutoUnlink then
        lt.assertEquals(file_exists(TestUnixSock), false)
    else
        os.remove(TestUnixSock)
    end
end

function test_socket:test_bind()
    do
        local fd = lt.assertIsUserdata(socket 'tcp')
        lt.assertIsBoolean(fd:bind('127.0.0.1', 0))
        fd:close()
    end
    do
        local fd = lt.assertIsUserdata(socket 'udp')
        lt.assertIsBoolean(fd:bind('127.0.0.1', 0))
        fd:close()
    end
    do
        lt.assertErrorMsgEquals([[bad argument #2 to '?' (invalid option 'icmp')]], socket, 'icmp')
    end
    do
        os.remove(TestUnixSock)
        local fd = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(fd:bind(TestUnixSock))
        lt.assertEquals(file_exists(TestUnixSock), true)
        fd:close()
    end
    do
        local fd = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(fd:bind(TestUnixSock))
        lt.assertEquals(file_exists(TestUnixSock), true)
        fd:close()
        detectAutoUnlink()
    end
end

function test_socket:test_tcp_connect()
    local server = lt.assertIsUserdata(socket "tcp")
    lt.assertIsBoolean(server:bind('127.0.0.1', 0))
    lt.assertIsBoolean(server:listen())
    local address, port = server:info('socket')
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket "tcp")
        lt.assertIsBoolean(client:connect('127.0.0.1', port))
        client:close()
    end
    server:close()
end

function test_socket:test_unix_connect()
    os.remove(TestUnixSock)
    --TODO: 某些低版本的windows过不了？
    --lt.assertEquals(socket 'unix':connect(TestUnixSock), nil)
    --lt.assertEquals(file_exists(TestUnixSock), false)

    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists(TestUnixSock), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(client:connect(TestUnixSock))
        client:close()
    end
    server:close()
    detectAutoUnlink()
end

function test_socket:test_tcp_accept()
    local server = lt.assertIsUserdata(socket "tcp")
    lt.assertIsBoolean(server:bind('127.0.0.1', 0))
    lt.assertIsBoolean(server:listen())
    local address, port = server:info('socket')
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket "tcp")
        lt.assertIsBoolean(client:connect('127.0.0.1', port))
        local rd, _ = socket.select({server}, nil)
        local _, wr = socket.select(nil, {client})
        lt.assertIsTable(rd)
        lt.assertIsTable(wr)
        lt.assertEquals(rd[1], server)
        lt.assertEquals(wr[1], client)
        local session = lt.assertIsUserdata(server:accept())
        lt.assertEquals(client:status(), true)
        lt.assertEquals(session:status(), true)
        session:close()
        client:close()
    end
    server:close()
end

function test_socket:test_unix_accept()
    os.remove(TestUnixSock)
    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists(TestUnixSock), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(client:connect(TestUnixSock))
        local rd, _ = socket.select({server}, nil)
        local _, wr = socket.select(nil, {client})
        lt.assertIsTable(rd)
        lt.assertIsTable(wr)
        lt.assertEquals(rd[1], server)
        lt.assertEquals(wr[1], client)
        local session = lt.assertIsUserdata(server:accept())
        lt.assertEquals(client:status(), true)
        lt.assertEquals(session:status(), true)
        session:close()
        client:close()
    end
    server:close()
    detectAutoUnlink()
end

function test_socket:test_pair()
    local server, client = assert(socket.pair())
    lt.assertIsUserdata(server)
    lt.assertIsUserdata(client)
    syncEcho(server, client, "ok")
    client:close()
    server:close()
end

local function createEchoThread(name, ...)
return thread.thread(([[
    -- %s
    local protocol, address, port = ...
    local socket = require 'bee.socket'
    local client = assert(socket(protocol))
    client:connect(address, port)
    local _, wr = socket.select(nil, {client})
    assert(wr[1] == client)
    assert(client:status())
    local queue = ''
    while true do
        local rd, wr = socket.select({client}, {client})
        if rd and rd[1] then
            local data = client:recv()
            if data == nil then
                break
            elseif data == false then
            else
                queue = queue .. data
            end
        end
        if wr and wr[1] then
            if #queue == 0 then
                goto continue
            end
            local n = client:send(queue)
            if n == nil then
                break
            elseif n == false then
            else
                queue = queue:sub(n + 1)
            end
        end
        ::continue::
    end
    client:close()
]]):format(name), ...)

end

local function createTcpEchoTest(name, f)
    local server = lt.assertIsUserdata(socket "tcp")
    lt.assertIsBoolean(server:bind('127.0.0.1', 0))
    lt.assertIsBoolean(server:listen())
    local _, port = server:info('socket')
    lt.assertIsNumber(port)
    local client = createEchoThread(name, 'tcp', '127.0.0.1', port)
    local rd, _ = socket.select({server}, nil)
    assert(rd[1], server)
    local session = server:accept()
    assert(session and session:status())

    f(session)

    session:close()
    server:close()
    thread.wait(client)
    assertNotThreadError()
end

local function createUnixEchoTest(name, f)
    os.remove(TestUnixSock)
    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    local client = createEchoThread(name, 'unix',(TestUnixSock))
    local rd, _ = socket.select({server}, nil)
    assert(rd and rd[1], server)
    local session = server:accept()
    assert(session and session:status())

    f(session)

    session:close()
    server:close()
    thread.wait(client)
    assertNotThreadError()
    detectAutoUnlink()
end

local function testEcho1()
end

local function testEcho2(session)
    syncEcho(session, session, "ok")

    lt.assertEquals(syncSend(session, "ok(1)"), true)
    lt.assertEquals(syncSend(session, "ok(2)"), true)
    lt.assertEquals(syncRecv(session, 10), "ok(1)ok(2)")

    lt.assertEquals(syncSend(session, "1234567890"), true)
    lt.assertEquals(syncRecv(session, 2), "12")
    lt.assertEquals(syncRecv(session, 2), "34")
    lt.assertEquals(syncRecv(session, 2), "56")
    lt.assertEquals(syncRecv(session, 2), "78")
    lt.assertEquals(syncRecv(session, 2), "90")
end

local function testEcho3(session)
    local t = {}
    for _ = 1, 10000 do
        t[#t+1] = tostring(math.random(1, 100000))
    end
    local s = table.concat(t, ",")
    syncEcho(session, session, s)
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

function test_socket:test_dump()
    local server = lt.assertIsUserdata(socket "tcp")
    lt.assertIsBoolean(server:bind('127.0.0.1', 0))
    lt.assertIsBoolean(server:listen())
    local bindata = server:detach()
    server = socket.fd(bindata)
    local address, port = server:info('socket')
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket "tcp")
        lt.assertIsBoolean(client:connect('127.0.0.1', port))
        client:close()
    end
    server:close()
end
