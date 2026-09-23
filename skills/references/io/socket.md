# bee.socket

TCP / UDP / Unix 套接字。签名见 `meta/socket.lua`，行为契约见 `test/test_socket.lua`。

## 非阻塞三态返回（本模块最重要的约定）

| 返回值 | 含义 |
|--------|------|
| 值（`true`/数据/字节数/新 fd） | 成功 |
| `false` | 需等待，配合 `bee.select` / `bee.epoll` 重试 |
| `nil, errmsg` | 失败或对端关闭 |

- `accept()` / `recv()` 的 `nil` 表示**对端关闭**，不是错误。
- `send()` 返回**已发送字节数**，partial write 要自己切片重试。
- `connect()` 是非阻塞的：之后等可写再 `status()` 判断是否真的连上。

## 用法

```lua
local socket = require "bee.socket"
local select = require "bee.select"

local server = assert(socket.create "tcp")   -- "tcp"|"udp"|"unix"|"tcp6"|"udp6"
assert(server:bind("127.0.0.1", 0))          -- 端口 0 = 系统分配
assert(server:listen())                      -- backlog 默认 5
local address, port = server:info "socket":value()   -- "socket" 本端 / "peer" 对端

local client = assert(socket.create "tcp")
client:connect("127.0.0.1", port)
-- 等可写后：assert(client:status())

local session = assert(server:accept())      -- false = 尚无连接
session:close(); client:close(); server:close()
```

UDP（`test_socket:test_udp`）：

```lua
local a, b = assert(socket.create "udp"), assert(socket.create "udp")
a:bind("127.0.0.1", 0); b:bind("127.0.0.1", 0)
local a_ep, b_ep = a:info "socket", b:info "socket"
assert(a:sendto("123", b_ep) == 3)
local data, from_ep = b:recvfrom()           -- 需先等 b 可读
assert(data == "123" and from_ep == a_ep)
```

句柄移交（`test_socket:test_dump`）：

```lua
local h = server:detach()      -- 交出裸句柄并放弃所有权
server = socket.fd(h)          -- 再包装回来
```

## 常见用法模板

同步等待 + 收发（`test_socket.lua` 的 `simple_select`）：

```lua
local function simple_select(fd, mode)
    local s <close> = select.create()
    if mode == "r" then
        s:event_add(fd, select.SELECT_READ)
    elseif mode == "w" then
        s:event_add(fd, select.SELECT_WRITE)
    else
        s:event_add(fd, select.SELECT_READ | select.SELECT_WRITE)
    end
    s:wait()
end

local function syncSend(fd, data)
    while true do
        simple_select(fd, "w")
        local n = fd:send(data)
        if not n then return n, data end
        data = data:sub(n + 1)
        if data == "" then return true end
    end
end
```

回显服务端（`test_socket.lua` 的 echo 用例，客户端跑在 `thread.create` 里）：

```lua
while true do
    local event = simple_select(client, "rw")
    if event & select.SELECT_READ then
        local data = client:recv()
        if data == nil then break            -- 对端关闭
        elseif data ~= false then queue = queue .. data end
    end
    if event & select.SELECT_WRITE then
        if #queue > 0 then
            local n = client:send(queue)
            if n == nil then break
            elseif n ~= false then queue = queue:sub(n + 1) end
        end
    end
end
```

## 注意事项

- 参数校验错误会 `error()`，文案如 `bad argument #1 to 'bee.socket.create' (invalid option 'icmp')`。
- Unix socket 关闭后是否自动 unlink 依平台，测试里用 `detectAutoUnlink` 探测。
- 对端关闭后继续 `send` 不应崩溃（`test_SIGPIPE` 专门覆盖）。
- 跨线程使用 socket 要把句柄传过去或让线程自己 `create`，不能共享 userdata。
