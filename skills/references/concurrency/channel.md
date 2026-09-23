# bee.channel

线程间通信（命名通道）。签名见 `meta/channel.lua`，行为契约见 `test/test_channel.lua`。

## 要点

- 通道是**全局命名**的：`channel.query(name)` 在别的线程里靠名字找回同一个通道，所以名字要唯一且双方约定一致；`create` 重名会 `error`（`destroy` 后可以重新 `create`）。
- `push(...)` 内部序列化，类型限制同 [serialization](../core/serialization.md)。
- `pop()` 是 **FIFO 逐条出队**，返回 `(ok, ...)`：`ok == false` 表示通道为空。
- `fd()` 给 epoll/select 用，能等可读，避免空转。

## 用法

```lua
local channel = require "bee.channel"

local chan = channel.create "test"
chan:push(1024); chan:push(1025)
local ok, v = chan:pop(); assert(ok == true and v == 1024)   -- 第一个返回值是 ok
ok, v = chan:pop(); assert(ok == true and v == 1025)
ok, v = chan:pop()          -- ok == false，通道已空
channel.destroy "test"
```

worker + 请求/响应（双向要建两个通道）：

```lua
local thread  = require "bee.thread"
local channel = require "bee.channel"

local req = channel.create "testReq"
local res = channel.create "testRes"

local thd = thread.create([[
    local thread  = require "bee.thread"
    local channel = require "bee.channel"
    local req = channel.query "testReq"
    local res = channel.query "testRes"
    local function dispatch(ok, what, ...)
        if not ok then return end
        if what == "exit" then return true end
        res:push(what, ...)
    end
    while not dispatch(req:pop()) do
        thread.sleep(0)                    -- 空转等待
    end
]])

req:push("echo", 1, { A = { B = "C" } })
local ok, what, arg = res:pop()
req:push "exit"
thread.wait(thd)
channel.destroy "testReq"; channel.destroy "testRes"
```

用 `fd()` 接多路复用，替掉空转（`test_channel:test_fd`）：

```lua
local epoll = require "bee.epoll"

local epfd <close> = epoll.create(16)
epfd:event_add(req:fd(), epoll.EPOLLIN)
for _, event in epfd:wait() do
    if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then error "unknown error" end
    if event & epoll.EPOLLIN ~= 0 then
        while true do                        -- 取空为止
            local ok, what, ... = req:pop()
            if not ok then break end
            -- 分发；收到 "exit" 则 return
        end
    end
end
```

主线程侧同理监听 `res:fd()`；`bee.async` 里可用 `as:submit_poll(chan:fd(), udata)`。

## 注意事项

- 单个通道是**单向队列**，双向通信要建两个。
- `destroy` 会清空通道内数据，不要依赖销毁后还能 `pop`。
- 用 `fd()` 等可读时，收到通知后必须 `pop` 到 `ok == false`，否则会一直就绪。
