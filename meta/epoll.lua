---@meta bee.epoll

---Epoll I/O多路复用库
---跨平台的epoll风格API，在Windows上使用IOCP实现
---@class bee.epoll
---@field EPOLLIN integer 可读事件
---@field EPOLLPRI integer 紧急数据可读
---@field EPOLLOUT integer 可写事件
---@field EPOLLERR integer 错误事件
---@field EPOLLHUP integer 挂起事件
---@field EPOLLRDNORM integer 普通数据可读
---@field EPOLLRDBAND integer 优先数据可读
---@field EPOLLWRNORM integer 普通数据可写
---@field EPOLLWRBAND integer 优先数据可写
---@field EPOLLMSG integer 消息事件
---@field EPOLLRDHUP integer 对端关闭连接
---@field EPOLLONESHOT integer 一次性事件（触发后自动删除）
local epoll = {}

---Epoll实例对象
---@class bee.epoll.fd
local epfd = {}

---等待事件
---@param timeout? integer 超时时间，单位为毫秒，-1表示无限等待
---@return fun(): any?, integer? iterator 返回迭代器函数，迭代产生 (关联对象, 事件标志)
function epfd:wait(timeout)
end

---关闭Epoll实例
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function epfd:close()
end

---添加事件监听
---@param fd bee.socket.fd|lightuserdata 要监听的文件描述符或套接字
---@param events integer 事件标志，可组合多个EPOLL*常量
---@param userdata? any 关联的用户数据，默认为fd本身
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function epfd:event_add(fd, events, userdata)
end

---修改事件监听
---@param fd bee.socket.fd|lightuserdata 文件描述符或套接字
---@param events integer 新的事件标志
---@param userdata? any 新的关联用户数据
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function epfd:event_mod(fd, events, userdata)
end

---删除事件监听
---@param fd bee.socket.fd|lightuserdata 文件描述符或套接字
---@return boolean? 成功返回true，失败返回nil
---@return string? 错误消息
function epfd:event_del(fd)
end

---创建Epoll实例
---@param max_events integer 最大事件数量（必须大于0）
---@return bee.epoll.fd? Epoll实例对象
---@return string? 错误消息
function epoll.create(max_events)
end

return epoll
