---
name: bee-crash
description: 用 bee.crash 安装崩溃处理器、在进程崩溃时落 dump 文件（create_handler、dump 路径与 "-" 关闭落盘）。当需要捕获 native crash 现场、生成崩溃报告或想显式关闭 dump 写入时使用。
---

# bee.crash

`require "bee.crash"`，对应 `meta/crash.lua`、`binding/lua_crash.cpp`、`test/test.lua`。

## API

```lua
local crash = require "bee.crash"

local handler <close> = crash.create_handler(dump_path)   --> handler userdata
```

- `dump_path` 是**目录**：崩溃日志写成 `<dump_path>/crash_<nanoid>.log`。
- `dump_path` 传 `"-"` 时**关闭落盘**（崩溃时只把日志打印到控制台），测试入口就是这么用的：

```lua
-- test/test.lua
local crash = require "bee.crash"
local _ = crash.create_handler "-"
```

- handler 的 userdata 没有额外方法，靠 `<close>` / GC 管理生命周期。

## 注意事项

- 只在 Windows + MSVC（且非 address sanitizer）构建下真正生效，其他平台是 `empty_handler`，构造调用是 **no-op**（见 `bee/crash/handler.h`）。因此跨平台代码可以无条件调用。
- 路径是 `luaL_checkstring`，必须传字符串；非 Windows 平台不会校验路径是否存在。
- 用途是捕获 native 层崩溃（段错误、未处理异常），Lua 的 `pcall` 错误栈不在其覆盖范围内。
- 需要在崩溃后分析时，把 `dump_path` 指向可写目录并在测试/CI 里收集该目录；不希望生成文件时用 `"-"`。
