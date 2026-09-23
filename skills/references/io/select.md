# bee.select

select 风格 I/O 多路复用。签名见 `meta/select.lua`，行为契约见 `test/test_socket.lua`。

## 要点

- 事件是**位标志**：`select.SELECT_READ` | `select.SELECT_WRITE`，只有这两个。
- `fd` 可以是 `bee.socket.fd`，也可以是裸 `lightuserdata`（如 `channel:fd()`）。
- `event_add` 的第三个参数是迭代时回传的关联对象，默认是 fd 自身。
- `wait([timeout])` 返回**迭代器**，迭代产出 `(userdata, event)`；空迭代表示超时，`timeout` 单位毫秒、`-1`/省略为无限等待。
- 同一轮可能读、写同时就绪，所以判断标志要**按位与**，多次迭代要**按位或**累加。
- `select` 只做就绪通知，不消费数据，也不报错误事件；收发仍需自己 `fd:recv` / `fd:send`。

## 用法

```lua
local select = require "bee.select"

local ctx <close> = select.create()
ctx:event_add(fd, select.SELECT_READ | select.SELECT_WRITE)
for obj, event in ctx:wait() do
    if event & select.SELECT_READ ~= 0 then ... end
    if event & select.SELECT_WRITE ~= 0 then ... end
end
```

只关心「有没有就绪」时可以把事件累加（来自 `test_socket.lua` 的 `simple_select`）：

```lua
local function simple_select(fd, mode)
    local s <close> = select.create()
    if mode == "r" then
        s:event_add(fd, select.SELECT_READ)
        s:wait()
    elseif mode == "w" then
        s:event_add(fd, select.SELECT_WRITE)
        s:wait()
    elseif mode == "rw" then
        s:event_add(fd, select.SELECT_READ | select.SELECT_WRITE)
        local event = 0
        for _, e in s:wait() do
            event = event | e
        end
        return event
    else
        assert(false)
    end
end
```

## 注意事项

- 一次性等待建议用 `local s <close> = select.create()`（to-be-closed），避免忘记 `close`。
- 与 `bee.epoll` 的差异：epoll 的 `event_*` 返回 `nil, err` 且支持 `EPOLLRDHUP` / oneshot 等语义，`bee.select` 的返回 `boolean`、只有读/写两种标志。
- 需要「一次投递一次完成事件」用 `bee.async`，不要在 select 上自己拼状态机。
