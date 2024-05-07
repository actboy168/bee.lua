local lt = require "ltest"
local socket = require "bee.socket"
local select = require "bee.select"
local thread = require "bee.thread"
local fs = require "bee.filesystem"

local function simple_select(fd, mode)
    local s <close> = select.create()
    if mode == "r" then
        s:event_add(fd, select.SELECT_READ)
        s:wait()
    elseif mode == "w" then
        s:event_add(fd, select.SELECT_WRITE)
        s:wait()
    elseif mode == "rw" then
        s:event_add(fd, select.SELECT_READ | select.SELECT_WRITE)
        local event = 0
        for _, e in s:wait() do
            event = event | e
        end
        return event
    else
        assert(false)
    end
end

local function assertNotThreadError()
    local msg = thread.errlog()
    if msg then
        lt.failure(msg)
    end
end

local function file_exists(filename)
    local f, _, errno = io.open(filename, "r")
    if f then
        f:close()
        return true
    end
    return errno ~= 2
end

local function syncSend(fd, data)
    while true do
        simple_select(fd, "w")
        local n = fd:send(data)
        if not n then
            return n, data
        else
            data = data:sub(n + 1)
            if data == "" then
                return true
            end
        end
    end
end

local function syncRecv(fd, n)
    local res = ""
    while true do
        simple_select(fd, "r")
        local data = fd:recv(n)
        if data == nil then
            return nil, n
        elseif data == false then
        else
            n = n - #data
            res = res..data
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

local TestUnixSock = "test.unixsock"

local supportAutoUnlink = false
local function detectAutoUnlink()
    if supportAutoUnlink then
        lt.assertEquals(file_exists(TestUnixSock), false)
    else
        fs.remove(TestUnixSock)
    end
end

function test_socket:test_bind()
    do
        local fd = lt.assertIsUserdata(socket.create "tcp")
        lt.assertIsBoolean(fd:bind("127.0.0.1", 0))
        fd:close()
    end
    do
        local fd = lt.assertIsUserdata(socket.create "udp")
        lt.assertIsBoolean(fd:bind("127.0.0.1", 0))
        fd:close()
    end
    do
        lt.assertErrorMsgEquals([[bad argument #1 to 'bee.socket.create' (invalid option 'icmp')]], socket.create, "icmp")
    end
    do
        fs.remove(TestUnixSock)
        local fd = lt.assertIsUserdata(socket.create "unix")
        lt.assertIsBoolean(fd:bind(TestUnixSock))
        lt.assertEquals(file_exists(TestUnixSock), true)
        fd:close()
    end
    do
        local fd = lt.assertIsUserdata(socket.create "unix")
        lt.assertIsBoolean(fd:bind(TestUnixSock))
        lt.assertEquals(file_exists(TestUnixSock), true)
        fd:close()
        detectAutoUnlink()
    end
end

function test_socket:test_tcp_connect()
    local server = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(server:bind("127.0.0.1", 0))
    lt.assertIsBoolean(server:listen())
    local address, port = server:info "socket":value()
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket.create "tcp")
        lt.assertIsBoolean(client:connect("127.0.0.1", port))
        client:close()
    end
    server:close()
end

function test_socket:test_unix_connect()
    fs.remove(TestUnixSock)
    --TODO: 某些低版本的windows过不了？
    --lt.assertEquals(socket.create "unix":connect(TestUnixSock), nil)
    --lt.assertEquals(file_exists(TestUnixSock), false)

    local server = lt.assertIsUserdata(socket.create "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists(TestUnixSock), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket.create "unix")
        lt.assertIsBoolean(client:connect(TestUnixSock))
        client:close()
    end
    server:close()
    detectAutoUnlink()
end

function test_socket:test_tcp_accept()
    local server = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(server:bind("127.0.0.1", 0))
    lt.assertIsBoolean(server:listen())
    local address, port = server:info "socket":value()
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket.create "tcp")
        lt.assertIsBoolean(client:connect("127.0.0.1", port))
        simple_select(server, "r")
        simple_select(client, "w")
        local session = lt.assertIsUserdata(server:accept())
        lt.assertEquals(client:status(), true)
        lt.assertEquals(session:status(), true)
        session:close()
        client:close()
    end
    server:close()
end

function test_socket:test_unix_accept()
    fs.remove(TestUnixSock)
    local server = lt.assertIsUserdata(socket.create "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    lt.assertEquals(file_exists(TestUnixSock), true)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket.create "unix")
        lt.assertIsBoolean(client:connect(TestUnixSock))
        simple_select(server, "r")
        simple_select(client, "w")
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
    return thread.create(([[
    -- %s
    local protocol, address, port = ...
    local socket = require "bee.socket"
    local select = require "bee.select"
    local function simple_select(fd, mode)
        local s <close> = select.create()
        if mode == "r" then
            s:event_add(fd, select.SELECT_READ)
            s:wait()
        elseif mode == "w" then
            s:event_add(fd, select.SELECT_WRITE)
            s:wait()
        elseif mode == "rw" then
            s:event_add(fd, select.SELECT_READ | select.SELECT_WRITE)
            local event = 0
            for _, e in s:wait() do
                event = event | e
            end
            return event
        else
            assert(false)
        end
    end

    local client = assert(socket.create(protocol))
    client:connect(address, port)
    simple_select(client, "w")
    assert(client:status())
    local queue = ""
    while true do
        local event = simple_select(client, "rw")
        if event & select.SELECT_READ then
            local data = client:recv()
            if data == nil then
                break
            elseif data == false then
            else
                queue = queue .. data
            end
        end
        if event & select.SELECT_WRITE then
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
    local server = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(server:bind("127.0.0.1", 0))
    lt.assertIsBoolean(server:listen())
    local _, port = server:info "socket":value()
    lt.assertIsNumber(port)
    local client = createEchoThread(name, "tcp", "127.0.0.1", port)
    simple_select(server, "r")
    local session = lt.assertIsUserdata(server:accept())
    lt.assertEquals(session:status(), true)

    f(session)

    session:close()
    server:close()
    thread.wait(client)
    assertNotThreadError()
end

local function createUnixEchoTest(name, f)
    fs.remove(TestUnixSock)
    local server = lt.assertIsUserdata(socket.create "unix")
    lt.assertIsBoolean(server:bind(TestUnixSock))
    lt.assertIsBoolean(server:listen())
    local client = createEchoThread(name, "unix", (TestUnixSock))
    simple_select(server, "r")
    local session = lt.assertIsUserdata(server:accept())
    lt.assertEquals(session:status(), true)

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
    createTcpEchoTest("tcp_echo_1", testEcho1)
end

function test_socket:test_unix_echo_1()
    createUnixEchoTest("unix_echo_1", testEcho1)
end

function test_socket:test_tcp_echo_2()
    createTcpEchoTest("tcp_echo_2", testEcho2)
end

function test_socket:test_unix_echo_2()
    createUnixEchoTest("unix_echo_2", testEcho2)
end

function test_socket:test_tcp_echo_3()
    createTcpEchoTest("tcp_echo_3", testEcho3)
end

function test_socket:test_unix_echo_3()
    createUnixEchoTest("unix_echo_3", testEcho3)
end

function test_socket:test_dump()
    local server = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(server:bind("127.0.0.1", 0))
    lt.assertIsBoolean(server:listen())
    local bindata = server:detach()
    server = socket.fd(bindata)
    local address, port = server:info "socket":value()
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    for _ = 1, 2 do
        local client = lt.assertIsUserdata(socket.create "tcp")
        lt.assertIsBoolean(client:connect("127.0.0.1", port))
        client:close()
    end
    server:close()
end

function test_socket:test_SIGPIPE()
    local server = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(server:bind("127.0.0.1", 0))
    lt.assertIsBoolean(server:listen())
    local address, port = server:info "socket":value()
    lt.assertIsString(address)
    lt.assertIsNumber(port)
    local client = lt.assertIsUserdata(socket.create "tcp")
    lt.assertIsBoolean(client:connect("127.0.0.1", port))
    server:close()
    for _ = 1, 10 do
        syncSend(client, "test")
    end
    client:close()
end

function test_socket:test_udp()
    local a_fd = lt.assertIsUserdata(socket.create "udp")
    local b_fd = lt.assertIsUserdata(socket.create "udp")
    lt.assertEquals(a_fd:bind("127.0.0.1", 0), true)
    lt.assertEquals(b_fd:bind("127.0.0.1", 0), true)
    local a_ep = a_fd:info "socket"
    local b_ep = b_fd:info "socket"
    for _, MSG in ipairs { "", "123" } do
        lt.assertEquals(a_fd:sendto(MSG, b_ep), #MSG)
        simple_select(b_fd, "r")
        local r, r_ep = b_fd:recvfrom()
        lt.assertEquals(MSG, r)
        lt.assertEquals(a_ep, r_ep)
    end
    a_fd:close()
    b_fd:close()
end

function test_socket:test_udp_unreachable()
    local a_fd = lt.assertIsUserdata(socket.create "udp")
    local b_fd = lt.assertIsUserdata(socket.create "udp")
    lt.assertEquals(a_fd:bind("127.0.0.1", 0), true)
    lt.assertEquals(b_fd:bind("127.0.0.1", 0), true)
    local b_ep = b_fd:info "socket"
    b_fd:close()
    lt.assertEquals(a_fd:sendto("ping", b_ep), 4)
    lt.assertEquals(a_fd:recvfrom(), false)
    lt.assertEquals(a_fd:recvfrom(), false)
    a_fd:close()
end
