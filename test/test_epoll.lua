local lt = require "ltest"
local epoll = require "bee.epoll"
local socket = require "bee.socket"
local time = require "bee.time"
local m = lt.test "epoll"

local function SimpleServer(protocol, ...)
    local fd = assert(socket.create(protocol))
    assert(fd:bind(...))
    assert(fd:listen())
    return fd
end

local function SimpleClient(protocol, ...)
    local fd = assert(socket.create(protocol))
    local ok, err = fd:connect(...)
    assert(ok ~= nil, err)
    return fd
end

function m.test_create()
    lt.assertFailed("maxevents is less than or equal to zero.", epoll.create(-1))
    lt.assertFailed("maxevents is less than or equal to zero.", epoll.create(0))
    local epfd <close> = epoll.create(16)
    lt.assertIsUserdata(epfd)
end

function m.test_close()
    local epfd = epoll.create(16)
    local fd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    lt.assertEquals(true, epfd:event_add(fd, 0))
    lt.assertEquals(true, epfd:close())

    lt.assertEquals(epfd:close(), nil)
    lt.assertEquals(epfd:event_add(fd, 0), nil)
end

function m.test_event()
    local epfd = epoll.create(16)
    local fd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    lt.assertIsNil(epfd:event_mod(fd, 0))
    lt.assertIsNil(epfd:event_del(fd))

    lt.assertEquals(true, epfd:event_add(fd, 0))
    lt.assertIsNil(epfd:event_add(fd, 0))
    lt.assertEquals(true, epfd:event_mod(fd, 0))
    lt.assertEquals(true, epfd:event_del(fd))
    lt.assertIsNil(epfd:event_mod(fd, 0))
    lt.assertIsNil(epfd:event_del(fd))
    lt.assertEquals(true, epfd:event_add(fd, 0))
    lt.assertIsNil(epfd:event_add(fd, 0))
    lt.assertEquals(true, epfd:event_mod(fd, 0))
    lt.assertEquals(true, epfd:event_del(fd))

    epfd:close()
end

function m.test_enum()
    lt.assertEquals(epoll.EPOLLIN, 1 << 0)
    lt.assertEquals(epoll.EPOLLPRI, 1 << 1)
    lt.assertEquals(epoll.EPOLLOUT, 1 << 2)
    lt.assertEquals(epoll.EPOLLERR, 1 << 3)
    lt.assertEquals(epoll.EPOLLHUP, 1 << 4)
    lt.assertEquals(epoll.EPOLLRDNORM, 1 << 6)
    lt.assertEquals(epoll.EPOLLRDBAND, 1 << 7)
    lt.assertEquals(epoll.EPOLLWRNORM, 1 << 8)
    lt.assertEquals(epoll.EPOLLWRBAND, 1 << 9)
    lt.assertEquals(epoll.EPOLLMSG, 1 << 10)
    lt.assertEquals(epoll.EPOLLRDHUP, 1 << 13)
    lt.assertEquals(epoll.EPOLLONESHOT, 1 << 30)
end

function m.test_wait()
    do
        local epfd = epoll.create(16)
        epfd:close()
        lt.assertIsNil(epfd:wait())
    end
    do
        local epfd <close> = epoll.create(16)
        lt.assertIsFunction(epfd:wait(0))
        for _ in epfd:wait(0) do
            lt.failure "Shouldn't run to here."
        end
    end
    local epfd <close> = epoll.create(16)
    local fd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    epfd:event_add(fd, 0, fd)
    for _ in epfd:wait(0) do
        lt.failure "Shouldn't run to here."
    end
end

local events = {
    "EPOLLIN",
    "EPOLLPRI",
    "EPOLLOUT",
    "EPOLLERR",
    "EPOLLHUP",
    "EPOLLRDNORM",
    "EPOLLRDBAND",
    "EPOLLWRNORM",
    "EPOLLWRBAND",
    "EPOLLMSG",
    "EPOLLRDHUP",
    "EPOLLONESHOT",
    "EPOLLET",
}

local function event_tostring(e)
    if e == 0 then
        lt.failure "unknown flags: 0"
        return "0"
    end
    local r = {}
    for _, name in ipairs(events) do
        local v = epoll[name]
        if v and e & v ~= 0 then
            r[#r+1] = name
            e = e & ~v
            if e == 0 then
                return table.concat(r, " | ")
            end
        end
    end
    r[#r+1] = string.format("0x%x", e)
    local res = table.concat(r, " | ")
    lt.failure("unknown flags: %s", res)
    return res
end

local function event_tointeger(e)
    local r = 0
    for name in e:gmatch "[A-Z]+" do
        local v = epoll[name]
        if v then
            r = r | v
        end
    end
    return r
end

local function event_bor(a, b)
    if a then
        return event_tostring(event_tointeger(a) | b)
    end
    return event_tostring(b)
end

local function assertEpollWait(epfd, values)
    local actual = {}
    local expected = {}
    if type(values[1]) == "table" then
        for _, v in ipairs(values) do
            expected[v[1]] = v[2]
        end
    else
        local v = values
        if v[1] then
            expected[v[1]] = v[2]
        end
    end

    local start = time.monotonic()
    while time.monotonic() - start < 100 do
        for s, event in epfd:wait(1) do
            actual[s] = event_bor(actual[s], event)
        end
        if lt.equals(actual, expected) then
            return
        end
    end
    lt.assertEquals(actual, expected)
end

function m.test_connect_event()
    do
        local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
        local cfd <close> = SimpleClient("tcp", sfd:info "socket")
        local sep <const> = epoll.create(16)
        local cep <const> = epoll.create(16)
        sep:event_add(sfd, epoll.EPOLLIN | epoll.EPOLLOUT, sfd)
        cep:event_add(cfd, epoll.EPOLLIN | epoll.EPOLLOUT, cfd)
        assertEpollWait(sep, { sfd, "EPOLLIN" })
        assertEpollWait(cep, { cfd, "EPOLLOUT" })
    end
    do
        local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
        local cfd <close> = SimpleClient("tcp", sfd:info "socket")
        local sep <const> = epoll.create(16)
        local cep <const> = epoll.create(16)
        sep:event_add(sfd, epoll.EPOLLIN | epoll.EPOLLOUT, "")
        sep:event_mod(sfd, epoll.EPOLLIN | epoll.EPOLLOUT, "server")
        cep:event_add(cfd, epoll.EPOLLIN | epoll.EPOLLOUT, "")
        cep:event_mod(cfd, epoll.EPOLLIN | epoll.EPOLLOUT, "client")
        assertEpollWait(sep, { "server", "EPOLLIN" })
        assertEpollWait(cep, { "client", "EPOLLOUT" })
    end
end

function m.test_send_recv_event()
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient("tcp", sfd:info "socket")
    local sep <const> = epoll.create(16)
    local cep <const> = epoll.create(16)
    sep:event_add(sfd, epoll.EPOLLIN | epoll.EPOLLOUT, sfd)
    cep:event_add(cfd, epoll.EPOLLIN | epoll.EPOLLOUT, cfd)
    assertEpollWait(sep, { sfd, "EPOLLIN" })
    assertEpollWait(cep, { cfd, "EPOLLOUT" })
    local newfd = sfd:accept()
    sep:event_add(newfd, epoll.EPOLLIN | epoll.EPOLLOUT, newfd)
    assertEpollWait(sep, { newfd, "EPOLLOUT" })
    local function test(data)
        lt.assertEquals(newfd:send(data), #data)
        assertEpollWait(sep, { newfd, "EPOLLOUT" })
        assertEpollWait(cep, { cfd, "EPOLLIN | EPOLLOUT" })
        lt.assertEquals(cfd:recv(), data)
        lt.assertEquals({ cfd:recv() }, { false })
        assertEpollWait(cep, { cfd, "EPOLLOUT" })
    end
    test "1234567890"
end

function m.test_shutdown_event()
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient("tcp", sfd:info "socket")
    local sep <const> = epoll.create(16)
    local cep <const> = epoll.create(16)
    sep:event_add(sfd, epoll.EPOLLIN | epoll.EPOLLOUT | epoll.EPOLLRDHUP, sfd)
    cep:event_add(cfd, epoll.EPOLLIN | epoll.EPOLLOUT | epoll.EPOLLRDHUP, cfd)
    assertEpollWait(sep, { sfd, "EPOLLIN" })
    assertEpollWait(cep, { cfd, "EPOLLOUT" })
    local newfd <close> = sfd:accept()
    sep:event_add(newfd, epoll.EPOLLIN | epoll.EPOLLOUT | epoll.EPOLLRDHUP, newfd)

    newfd:shutdown "w"
    assertEpollWait(sep, { newfd, "EPOLLOUT" })
    lt.assertIsNil(newfd:send "")
    assertEpollWait(cep, { cfd, "EPOLLIN | EPOLLOUT | EPOLLRDHUP" })
    lt.assertEquals({ newfd:recv() }, { false })

    newfd:shutdown "r"
    assertEpollWait(cep, { cfd, "EPOLLIN | EPOLLOUT | EPOLLRDHUP" })
    lt.assertIsNil(newfd:recv())
end

local EPOLLIN <const> = epoll.EPOLLIN
local EPOLLOUT <const> = epoll.EPOLLOUT
local EPOLLERR <const> = epoll.EPOLLERR
local EPOLLHUP <const> = epoll.EPOLLHUP

local function create_stream(epfd, fd)
    local sock = { fd = fd }
    local kMaxReadBufSize <const> = 4 * 1024
    local rbuf = ""
    local wbuf = ""
    local closed = false
    local halfclose_r = false
    local halfclose_w = false
    local event_r = false
    local event_w = false
    local event_mask = 0
    local function closefd()
        epfd:event_del(fd)
        fd:close()
    end
    local function event_update()
        local mask = 0
        if event_r then
            mask = mask | EPOLLIN
        end
        if event_w then
            mask = mask | EPOLLOUT
        end
        if mask ~= event_mask then
            epfd:event_mod(fd, mask)
            event_mask = mask
        end
    end
    local function force_close()
        closefd()
        assert(halfclose_r)
        assert(halfclose_w)
    end
    local function on_read()
        local data, err = fd:recv()
        if data == nil then
            if err then
                lt.failure("recv error: %s", err)
            end
            halfclose_r = true
            event_r = false
            if halfclose_w then
                force_close()
            elseif #wbuf == 0 then
                halfclose_w = true
                event_w = false
                force_close()
            else
                event_update()
            end
        elseif data == false then
        else
            rbuf = rbuf..data
            if #rbuf > kMaxReadBufSize then
                event_r = false
                event_update()
            end
            sock:on_recv()
        end
    end
    local function on_write()
        local n, err = fd:send(wbuf)
        if n == nil then
            if err then
                lt.failure("send error: %s", err)
            end
            halfclose_w = true
            event_w = false
            if halfclose_r then
                force_close()
            else
                event_update()
            end
        elseif n == false then
        else
            wbuf = wbuf:sub(n + 1)
            if #wbuf == 0 then
                event_w = false
                event_update()
            end
        end
    end
    local function on_event(e)
        if e & (EPOLLERR | EPOLLHUP) ~= 0 then
            e = e & (EPOLLIN | EPOLLOUT)
        end
        if e & EPOLLIN ~= 0 then
            assert(not halfclose_r)
            on_read()
        end
        if e & EPOLLOUT ~= 0 then
            if not halfclose_w then
                on_write()
            end
        end
    end
    epfd:event_add(fd, 0, on_event)
    event_r = true
    event_update()

    function sock:send(data)
        if closed then
            return
        end
        if #data > 0 then
            wbuf = wbuf..data
            event_w = true
            event_update()
        end
    end

    function sock:recv(n)
        if n == 0 then
            return ""
        end
        local res
        local full = #rbuf >= kMaxReadBufSize
        if n == nil then
            res = rbuf
            rbuf = ""
        elseif n <= #rbuf then
            res = rbuf:sub(1, n)
            rbuf = rbuf:sub(n + 1)
        elseif n >= kMaxReadBufSize then
            res = rbuf
            rbuf = ""
        else
            return
        end
        if full then
            event_r = true
            event_update()
        end
        return res
    end

    function sock:close()
        if closed or halfclose_r then
            return
        end
        halfclose_r = true
        event_r = false
        if halfclose_w then
            force_close()
        else
            event_update()
        end
    end

    return sock
end

local function create_listen(epfd, fd)
    local sock = { fd = fd }
    local function closefd()
        epfd:event_del(fd)
        fd:close()
    end
    local function on_event(e)
        if e & (EPOLLERR | EPOLLHUP) ~= 0 then
            closefd()
            return
        end
        if e & EPOLLIN ~= 0 then
            sock:on_accept()
        end
    end
    epfd:event_add(fd, EPOLLIN, on_event)
    return sock
end

function m.test_pingpong()
    local epfd = epoll.create(512)
    local quit = false
    local s = SimpleServer("tcp", "127.0.0.1", 0)
    local server = create_listen(epfd, s)
    function server:on_accept()
        local newfd = assert(self.fd:accept())
        local session = create_stream(epfd, newfd)
        function session:on_recv()
            local res = self:recv(6)
            if res == nil then
                return
            end
            local strid = res:match "^PING%-(%d)$"
            lt.assertIsString(strid)
            self:send("PONG-"..strid)
        end
    end

    local c = SimpleClient("tcp", s:info "socket")
    local client = create_stream(epfd, c)
    client:send "PING-1"
    function client:on_recv()
        local res = self:recv(6)
        if res == nil then
            return
        end
        local strid = res:match "^PONG%-(%d)$"
        lt.assertIsString(strid)
        local id = tonumber(strid)
        if id < 9 then
            self:send("PING-"..(id + 1))
        else
            quit = true
        end
    end

    while not quit do
        for f, event in epfd:wait() do
            f(event)
        end
    end
    epfd:close()
end
