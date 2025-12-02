---@meta bee.crash

---崩溃处理库
---@class bee.crash
local crash = {}

---崩溃处理器对象
---@class bee.crash.handler
local handler = {}

---创建崩溃处理器
---当程序崩溃时，会在指定路径生成dump文件
---@param dump_path string dump文件保存路径
---@return bee.crash.handler 崩溃处理器对象
function crash.create_handler(dump_path)
end

return crash
