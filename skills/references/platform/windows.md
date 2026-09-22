# bee.windows

`require "bee.windows"`，对应 `meta/windows.lua`、`test/test_windows.lua`。**仅 Windows 可用**，其他平台 `require` 会失败，需自行按 `platform.os` 分支或 pcall。

## API

| 函数 | 说明 |
|------|------|
| `windows.u2a(str)` | UTF-8 → ANSI(GBK) |
| `windows.a2u(str)` | ANSI(GBK) → UTF-8 |
| `windows.filemode(file, mode)` | 设置文本/二进制模式，`"t"` 文本、`"b"` 二进制，返回 `boolean` |
| `windows.isatty(file)` | 句柄是否为终端，返回 `boolean` |
| `windows.write_console(file, msg)` | 用 `WriteConsoleW` 写控制台，正确输出 UTF-16，返回写入字符数 |
| `windows.is_ssd(drive)` | 驱动器是否 SSD，`drive` 形如 `"C:"` 或 `"C"` |
| `windows.find_file_holders(filepath)` | 通过 NT API 枚举句柄表，返回占用该文件的 PID 数组 |
| `windows.process_name(pid)` | PID → 进程名（如 `"notepad.exe"`），失败返回空字符串 |

## 用法

编码转换与终端输出：

```lua
local windows = require "bee.windows"

local ansi = windows.u2a "中文"      -- GBK 字节串，可交给只认 ANSI 的 API
local utf8 = windows.a2u(ansi)

if windows.isatty(io.stdout) then
    windows.write_console(io.stdout, "中文\n")   -- 避免 CRT 编码问题
end
```

管道/文件读写时的模式控制（`test_subprocess` 里用它关掉 CRLF 转换）：

```lua
windows.filemode(io.stdin, "b")
assert(io.read "a" == "\r\n")        -- 二进制模式下读到原始换行
```

排查文件占用：

```lua
local pids = windows.find_file_holders "d:/build/out.exe"
for _, pid in ipairs(pids) do
    print(pid, windows.process_name(pid))
end
```

## 注意事项

- `windows.is_ssd` 的参数是驱动器名，`"C"` 与 `"C:"` 都接受。
- `windows.find_file_holders` 需要相应权限，且只列出**当前进程可见**的句柄持有者。
- 遇到含代理对/生僻字的路径（WTF-8 场景），Lua 层字符串是 UTF-8 编码，文件 API 由库内部转换，测试 `test_windows:test_wtf8` 覆盖了 `io.open` 写这种文件名。
- 非 Windows 平台不要 `require` 本模块。
