local ltask = require "ltask"
local socket = require "bee.socket"
local epoll = require "bee.epoll"

local epfd = epoll.create(512)

local EPOLLIN <const> = epoll.EPOLLIN
local EPOLLOUT <const> = epoll.EPOLLOUT
local EPOLLERR <const> = epoll.EPOLLERR
local EPOLLHUP <const> = epoll.EPOLLHUP

local kMaxReadBufSize <const> = 64 * 1024

local status = {}
local handle = {}

local function fd_update(s)
    local flags = 0
    if s.r then
        flags = flags | EPOLLIN
    end
    if s.w then
        flags = flags | EPOLLOUT
    end
    if flags ~= s.event_flags then
        epfd:event_mod(s.fd, flags)
        s.event_flags = flags
    end
end

local function fd_set_read(s)
    if s.shutdown_r then
        return
    end
    s.r = true
    fd_update(s)
end

local function fd_clr_read(s)
    s.r = nil
    fd_update(s)
end

local function fd_set_write(s)
    if s.shutdown_w then
        return
    end
    s.w = true
    fd_update(s)
end

local function fd_clr_write(s)
    s.w = nil
    fd_update(s)
end

local function fd_init(fd)
    local s = status[fd]
    local function on_event(e)
        if e & (EPOLLERR | EPOLLHUP) ~= 0 then
            e = e | EPOLLIN | EPOLLOUT
        end
        if e & EPOLLIN ~= 0 then
            assert(not s.shutdown_r)
            s:on_read()
        end
        if e & EPOLLOUT ~= 0 then
            if not s.shutdown_w then
                s:on_write()
            end
        end
    end
    epfd:event_add(fd, 0, on_event)
end

local function create_handle(fd)
    local h = handle[fd]
    if h then
        return h
    end
    h = #handle + 1
    handle[h] = fd
    handle[fd] = h
    return h
end

local function close(s)
    local fd = s.fd
    epfd:event_del(fd)
    fd:close()
    assert(s.shutdown_r)
    assert(s.shutdown_w)
    if s.wait_read then
        assert(#s.wait_read == 0)
    end
    if s.wait_write then
        assert(#s.wait_write == 0)
    end
    if s.wait_close then
        for _, token in ipairs(s.wait_close) do
            ltask.wakeup(token)
        end
    end
end

local function close_write(s)
    if s.shutdown_r and s.shutdown_w then
        return
    end
    if not s.shutdown_w then
        s.shutdown_w = true
        fd_clr_write(s)
    end
    if s.shutdown_r then
        fd_clr_read(s)
        close(s)
    end
end

local function close_read(s)
    if s.shutdown_r and s.shutdown_w then
        return
    end
    if not s.shutdown_r then
        s.shutdown_r = true
        fd_clr_read(s)
        if s.wait_read then
            for i, token in ipairs(s.wait_read) do
                ltask.wakeup(token)
                s.wait_read[i] = nil
            end
        end
    end
    if s.shutdown_w then
        close(s)
    elseif not s.wait_write or #s.wait_write == 0 then
        s.shutdown_w = true
        fd_clr_write(s)
        close(s)
    end
end

local function stream_dispatch_read(s)
    while #s.wait_read > 0 do
        local token = s.wait_read[1]
        local n = token[1]
        if n == nil then
            ltask.wakeup(token, s.readbuf)
            s.readbuf = ""
            table.remove(s.wait_read, 1)
        else
            if n > #s.readbuf then
                break
            end
            ltask.wakeup(token, s.readbuf:sub(1, n))
            s.readbuf = s.readbuf:sub(n + 1)
            table.remove(s.wait_read, 1)
        end
    end
    if #s.readbuf > kMaxReadBufSize then
        fd_clr_read(s)
    end
end

local function stream_on_read(s)
    -- 循环读直到 EAGAIN，充分消费内核缓冲区
    local parts
    while true do
        local data = s.fd:recv()
        if data == nil then
            -- EOF / 连接关闭
            if parts then
                s.readbuf = s.readbuf .. table.concat(parts)
                parts = nil
            end
            stream_dispatch_read(s)
            close_read(s)
            return
        elseif data == false then
            -- EAGAIN，本轮读完
            break
        else
            if parts then
                parts[#parts + 1] = data
            else
                parts = { data }
            end
        end
    end
    if parts then
        s.readbuf = s.readbuf .. table.concat(parts)
    end
    stream_dispatch_read(s)
end

local function stream_on_write(s)
    -- 首次 EPOLLOUT 意味着 connect 完成
    s.connected = true
    while #s.wait_write > 0 do
        local data = s.wait_write[1]
        local n, err = s.fd:send(data[1])
        if n == nil then
            for i, token in ipairs(s.wait_write) do
                ltask.interrupt(token, err or "Write close.")
                s.wait_write[i] = nil
            end
            close_write(s)
            return
        elseif n == false then
            return
        else
            if n == #data[1] then
                local token = table.remove(s.wait_write, 1)
                ltask.wakeup(token, n)
                if #s.wait_write == 0 then
                    fd_clr_write(s)
                    return
                end
            else
                data[1] = data[1]:sub(n + 1)
                return
            end
        end
    end
end

local function create_stream(newfd, connected)
    local s = {
        fd = newfd,
        readbuf = "",
        wait_read = {},
        wait_write = {},
        shutdown_r = false,
        shutdown_w = false,
        r = false,
        w = false,
        event_flags = 0,
        connected = connected,
        on_read = stream_on_read,
        on_write = stream_on_write,
    }
    status[newfd] = s
    newfd:option("nodelay", 1)
    fd_init(newfd)
    fd_set_read(s)
    return create_handle(newfd)
end

local S = {}

function S.listen(protocol, ...)
    local fd, err = socket.create(protocol)
    if not fd then
        return nil, err
    end
    local ok, err = fd:bind(...)
    if not ok then
        return nil, err
    end
    ok, err = fd:listen()
    if not ok then
        return nil, err
    end
    status[fd] = {
        fd = fd,
        shutdown_r = false,
        shutdown_w = true,
        r = false,
        w = false,
        event_flags = 0,
    }
    fd_init(fd)
    return create_handle(fd)
end

function S.connect(protocol, host, port)
    -- 如果传入了 host 和 port，先通过 DNS 解析获取 endpoint
    -- 然后根据解析结果的地址族自动选择正确的协议（tcp/tcp6）
    local ep
    if host and port then
        ep = socket.endpoint("hostname", host, port)
        if not ep then
            return nil, string.format("resolve hostname failed: %s:%d", host, port)
        end
        -- 根据 endpoint 的地址族选择正确的协议
        local _, _, family = ep:value()
        if family == "inet6" then
            if protocol == "tcp" then
                protocol = "tcp6"
            elseif protocol == "udp" then
                protocol = "udp6"
            end
        end
    end
    local fd, err = socket.create(protocol)
    if not fd then
        return nil, err
    end
    local r, err
    if ep then
        r, err = fd:connect(ep)
    else
        r, err = fd:connect(host, port)
    end
    if r == nil then
        return nil, err
    end
    -- r == true：连接立即成功（loopback 可能）；r == false：EINPROGRESS
    return create_stream(fd, r == true)
end

function S.accept(h)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    s.on_read = ltask.wakeup
    fd_set_read(s)
    ltask.wait(s)
    local newfd, err = fd:accept()
    if not newfd then
        return nil, err
    end
    local ok, err = newfd:status()
    if not ok then
        return nil, err
    end
    return create_stream(newfd, true)
end

function S.send(h, data)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    if not s.wait_write then
        error "Write not allowed."
        return
    end
    if s.shutdown_w then
        return
    end
    if data == "" then
        return 0
    end
    -- 队列非空或连接尚未就绪时直接排队，保证有序
    if #s.wait_write > 0 or not s.connected then
        local token = { data }
        s.wait_write[#s.wait_write + 1] = token
        fd_set_write(s)
        return ltask.wait(token)
    end
    -- 乐观写：直接尝试发送，避免不必要的 epoll 往返
    local n, err = s.fd:send(data)
    if n == nil then
        -- 连接出错
        close_write(s)
        return nil, err
    elseif n == false then
        -- EAGAIN：内核缓冲区满，挂起等待 EPOLLOUT
        local token = { data }
        s.wait_write[#s.wait_write + 1] = token
        fd_set_write(s)
        return ltask.wait(token)
    elseif n < #data then
        -- 部分写入：剩余数据入队等 EPOLLOUT
        local token = { data:sub(n + 1) }
        s.wait_write[#s.wait_write + 1] = token
        fd_set_write(s)
        return ltask.wait(token)
    else
        -- 全部写完，直接返回
        return n
    end
end

function S.recv(h, n)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    if not s.readbuf then
        error "Read not allowed."
        return
    end
    if s.shutdown_r then
        if not n then
            if s.readbuf == "" then
                return
            end
        else
            if n > #s.readbuf then
                return
            end
        end
    end
    local sz = #s.readbuf
    if not n then
        if sz == 0 then
            local token = {
            }
            s.wait_read[#s.wait_read + 1] = token
            return ltask.wait(token)
        end
        local ret = s.readbuf
        if sz > kMaxReadBufSize then
            fd_set_read(s)
        end
        s.readbuf = ""
        return ret
    else
        if n <= sz then
            local ret = s.readbuf:sub(1, n)
            if sz > kMaxReadBufSize and sz - n <= kMaxReadBufSize then
                fd_set_read(s)
            end
            s.readbuf = s.readbuf:sub(n + 1)
            return ret
        else
            local token = { n }
            s.wait_read[#s.wait_read + 1] = token
            return ltask.wait(token)
        end
    end
end

function S.close(h)
    local fd = handle[h]
    if fd then
        local s = status[fd]
        close_read(s)
        if not s.shutdown_w then
            local token = {}
            if s.wait_close then
                s.wait_close[#s.wait_close + 1] = token
            else
                s.wait_close = { token }
            end
            ltask.wait(token)
        end
        handle[h] = nil
        handle[fd] = nil
        status[fd] = nil
    end
end

function S.is_closed(h)
    local fd = handle[h]
    if fd then
        local s = status[fd]
        return s.shutdown_w and s.shutdown_r
    end
end

local fd_mt = {}
fd_mt.__index = fd_mt

function fd_mt:accept(...)
    local fd, err = ltask.call("accept", self.fd, ...)
    if not fd then
        return nil, err
    end
    return setmetatable({ fd = fd }, fd_mt)
end

function fd_mt:send(...)
    return ltask.call("send", self.fd, ...)
end

function fd_mt:recv(...)
    return ltask.call("recv", self.fd, ...)
end

function fd_mt:close(...)
    return ltask.call("close", self.fd, ...)
end

function fd_mt:is_closed(...)
    return ltask.call("is_closed", self.fd, ...)
end

local net = {}

function net.wait(timeout)
    for f, event in epfd:wait(timeout) do
        f(event)
    end
end

function net.listen(...)
    local fd, err = ltask.call("listen", ...)
    if not fd then
        return nil, err
    end
    return setmetatable({ fd = fd }, fd_mt)
end

function net.connect(...)
    local fd, err = ltask.call("connect", ...)
    if not fd then
        return nil, err
    end
    return setmetatable({ fd = fd }, fd_mt)
end

net.fork = ltask.fork
net.schedule = ltask.schedule
net.yield = ltask.yield

ltask.dispatch(S)

return net
