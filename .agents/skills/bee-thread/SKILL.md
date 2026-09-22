---
name: bee-thread
description: 用 bee.thread 创建原生线程（create 传源码字符串与参数、wait、sleep、setname、errlog、线程 id 与 preload_module，线程间不共享全局变量）。当需要并行执行 Lua 代码或搭建多线程 worker 时使用。
---

# bee.thread

`require "bee.thread"`，对应 `meta/thread.lua`、`test/test_thread.lua`。

## API

```lua
local thread = require "bee.thread"

thread.create(source, ...)   --> handle(lightuserdata)   -- source 是 Lua 源码字符串
thread.wait(handle)                                      -- 等线程结束
thread.sleep(msec)
thread.errlog()              --> string | nil            -- 取走并清空线程错误日志
thread.setname(name)                                     -- 给当前线程命名（调试用）
thread.id                    -- 主线程为 0，其他线程非 0
thread.preload_module(L)     -- 新线程内部用，注册 bee.* 模块到指定 lua_State
```

## 用法

```lua
local thread = require "bee.thread"

GLOBAL = true
local thd = thread.create([[
    local thread = require "bee.thread"
    local args = ...                 -- 传给 create 的额外参数会被序列化后传入
    assert(GLOBAL == nil)            -- 线程不共享全局变量
    assert(thread.id ~= 0)
    thread.setname "worker"
]], "hello")
thread.wait(thd)
assert(thread.errlog() == nil)
```

线程内错误不会中断主线程，集中记录在 `errlog`（`test_thread:test_thread_3`）：

```lua
local thd = thread.create [[ error "Test thread error." ]]
thread.wait(thd)
local msg = thread.errlog()
assert(string.find(msg, "Test thread error.", nil, true))
```

## 注意事项

- `source` 必须是**字符串源码**，不能传函数；新线程只拿到自己的环境，只能通过 `require "bee.*"` 或参数传递数据。
- 参数与返回数据要经过 `bee.serialization`，限制见 `bee-serialization` skill（不能传 userdata、`thread`、普通 Lua function）。
- 线程内拿不到主线程的全局变量，测试里专门验证了 `GLOBAL == nil`。
- 每个用例结束后应 `lt.assertEquals(thread.errlog(), nil)` 检查是否遗留线程错误（`test_*.lua` 的 `assertNotThreadError` 约定）。
- macOS/BSD 上 `thread.sleep` 用例在 `test/test_skip.lua` 里被跳过，跨平台测试注意平台差异。
- 线程间通信不要共享 userdata，用 `bee.channel`。
