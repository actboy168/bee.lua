local lt = require 'ltest'
local socket = require 'bee.socket'
local platform = require 'bee.platform'
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

local test_socket = lt.test "socket"

function test_socket:setup()
    if platform.OS == "Windows" then
        socket.simulationUDS(self.UDS)
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
        os.remove 'test.unixsock'
        local fd = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(fd:bind('test.unixsock'))
        lt.assertEquals(file_exists('test.unixsock'), true)
        fd:close()
        lt.assertEquals(file_exists('test.unixsock'), false)
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
    os.remove 'test.unixsock'
    lt.assertEquals(socket 'unix':connect('test.unixsock'), nil)
    lt.assertEquals(file_exists('test.unixsock'), false)

    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind('test.unixsock'))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists('test.unixsock'), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(client:connect 'test.unixsock')
        client:close()
    end
    server:close()
    lt.assertEquals(file_exists('test.unixsock'), false)
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
    os.remove 'test.unixsock'
    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind('test.unixsock'))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists('test.unixsock'), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket 'unix')
        lt.assertIsBoolean(client:connect 'test.unixsock')
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
    lt.assertEquals(file_exists('test.unixsock'), false)
end

function test_socket:test_pair()
    local server, client = assert(socket.pair())
    lt.assertIsUserdata(server)
    lt.assertIsUserdata(client)
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
    os.remove 'test.unixsock'
    local server = lt.assertIsUserdata(socket "unix")
    lt.assertIsBoolean(server:bind('test.unixsock'))
    lt.assertIsBoolean(server:listen())
    local client = createEchoThread(name, 'unix', 'test.unixsock')
    local rd, _ = socket.select({server}, nil)
    assert(rd[1], server)
    local session = server:accept()
    assert(session and session:status())

    f(session)

    session:close()
    server:close()
    thread.wait(client)
    assertNotThreadError()
    lt.assertEquals(file_exists('test.unixsock'), false)
end

local function syncSend(fd, data)
    while true do
        socket.select(nil, {fd})
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
        socket.select({fd}, nil)
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
    lt.assertEquals(syncSend(session, "ok"), true)
    lt.assertEquals(syncRecv(session, 2), "ok")

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
    lt.assertEquals(syncSend(session, s), true)
    lt.assertEquals(syncRecv(session, #s), s)
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
    local bindata = socket.dump(server)
    server = socket.undump(bindata)
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

if platform.OS == "Windows" then
    local test_socket_1 = lt.test "socket"
    local test_socket_2 = lt.test "socket-uds"
    for _, k in ipairs(test_socket_1) do
        test_socket_2[k] = test_socket_1[k]
    end
    test_socket_1.UDS = false
    test_socket_2.UDS = true
end
