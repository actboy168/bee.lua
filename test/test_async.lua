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
        for reqid, status, bytes, errcode in as:wait(100) do
            return reqid, status, bytes, errcode
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

--- 测试 TCP write 和 read
function m.test_tcp_write_read()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    local wb = assert(async.writebuf(64 * 1024))
    wb:write("hello")
    lt.assertEquals(as:submit_write(wb, cfd, 1), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 0) -- writebuf completion always returns 0 bytes

    -- 提交流式读操作
    local rb = assert(async.readbuf(64))
    lt.assertEquals(as:submit_read(rb, newfd, 2), true)
    reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 2)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 5)
    -- 从 ring buffer 精确读取
    lt.assertEquals(rb:read(5), "hello")

    newfd:close()
end

--- 测试 TCP accept
function m.test_tcp_accept()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    lt.assertEquals(as:submit_accept(sfd, 1), true)

    -- 连接到服务器以触发 accept
    local cfd <close> = SimpleClient(as, "tcp", "127.0.0.1", port)

    local reqid, status, newfd = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    -- accept 完成后返回新的 socket userdata
    lt.assertIsUserdata(newfd)

    -- 关闭 accepted fd
    newfd:close()
end

--- 测试 TCP connect
function m.test_tcp_connect()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    local cfd <close> = assert(socket.create "tcp")
    assert(as:associate(cfd))
    lt.assertEquals(as:submit_connect(cfd, "127.0.0.1", port, 1), true)

    local reqid, status = wait_completion(as)
    lt.assertEquals(reqid, 1)
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

    -- 提交文件读操作
    lt.assertEquals(as:submit_file_read(rf, 128, 0, 1), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, "hello async file io")

    rf:close()

    -- 打开文件用于写入，关联到异步实例
    local wf = assert(io.open(filepath:string(), "wb"))
    assert(as:associate_file(wf))
    lt.assertEquals(as:submit_file_write(wf, "written by async", 0, 2), true)
    reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 2)
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

    -- 从偏移量 10 读取
    local rf = assert(io.open(filepath:string(), "rb"))
    assert(as:associate_file(rf))

    lt.assertEquals(as:submit_file_read(rf, 6, 10, 1), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 1)
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
    lt.assertEquals(as:submit_read(rb, newfd, 1), true)
    local reqid, status = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, CLOSE)

    newfd:close()
end

--- 测试多个并发请求
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

    -- 提交多个写操作
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
        for reqid, status, bytes in as:wait(100) do
            results[#results+1] = { reqid = reqid, status = status, bytes = bytes }
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
    lt.assertEquals(as:submit_write(wb, cfd, 1), true)
    wait_completion(as)  -- write done

    -- 投递 stream read
    lt.assertEquals(as:submit_read(rb, newfd, 2), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 2)
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
        lt.assertEquals(as:submit_read(rb, newfd, 2), true)
        local _, status = wait_completion(as)
        lt.assertEquals(status, SUCCESS)
    end

    local function send(data, reqid)
        local w = assert(async.writebuf(64 * 1024))
        w:write(data)
        lt.assertEquals(as:submit_write(w, cfd, reqid), true)
        wait_completion(as)
    end

    -- 默认分隔符 \r\n
    send("hello\r\nworld\r\n", 1)
    recv()
    lt.assertEquals(rb:readline(), "hello\r\n")
    lt.assertEquals(rb:readline(), "world\r\n")
    lt.assertEquals(rb:readline(), nil)

    -- 自定义分隔符 \n
    send("foo\nbar\n", 3)
    recv()
    lt.assertEquals(rb:readline("\n"), "foo\n")
    lt.assertEquals(rb:readline("\n"), "bar\n")
    lt.assertEquals(rb:readline("\n"), nil)

    -- 多字节自定义分隔符
    send("a|b|c|b|", 5)
    recv()
    lt.assertEquals(rb:readline("|b|"), "a|b|")
    lt.assertEquals(rb:readline("|b|"), "c|b|")
    lt.assertEquals(rb:readline("|b|"), nil)

    -- 不完整行返回 nil，read() 仍可取出
    send("no-sep", 7)
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
        lt.assertEquals(as:submit_connect(cfd, "127.0.0.1", port, 1), true)

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

    -- 多段写入同一个 writebuf，单次 submit_write 发完
    local wb = assert(async.writebuf(64 * 1024))
    local parts = { "hel", "lo,", " wo", "rld" }
    local expected = table.concat(parts)
    for _, s in ipairs(parts) do wb:write(s) end
    lt.assertEquals(as:submit_write(wb, cfd, 1), true)
    local reqid, status = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)

    -- 验证数据完整性：可能分多次收到（如 BSD/kqueue 逐段发送）
    local rb = assert(async.readbuf(64))
    local received = 0
    while received < #expected do
        lt.assertEquals(as:submit_read(rb, newfd, 2), true)
        local _, rstatus, rbytes = wait_completion(as)
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

--- 测试 submit_poll：只监听 fd 可读性，不消费数据
function m.test_submit_poll()
    local as <close> = assert(async.create(64))
    local sfd <close> = SimpleServer(as, "tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient(as, "tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)

    -- 对服务端 fd 提交 poll 请求
    assert(as:associate(newfd))
    lt.assertEquals(as:submit_poll(newfd, 1), true)

    -- 客户端发送数据，触发服务端 fd 可读
    local wb = assert(async.writebuf(64 * 1024))
    wb:write("poll_test")
    lt.assertEquals(as:submit_write(wb, cfd, 2), true)

    -- 收集 write(reqid=2) 和 poll(reqid=1) 两个 completions，顺序不确定
    local results = {}
    local timeout = 1000
    local start = time.monotonic()
    while #results < 2 and time.monotonic() - start < timeout do
        for reqid, status, bytes, errcode in as:wait(100) do
            results[#results+1] = { reqid = reqid, status = status, bytes = bytes, errcode = errcode }
        end
    end
    lt.assertEquals(#results, 2)

    -- 按 reqid 找到 poll completion
    local poll_result
    for _, r in ipairs(results) do
        if r.reqid == 1 then
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
    lt.assertEquals(as:submit_read(rb, newfd, 3), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 3)
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

    -- 提交 poll 请求
    lt.assertEquals(as:submit_poll(ch:fd(), 1), true)

    -- 向 channel push 数据，触发 fd 可读
    ch:push("hello", 42)

    -- 等待 poll completion
    local reqid, status, bytes, errcode = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 0)
    lt.assertEquals(errcode, 0)

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
