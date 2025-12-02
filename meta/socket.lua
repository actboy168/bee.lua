---@meta bee.socket

---套接字库
---@class bee.socket
local socket = {}

---网络端点对象
---@class bee.endpoint
local endpoint = {}

---获取端点的值
---对于 inet/inet6：返回 IP地址, 端口号
---对于 unix：返回 路径, 类型
---@return string 地址或路径
---@return integer 端口号或类型
function endpoint:value()
end

---套接字对象
---@class bee.socket.fd
local fd = {}

---连接到指定地址
---@param address string|bee.endpoint 地址（可以是IP:端口、主机名:端口或Unix套接字路径）
---@param port? integer 端口号（当address为字符串时需要）
---@return boolean? 成功返回true/false(等待中)，失败返回nil
---@return string? 错误消息
function fd:connect(address, port)
end

---绑定到指定地址
---@param address string|bee.endpoint 地址
---@param port? integer 端口号
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:bind(address, port)
end

---开始监听连接
---@param backlog? integer 等待队列长度，默认为5
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:listen(backlog)
end

---接受一个连接
---@return bee.socket.fd|boolean|nil 成功返回新套接字，等待中返回false，失败返回nil
---@return string? 错误消息
function fd:accept()
end

---接收数据
---@param len? integer 最大接收长度，默认为缓冲区大小
---@return string|boolean|nil 成功返回数据，等待中返回false，连接关闭返回nil
---@return string? 错误消息
function fd:recv(len)
end

---发送数据
---@param data string 要发送的数据
---@return integer|boolean|nil 成功返回发送的字节数，等待中返回false，失败返回nil
---@return string? 错误消息
function fd:send(data)
end

---从UDP套接字接收数据
---@param len? integer 最大接收长度，默认为缓冲区大小
---@return string|boolean|nil data 成功返回数据，等待中返回false，失败返回nil
---@return bee.endpoint|string? endpoint_or_error 成功返回发送方端点，失败返回错误消息
function fd:recvfrom(len)
end

---通过UDP套接字发送数据到指定地址
---@param data string 要发送的数据
---@param address string|bee.endpoint 目标地址
---@param port? integer 端口号
---@return integer|boolean|nil 成功返回发送的字节数，等待中返回false，失败返回nil
---@return string? 错误消息
function fd:sendto(data, address, port)
end

---关闭套接字的读/写方向
---@param how? "r"|"w" 关闭方向：r=读，w=写，默认关闭双向
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:shutdown(how)
end

---获取套接字状态
---检查连接是否成功建立
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:status()
end

---获取套接字信息
---@param which "peer"|"socket" 获取对端信息还是本端信息
---@return bee.endpoint? 端点对象
---@return string? 错误消息
function fd:info(which)
end

---设置套接字选项
---@param opt "reuseaddr"|"sndbuf"|"rcvbuf" 选项名称
---@param value integer 选项值
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:option(opt, value)
end

---获取套接字的原始句柄
---@return lightuserdata 原始文件描述符
function fd:handle()
end

---分离套接字，返回原始句柄并释放所有权
---@return lightuserdata 原始文件描述符
function fd:detach()
end

---关闭套接字
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function fd:close()
end

---创建套接字
---@param protocol "tcp"|"udp"|"unix"|"tcp6"|"udp6" 协议类型
---@return bee.socket.fd? 套接字对象
---@return string? 错误消息
function socket.create(protocol)
end

---创建端点对象
---@param type "unix" 端点类型
---@param path string Unix套接字路径
---@return bee.endpoint? 端点对象
---@overload fun(type: "hostname", name: string, port: integer): bee.endpoint?
---@overload fun(type: "inet", ip: string, port: integer): bee.endpoint?
---@overload fun(type: "inet6", ip: string, port: integer): bee.endpoint?
function socket.endpoint(type, path)
end

---创建一对已连接的套接字
---@return bee.socket.fd? fd1 第一个套接字
---@return bee.socket.fd|string? fd2_or_error 第二个套接字或错误消息
function socket.pair()
end

---从原始文件描述符创建套接字对象
---@param handle lightuserdata 原始文件描述符
---@param no_ownership? boolean 如果为true，不接管所有权（不会自动关闭）
---@return bee.socket.fd 套接字对象
function socket.fd(handle, no_ownership)
end

---获取主机名
---@return string? 主机名
---@return string? 错误消息
function socket.gethostname()
end

return socket
