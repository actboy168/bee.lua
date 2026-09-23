# bee.windows

Windows 专有工具。签名见 `meta/windows.lua`，行为契约见 `test/test_windows.lua`。

**仅 Windows 可用**：其他平台 `require` 会失败，按 `platform.os` 分支或 `pcall` 包裹。

## 要点

- 编码：`u2a` / `a2u` 在 UTF-8 与 ANSI(GBK) 之间转换，只认 ANSI 的老 API 用得上。
- 控制台：`isatty(file)` 判断句柄是不是终端；`write_console(file, msg)` 走 `WriteConsoleW`，能正确输出 UTF-16，避开 CRT 编码问题。
- 文本模式：`filemode(file, "t"|"b")` 切 CRT 的换行转换，二进制模式下 `io.read "a"` 才会拿到原始 `\r\n`。
- 排查占用：`find_file_holders(filepath)` 通过 NT API 枚举句柄表返回 PID 数组，`process_name(pid)` 把 PID 换成进程名（失败返回空串）。
- `is_ssd(drive)` 判断驱动器是否 SSD，`"C"` 与 `"C:"` 都接受。

## 用法

```lua
local windows = require "bee.windows"

local ansi = windows.u2a "中文"      -- GBK 字节串
local utf8 = windows.a2u(ansi)

if windows.isatty(io.stdout) then
    windows.write_console(io.stdout, "中文\n")
end

windows.filemode(io.stdin, "b")      -- 关掉 CRLF 转换（test_subprocess 里这么用）

local pids = windows.find_file_holders "d:/build/out.exe"
for _, pid in ipairs(pids) do
    print(pid, windows.process_name(pid))
end
```

## 注意事项

- `find_file_holders` 需要相应权限，且只列出**当前进程可见**的句柄持有者。
- 含代理对/生僻字的路径（WTF-8 场景）：Lua 层字符串仍是 UTF-8，转换由库内部处理，`test_windows:test_wtf8` 覆盖了用这种文件名 `io.open`。
- 非 Windows 平台不要 `require` 本模块。
