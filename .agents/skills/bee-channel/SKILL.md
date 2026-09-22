---
name: bee-channel
description: 用 bee.channel 做线程间通信（create/query/destroy 命名通道、box:push/pop 序列化传递、box:fd 接入 epoll/select 等待可读）。当需要多线程收发消息、或搭建 worker 请求-响应模型时使用。
---

# bee.channel

`require "bee.channel"`，对应 `meta/channel.lua`、`test/test_channel.lua`。

## API

```lua
local channel = require "bee.channel"

local box = channel.create(name)      -- 名称必须唯一，重复则 error: "Duplicate channel 'test'"
local box = channel.query(name)       --> box | nil, err
channel.destroy(name)                 -- 清空数据并销毁

box:push(...)          -- 序列化后入队（类型限制同 bee.serialization）
local ok, ... = box:pop()             -- ok == false 表示通道为空（此时第二个返回值为 nil）
box:fd()               --> lightuserdata  -- 用于 epoll/select 等可读
```

`pop` 逐条出队，FIFO：

```lua
local chan = channel.create "test"
chan:push(1024); chan:push(1025)
local ok, v = chan:pop(); assert(ok == true and v == 1024)   -- pop 第一个返回值是 ok，第二个才是数据
ok, v = chan:pop(); assert(ok == true and v == 1025)
ok, v = chan:pop()          -- ok == false，通道已空（v 为 nil）
channel.destroy "test"
```

## 用法：worker + 请求/响应

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
local ok, what, arg = res:pop()            -- 阻塞式轮询
req:push "exit"
thread.wait(thd)
channel.destroy "testReq"; channel.destroy "testRes"
```

## 用法：用 fd 参与多路复用（避免空转）

worker 端监听 `req:fd()`，取到 `EPOLLIN` 后循环 `pop` 直到取空（`test_channel:test_fd`）：

```lua
local epoll = require "bee.epoll"
local epfd <close> = epoll.create(16)
epfd:event_add(req:fd(), epoll.EPOLLIN)
for _, event in epfd:wait() do
    if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then error "unknown error" end
    if event & epoll.EPOLLIN ~= 0 then
        while true do
            local ok, what, ... = req:pop()
            if not ok then break end
            -- 分发；收到 "exit" 则 return
        end
    end
end
```

主线程侧同理监听 `res:fd()`；`bee.async` 里可用 `as:submit_poll(chan:fd(), udata)`。

## 注意事项

- 通道是**全局命名**的：`channel.query` 在别的线程里靠名字找回同一个通道，因此名字要唯一且双方约定一致。
- `create` 一个已存在的名字会 `error`；`test_reset_1` 说明 `destroy` 后可以重新 `create` 同名通道。
- 传的数据经序列化，不能传 userdata / `thread` / 普通 Lua function（报错文案见 `bee-serialization`）。
- 通道内数据在 `destroy` 时被清空，不要依赖销毁后还能 `pop`。
- 双向通信要建两个通道（req/res），单个通道是单向队列。
