---@meta bee.sys

---系统工具库
---@class bee.sys
local sys = {}

---获取当前可执行文件的路径
---@return bee.fspath? # 路径对象
---@return string? # 错误消息
function sys.exe_path()
end

---获取当前动态库的路径
---@return bee.fspath? # 路径对象
---@return string? # 错误消息
function sys.dll_path()
end

---创建文件锁
---如果文件已被锁定，返回nil
---锁会在文件句柄关闭时自动释放
---@param path string|bee.fspath 锁文件路径
---@return file*? # 文件句柄（同时也是锁）
---@return string? # 错误消息
function sys.filelock(path)
end

---获取文件的完整路径
---会解析符号链接等
---@param path string|bee.fspath 文件路径
---@return bee.fspath? # 完整路径
---@return string? # 错误消息
function sys.fullpath(path)
end

return sys
