# bee.debugging

`require "bee.debugging"`，对应 `meta/debugging.lua`、`binding/lua_debugging.cpp`。底层是 `std::breakpoint()` / `std::is_debugger_present()`。

## API

```lua
local debugging = require "bee.debugging"

debugging.breakpoint()                 -- 无条件触发断点指令
debugging.is_debugger_present()        --> boolean
debugging.breakpoint_if_debugging()    -- 仅当有调试器附加时才中断，否则 no-op（安全版本）
```

`breakpoint()` 的底层实现依编译器而定（`bee/nonstd/debugging.h`）：

| 构建环境 | 实现 |
|----------|------|
| 有 `<debugging>`（C++26 `__cpp_lib_debugging`） | `std::breakpoint()` |
| MSVC（无 `<debugging>`） | `__debugbreak()` |
| clang（无 `<debugging>`） | `__builtin_debugtrap()` |
| 其它（如 GCC，无 `<debugging>`） | 函数体为空，**no-op** |

`is_debugger_present()`：Windows 用 `IsDebuggerPresent()`，macOS 用 `sysctl` 的 `P_TRACED`，其他平台恒返回 `false`（此时 `breakpoint_if_debugging()` 也就恒为 no-op）。

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
- `breakpoint()` **不判断**是否有调试器：在实现了 trap 的构建环境里（C++26 `std::breakpoint()` / MSVC / clang），没有调试器附加时断点异常会交给系统默认处理器（可能直接终止进程）；而 GCC 等无 trap 实现的分支里它只是 no-op。因此生产代码里应优先用 `breakpoint_if_debugging()`。
- `is_debugger_present()` 也可用于按环境切换日志级别。
- 本模块目前没有独立测试文件。
