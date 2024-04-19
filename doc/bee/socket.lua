---@meta bee.socket

local socket = {}

---@alias bee.protocol
---| "tcp"
---| "udp"
---| "unix"
---| "tcp6"
---| "udp6"

---@alias bee.endpoint_ctor
---| "unix"
---| "hostname"
---| "inet"
---| "inet6"

---@alias bee.shutdown_flag
---| "r"
---| "w"

---@alias bee.socket_info
---| "peer"
---| "socket"

---@alias bee.socket_option
---| "reuseaddr"
---| "sndbuf"
---| "rcvbuf"

do
    ---@class bee.endpoint
    local endpoint = {}

    ---@nodiscard
    ---@return string
    ---@return integer
    function endpoint.value()
    end
end

do
    ---@class bee.fd
    local fd = {}

    ---@nodiscard
    ---@overload fun(self, path:string):true|false|nil, string?
    ---@overload fun(self, ip:string, port:integer):true|false|nil, string?
    ---@param ep bee.endpoint
    ---@return true|false|nil
    ---@return string? errmsg
    function fd:connect(ep)
    end

    ---@nodiscard
    ---@overload fun(self, path:string):true|nil, string?
    ---@overload fun(self, ip:string, port:integer):true|nil, string?
    ---@param ep bee.endpoint
    ---@return true|nil
    ---@return string? errmsg
    function fd:bind(ep)
    end

    ---@nodiscard
    ---@param backlog? integer
    ---@return true|nil
    ---@return string? errmsg
    function fd:listen(backlog)
    end

    ---@nodiscard
    ---@return bee.fd|false|nil
    ---@return string? errmsg
    function fd:accept()
    end

    ---@nodiscard
    ---@param n? integer
    ---@return string|false|nil
    ---@return string? errmsg
    function fd:recv(n)
    end

    ---@nodiscard
    ---@param data string
    ---@return integer|false|nil
    ---@return string? errmsg
    function fd:send(data)
    end

    ---@nodiscard
    ---@param n? integer
    ---@return integer|false|nil
    ---@return bee.endpoint|string
    function fd:recvfrom(n)
    end

    ---@nodiscard
    ---@overload fun(self, data:string, path:string):integer|false|nil, string?
    ---@overload fun(self, data:string, ip:string, port:integer):integer|false|nil, string?
    ---@param data string
    ---@param ep bee.endpoint
    ---@return integer|false|nil
    ---@return string? errmsg
    function fd:sendto(data, ep)
    end

    ---@param flag? bee.shutdown_flag
    ---@return true|nil
    ---@return string? errmsg
    function fd:shutdown(flag)
    end

    ---@nodiscard
    ---@return true|nil
    ---@return string? errmsg
    function fd:status()
    end

    ---@nodiscard
    ---@param option bee.socket_info
    ---@return bee.endpoint|nil
    ---@return string? errmsg
    function fd:info(option)
    end

    ---@param opt bee.socket_option
    ---@param value integer
    ---@return true|nil
    ---@return string? errmsg
    function fd:option(opt, value)
    end

    ---@nodiscard
    ---@return lightuserdata
    function fd:handle()
    end

    ---@nodiscard
    ---@return lightuserdata
    function fd:detach()
    end

    ---@return true|nil
    ---@return string? errmsg
    function fd:close()
    end
end

do
    ---@class bee.fd_no_ownership
    local fd = {}

    ---@nodiscard
    ---@overload fun(self, path:string):true|false|nil, string?
    ---@overload fun(self, ip:string, port:integer):true|false|nil, string?
    ---@param ep bee.endpoint
    ---@return true|false|nil
    ---@return string? errmsg
    function fd:connect(ep)
    end

    ---@nodiscard
    ---@overload fun(self, path:string):true|nil, string?
    ---@overload fun(self, ip:string, port:integer):true|nil, string?
    ---@param ep bee.endpoint
    ---@return true|nil
    ---@return string? errmsg
    function fd:bind(ep)
    end

    ---@nodiscard
    ---@param backlog? integer
    ---@return true|nil
    ---@return string? errmsg
    function fd:listen(backlog)
    end

    ---@nodiscard
    ---@return bee.fd|false|nil
    ---@return string? errmsg
    function fd:accept()
    end

    ---@nodiscard
    ---@param n? integer
    ---@return string|false|nil
    ---@return string? errmsg
    function fd:recv(n)
    end

    ---@nodiscard
    ---@param data string
    ---@return integer|false|nil
    ---@return string? errmsg
    function fd:send(data)
    end

    ---@nodiscard
    ---@param n? integer
    ---@return integer|false|nil
    ---@return bee.endpoint|string
    function fd:recvfrom(n)
    end

    ---@nodiscard
    ---@overload fun(self, data:string, path:string):integer|false|nil, string?
    ---@overload fun(self, data:string, ip:string, port:integer):integer|false|nil, string?
    ---@param data string
    ---@param ep bee.endpoint
    ---@return integer|false|nil
    ---@return string? errmsg
    function fd:sendto(data, ep)
    end

    ---@param flag? bee.shutdown_flag
    ---@return true|nil
    ---@return string? errmsg
    function fd:shutdown(flag)
    end

    ---@nodiscard
    ---@return true|nil
    ---@return string? errmsg
    function fd:status()
    end

    ---@nodiscard
    ---@param option bee.socket_info
    ---@return bee.endpoint|nil
    ---@return string? errmsg
    function fd:info(option)
    end

    ---@param opt bee.socket_option
    ---@param value integer
    ---@return true|nil
    ---@return string? errmsg
    function fd:option(opt, value)
    end

    ---@nodiscard
    ---@return lightuserdata
    function fd:handle()
    end

    ---@nodiscard
    ---@return lightuserdata
    function fd:detach()
    end
end

---@nodiscard
---@param protocol bee.protocol
---@return bee.fd?
---@return string? errmsg
function socket.create(protocol)
end

---@nodiscard
---@param ctor bee.endpoint_ctor
---@param str string
---@param port? integer
---@return bee.endpoint
function socket.endpoint(ctor, str, port)
end

---@nodiscard
---@return bee.fd | nil
---@return bee.fd | string
function socket.pair(protocol)
end

---@nodiscard
---@param fd lightuserdata
---@param no_ownership? boolean
---@return bee.fd | bee.fd_no_ownership
function socket.fd(fd, no_ownership)
end

return socket
