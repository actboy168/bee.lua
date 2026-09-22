# bee.async

`require "bee.async"`，对应 `meta/async.lua`、`test/test_async.lua`。macOS 用 GCD，Windows 用 IOCP，Linux 用 io_uring/epoll。

模型：**一次投递 → 一次 completion**，投递时传入的 `udata`（token）原样回传。

## API

```lua
local async = require "bee.async"

local as <close> = assert(async.create(64))   -- create(max_completions)，默认 64；<=0 报 "max_completions is less than or equal to zero."

as:associate(fd)                       -- Windows/IOCP 必需，其他平台 no-op
as:associate_file(file)                -- 文件 I/O 前必须调用（io.open 得到的 file*）
as:cancel(fd)                          -- 取消该 fd 上所有未完成操作
as:poll()  /  as:wait([timeout_ms])    -- 非阻塞 / 阻塞，返回完成事件迭代器
as:stop()
```

投递接口：

```lua
as:submit_read(rb, fd, udata)                       --> true | false(背压) | nil, err
as:submit_write(wb, fd, udata)                      --> true | nil, err
as:submit_accept(listen_fd, udata)
as:submit_connect(fd, host, port, udata)            -- 也接受 bee.endpoint 重载
as:submit_file_read(file, len [, offset = 0], udata)
as:submit_file_write(file, data [, offset = 0], udata)
as:submit_poll(fd, udata)                           -- 只监听可读，不消费数据
```

缓冲区：

```lua
local wb = assert(async.writebuf(64 * 1024))       -- writebuf(hwm)，默认 65536
wb:write(data)      --> true 表示缓冲 >= hwm，调用方应自行背压
wb:buffered()       --> 当前排队字节数
wb:close()          -- 流关闭时丢弃未发数据

local rb = assert(async.readbuf(bufsize))            -- readbuf(bufsize)，向上取整到 2 的幂；<=0 报 "bufsize must be positive"
rb:read([n])        --> string | nil（数据不足）；n 省略取全部可用
rb:readline([sep = "\r\n"])   --> string | nil（未找到分隔符）
```

## completion 迭代器

```lua
for op, udata, status, data, errcode in as:wait(timeout_ms) do
    -- op     : OP_READ / OP_WRITE / OP_ACCEPT / OP_CONNECT / OP_FILE_READ / OP_FILE_WRITE / OP_POLL
    -- status : SUCCESS / CLOSE / ERROR / CANCEL
    -- data   : accept -> 新 socket userdata；file_read -> 读到的字符串；其余 -> 传输字节数
end
```

写入的完成事件 `bytes` 恒为 `0`（数据已由 C 层 drain 完，包括 partial write 重试）。

## 完整示例（取自 `test_async.lua`）

```lua
local async = require "bee.async"
local socket = require "bee.socket"
local as <close> = assert(async.create(64))

-- 服务端/客户端都要先 associate
local sfd <close> = assert(socket.create "tcp")
assert(as:associate(sfd))
assert(sfd:bind("127.0.0.1", 0)); assert(sfd:listen())

local _, port = sfd:info "socket":value()

local cfd <close> = assert(socket.create "tcp")
assert(as:associate(cfd))
local ok, err = cfd:connect("127.0.0.1", port)     -- 可能返回 false(等待中)，用 submit_connect 更常见
assert(ok ~= nil, err)

-- 接受连接（测试里用 select 等可读，再 sfd:accept 并 associate）
assert(as:submit_accept(sfd, "accept_token"))      -- completion 的 data 即新 socket userdata

-- 写
local wb = assert(async.writebuf(64 * 1024))
wb:write "hello"
assert(as:submit_write(wb, cfd, "write_token"))

local op, token, status, bytes
for _op, _tok, _st, _data in as:wait(1000) do        -- wait/poll 返回的是迭代器
    op, token, status, bytes = _op, _tok, _st, _data
    break
end
-- op == async.OP_WRITE, token == "write_token", status == async.SUCCESS, bytes == 0

-- 读（数据在 ring buffer 里自取）
local rb = assert(async.readbuf(64))
assert(as:submit_read(rb, newfd, { id = 42 }))
-- 收到 OP_READ + SUCCESS 后：
local data = rb:read(5)      -- 精确字节数；不足返回 nil
local line = rb:readline()   -- 或按行取
```

文件 I/O：

```lua
local rf = assert(io.open(path, "rb"))
assert(as:associate_file(rf))
assert(as:submit_file_read(rf, 128, 0, "fread"))
-- completion: op == OP_FILE_READ, data == 读到的字符串（不是字节数）
```

## 注意事项

- `associate` / `associate_file` 必须在**首次提交 I/O 之前**完成；重复 `associate` 同一 socket 是允许的。
- `submit_read` 有背压：ring buffer 空闲不足返回 `false`，重试前需先 `rb:read()` 腾出空间。
- 对端关闭时读操作产生 `status == CLOSE`，不是 `ERROR`。
- 关闭 fd 前建议 `as:cancel(fd)`，确保未完成操作及时回收（Windows 上尤其重要）。
- `submit_poll` 只通知可读，典型用途是监听 `channel:fd()` 后自行 `channel:pop()`。
