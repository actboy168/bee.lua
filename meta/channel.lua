---@meta bee.channel

---通道库，用于线程间通信
---@class bee.channel
local channel = {}

---通道对象
---@class bee.channel.box
local channel_box = {}

---向通道推送数据
---数据会被序列化后发送，支持的数据类型与 bee.serialization 相同
---@param ... any 要推送的数据
function channel_box:push(...)
end

---从通道弹出数据
---@return boolean ok # 是否成功弹出数据，false表示通道为空
---@return any ... # 弹出的数据
function channel_box:pop()
end

---获取通道的文件描述符
---可用于 epoll/select 等待通道有数据
---@return lightuserdata fd # 文件描述符
function channel_box:fd()
end

---创建一个新的通道
---@param name string 通道名称，必须唯一
---@return bee.channel.box # 通道对象
function channel.create(name)
end

---销毁一个通道
---会清空通道中的所有数据
---@param name string 通道名称
function channel.destroy(name)
end

---查询一个已存在的通道
---@param name string 通道名称
---@return bee.channel.box? # 通道对象，如果不存在则返回nil
---@return string? # 错误消息
function channel.query(name)
end

return channel
