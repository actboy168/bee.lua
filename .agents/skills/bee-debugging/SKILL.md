---
name: bee-debugging
description: 用 bee.debugging 触发断点与探测调试器（breakpoint、breakpoint_if_debugging、is_debugger_present）。当需要让调试器在指定位置中断、或按是否挂调试器切换行为时使用。
---

# bee.debugging

`require "bee.debugging"`，对应 `meta/debugging.lua`、`binding/lua_debugging.cpp`。底层是 `std::breakpoint()` / `std::is_debugger_present()`。

## API

```lua
local debugging = require "bee.debugging"

debugging.breakpoint()                 -- 无条件断点（无调试器时会走平台默认的 trap/SIGTRAP 语义）
debugging.is_debugger_present()        --> boolean
debugging.breakpoint_if_debugging()    -- 仅当有调试器附加时才中断，否则 no-op（安全版本）
```

## 用法

想在调试器里断下来，但不想让正常运行时崩溃，用 `breakpoint_if_debugging`：

```lua
local debugging = require "bee.debugging"

if debugging.is_debugger_present() then
    -- 调试模式下走额外校验
end

debugging.breakpoint_if_debugging()    -- 挂调试器则中断，否则什么都不发生
```

## 注意事项

- 这是 C/C++ 层的原生断点，不是 Lua 的 `debug.sethook`；在 VS/VSCode 附加进程时会停在 native 调用栈上。
- `breakpoint()` 在**没有**调试器附加时行为由平台决定（通常是触发异常/trap），生产代码里应优先用 `breakpoint_if_debugging()`。
- `is_debugger_present()` 也可用于按环境切换日志级别。
- 本模块目前没有独立测试文件。
