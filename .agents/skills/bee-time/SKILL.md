---
name: bee-time
description: 用 bee.time 获取毫秒级时间（time 墙钟、monotonic 单调递增、thread 线程 CPU 时间）。当需要测量耗时、实现超时/退避、或在线程中计时时使用。
---

# bee.time

`require "bee.time"`，对应 `meta/time.lua`、`test/test_time.lua`。三个函数都返回**毫秒整数**。

## API

```lua
local time = require "bee.time"

time.time()        -- 自 Unix 纪元(1970-01-01 UTC) 起的毫秒数（墙钟，会受系统时间调整影响）
time.monotonic()   -- 单调递增毫秒数，测间隔用这个
time.thread()      -- 当前线程已消耗的 CPU 时间（毫秒）
```

## 用法

测量耗时（`test_thread:test_sleep`）：

```lua
local time = require "bee.time"
local thread = require "bee.thread"

local t1 = time.monotonic()
thread.sleep(1)
local t2 = time.monotonic()
assert(t2 - t1 >= 1)
```

与 `os.time()` 的关系（`test_time:test_now`）：`os.time() * 1000` 与 `time.time()` 相差不超过 2 秒。

超时轮询（`test_async.lua` 的 `wait_completion`）：

```lua
local start = time.monotonic()
while time.monotonic() - start < timeout then
    for op, token, st, data, errcode in as:wait(100) do
        return op, token, st, data, errcode
    end
end
error "wait_completion timeout"
```

## 注意事项

- 计时一律用 `monotonic()`，`time()` 可能被系统时间调整（NTP、手动改钟）拉回或跳过。
- 单位是毫秒，不是秒；不要与 `os.time()`（秒）混用。
- 精度/粒度依平台，`test_time:test_monotonic` 只断言 `> 0`。
