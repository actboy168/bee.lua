---
name: bee-lua
description: bee.lua 运行时库指南——Lua 5.4/5.5 上的跨平台系统级绑定（异步 I/O、网络、子进程、线程、文件系统、文件监控、崩溃 dump）。当需要在本项目里编写、修改或排查使用 `require "bee.*"` 的 Lua 代码，涉及 socket/select/epoll/async、thread/channel/serialization、subprocess、filesystem/filewatch、time/sys/crash/debugging/windows，或需要按平台差异处理行为时，使用本 skill。即使用户没有明确提到 bee.lua，只要上下文是在调用 `bee` 命名空间下的模块，也应使用。不要用于纯 Lua 语言/标准库问题、与 bee 无关的第三方 C 模块，或本项目 C++ 层 `bee/`、`binding/` 的实现细节（那些看源码）。
---

# bee.lua

Lua 扩展库，为 Lua 5.4 / 5.5 提供系统级原生绑定。权威签名在 `meta/*.lua`（LuaLS 注解），行为契约在 `test/test_*.lua`。

## 快速开始

```lua
local socket = require "bee.socket"
local select = require "bee.select"

local server <close> = assert(socket.create "tcp")
assert(server:bind("127.0.0.1", 0))
assert(server:listen())
local _, port = server:info "socket":value()

local s <close> = select.create()
s:event_add(server, select.SELECT_READ)
s:wait()
local conn <close> = assert(server:accept())
```

## 模块索引

| 模块 | 文档 | 用途 |
|------|------|------|
| `bee.platform` | [platform](references/platform/platform.md) | 平台/编译器/架构信息（纯数据表） |
| `bee.filesystem` | [filesystem](references/core/filesystem.md) | 路径与文件系统操作 |
| `bee.serialization` | [serialization](references/core/serialization.md) | 序列化（线程/通道的底层） |
| `bee.time` | [time](references/core/time.md) | 墙钟 / 单调 / 线程 CPU 时间 |
| `bee.sys` | [sys](references/core/sys.md) | 可执行文件路径、文件锁 |
| `bee.socket` | [socket](references/io/socket.md) | TCP/UDP/Unix socket |
| `bee.select` | [select](references/io/select.md) | select 风格多路复用 |
| `bee.epoll` | [epoll](references/io/epoll.md) | epoll 风格多路复用（Windows 走 IOCP） |
| `bee.async` | [async](references/io/async.md) | 异步 I/O（IOCP / io_uring / GCD） |
| `bee.filewatch` | [filewatch](references/io/filewatch.md) | 文件监控 |
| `bee.thread` | [thread](references/concurrency/thread.md) | 线程 |
| `bee.channel` | [channel](references/concurrency/channel.md) | 线程间通信 |
| `bee.subprocess` | [subprocess](references/process/subprocess.md) | 子进程与管道 |
| `bee.windows` | [windows](references/platform/windows.md) | Windows 专有工具 |
| `bee.crash` | [crash](references/platform/crash.md) | 崩溃 dump |
| `bee.debugging` | [debugging](references/platform/debugging.md) | 断点 / 调试器探测 |

## 跨模块约定

- 模块都在 `bee.*` 命名空间：`local socket = require "bee.socket"`。
- **三态返回值**（socket / epoll / select 等非阻塞接口）：成功 → 值；`false` → 需等待（非错误）；`nil, errmsg` → 失败/对端关闭。参数校验错误才 `error()`。
- 句柄类对象用 to-be-closed 管理：`local fd <close> = assert(socket.create "tcp")`。
- 子进程管道是标准 `file*`，用 `:read "a"` / `:write` / `:close`。
- 线程/通道传值经 `bee.serialization`，只支持 `nil/boolean/number/string/table/light C function`。
- 平台上不可用的模块/接口在测试中跳过（`test/test_skip.lua`、`test/supported.lua`）。

## 构建与测试

```bash
luamake                    # 编译 + 测试
luamake -notest            # 只编译
luamake test -v            # 只测试，详细输出
luamake test -v <pattern>  # 只跑名称匹配的用例（如 socket.test_udp）
```

测试基于 ltest，文件在 `test/`：

```lua
local lt = require "ltest"
local m = lt.test "module"

function m:test_case()
    lt.assertEquals(a, b)
    lt.assertNil(x); lt.assertIsUserdata(fd); lt.assertTrue(cond)
    lt.assertError(function () ... end)
    lt.assertErrorMsgEquals("max_completions is less than or equal to zero.", async.create, 0)
    lt.failure "msg"
end
```

常用辅助：

- `test/shell.lua` — `shell:runlua(script, spawn_options)` 起带正确 `package.cpath` 的 Lua 子进程；`shell:add_readonly/del_readonly`；`shell:pwd()`；`shell.is_luamake`。
- `test/supported.lua` — `supported "symlink"` / `supported "hardlink"` 特性探测（结果缓存）。
- `test/test_skip.lua` — 按平台 `lt.skip "module.test_name"` 跳过用例。
- `test/test.lua` — 入口：设置 `package.path/cpath`、按平台装载测试文件、`lt.run()` 后 `os.exit`。

## 可选链（编译期 patch）

`?.` / `?:` / `?[...]` / `f?(...)` 是 vendored Lua 的补丁语法，仅当 `luamake -optchain` 构建时可用：

```lua
local a = obj?.a?.b?.c            -- 链上任一环节为 nil 即短路为 nil
local v = t?[1]?[2]
local r = obj?:method(args)       -- 只保护接收者，方法本身不存在仍报错
local x = f?(1, 2)
```

- 只有 `nil` 短路，`false` 会照常报错。
- 短路时参数/键不会被求值；接收者只求值一次。
- 短路只覆盖链本身：`(nothing?.b).c`、`nothing?.b + 1` 仍报错。
- 不可作为赋值目标。
- `test/test_optional_chain.lua` 还锁定了生成的字节码布局。
