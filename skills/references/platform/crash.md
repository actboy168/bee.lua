# bee.crash

崩溃处理器：进程崩溃时落 dump。签名见 `meta/crash.lua`，行为契约见 `test/test.lua`（入口就在用），实现在 `bee/crash/`。

## 要点

- `create_handler(dump_path)` 的 `dump_path` 是**目录**，崩溃日志写成 `<dump_path>/crash_<nanoid>.log`。
- `dump_path` 传 `"-"` 时**关闭落盘**，只在崩溃时把日志打到控制台：

```lua
-- test/test.lua 的用法
local crash = require "bee.crash"
local _ = crash.create_handler "-"
```

- handler 的 userdata 没有额外方法，靠 `<close>` / GC 管理生命周期。

## 注意事项

- 只在 **Windows + MSVC**（且非 address sanitizer）构建下真正生效，其他平台是 `empty_handler`，构造调用是 **no-op**（`bee/crash/handler.h`）。因此跨平台代码可以无条件调用，但别指望在 Linux/macOS 上拿到 dump。
- 参数是 `luaL_checkstring`，必须传字符串；非 Windows 平台不会校验路径是否存在。
- 捕获的是 native 层崩溃（段错误、未处理异常），Lua 层的 `pcall` 错误栈不在覆盖范围。
- 需要在崩溃后分析时，把 `dump_path` 指向可写目录并在 CI 里收集；不想生成文件就用 `"-"`。
