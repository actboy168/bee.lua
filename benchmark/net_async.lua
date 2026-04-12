local ltask = require "ltask"
local socket = require "bee.socket"
local async = require "bee.async"

local asfd = async.create(512)

local SUCCESS <const> = async.SUCCESS

local kReadBufSize  <const> = 64 * 1024  -- per-stream ring buffer size
local kWriteHWM     <const> = 64 * 1024  -- write queue high-water-mark (bytes)

local status = {}
local handle = {}

local next_reqid = 1
local function alloc_reqid()
    local id = next_reqid
    next_reqid = id + 1
    return id
end

local pending = {}

-- Pending type constants
local PTYPE_READ    = 1
local PTYPE_ACCEPT  = 2
local PTYPE_CONNECT = 3
local PTYPE_WRITE   = 4

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

local function close_stream(s)
    if s.closed then
        return
    end
    s.closed = true
    if s.wait_read then
        for i, token in ipairs(s.wait_read) do
            ltask.wakeup(token)
            s.wait_read[i] = nil
        end
    end
    if s.wb then
        for _, token in ipairs(s.wait_write) do
            ltask.wakeup(token)
        end
        s.wait_write = {}
        s.wb:close()
        s.wb = nil
    end
    if s.wait_close then
        for _, token in ipairs(s.wait_close) do
            ltask.wakeup(token)
        end
        s.wait_close = nil
    end
    asfd:cancel(s.fd)
    s.fd:close()
end

local function stream_submit_read(s)
    if s.closed then
        return
    end
    while s.read_in_flight < 1 do
        local reqid = alloc_reqid()
        if not asfd:submit_read(s.rb, s.fd, reqid) then
            break
        end
        s.read_in_flight = s.read_in_flight + 1
        pending[reqid] = { PTYPE_READ, s }
    end
end

local function stream_on_read(s, bytes)
    s.read_in_flight = s.read_in_flight - 1
    if not bytes or bytes == 0 then
        close_stream(s)
        return
    end
    while s.wait_read and #s.wait_read > 0 do
        local token = s.wait_read[1]
        local n = token[1]
        local data = s.rb:read(n)
        if not data then
            break
        end
        table.remove(s.wait_read, 1)
        ltask.wakeup(token, data)
    end
    if not s.closed then
        stream_submit_read(s)
    end
end

-- Submit the writebuf to the async layer if not already in-flight.
local function stream_submit_write(s)
    if s.closed or not s.wb or s.wb_in_flight then
        return
    end
    if s.wb:buffered() == 0 then
        return
    end
    local reqid = alloc_reqid()
    if not asfd:submit_write(s.wb, s.fd, reqid) then
        close_stream(s)
        return
    end
    s.wb_in_flight = true
    pending[reqid] = { PTYPE_WRITE, s }
end

-- Called when a submit_write completion arrives (queue fully drained).
local function stream_on_write(s)
    s.wb_in_flight = false
    -- Wake up all blocked writers.
    local ww = s.wait_write
    s.wait_write = {}
    for _, token in ipairs(ww) do
        ltask.wakeup(token)
    end
    -- If more data was enqueued while draining, resubmit.
    if s.wb and s.wb:buffered() > 0 then
        stream_submit_write(s)
    end
end

local function create_stream(newfd)
    local s = {
        fd             = newfd,
        rb             = async.readbuf(kReadBufSize),
        wb             = async.writebuf(kWriteHWM),
        wait_read      = {},
        wait_write     = {},
        wb_in_flight   = false,
        closed         = false,
        read_in_flight = 0,
    }
    status[newfd] = s
    newfd:option("nodelay", 1)
    stream_submit_read(s)
    return create_handle(newfd)
end

local S = {}

function S.listen(protocol, ...)
    local fd, err = socket.create(protocol)
    if not fd then
        return nil, err
    end
    asfd:associate(fd)
    local ok, err = fd:bind(...)
    if not ok then
        return nil, err
    end
    ok, err = fd:listen()
    if not ok then
        return nil, err
    end
    local s = {
        fd = fd,
        closed = false,
        is_listener = true,
    }
    status[fd] = s
    return create_handle(fd)
end

function S.connect(protocol, host, port)
    if host and port then
        local ep = socket.endpoint("hostname", host, port)
        if not ep then
            return nil, string.format("resolve hostname failed: %s:%d", host, port)
        end
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
    asfd:associate(fd)
    local reqid = alloc_reqid()
    local token = {}
    pending[reqid] = { PTYPE_CONNECT, token }
    local ok, cerr = asfd:submit_connect(fd, host, port, reqid)
    if not ok then
        pending[reqid] = nil
        fd:close()
        return nil, cerr
    end
    local result = ltask.wait(token)
    if not result then
        fd:close()
        return nil, "connect failed"
    end
    return create_stream(fd)
end

function S.accept(h)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    assert(s.is_listener, "Not a listener.")
    local reqid = alloc_reqid()
    local token = {}
    pending[reqid] = { PTYPE_ACCEPT, token }
    asfd:submit_accept(fd, reqid)
    local newfd = ltask.wait(token)
    if not newfd then
        return nil, "accept failed"
    end
    local ok, err = newfd:status()
    if not ok then
        newfd:close()
        return nil, err
    end
    return create_stream(newfd)
end

function S.send(h, data)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    if s.closed or not s.wb then
        return
    end
    if data == "" then
        return
    end
    -- Enqueue; wb:write returns true when buffered >= hwm (backpressure).
    local full = s.wb:write(data)
    -- Kick off a submit if nothing is currently in-flight.
    if not s.wb_in_flight then
        stream_submit_write(s)
    end
    -- Block if we pushed the buffer over hwm.
    if full then
        local token = {}
        s.wait_write[#s.wait_write + 1] = token
        ltask.wait(token)
    end
    return true
end

function S.recv(h, n)
    local fd = assert(handle[h], "Invalid fd.")
    local s = status[fd]
    if not s.rb then
        error "Read not allowed."
        return
    end
    local data = s.rb:read(n)
    if data then
        stream_submit_read(s)
        return data
    end
    if s.closed then
        return
    end
    local token = { n }
    s.wait_read[#s.wait_read + 1] = token
    stream_submit_read(s)
    return ltask.wait(token)
end

function S.close(h)
    local fd = handle[h]
    if fd then
        local s = status[fd]
        close_stream(s)
        handle[h] = nil
        handle[fd] = nil
        status[fd] = nil
    end
end

function S.is_closed(h)
    local fd = handle[h]
    if fd then
        return status[fd].closed
    end
    return true
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

local function process_completions(iter)
    for reqid, st, data, _ in iter do
        local p = pending[reqid]
        if not p then
            goto continue
        end
        pending[reqid] = nil
        local ptype = p[1]
        if ptype == PTYPE_READ then
            local s = p[2]
            if st == SUCCESS then
                stream_on_read(s, data)
            else
                stream_on_read(s, nil)
            end
        elseif ptype == PTYPE_WRITE then
            local s = p[2]
            if st == SUCCESS then
                stream_on_write(s)
            else
                close_stream(s)
            end
        elseif ptype == PTYPE_ACCEPT then
            ltask.wakeup(p[2], st == SUCCESS and data or nil)
        elseif ptype == PTYPE_CONNECT then
            ltask.wakeup(p[2], st == SUCCESS and true or nil)
        end
        ::continue::
    end
end

function net.wait(timeout)
    process_completions(asfd:wait(timeout))
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
