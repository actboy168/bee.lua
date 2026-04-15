local lt = require "ltest"
local async = require "bee.async"
local socket = require "bee.socket"
local time = require "bee.time"
local select = require "bee.select"
local platform = require "bee.platform"

local m = lt.test "async"

local SUCCESS <const> = async.SUCCESS
local CLOSE <const> = async.CLOSE
local ERROR <const> = async.ERROR
local CANCEL <const> = async.CANCEL

local function SimpleServer(as, protocol, ...)
    local fd = assert(socket.create(protocol))
    assert(as:associate(fd))
    assert(fd:bind(...))
    assert(fd:listen())
    return fd
end

local function SimpleClient(as, protocol, ...)
    local fd = assert(socket.create(protocol))
    assert(as:associate(fd))
    local ok, err = fd:connect(...)
    assert(ok ~= nil, err)
    return fd
end

local function wait_accept(sfd)
    local s <close> = select.create()
    s:event_add(sfd, select.SELECT_READ)
    s:wait()
    return assert(sfd:accept())
end

local function wait_completion(as, timeout)
    timeout = timeout or 1000
    local start = time.monotonic()
    while time.monotonic() - start < timeout do
        for op, token, st, data, errcode in as:wait(100) do
            return token, st, data, errcode
        end
    end
    lt.failure("wait_completion timeout")
end

--- 测试创建和基本属性
function m.test_create()
    lt.assertFailed("max_completions is less than or equal to zero.", async.create(-1))
    lt.assertFailed("max_completions is less than or equal to zero.", async.create(0))
    local as <close> = async.create(64)
    lt.assertIsUserdata(as)
end

--- 测试枚举值
function m.test_enum()
    lt.assertIsNumber(SUCCESS)
    lt.assertIsNumber(CLOSE)
    lt.assertIsNumber(ERROR)
    lt.assertIsNumber(CANCEL)
    -- 枚举值应该是不同的
    lt.assertEquals(SUCCESS ~= CLOSE, true)
    lt.assertEquals(SUCCESS ~= ERROR, true)
    lt.assertEquals(SUCCESS ~= CANCEL, true)
    lt.assertEquals(CLOSE ~= ERROR, true)
    lt.assertEquals(CLOSE ~= CANCEL, true)
    lt.assertEquals(ERROR ~= CANCEL, true)
end

--- 测试 poll 在没有完成事件时返回空
function m.test_poll_empty()
    local as <close> = assert(async.create(64))
    local count = 0
    for _ in as:poll() do
        count = count + 1
    end
    lt.assertEquals(count, 0)
end

--- 测试 wait 超时
function m.test_wait_timeout()
    local as <close> = assert(async.create(64))
    local start = time.monotonic()
    local count = 0
    for _ in as:wait(100) do
        count = count + 1
    end
    local elapsed = time.monotonic() - start
    lt.assertEquals(count, 0)
    -- 只验证确实等待了接近 timeout，避免依赖易抖动的严格上界
    lt.assertEquals(elapsed >= 50, true)
end

--- 测试 TCP write 和 read，token 为字符串
function m.test_tcp_write_read()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    local wb = assert(async.writebuf(64 * 1024))
    wb:write("hello")
    lt.assertEquals(as:submit_write(wb, cfd, "write_token"), true)
    local op, token, status, bytes = wait_completion(as)
    lt.assertEquals(op, async.OP_WRITEV)
    lt.assertEquals(token, "write_token")
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 0) -- writebuf completion always returns 0 bytes

    -- 提交流式读操作，token 为 table
    local read_token = { id = 42 }
    local rb = assert(async.readbuf(64))
    lt.assertEquals(as:submit_read(rb, newfd, read_token), true)
    op, token, status, bytes = wait_completion(as)
    lt.assertEquals(op, async.OP_READ)
    lt.assertEquals(token, read_token)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 5)
    -- 从 ring buffer 精确读取
    lt.assertEquals(rb:read(5), "hello")

    newfd:close()
end

--- 测试 TCP accept，token 为 table
function m.test_tcp_accept()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    local accept_token = { op = "accept" }
    lt.assertEquals(as:submit_accept(sfd, accept_token), true)

    -- 连接到服务器以触发 accept
    local cfd <close> = SimpleClient(as, "tcp", "127.0.0.1", port)

    local _, token, status, newfd = wait_completion(as)
    lt.assertEquals(token, accept_token)
    lt.assertEquals(status, SUCCESS)
    -- accept 完成后返回新的 socket userdata
    lt.assertIsUserdata(newfd)

    -- 关闭 accepted fd
    newfd:close()
end

--- 测试 TCP connect，token 为数字
function m.test_tcp_connect()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    local cfd <close> = assert(socket.create "tcp")
    assert(as:associate(cfd))
    lt.assertEquals(as:submit_connect(cfd, "127.0.0.1", port, 99), true)

    local _, token, status = wait_completion(as)
    lt.assertEquals(token, 99)
    lt.assertEquals(status, SUCCESS)
end

--- 测试文件读写
function m.test_file_read_write()
    local as <close> = assert(async.create(64))
    local fs = require "bee.filesystem"
    local filepath = fs.current_path() / "test_async_file.txt"

    -- 创建测试文件
    do
        local f = assert(io.open(filepath:string(), "wb"))
        f:write("hello async file io")
        f:close()
    end

    -- 打开文件用于读取，关联到异步实例
    local rf = assert(io.open(filepath:string(), "rb"))
    assert(as:associate_file(rf))

    -- 提交文件读操作，token 为字符串
    lt.assertEquals(as:submit_file_read(rf, 128, 0, "fread"), true)
    local op, token, status, bytes = wait_completion(as)
    lt.assertEquals(op, async.OP_FILE_READ)
    lt.assertEquals(token, "fread")
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, "hello async file io")

    rf:close()

    -- 打开文件用于写入，关联到异步实例
    local wf = assert(io.open(filepath:string(), "wb"))
    assert(as:associate_file(wf))
    lt.assertEquals(as:submit_file_write(wf, "written by async", 0, "fwrite"), true)
    op, token, status, bytes = wait_completion(as)
    lt.assertEquals(op, async.OP_FILE_WRITE)
    lt.assertEquals(token, "fwrite")
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 16) -- "written by async" 长度为 16

    wf:close()

    -- 验证写入内容
    local fr = assert(io.open(filepath:string(), "rb"))
    lt.assertEquals(fr:read "*a", "written by async")
    fr:close()

    fs.remove(filepath)
end

--- 测试文件偏移量读写
function m.test_file_read_write_offset()
    local as <close> = assert(async.create(64))
    local fs = require "bee.filesystem"
    local filepath = fs.current_path() / "test_async_offset.txt"

    -- 创建测试文件
    do
        local f = assert(io.open(filepath:string(), "wb"))
        f:write("0123456789ABCDEF")
        f:close()
    end

    -- 从偏移量 10 读取，token 为数字
    local rf = assert(io.open(filepath:string(), "rb"))
    assert(as:associate_file(rf))

    lt.assertEquals(as:submit_file_read(rf, 6, 10, 1), true)
    local _, token, status, bytes = wait_completion(as)
    lt.assertEquals(token, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, "ABCDEF")

    rf:close()
    fs.remove(filepath)
end

--- 测试读取已关闭的连接
function m.test_read_closed()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)

    -- 关闭客户端
    cfd:close()

    -- 在服务端提交读操作应该收到 close 状态
    local rb = assert(async.readbuf(64))
    local read_token = { op = "read_closed" }
    lt.assertEquals(as:submit_read(rb, newfd, read_token), true)
    local _, token, status = wait_completion(as)
    lt.assertEquals(token, read_token)
    lt.assertEquals(status, CLOSE)

    newfd:close()
end

--- 测试多个并发请求，token 为循环变量（数字）
function m.test_multiple_requests()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    -- 创建多个客户端连接
    local clients = {}
    local servers = {}
    for i = 1, 3 do
        clients[i] = SimpleClient(as, "tcp", "127.0.0.1", port)
        servers[i] = wait_accept(sfd)
    end

    -- 提交多个写操作，token 为数字
    local wbs = {}
    for i = 1, 3 do
        wbs[i] = assert(async.writebuf(64 * 1024))
        wbs[i]:write("msg"..i)
        lt.assertEquals(as:submit_write(wbs[i], clients[i], i), true)
    end

    -- 收集所有完成事件
    local results = {}
    local deadline = time.monotonic() + 1000
    while #results < 3 and time.monotonic() < deadline do
        for op, token, status, bytes in as:wait(100) do
            results[#results+1] = { op = op, token = token, status = status, bytes = bytes }
        end
    end
    lt.assertEquals(#results, 3)

    -- 验证所有请求都成功
    for _, r in ipairs(results) do
        lt.assertEquals(r.status, SUCCESS)
        lt.assertEquals(r.bytes, 0) -- writebuf completion always returns 0 bytes
    end

    for i = 1, 3 do
        clients[i]:close()
        servers[i]:close()
    end
end

--- 测试 readbuf 创建和 ring buffer 基本操作
function m.test_readbuf()
    -- 无效参数
    lt.assertErrorMsgEquals("bufsize must be positive", async.readbuf, 0)
    lt.assertErrorMsgEquals("bufsize must be positive", async.readbuf, -1)

    local rb = assert(async.readbuf(64))
    lt.assertIsUserdata(rb)

    -- 空 ring buffer：read 返回 nil
    lt.assertEquals(rb:read(1), nil)
    lt.assertEquals(rb:read(), nil)
end

--- 测试 submit_read + rb:read 流式接收
function m.test_stream_read()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    local rb = assert(async.readbuf(256))

    -- 发送两段数据
    local wb = assert(async.writebuf(64 * 1024))
    wb:write("helloworld")
    lt.assertEquals(as:submit_write(wb, cfd, "w1"), true)
    wait_completion(as)  -- write done

    -- 投递 stream read，token 为字符串
    lt.assertEquals(as:submit_read(rb, newfd, "r1"), true)
    local _, token, status, bytes = wait_completion(as)
    lt.assertEquals(token, "r1")
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes >= 1, true)

    -- rb:read(n) 不足时返回 nil
    local total = rb:read()   -- 取全部
    lt.assertEquals(total ~= nil, true)
    lt.assertEquals(#total >= 1, true)

    -- 再取时为空
    lt.assertEquals(rb:read(), nil)

    newfd:close()
end

--- 测试 rb:readline
function m.test_readline()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    local rb = assert(async.readbuf(256))

    local function recv()
        lt.assertEquals(as:submit_read(rb, newfd, "recv"), true)
        local _, _, status = wait_completion(as)
        lt.assertEquals(status, SUCCESS)
    end

    local function send(data, token)
        local w = assert(async.writebuf(64 * 1024))
        w:write(data)
        lt.assertEquals(as:submit_write(w, cfd, token), true)
        wait_completion(as)
    end

    -- 默认分隔符 \r\n
    send("hello\r\nworld\r\n", "s1")
    recv()
    lt.assertEquals(rb:readline(), "hello\r\n")
    lt.assertEquals(rb:readline(), "world\r\n")
    lt.assertEquals(rb:readline(), nil)

    -- 自定义分隔符 \n
    send("foo\nbar\n", "s2")
    recv()
    lt.assertEquals(rb:readline("\n"), "foo\n")
    lt.assertEquals(rb:readline("\n"), "bar\n")
    lt.assertEquals(rb:readline("\n"), nil)

    -- 多字节自定义分隔符
    send("a|b|c|b|", "s3")
    recv()
    lt.assertEquals(rb:readline("|b|"), "a|b|")
    lt.assertEquals(rb:readline("|b|"), "c|b|")
    lt.assertEquals(rb:readline("|b|"), nil)

    -- 不完整行返回 nil，read() 仍可取出
    send("no-sep", "s4")
    recv()
    lt.assertEquals(rb:readline(), nil)
    lt.assertEquals(rb:read(), "no-sep")

    newfd:close()
end

if platform.os == "windows" then
    --- 测试 associate 和 cancel（仅 Windows）
    function m.test_associate_cancel()
        local as <close> = assert(async.create(64))
        local sfd <close> = assert(socket.create("tcp"))
        -- associate 应该成功
        lt.assertEquals(as:associate(sfd), true)
        -- 重复 associate 同一个 socket 也应该成功（已关联）
        lt.assertEquals(as:associate(sfd), true)

        assert(sfd:bind("127.0.0.1", 0))
        assert(sfd:listen())
        local _, port = sfd:info "socket":value()

        local cfd <close> = assert(socket.create "tcp")
        lt.assertEquals(as:associate(cfd), true)
        lt.assertEquals(as:submit_connect(cfd, "127.0.0.1", port, "connect_tok"), true)

        -- cancel 不应该崩溃
        as:cancel(cfd)

        -- 等待 connect 完成（可能是成功或取消）
        wait_completion(as)
    end
end

--- 测试 writebuf 多段发送
function m.test_writebuf()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)

    -- 多段写入同一个 writebuf，单次 submit_write 发完，token 为 table
    local wb = assert(async.writebuf(64 * 1024))
    local parts = { "hel", "lo,", " wo", "rld" }
    local expected = table.concat(parts)
    for _, s in ipairs(parts) do wb:write(s) end
    local write_tok = { id = "wb_tok" }
    lt.assertEquals(as:submit_write(wb, cfd, write_tok), true)
    local _, token, status = wait_completion(as)
    lt.assertEquals(token, write_tok)
    lt.assertEquals(status, SUCCESS)

    -- 验证数据完整性：可能分多次收到（如 BSD/kqueue 逐段发送）
    local rb = assert(async.readbuf(64))
    local received = 0
    while received < #expected do
        lt.assertEquals(as:submit_read(rb, newfd, "read_tok"), true)
        local _, _, rstatus, rbytes = wait_completion(as)
        lt.assertEquals(rstatus, SUCCESS)
        received = received + rbytes
    end
    lt.assertEquals(rb:read(#expected), expected)

    -- wb:buffered()
    local wb2 = assert(async.writebuf(64 * 1024))
    lt.assertEquals(wb2:buffered(), 0)
    wb2:write("ping")
    lt.assertEquals(wb2:buffered(), 4)

    -- wb:write 背压：hwm=4, 写入4字节应返回 true
    local wb3 = assert(async.writebuf(4))
    lt.assertEquals(wb3:write("abcd"), true)   -- buffered(4) >= hwm(4)
    lt.assertEquals(wb3:write("x"), true)       -- still >= hwm

    newfd:close()
end

--- 测试 submit_poll：只监听 fd 可读性，不消费数据，token 为字符串
function m.test_submit_poll()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)

    -- 对服务端 fd 提交 poll 请求
    assert(as:associate(newfd))
    local poll_tok = "poll"
    lt.assertEquals(as:submit_poll(newfd, poll_tok), true)

    -- 客户端发送数据，触发服务端 fd 可读
    local wb = assert(async.writebuf(64 * 1024))
    wb:write("poll_test")
    local write_tok = "write"
    lt.assertEquals(as:submit_write(wb, cfd, write_tok), true)

    -- 收集 write 和 poll 两个 completions，顺序不确定
    local results = {}
    local timeout = 1000
    local start = time.monotonic()
    while #results < 2 and time.monotonic() - start < timeout do
        for op, token, status, bytes, errcode in as:wait(100) do
            results[#results+1] = { op = op, token = token, status = status, bytes = bytes, errcode = errcode }
        end
    end
    lt.assertEquals(#results, 2)

    -- 按 token 找到 poll completion
    local poll_result
    for _, r in ipairs(results) do
        if r.token == poll_tok then
            poll_result = r
            break
        end
    end
    lt.assertIsTable(poll_result)
    lt.assertEquals(poll_result.status, SUCCESS)
    lt.assertEquals(poll_result.bytes, 0)   -- poll 不消费数据，bytes 为 0
    lt.assertEquals(poll_result.errcode, 0)

    -- 验证数据未被消费：仍然可以正常读取
    local rb = assert(async.readbuf(64))
    local read_tok = "read"
    lt.assertEquals(as:submit_read(rb, newfd, read_tok), true)
    local _, token, status, bytes = wait_completion(as)
    lt.assertEquals(token, read_tok)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 9) -- "poll_test" 长度为 9
    lt.assertEquals(rb:read(9), "poll_test")

    newfd:close()
end

--- 测试 submit_poll 配合 channel fd
function m.test_submit_poll_channel()
    local channel = require "bee.channel"
    channel.create "poll_test_ch"
    local ch = channel.query "poll_test_ch"

    local as <close> = assert(async.create(64))
    assert(as:associate(ch:fd()))

    -- 提交 poll 请求，token 为 table
    local poll_tok = { ch = "poll_test_ch" }
    lt.assertEquals(as:submit_poll(ch:fd(), poll_tok), true)

    -- 向 channel push 数据，触发 fd 可读
    ch:push("hello", 42)

    -- 等待 poll completion
    local got_op, got_tok, got_status, got_bytes, got_errcode
    local deadline2 = time.monotonic() + 1000
    while not got_op and time.monotonic() < deadline2 do
        for op, token, status, bytes, errcode in as:wait(100) do
            got_op      = op
            got_tok     = token
            got_status  = status
            got_bytes   = bytes
            got_errcode = errcode
        end
    end
    lt.assertEquals(got_tok, poll_tok)
    lt.assertEquals(got_status, SUCCESS)
    lt.assertEquals(got_bytes, 0)
    lt.assertEquals(got_errcode, 0)

    -- 验证数据未被消费：channel pop 仍然可以取到数据
    local ok, msg, num = ch:pop()
    lt.assertEquals(ok, true)
    lt.assertEquals(msg, "hello")
    lt.assertEquals(num, 42)

    channel.destroy "poll_test_ch"
end

--- 测试 stop
function m.test_stop()
    local as = assert(async.create(64))
    lt.assertEquals(as:stop(), true)
end

--- 测试 close 标记（__close 元方法）
function m.test_close()
    do
        local as <close> = async.create(64)
        -- 离开作用域时应该自动调用 stop
    end
    -- 没有崩溃即通过
end

--- 测试 submit_readv：ring_buf 回绕时一次提交两段
-- 场景：先收满 (cap-2) 字节使 tail 接近末尾，消费后再用 submit_readv
-- 此时 write_ptr 指向末尾偏移 (cap-2)，write_len=2，free_cap=cap；
-- submit_read 会构造两段 iobuf：第一段 2 字节到缓冲区末尾，
-- 第二段 (cap-2) 字节回绕到缓冲区头部，覆盖全部空闲空间。
function m.test_readv_wraparound()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    assert(as:associate(newfd))

    -- cap=16，先收 14 字节（cap-2）使 tail=14
    -- 先提交读再发送，确保不与后续 send 合并到同一个 TCP segment。
    local rb = assert(async.readbuf(16))
    lt.assertEquals(as:submit_read(rb, newfd, "fill"), true)
    local fill_data = string.rep("x", 14)
    cfd:send(fill_data)
    local _, _, s1, b1 = wait_completion(as)
    lt.assertEquals(s1, SUCCESS)
    -- 可能分批到达，循环直到收到全部 14 字节
    local got = b1
    while got < 14 do
        lt.assertEquals(as:submit_read(rb, newfd, "fill"), true)
        local _, _, s2, b2 = wait_completion(as)
        lt.assertEquals(s2, SUCCESS)
        got = got + b2
    end
    -- 消费掉全部数据：head 追上 tail（=14），缓冲区清空
    rb:read(14)

    -- 现在 tail=14, head=14，write_ptr 在偏移 14（=14 & 15），write_len=2，free_cap=16
    -- submit_read 会用两段：[offset14..15] + [offset0..13]
    -- 先提交读再发送，确保 submit_read 时 ring_buf 处于回绕状态
    local payload = string.rep("A", 2) .. string.rep("B", 14)  -- 16 字节，填满两段
    lt.assertEquals(as:submit_read(rb, newfd, "readv_tok"), true)
    cfd:send(payload)
    local op, tok, rs, rb2 = wait_completion(as)
    lt.assertEquals(tok, "readv_tok")
    lt.assertEquals(rs, SUCCESS)
    lt.assertEquals(op, async.OP_READV)
    -- 收到的字节数应等于发送量（16 字节 ≤ free_cap=16）
    lt.assertEquals(rb2 > 0, true)

    -- 数据完整性：循环读取直到收到全部 16 字节
    local received = rb2
    while received < 16 do
        lt.assertEquals(as:submit_read(rb, newfd, "readv_tok2"), true)
        local _, _, rs2, rb3 = wait_completion(as)
        lt.assertEquals(rs2, SUCCESS)
        received = received + rb3
    end
    lt.assertEquals(rb:read(16), payload)

    newfd:close()
end

--- 测试 writev 批量提交：多个 entry 一次 submit_write，只产生一次 completion
function m.test_writev_batch()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    assert(as:associate(newfd))

    -- 多个小 entry 写入同一个 writebuf，一次提交应合并为单次 OP_WRITEV completion
    local wb = assert(async.writebuf(64 * 1024))
    local parts = { "aaa", "bbb", "ccc", "ddd", "eee" }
    local expected = table.concat(parts)
    for _, s in ipairs(parts) do wb:write(s) end

    local write_tok = "batch_write"
    lt.assertEquals(as:submit_write(wb, cfd, write_tok), true)
    local op, token, wstatus = wait_completion(as)
    lt.assertEquals(token, write_tok)
    lt.assertEquals(wstatus, SUCCESS)
    lt.assertEquals(op, async.OP_WRITEV)

    -- 验证数据完整性：循环读取直到收满 #expected 字节
    local rb = assert(async.readbuf(64))
    local received = 0
    while received < #expected do
        lt.assertEquals(as:submit_read(rb, newfd, "r"), true)
        local _, _, rstatus, rbytes = wait_completion(as)
        lt.assertEquals(rstatus, SUCCESS)
        received = received + rbytes
    end
    lt.assertEquals(rb:read(#expected), expected)

    newfd:close()
end
