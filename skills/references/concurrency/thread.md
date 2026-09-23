# bee.thread

原生线程。签名见 `meta/thread.lua`，行为契约见 `test/test_thread.lua`。

## 要点

- `create(source, ...)` 的 `source` 是**Lua 源码字符串**，不能传函数；额外参数序列化后在新线程里以 `...` 取得。
- 新线程**不共享全局变量**，只能靠 `require "bee.*"` 或参数拿数据。
- `thread.id` 主线程为 0，其他线程非 0。
- 线程里的错误不会中断主线程，累积在 `errlog()`（读取即取走并清空）。
- 跨线程传值走 `bee.serialization`，类型限制见 [serialization](../core/serialization.md)。
- 线程间通信不要共享 userdata，用 `bee.channel`。

## 用法

```lua
local thread = require "bee.thread"

GLOBAL = true
local thd = thread.create([[
    local thread = require "bee.thread"
    local args = ...                 -- 传给 create 的额外参数
    assert(GLOBAL == nil)            -- 线程不共享全局变量
    assert(thread.id ~= 0)
    thread.setname "worker"
]], "hello")
thread.wait(thd)
assert(thread.errlog() == nil)
```

线程内的错误（`test_thread:test_thread_3`）：

```lua
local thd = thread.create [[ error "Test thread error." ]]
thread.wait(thd)
local msg = thread.errlog()
assert(string.find(msg, "Test thread error.", nil, true))
```

## 注意事项

- 每个用例结束都应检查 `thread.errlog() == nil`（`test_*.lua` 里的 `assertNotThreadError` 约定），避免错误被静默吞掉。
- macOS / BSD 上 `thread.sleep` 用例在 `test/test_skip.lua` 里被跳过，跨平台测试注意这一点。
- `thread.sleep(0)` 是合法的「让出」写法，`test_channel` 的 worker 空转循环就靠它。
