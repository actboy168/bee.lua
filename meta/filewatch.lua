---@meta bee.filewatch

---文件监控库
---@class bee.filewatch
local filewatch = {}

---文件监控器对象
---@class bee.filewatch.watch
local watch = {}

---添加监控路径
---@param path string 要监控的路径（会自动转换为绝对路径）
function watch:add(path)
end

---设置是否递归监控子目录
---@param enable boolean 是否启用递归监控
---@return boolean 是否成功
function watch:set_recursive(enable)
end

---设置是否跟踪符号链接
---@param enable boolean 是否跟踪符号链接
---@return boolean 是否成功（某些平台可能不支持）
function watch:set_follow_symlinks(enable)
end

---设置过滤器函数
---过滤器接收路径字符串，返回true表示接受该事件
---@param filter? fun(path: string): boolean 过滤函数，nil则清除过滤器
---@return boolean 是否成功
function watch:set_filter(filter)
end

---检查并获取文件变更事件
---@return string|nil type 事件类型："modify"（修改）或"rename"（重命名），无事件返回nil
---@return string|nil path 发生变更的文件路径
function watch:select()
end

---创建文件监控器
---@return bee.filewatch.watch 监控器对象
function filewatch.create()
end

return filewatch
