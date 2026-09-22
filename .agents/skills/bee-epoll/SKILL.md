---
name: bee-epoll
description: 用 bee.epoll 做 epoll 风格 I/O 多路复用（create/event_add/event_mod/event_del/wait 迭代器、EPOLLIN/EPOLLOUT 等位标志、关联自定义 userdata，Windows 下由 IOCP 实现）。当需要监听多个 fd 或 channel 可读事件时使用。
---

# bee.epoll

`require "bee.epoll"`，对应 `meta/epoll.lua`、`test/test_epoll.lua`、`test/test_channel.lua`。

跨平台 epoll 风格 API，Windows 上由 IOCP 实现，因此返回错误的形式是 `nil, err`。

## API

```lua
local epoll = require "bee.epoll"

local epfd <close> = assert(epoll.create(16))       -- max_events 必须 > 0，否则 error
epfd:event_add(fd, events [, userdata])             --> true | nil, err
epfd:event_mod(fd, events [, userdata])             --> true | nil, err
epfd:event_del(fd)                                  --> true | nil, err
epfd:wait([timeout])                                --> iterator | nil（已 close 时）
epfd:close()                                        --> true | nil, err（重复 close 返回 nil）
```

- `fd` 可为 `bee.socket.fd` 或 `lightuserdata`（如 `channel:fd()`）。
- `userdata` 是迭代回传的关联对象，默认 fd 自身。
- `timeout` 毫秒，`-1`/省略为无限等待。
- 重复 `event_add` 同一个 fd、或对未添加的 fd `event_mod`/`event_del` 返回 `nil`（不抛错）。

## 事件常量

按位定义，`test_epoll:test_enum` 锁定了取值：

```lua
epoll.EPOLLIN       -- 1 << 0   可读
epoll.EPOLLPRI      -- 1 << 1
epoll.EPOLLOUT      -- 1 << 2   可写
epoll.EPOLLERR      -- 1 << 3
epoll.EPOLLHUP      -- 1 << 4
epoll.EPOLLRDNORM   -- 1 << 6
epoll.EPOLLRDBAND   -- 1 << 7
epoll.EPOLLWRNORM   -- 1 << 8
epoll.EPOLLWRBAND   -- 1 << 9
epoll.EPOLLMSG      -- 1 << 10
epoll.EPOLLRDHUP    -- 1 << 13  对端关闭
epoll.EPOLLONESHOT  -- 1 << 30  一次性
```

## 用法

```lua
local epfd <close> = assert(epoll.create(16))
epfd:event_add(res_chan:fd(), epoll.EPOLLIN, "res")

for obj, event in epfd:wait() do
    if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
        error "unknown error"
    end
    if event & epoll.EPOLLIN ~= 0 then
        -- 就绪通知，数据仍需自行消费
        while true do
            local ok, v = res_chan:pop()
            if not ok then break end
            print(obj, v)
        end
    end
end
```

`test_channel:test_fd` 是完整范例：worker 线程里 `epfd:event_add(req:fd(), epoll.EPOLLIN)`，主线程监听 `res:fd()`，双方用通道收发。

## 注意事项

- `epoll.create(max_events)` 对 `<= 0` 的参数直接 `error`：`maxevents is less than or equal to zero.`（测试用 `lt.assertFailed` 断言）。
- `wait` 返回空迭代表示超时；用作非阻塞轮询时传 `0`。
- `epoll` 只做就绪通知（水平触发语义由底层决定），不消费数据；`channel:pop()` 到空为止是标准收尾方式。
- 需要更简单的 `SELECT_READ/SELECT_WRITE` 语义用 `bee.select`；需要一次投递一次完成事件用 `bee.async`。
