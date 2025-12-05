---@meta bee.windows

---Windows 平台特定功能库
---@class bee.windows
local windows = {}

---将 UTF-8 字符串转换为 ANSI (GBK) 编码
---@param str string UTF-8 编码的字符串
---@return string # ANSI (GBK) 编码的字符串
function windows.u2a(str)
end

---将 ANSI (GBK) 字符串转换为 UTF-8 编码
---@param str string ANSI (GBK) 编码的字符串
---@return string # UTF-8 编码的字符串
function windows.a2u(str)
end

---设置文件的文本/二进制模式
---@param file file* 文件句柄
---@param mode string 模式字符串，'t' 表示文本模式，'b' 表示二进制模式
---@return boolean # 是否设置成功
function windows.filemode(file, mode)
end

---判断文件句柄是否为 TTY (终端)
---@param file file* 文件句柄
---@return boolean # 是否为终端
function windows.isatty(file)
end

---向 Windows 控制台写入内容
---使用 WriteConsoleW API，能正确处理 UTF-16 编码
---@param file file* 文件句柄（通常为 io.stdout 或 io.stderr）
---@param msg string 要写入的消息
---@return integer # 写入的字符数
function windows.write_console(file, msg)
end

---检测磁盘驱动器是否为 SSD
---@param drive string 驱动器名称，例如 "C:" 或 "C"
---@return boolean # 是否为 SSD
function windows.is_ssd(drive)
end

return windows
