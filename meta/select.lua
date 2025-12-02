---@meta bee.select

---Select I/O多路复用库
---@class bee.select
---@field SELECT_READ integer 读事件标志
---@field SELECT_WRITE integer 写事件标志
local select = {}

---Select上下文对象
---@class bee.select.ctx
local ctx = {}

---等待事件
---@param timeout? integer 超时时间，单位为毫秒，-1表示无限等待
---@return fun(): any?, integer? iterator # 返回迭代器函数，迭代产生 (关联对象, 事件标志)
function ctx:wait(timeout)
end

---关闭Select上下文
function ctx:close()
end

---添加事件监听
---@param fd bee.socket.fd|lightuserdata 要监听的文件描述符或套接字
---@param events integer 事件标志，可组合 SELECT_READ 和 SELECT_WRITE
---@param userdata? any 关联的用户数据，默认为fd本身
---@return boolean # 是否成功
function ctx:event_add(fd, events, userdata)
end

---修改事件监听
---@param fd bee.socket.fd|lightuserdata 文件描述符或套接字
---@param events integer 新的事件标志
---@return boolean # 是否成功
function ctx:event_mod(fd, events)
end

---删除事件监听
---@param fd bee.socket.fd|lightuserdata 文件描述符或套接字
---@return boolean # 是否成功
function ctx:event_del(fd)
end

---创建Select上下文
---@return bee.select.ctx # Select上下文对象
function select.create()
end

return select
