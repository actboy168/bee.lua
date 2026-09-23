# bee.async

跨平台异步 I/O：macOS 用 GCD，Windows 用 IOCP，Linux 用 io_uring/epoll。签名见 `meta/async.lua`，行为契约见 `test/test_async.lua`。

模型：**一次投递 → 一次 completion**，投递时传入的 token 原样回传。

## 要点

- `associate(fd)` / `associate_file(file)` 必须在**首次提交 I/O 之前**完成；重复 `associate` 同一 socket 允许。
- `wait(timeout_ms)` 阻塞、`poll()` 非阻塞，都返回**迭代器**，产出 `(op, token, status, data, errcode)`：
  - `op`：`OP_READ` / `OP_WRITE` / `OP_ACCEPT` / `OP_CONNECT` / `OP_FILE_READ` / `OP_FILE_WRITE` / `OP_POLL`。
  - `status`：`SUCCESS` / `CLOSE` / `ERROR` / `CANCEL`；**对端关闭是 `CLOSE`，不是 `ERROR`**。
  - `data`：`OP_ACCEPT` 是新 socket userdata；`OP_FILE_READ` 是读到的字符串；其余是字节数。
- 读写缓冲区独立于 socket：`OP_READ` 的数据在 `readbuf` 里，要自己 `rb:read()` 取。
- **写入的 completion `bytes` 恒为 `0`**（C 层已 drain 完，含 partial write 重试）。
- `submit_read` 有背压：ring buffer 空闲不足返回 `false`（不是错误），重试前先 `rb:read()` 腾空间。
- `writebuf:write(data)` 返回 `true` 表示缓冲已达 hwm，调用方要自己背压。
- 关闭 fd 前建议 `as:cancel(fd)`，回收未完成操作（Windows 上尤其重要）。

## 用法（取自 `test_async.lua`）

```lua
local async = require "bee.async"
local socket = require "bee.socket"

local as <close> = assert(async.create(64))          -- create(max_completions)
local sfd <close> = assert(socket.create "tcp")
assert(as:associate(sfd))
assert(sfd:bind("127.0.0.1", 0)); assert(sfd:listen())
local _, port = sfd:info "socket":value()

local cfd <close> = assert(socket.create "tcp")
assert(as:associate(cfd))
local ok, err = cfd:connect("127.0.0.1", port)
assert(ok ~= nil, err)

assert(as:submit_accept(sfd, "accept_token"))        -- completion 的 data 即新 socket
assert(as:submit_read(rb, newfd, { id = 42 }))       -- token 可以是任意

local op, token, status, bytes
for _op, _tok, _st, _data in as:wait(1000) do
    op, token, status, bytes = _op, _tok, _st, _data
    break
end
-- op == async.OP_READ；收到 SUCCESS 后从 ring buffer 取数据：
local data = rb:read(5)      -- 精确字节数；不足返回 nil
local line = rb:readline()   -- 或按行取，默认分隔符 "\r\n"
```

文件 I/O 要额外的 `associate_file`：

```lua
local rf = assert(io.open(path, "rb"))
assert(as:associate_file(rf))
assert(as:submit_file_read(rf, 128, 0, "fread"))
-- completion: op == OP_FILE_READ，data 是读到的字符串（不是字节数）
```

## 注意事项

- `submit_poll(fd, udata)` 只通知可读、不消费数据，典型用途是监听 `channel:fd()` 后自行 `channel:pop()`。
- `create` 的 `max_completions <= 0` 与 `readbuf` 的 `bufsize <= 0` 都是 `error`，文案见测试断言。
- 事件循环里别在 completion 回调内阻塞等待同一 fd 的下一个事件，会死锁；测试里的 `wait_completion` 是超时轮询写法，可参考。
