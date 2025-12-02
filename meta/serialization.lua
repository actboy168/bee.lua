---@meta bee.serialization

---序列化库
---用于在线程间传递复杂数据结构
---@class bee.serialization
local serialization = {}

---将数据序列化并返回轻量用户数据指针
---支持的类型：nil, boolean, number, string, table, light C function
---不支持的类型：function(非light C function), thread, userdata
---@param ... any 要序列化的数据
---@return lightuserdata # 序列化后的数据指针（需要调用unpack释放）
function serialization.pack(...)
end

---将数据序列化为字符串
---支持的类型与pack相同
---@param ... any 要序列化的数据
---@return string # 序列化后的字符串
function serialization.packstring(...)
end

---反序列化数据
---@param data lightuserdata|string|userdata|function 序列化的数据
---@return any ... # 反序列化后的数据
function serialization.unpack(data)
end

---将userdata转换为lightuserdata
---@param ud userdata 用户数据
---@return lightuserdata # 轻量用户数据
function serialization.lightuserdata(ud)
end

return serialization
