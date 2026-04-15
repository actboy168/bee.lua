---@meta bee.async

---异步I/O库
---跨平台的异步I/O API，在macOS上使用GCD实现，在Windows上使用IOCP实现，在Linux上使用io_uring/epoll实现
---@class bee.async
---@field SUCCESS integer 操作成功
---@field CLOSE integer 连接关闭
---@field ERROR integer 操作错误
---@field CANCEL integer 操作取消
---@field OP_READ integer 流式读操作
---@field OP_READV integer 流式读操作（ring buffer 回绕时双段）
---@field OP_WRITE integer 单次写操作
---@field OP_WRITEV integer writebuf 写操作
---@field OP_ACCEPT integer accept 操作
---@field OP_CONNECT integer connect 操作
---@field OP_FILE_READ integer 文件读操作
---@field OP_FILE_WRITE integer 文件写操作
---@field OP_POLL integer poll 操作
local async = {}

---异步I/O实例对象
---@class bee.async.fd
local asfd = {}

---提交流式异步读操作（使用 ring buffer，自动处理回绕）
---若 ring buffer 空闲不足（背压）则不投递，返回 false。
---当空闲区域跨越缓冲区末尾时（回绕场景），自动拆分为两段一次提交，
---减少系统调用次数；无回绕时等同于单段读取。
---回绕时 completion 的 op 为 OP_READV，无回绕时为 OP_READ。
---底层 submit 系统调用失败时返回 nil, err。
---@param rb bee.async.readbuf 接收缓冲区对象
---@param fd bee.socket.fd socket 对象
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功投递返回true，背压返回false，系统调用失败返回nil
---@return string? # 系统调用失败时的错误消息
function asfd:submit_read(rb, fd, udata)
end

---提交 writebuf 异步写操作
---将 wb 队列中的所有数据写入 fd。C 层自动 drain（包括 partial write 重试），
---队列清空后产生一次 Lua-visible completion（bytes=0）。
---若 wb 为空或已有 in-flight 请求，则直接返回 true 不投递。
---@param wb bee.async.writebuf 写缓冲区对象
---@param fd bee.socket.fd socket 对象
---@param udata any 用户自定义数据，队列清空时作为 completion 的 udata 返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_write(wb, fd, udata)
end

---提交异步accept操作
---@param listen_fd bee.socket.fd 监听 socket 对象
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_accept(listen_fd, udata)
end

---提交异步connect操作
---@param fd bee.socket.fd socket 对象
---@param host string 目标主机名或IP地址
---@param port integer 目标端口号
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
---@overload fun(self: bee.async.fd, fd: bee.socket.fd, ep: bee.socket.endpoint, udata: any): boolean?, string?
function asfd:submit_connect(fd, host, port, udata)
end

---将文件关联到当前异步I/O实例（仅 Windows/IOCP）
---Windows 下会就地替换传入文件对象的底层句柄为 overlapped/IOCP 关联句柄
---必须在首次提交文件 I/O 操作之前调用；非 Windows 平台为 no-op 并直接返回 true
---@param fd file* 原始文件对象（通过 io.open 获取；调用后原对象被就地更新）
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:associate_file(fd)
end

---提交异步文件读操作
---@param fd file* 文件对象
---@param len integer 读取长度
---@param offset? integer 文件偏移量，默认为0
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_file_read(fd, len, offset, udata)
end

---提交异步文件写操作
---@param fd file* 文件对象
---@param data string 要写入的数据
---@param offset? integer 文件偏移量，默认为0
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_file_write(fd, data, offset, udata)
end

---提交异步 poll 操作
---仅监听 fd 的可读性，不消费任何数据。当 fd 变为可读时产生一次 completion。
---适用于监听 channel 的 ev fd，收到通知后由调用方自行调用 channel:pop() 消费数据。
---@param fd bee.socket.fd socket 对象（或 channel:fd() 返回的 fd）
---@param udata any 用户自定义数据，completion 时原样返回
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function asfd:submit_poll(fd, udata)
end

---将 socket 关联到当前异步I/O实例（仅 Windows/IOCP）
---必须在首次提交任何 I/O 操作之前调用
---@param fd bee.socket.fd socket 对象
---@return boolean # 成功返回true，失败返回false
function asfd:associate(fd)
end

---取消指定 socket 上的所有待处理 I/O 操作（仅 Windows/IOCP）
---通常在关闭 socket 前调用，以确保所有 overlapped 操作及时完成
---@param fd bee.socket.fd socket 对象
function asfd:cancel(fd)
end

---轮询已完成的I/O事件（非阻塞）
---accept 操作完成时第四个返回值为新的 socket userdata，file_read 完成时为读取到的字符串数据，其他操作为 bytes_transferred
---@return fun(): integer, any, integer, integer|bee.socket.fd|string, integer # 迭代器，产生 (op, udata, status, bytes_transferred|accepted_socket|read_data, error_code)
function asfd:poll()
end

---等待已完成的I/O事件（阻塞）
---accept 操作完成时第四个返回值为新的 socket userdata，file_read 完成时为读取到的字符串数据，其他操作为 bytes_transferred
---@param timeout? integer 超时时间，单位为毫秒，-1表示无限等待
---@return fun(): integer, any, integer, integer|bee.socket.fd|string, integer # 迭代器，产生 (op, udata, status, bytes_transferred|accepted_socket|read_data, error_code)
function asfd:wait(timeout)
end

---停止异步实例
---@return boolean
function asfd:stop()
end

---创建写缓冲区对象
---@param hwm? integer 高水位阈值（字节数），默认 65536
---@return bee.async.writebuf
function async.writebuf(hwm)
end

---写缓冲区对象
---由 async.writebuf() 创建，通过 wb:write / asfd:submit_write 使用
---@class bee.async.writebuf
local writebuf = {}

---将数据追加到写缓冲区
---@param data string 要发送的数据
---@return boolean # true 表示缓冲字节数 >= hwm（调用方应在 Lua 侧背压等待）
function writebuf:write(data)
end

---返回当前缓冲队列中的总字节数
---@return integer
function writebuf:buffered()
end

---释放队列中所有待发字符串（在流关闭时调用）
function writebuf:close()
end


---@param bufsize integer 缓冲区大小（会向上取整到最近的2的幂）
---@return bee.async.readbuf
function async.readbuf(bufsize)
end

---接收缓冲区对象（ring buffer）
---由 async.readbuf() 创建，通过 submit_read / rb:read() 使用
---@class bee.async.readbuf
local readbuf = {}

---从 ring buffer 读取数据
---@param n? integer 读取字节数，nil 表示读取全部可用数据
---@return string? # 成功返回数据字符串，数据不足返回 nil
function readbuf:read(n)
end

---从 ring buffer 读取一行（含末尾分隔符）
---@param sep? string 行分隔符，默认为 "\r\n"
---@return string? # 找到分隔符则返回该行（含分隔符），否则返回 nil
function readbuf:readline(sep)
end

---创建异步I/O实例
---@param max_completions? integer 最大完成事件数量，默认为64
---@return bee.async.fd? # 异步I/O实例
---@return string? # 错误消息
function async.create(max_completions)
end

return async
