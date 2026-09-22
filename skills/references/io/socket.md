# bee.socket

`require "bee.socket"`，对应 `meta/socket.lua`、`test/test_socket.lua`。

## 非阻塞三态返回（本模块最重要的约定）

| 返回值 | 含义 |
|--------|------|
| 值（`true`/数据/字节数/新 fd） | 成功 |
| `false` | 需等待，配合 `bee.select` / `bee.epoll` 重试 |
| `nil, errmsg` | 失败或对端关闭 |

`fd:accept()` / `fd:recv()` 的 `nil` 表示对端关闭；`fd:send()` 返回**已发送字节数**，partial write 需自行切片重试。

## 创建与连接

```lua
local socket = require "bee.socket"
local select = require "bee.select"

-- 协议："tcp" | "udp" | "unix" | "tcp6" | "udp6"
local server = assert(socket.create "tcp")
assert(server:bind("127.0.0.1", 0))          -- 端口 0 = 系统分配
assert(server:listen())                      -- backlog 默认 5
local address, port = server:info "socket":value()   -- "socket" 本端 / "peer" 对端

local client = assert(socket.create "tcp")
client:connect("127.0.0.1", port)            -- 非阻塞，之后等可写再 status()
-- 等可写后：
assert(client:status())                      -- true 表示连接建立

local session = assert(server:accept())      -- false = 尚无连接
session:close(); client:close(); server:close()
```

Unix socket：`socket.create "unix"` + `fd:bind(path)`，关闭后是否自动 unlink 依平台（测试中用 `detectAutoUnlink` 探测）。

## 读写

```lua
fd:recv([len])                --> string | false(等待) | nil(关闭), err
fd:send(data)                --> n | false(等待) | nil, err
fd:sendv(s1, s2, ...)        --> 一次系统调用向量化发送，返回总字节数
fd:recvfrom([len])           --> data, bee.endpoint | false | nil, err
fd:sendto(data, ep_or_addr [, port])   --> n | false | nil, err
```

UDP 示例（`test_socket:test_udp`）：

```lua
local a, b = assert(socket.create "udp"), assert(socket.create "udp")
a:bind("127.0.0.1", 0); b:bind("127.0.0.1", 0)
local a_ep, b_ep = a:info "socket", b:info "socket"
assert(a:sendto("123", b_ep) == 3)
local data, from_ep = b:recvfrom()           -- 需先等 b 可读
assert(data == "123" and from_ep == a_ep)
```

## 端点与其它工具

```lua
socket.endpoint("inet", ip, port)            -- 也有 "inet6" | "hostname" | "unix"
ep:value()                                   -- inet/inet6 返回 ip, port；unix 返回 path, type
socket.pair()                                --> fd1, fd2（一对已连接的 socket，测试里用于 echo）
socket.gethostname()                         --> string
socket.fd(handle [, no_ownership])           -- 从裸句柄包装
```

`fd:detach()` 交出裸句柄并放弃所有权，`socket.fd(h)` 可重新包装（`test_socket:test_dump`）：

```lua
local h = server:detach()
server = socket.fd(h)
```

## 其它 fd 方法

```lua
fd:option("reuseaddr"|"sndbuf"|"rcvbuf", value)
fd:shutdown("r"|"w")        -- 省略则双向
fd:handle()                 --> lightuserdata
```

## 常见用法模板

同步等待 + 收发（来自 `test_socket.lua` 的 `simple_select`）：

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
- `socket.create` 失败返回 `nil, err`（用 `assert` 包装）。
- 对端关闭后继续 `send` 不应崩溃（`test_SIGPIPE` 专门覆盖）。
- 跨线程使用 socket 需要传句柄或让线程自己 `create`，不能共享 userdata。
