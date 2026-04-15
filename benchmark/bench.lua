-- bench.lua: 对比 epoll 后端（net_epoll.lua）和 async 后端（net_async.lua）的性能
--
-- 用法（从 benchmark/ 目录运行）：
--   lua bench.lua [并发数] [每连接消息数] [消息大小]
--
-- 始终同时运行 epoll/async × rtt/pipeline 四种组合并输出对比。

local time     = require "bee.time"

local conns    = tonumber(arg and arg[1]) or 10
local msgs     = tonumber(arg and arg[2]) or 1000
local msg_size = tonumber(arg and arg[3]) or 4096

local function load_backend(name)
    package.loaded["net_epoll"] = nil
    package.loaded["net_async"] = nil
    package.loaded["ltask"]     = nil

    local net                   = require(name == "epoll" and "net_epoll" or "net_async")
    local ltask                 = require "ltask"

    local socket                = require "bee.socket"
    local probe                 = assert(socket.create "tcp")
    assert(probe:bind("127.0.0.1", 0))
    local _, port = probe:info("socket"):value()
    probe:close()

    return net, ltask, port
end

local function run_loop(net, ltask, finish_token)
    local exit_loop = false
    net.fork(function ()
        ltask.wait(finish_token)
        exit_loop = true
    end)
    while not exit_loop do
        net.schedule() -- 耗尽所有可运行任务
        net.wait(0)    -- I/O 完成事件触发新 wakeup
        -- 若 I/O 产生了新任务，继续调度；否则阻塞等待
        if not net.schedule() and not exit_loop then
            net.wait(1)
        end
    end
    net.wait(0)
    net.schedule()
end

-- 公共框架：启动 echo 服务端，并发运行 client_fn，返回统计
local function run_bench(backend_name, conns_count, client_fn)
    local net, ltask, port = load_backend(backend_name)
    local done             = 0
    local total_msg        = 0
    local total_lat        = 0
    local errors           = 0
    local token            = {}

    local function finish(ok_msgs, lat_sum)
        total_msg = total_msg + ok_msgs
        total_lat = total_lat + (lat_sum or 0)
        done      = done + 1
        if done >= conns_count then ltask.wakeup(token) end
    end

    net.fork(function ()
        local server = assert(net.listen("tcp", "127.0.0.1", port))
        while true do
            local cli = server:accept()
            if not cli then break end
            net.fork(function ()
                while true do
                    local data = cli:recv()
                    if not data or data == "" then break end
                    net.fork(function () cli:send(data) end)
                end
                cli:close()
            end)
        end
    end)

    for _ = 1, conns_count do
        net.fork(function ()
            local fd = net.connect("tcp", "127.0.0.1", port)
            if not fd then
                errors = errors + 1
                finish(0)
                return
            end
            local ok, lat = client_fn(net, fd)
            errors = errors + (ok < 0 and -ok or 0)
            fd:close()
            finish(ok > 0 and ok or 0, lat)
        end)
    end

    local t0 = time.monotonic()
    run_loop(net, ltask, token)
    local elapsed = time.monotonic() - t0

    return {
        total_msg = total_msg,
        bytes     = total_msg * msg_size * 2,
        elapsed   = elapsed,
        errors    = errors,
        lat_avg   = total_msg > 0 and (total_lat / total_msg * 1000) or 0,
    }
end

local function run_rtt(backend_name, conns_count, msgs_count, payload_size)
    local payload = string.rep("x", payload_size)
    return run_bench(backend_name, conns_count, function (_, fd)
        local ok, lat_sum = 0, 0
        for _ = 1, msgs_count do
            local t0 = time.monotonic()
            if not fd:send(payload) then return -1, 0 end
            local echo = fd:recv(payload_size)
            if echo and #echo == payload_size then
                ok      = ok + 1
                lat_sum = lat_sum + (time.monotonic() - t0)
            else
                return -1, 0
            end
        end
        return ok, lat_sum
    end)
end

local function run_pipeline(backend_name, conns_count, msgs_count, payload_size)
    local payload = string.rep("x", payload_size)
    return run_bench(backend_name, conns_count, function (net, fd)
        net.fork(function ()
            for _ = 1, msgs_count do fd:send(payload) end
        end)
        local received, expected = 0, msgs_count * payload_size
        while received < expected do
            local r = fd:recv()
            if not r then return -1, 0 end
            received = received + #r
        end
        return msgs_count, nil
    end)
end

local function print_result(label, r)
    local s       = math.max(r.elapsed / 1000, 0.001)
    local lat_str = r.lat_avg > 0 and string.format("%7.1fµs", r.lat_avg) or "    n/a  "
    print(string.format(
        "%-16s | msgs=%7d | time=%7.1fms | %9.0f msg/s | %6.2f MB/s | lat=%s | err=%d",
        label, r.total_msg, r.elapsed,
        r.total_msg / s, r.bytes / s / 1024 / 1024,
        lat_str, r.errors
    ))
end

local backends = { "epoll", "async" }
local modes    = { "rtt", "pipeline" }
local runners  = { rtt = run_rtt, pipeline = run_pipeline }

print(string.format(
    "=== bee.lua 后端性能对比 | 并发=%d | 每连接消息=%d | 消息大小=%d 字节 | 总消息=%d ===",
    conns, msgs, msg_size, conns * msgs
))
print(string.format("%-16s | %-7s | %-10s | %-14s | %-9s | %-10s | %s",
    "后端/模式", "总消息", "耗时", "吞吐(msg/s)", "吞吐(MB/s)", "平均RTT", "错误"))
print(string.rep("-", 100))

local results = {}
for _, mode in ipairs(modes) do
    results[mode] = {}
    for _, b in ipairs(backends) do
        local label = b.."/"..mode
        local ok, r = pcall(runners[mode], b, conns, msgs, msg_size)
        if ok then
            results[mode][b] = r
            print_result(label, r)
        else
            print(string.format("%-16s | 运行失败: %s", label, tostring(r)))
        end
    end
end

print(string.rep("-", 100))
for _, mode in ipairs(modes) do
    local e, a = results[mode]["epoll"], results[mode]["async"]
    if e and a then
        local es   = math.max(e.elapsed / 1000, 0.001)
        local as_  = math.max(a.elapsed / 1000, 0.001)
        local tput = (a.total_msg / as_) / (e.total_msg / es)
        local lat  = (e.lat_avg > 0 and a.lat_avg > 0)
            and string.format(" | 延迟 %.2fx", e.lat_avg / a.lat_avg) or ""
        print(string.format("[%-8s] async 相对 epoll: 吞吐 %.2fx%s  (>1 表示 async 更好)",
            mode, tput, lat))
    end
end
