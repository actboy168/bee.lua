---
name: bee-lua
description: bee.lua 运行时库指南——Lua 5.4/5.5 上的跨平台系统级绑定（异步 I/O、网络、子进程、线程、文件系统、文件监控、崩溃 dump）。当需要在本项目里编写、修改或排查使用 `require "bee.*"` 的 Lua 代码，涉及 socket/select/epoll/async、thread/channel/serialization、subprocess、filesystem/filewatch、time/sys/crash/debugging/windows，或需要按平台差异处理行为时，使用本 skill。即使用户没有明确提到 bee.lua，只要上下文是在调用 `bee` 命名空间下的模块，也应使用。不要用于纯 Lua 语言/标准库问题、与 bee 无关的第三方 C 模块，或本项目 C++ 层 `bee/`、`binding/` 的实现细节（那些看源码）。
---

# bee.lua

Lua 扩展库，为 Lua 5.4 / 5.5 提供系统级原生绑定。

## API 参考来源

**本 skill 不维护 bee 的 API 签名表**，以仓库里的文件为唯一权威来源：

| 目的 | 来源 |
|------|------|
| 函数签名、参数/返回值类型、字段与常量 | `meta/<module>.lua`（LuaLS/EmmyLua 注解，带中文说明） |
| 行为契约、错误文案、边界情况、可运行示例 | `test/test_<module>.lua` |
| 构建与测试命令、测试规范 | `AGENT.md` |
| C++ 层实现细节 | `bee/`、`binding/lua_<module>.cpp` |

## 模块索引

| 模块 | meta | 用途 |
|------|------|------|
| `bee.platform` | `meta/platform.lua` | 平台/编译器/架构信息（纯数据表） |
| `bee.filesystem` | `meta/filesystem.lua` | 路径与文件系统操作 |
| `bee.serialization` | `meta/serialization.lua` | 序列化（线程/通道的底层） |
| `bee.time` | `meta/time.lua` | 墙钟 / 单调 / 线程 CPU 时间 |
| `bee.sys` | `meta/sys.lua` | 可执行文件路径、文件锁 |
| `bee.socket` | `meta/socket.lua` | TCP/UDP/Unix socket |
| `bee.select` | `meta/select.lua` | select 风格多路复用 |
| `bee.epoll` | `meta/epoll.lua` | epoll 风格多路复用（Windows 走 IOCP） |
| `bee.async` | `meta/async.lua` | 异步 I/O（IOCP / io_uring / GCD） |
| `bee.filewatch` | `meta/filewatch.lua` | 文件监控 |
| `bee.thread` | `meta/thread.lua` | 线程 |
| `bee.channel` | `meta/channel.lua` | 线程间通信 |
| `bee.subprocess` | `meta/subprocess.lua` | 子进程与管道 |
| `bee.windows` | `meta/windows.lua` | Windows 专有工具 |
| `bee.crash` | `meta/crash.lua` | 崩溃 dump |
| `bee.debugging` | `meta/debugging.lua` | 断点 / 调试器探测 |

## 跨模块约定

- 模块都在 `bee.*` 命名空间：`local socket = require "bee.socket"`。
- **三态返回值**（socket / epoll / select 等非阻塞接口）：成功 → 值；`false` → 需等待（非错误）；`nil, errmsg` → 失败/对端关闭。参数校验错误才 `error()`。
- 句柄类对象用 to-be-closed 管理：`local fd <close> = assert(socket.create "tcp")`。
- 子进程管道是标准 `file*`，用 `:read "a"` / `:write` / `:close`。
- 线程/通道传值经 `bee.serialization`，只支持 `nil/boolean/number/string/table/light C function`。
- 平台上不可用的模块/接口在测试中跳过（`test/test_skip.lua`、`test/supported.lua`）。

## 可选链（编译期 patch）

`?.` / `?:` / `?[...]` / `f?(...)` 是 vendored Lua 的补丁语法，仅当 `luamake -optchain` 构建时可用（补丁机制见 `AGENT.md`）：

- 只有 `nil` 短路，`false` 会照常报错；短路时参数/键不会被求值，接收者只求值一次。
- 短路只覆盖链本身：`(nothing?.b).c`、`nothing?.b + 1` 仍报错。
- 不可作为赋值目标。
- 完整行为（含字节码布局）见 `test/test_optional_chain.lua`。
