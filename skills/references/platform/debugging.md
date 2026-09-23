# bee.debugging

断点与调试器探测。签名见 `meta/debugging.lua`，实现在 `binding/lua_debugging.cpp` + `bee/nonstd/debugging.h`。本模块目前没有独立测试文件。

## 要点

- 这是 C/C++ 层的原生断点，不是 Lua 的 `debug.sethook`；在 VS/VSCode 附加进程时会停在 native 调用栈上。
- `breakpoint()` **不判断**是否有调试器；`breakpoint_if_debugging()` 才是「有调试器才断」的安全版本。
- `is_debugger_present()`：Windows 用 `IsDebuggerPresent()`，macOS 用 `sysctl` 的 `P_TRACED`，其他平台恒返回 `false`（于是 `breakpoint_if_debugging()` 也恒为 no-op）。

`breakpoint()` 的底层实现依编译器而定：

| 构建环境 | 实现 |
|----------|------|
| 有 `<debugging>`（C++26 `__cpp_lib_debugging`） | `std::breakpoint()` |
| MSVC（无 `<debugging>`） | `__debugbreak()` |
| clang（无 `<debugging>`） | `__builtin_debugtrap()` |
| 其它（如 GCC，无 `<debugging>`） | 函数体为空，**no-op** |

在实现了 trap 的构建环境里，没有调试器附加时断点异常会交给系统默认处理器（可能直接终止进程）；没有 trap 实现的分支里则什么都不发生。所以生产代码里优先用 `breakpoint_if_debugging()`。

## 用法

```lua
local debugging = require "bee.debugging"

if debugging.is_debugger_present() then
    -- 调试模式下走额外校验
end

debugging.breakpoint_if_debugging()    -- 挂调试器则中断，否则什么都不发生
```

## 注意事项

- `is_debugger_present()` 也可以用来按环境切换日志级别——比自定义开关更可靠。
- 想让 Lua 层停下来看调用栈，请用 `debug.sethook` / 调试器；本模块只处理 native 断点。
