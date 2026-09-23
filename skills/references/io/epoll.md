# bee.epoll

epoll 风格 I/O 多路复用（Windows 由 IOCP 实现）。签名见 `meta/epoll.lua`，行为契约见 `test/test_epoll.lua`、`test/test_channel.lua`。

## 要点

- 常量是位标志，取值被 `test_epoll:test_enum` 锁定；`EPOLLRDHUP`（对端关闭）与 `EPOLLONESHOT`（一次性）是 select 没有的。
- `fd` 可为 `bee.socket.fd` 或 `lightuserdata`（如 `channel:fd()`）；`event_add` 的第三个参数是迭代回传的关联对象，默认 fd 自身。
- `wait([timeout])` 返回**迭代器**，空迭代表示超时；`timeout` 毫秒、`-1`/省略为无限等待，传 `0` 即非阻塞轮询。
- 只做就绪通知，**不消费数据**；`event_add` 到已存在的 fd、或对未注册的 fd `event_mod`/`event_del` 返回 `nil`（不抛错）。
- `epoll.create(max_events)` 对 `<= 0` 的参数直接 `error`：`maxevents is less than or equal to zero.`。

## 用法

```lua
local epoll = require "bee.epoll"

local epfd <close> = assert(epoll.create(16))
epfd:event_add(res_chan:fd(), epoll.EPOLLIN, "res")

for obj, event in epfd:wait() do
    if event & (epoll.EPOLLERR | epoll.EPOLLHUP) ~= 0 then
        error "unknown error"
    end
    if event & epoll.EPOLLIN ~= 0 then
        -- 就绪通知：数据要自己取空
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

- 取到 `EPOLLIN` 后**必须把数据取空**（`channel:pop()` 到 `ok == false`），否则下一次还会立刻就绪。
- 关闭实例前先 `event_del`，避免残留注册；实例 `close` 后 `wait` 会返回 `nil, "bad file descriptor"`。
- 需要更简单的读/写语义用 `bee.select`；需要「一次投递一次完成事件」用 `bee.async`。
