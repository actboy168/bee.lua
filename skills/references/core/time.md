# bee.time

毫秒级时间。签名见 `meta/time.lua`，行为契约见 `test/test_time.lua`。三个函数都返回**毫秒整数**。

## 要点

- `time.time()` 是墙钟，会被 NTP / 手动改钟影响；**测间隔一律用 `time.monotonic()`**。
- `time.thread()` 是当前线程已消耗的 CPU 时间。
- `time.time()` 与 `os.time() * 1000` 相差不超过 2 秒（`test_time:test_now`），但单位不同，别混用。

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

- 精度/粒度依平台，`test_time:test_monotonic` 只断言 `> 0`。
- 需要「等一段时间」用 `thread.sleep`（毫秒），不要忙等 `monotonic`。
