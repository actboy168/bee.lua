local lt = require "ltest"
local async = require "bee.async"
local socket = require "bee.socket"
local time = require "bee.time"
local select = require "bee.select"

local m = lt.test "async"

local SUCCESS <const> = async.SUCCESS
local CLOSE <const> = async.CLOSE
local ERROR <const> = async.ERROR
local CANCEL <const> = async.CANCEL

local function SimpleServer(protocol, ...)
    local fd = assert(socket.create(protocol))
    assert(fd:bind(...))
    assert(fd:listen())
    return fd
end

local function SimpleClient(protocol, ...)
    local fd = assert(socket.create(protocol))
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
    local as <close> = async.create(64)
    local count = 0
    for _ in as:poll() do
        count = count + 1
    end
    lt.assertEquals(count, 0)
end

--- 测试 wait 超时
function m.test_wait_timeout()
    local as <close> = async.create(64)
    local start = time.monotonic()
    local count = 0
    for _ in as:wait(100) do
        count = count + 1
    end
    local elapsed = time.monotonic() - start
    lt.assertEquals(count, 0)
    -- 超时应该大约在100ms左右
    lt.assertEquals(elapsed >= 50, true)
    lt.assertEquals(elapsed < 500, true)
end

--- 测试 TCP write 和 read
function m.test_tcp_write_read()
    local as <close> = async.create(64)
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient("tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)
    lt.assertEquals(as:submit_write(cfd, "hello", 1), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 5)

    -- 提交读操作
    lt.assertEquals(as:submit_read(newfd, 64, 2), true)
    reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 2)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, "hello")

    newfd:close()
end

--- 测试 TCP accept
function m.test_tcp_accept()
    local as <close> = async.create(64)
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    lt.assertEquals(as:submit_accept(sfd, 1), true)

    -- 连接到服务器以触发 accept
    local cfd <close> = SimpleClient("tcp", "127.0.0.1", port)

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
    local as <close> = async.create(64)
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    local cfd <close> = assert(socket.create "tcp")
    lt.assertEquals(as:submit_connect(cfd, "127.0.0.1", port, 1), true)

    local reqid, status = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
end

--- 测试文件读写
function m.test_file_read_write()
    local as <close> = async.create(64)
    local fs = require "bee.filesystem"
    local filepath = fs.current_path() / "test_async_file.txt"

    -- 创建测试文件
    do
        local f = assert(io.open(filepath:string(), "wb"))
        f:write("hello async file io")
        f:close()
    end

    -- 打开文件用于读取
    local rf = assert(io.open(filepath:string(), "rb"))

    -- 提交文件读操作
    lt.assertEquals(as:submit_file_read(rf, 128, 0, 1), true)
    local reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, "hello async file io")

    rf:close()

    -- 打开文件用于写入
    local wf = assert(io.open(filepath:string(), "wb"))
    lt.assertEquals(as:submit_file_write(wf, "written by async", 0, 2), true)
    reqid, status, bytes = wait_completion(as)
    lt.assertEquals(reqid, 2)
    lt.assertEquals(status, SUCCESS)
    lt.assertEquals(bytes, 16)  -- "written by async" 长度为 16

    wf:close()

    -- 验证写入内容
    local fr = assert(io.open(filepath:string(), "rb"))
    lt.assertEquals(fr:read "*a", "written by async")
    fr:close()

    fs.remove(filepath)
end

--- 测试文件偏移量读写
function m.test_file_read_write_offset()
    local as <close> = async.create(64)
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
    local as <close> = async.create(64)
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local cfd <close> = SimpleClient("tcp", sfd:info "socket")
    local newfd = wait_accept(sfd)

    -- 关闭客户端
    cfd:close()

    -- 在服务端提交读操作应该收到 close 状态
    lt.assertEquals(as:submit_read(newfd, 64, 1), true)
    local reqid, status = wait_completion(as)
    lt.assertEquals(reqid, 1)
    lt.assertEquals(status, CLOSE)

    newfd:close()
end

--- 测试多个并发请求
function m.test_multiple_requests()
    local as <close> = async.create(64)
    local sfd <close> = SimpleServer("tcp", "127.0.0.1", 0)
    local _, port = sfd:info "socket":value()

    -- 创建多个客户端连接
    local clients = {}
    local servers = {}
    for i = 1, 3 do
        clients[i] = SimpleClient("tcp", "127.0.0.1", port)
        servers[i] = wait_accept(sfd)
    end

    -- 提交多个写操作
    for i = 1, 3 do
        lt.assertEquals(as:submit_write(clients[i], "msg" .. i, i), true)
    end

    -- 收集所有完成事件
    local results = {}
    local deadline = time.monotonic() + 1000
    while #results < 3 and time.monotonic() < deadline do
        for reqid, status, bytes in as:wait(100) do
            results[#results + 1] = { reqid = reqid, status = status, bytes = bytes }
        end
    end
    lt.assertEquals(#results, 3)

    -- 验证所有请求都成功
    for _, r in ipairs(results) do
        lt.assertEquals(r.status, SUCCESS)
        lt.assertEquals(r.bytes, 4)  -- "msg1"/"msg2"/"msg3" 均为4字节
    end

    for i = 1, 3 do
        clients[i]:close()
        servers[i]:close()
    end
end

--- 测试 stop
function m.test_stop()
    local as = async.create(64)
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
