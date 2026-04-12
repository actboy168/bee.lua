---@meta bee.async

---异步I/O库
---跨平台的异步I/O API，在macOS上使用GCD实现，在Windows上使用IOCP实现，在Linux上使用io_uring/epoll实现
---@class bee.async
---@field SUCCESS integer 操作成功
---@field CLOSE integer 连接关闭
---@field ERROR integer 操作错误
---@field CANCEL integer 操作取消
local async = {}

---异步I/O实例对象
---@class bee.async.fd
local asfd = {}

---分配一个可写缓冲区（userdata）
---@param size integer 缓冲区大小（字节）
---@return userdata # 可写缓冲区
function asfd:newbuf(size)
end

---提交异步读操作
---@param fd userdata socket 对象（bee.socket）
---@param buffer userdata 接收缓冲区（通过 newbuf 分配）
---@param len integer 缓冲区长度
---@param request_id integer 请求ID，用于标识完成事件
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_read(fd, buffer, len, request_id)
end

---提交异步写操作
---@param fd userdata socket 对象（bee.socket）
---@param data string 要发送的数据
---@param request_id integer 请求ID
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_write(fd, data, request_id)
end

---提交异步accept操作
---@param listen_fd userdata 监听 socket 对象（bee.socket）
---@param request_id integer 请求ID
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_accept(listen_fd, request_id)
end

---提交异步connect操作
---@param fd userdata socket 对象（bee.socket）
---@param host string 目标主机名或IP地址
---@param port integer 目标端口号
---@param request_id integer 请求ID
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_connect(fd, host, port, request_id)
end

---提交异步文件读操作
---@param fd file* 文件对象（通过 io.open 获取）
---@param buffer userdata 接收缓冲区（通过 newbuf 分配）
---@param len integer 读取长度
---@param offset integer 文件偏移量
---@param request_id integer 请求ID
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_file_read(fd, buffer, len, offset, request_id)
end

---提交异步文件写操作
---@param fd file* 文件对象（通过 io.open 获取）
---@param data string 要写入的数据
---@param offset integer 文件偏移量
---@param request_id integer 请求ID
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_file_write(fd, data, offset, request_id)
end

---轮询已完成的I/O事件（非阻塞）
---accept 操作完成时第三个返回值为新的 socket userdata，其他操作为 bytes_transferred
---@return fun(): integer, integer, integer|userdata, integer # 迭代器，产生 (request_id, status, bytes_transferred|accepted_socket, error_code)
function asfd:poll()
end

---等待已完成的I/O事件（阻塞）
---accept 操作完成时第三个返回值为新的 socket userdata，其他操作为 bytes_transferred
---@param timeout? integer 超时时间，单位为毫秒，-1表示无限等待
---@return fun(): integer, integer, integer|userdata, integer # 迭代器，产生 (request_id, status, bytes_transferred|accepted_socket, error_code)
function asfd:wait(timeout)
end

---停止异步实例
---@return boolean
function asfd:stop()
end

---创建异步I/O实例
---@param max_completions? integer 最大完成事件数量，默认为64
---@return bee.async.fd? # 异步I/O实例
---@return string? # 错误消息
function async.create(max_completions)
end

return async
