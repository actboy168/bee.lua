# bee.lua Lua API 索引

各模块的详细 skill 见 `.agents/skills/bee-<module>/SKILL.md`。权威签名在 `meta/*.lua`（LuaLS 注解），行为契约在 `test/test_*.lua`。

## 模块 → skill

| 模块 | skill | 用途 |
|------|-------|------|
| `bee.platform` | [bee-platform](skills/bee-platform/SKILL.md) | 平台/编译器/架构信息（纯数据表） |
| `bee.filesystem` | [bee-filesystem](skills/bee-filesystem/SKILL.md) | 路径与文件系统操作 |
| `bee.socket` | [bee-socket](skills/bee-socket/SKILL.md) | TCP/UDP/Unix socket |
| `bee.select` | [bee-select](skills/bee-select/SKILL.md) | select 风格多路复用 |
| `bee.epoll` | [bee-epoll](skills/bee-epoll/SKILL.md) | epoll 风格多路复用（Windows 走 IOCP） |
| `bee.async` | [bee-async](skills/bee-async/SKILL.md) | 异步 I/O（IOCP / io_uring / GCD） |
| `bee.time` | [bee-time](skills/bee-time/SKILL.md) | 墙钟 / 单调 / 线程 CPU 时间 |
| `bee.thread` | [bee-thread](skills/bee-thread/SKILL.md) | 线程 |
| `bee.channel` | [bee-channel](skills/bee-channel/SKILL.md) | 线程间通信 |
| `bee.serialization` | [bee-serialization](skills/bee-serialization/SKILL.md) | 序列化（线程/通道的底层） |
| `bee.subprocess` | [bee-subprocess](skills/bee-subprocess/SKILL.md) | 子进程与管道 |
| `bee.filewatch` | [bee-filewatch](skills/bee-filewatch/SKILL.md) | 文件监控 |
| `bee.sys` | [bee-sys](skills/bee-sys/SKILL.md) | 可执行文件路径、文件锁 |
| `bee.crash` | [bee-crash](skills/bee-crash/SKILL.md) | 崩溃 dump |
| `bee.debugging` | [bee-debugging](skills/bee-debugging/SKILL.md) | 断点 / 调试器探测 |
| `bee.windows` | [bee-windows](skills/bee-windows/SKILL.md) | Windows 专有工具 |

## 跨模块约定

- 模块都在 `bee.*` 命名空间：`local socket = require "bee.socket"`。
- **三态返回值**（socket / epoll / select 等非阻塞接口）：成功 → 值；`false` → 需等待（非错误）；`nil, errmsg` → 失败/对端关闭。参数校验错误才 `error()`。
- 句柄类对象用 to-be-closed 管理：`local fd <close> = assert(socket.create "tcp")`。
- 子进程管道是标准 `file*`，用 `:read "a"` / `:write` / `:close`。
- 线程/通道传值经 `bee.serialization`，只支持 `nil/boolean/number/string/table/light C function`。

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
