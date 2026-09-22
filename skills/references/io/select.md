# bee.select

`require "bee.select"`，对应 `meta/select.lua`、`test/test_socket.lua`。

## API

```lua
local select = require "bee.select"

local ctx <close> = select.create()                     -- 不会失败，返回值不是 nil,err 形式
ctx:event_add(fd, events [, userdata])                  --> boolean
ctx:event_mod(fd, events)                               --> boolean
ctx:event_del(fd)                                       --> boolean
ctx:wait([timeout])                                     --> iterator
ctx:close()
```

- `events` 是位组合：`select.SELECT_READ` (读) | `select.SELECT_WRITE` (写)。
- `fd` 可以是 `bee.socket.fd`，也可以是裸 `lightuserdata`（如 `channel:fd()`）。
- `userdata` 为迭代时回传的关联对象，默认是 fd 自身。
- `timeout` 单位毫秒，`-1`（或省略）无限等待。

## wait 的正确用法

`wait` 返回**迭代器**，迭代产出 `(userdata, event)`；返回空迭代表示超时。

```lua
for obj, event in ctx:wait() do
    if event & select.SELECT_READ ~= 0 then ... end
    if event & select.SELECT_WRITE ~= 0 then ... end
end
```

`event` 是**位标志**：同一轮里可能既有读也有写就绪，需要按位或把多次迭代的 `event` 累加起来（来自 `test_socket.lua` 的 `simple_select`）：

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
- 事件常量只有 `SELECT_READ` / `SELECT_WRITE`；需要 epoll 语义（`EPOLLRDHUP`、oneshot 等）请改用 `bee.epoll`。
- `ctx:close()` 后再调用 `event_add` 等会失败；`bee.epoll` 对应接口返回 `nil, err`，`bee.select` 返回 `boolean`。
- 与 `bee.async` 不同，select 只做就绪通知，收发仍需自己调用 `fd:recv`/`fd:send`。
