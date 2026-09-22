---
name: bee-subprocess
description: 用 bee.subprocess 启动与管理子进程（spawn 配置表、stdin/stdout/stderr 管道或重定向、env/cwd、wait/kill/is_running/detach、select 批量等待、setenv/quotearg）。当需要调外部命令、跑测试子进程或用管道做进程间通信时使用。
---

# bee.subprocess

`require "bee.subprocess"`，对应 `meta/subprocess.lua`、`test/test_subprocess.lua`、`test/shell.lua`。

## spawn 配置表

```lua
local subprocess = require "bee.subprocess"
local p <close> = assert(subprocess.spawn {
    "lua", "-e", "io.write(io.read 'a')",   -- [1] 程序路径，其后为参数（数组可嵌套，会被展平）
    cwd = "some/dir",                        -- string | bee.fspath
    stdin  = true,                           -- true 建管道 | file* 直接接文件
    stdout = true,
    stderr = "stdout",                       -- true | file* | "stdout"(共享标准输出)
    env = { BEE_TEST = "ok", OTHER = false },-- false 表示删除该变量；缺省继承父进程环境
    suspended = false,                        -- 以挂起状态启动，后续 p:resume()
    detached = false,
    console = "new",                          -- Windows: "new"|"disable"|"inherit"|"detached"
    hideWindow = false,                       -- Windows 隐藏窗口
    searchPath = false,                       -- Windows 是否搜索 PATH
})
```

只有请求了对应管道的句柄才存在：`p.stdin` / `p.stdout` / `p.stderr`，都是标准 `file*`。

## 进程方法

```lua
p:wait()             --> exitcode | nil, err     -- 也用于收尸
p:kill([signum=15])  --> boolean                 -- kill(0) 只探测存活，不真杀
p:is_running()       --> boolean
p:get_id()           --> pid
p:resume()           -- 恢复 suspended 启动的进程
p:native_handle()    --> lightuserdata
p:detach()           -- 结束收尾，不再由本对象管理
```

## 典型用法

```lua
local p <close> = assert(subprocess.spawn {
    "lua", "-e", "io.write 'ok'",
    stdout = true, stderr = "stdout",
})
local out = p.stdout:read "a"          -- "ok"
assert(p:wait() == 0)
assert(p:detach() == true)             -- 测试里每个进程用完都 detach
```

管道当 stdin（`test_subprocess:test_stdio_1`）：

```lua
local p = assert(subprocess.spawn { "lua", "-e", "io.write(io.read 'a')", stdin = true, stdout = true })
assert(p:is_running())
p.stdin:write "ok"
p.stdin:close()                        -- 关闭后子进程才 EOF
assert(p:wait() == 0)
assert(p.stdout:read(2) == "ok")
assert(p.stdout:read(2) == nil)        -- 后续读到 nil
```

进程串联（把上一个的 stdout 当 stdin）：

```lua
local p1 = assert(subprocess.spawn { "lua", "-e", "io.write 'ok'", stdout = true })
local p2 = assert(subprocess.spawn { "lua", "-e", "io.write(io.read 'a')",
                                     stdin = p1.stdout, stdout = true })
p1:wait(); p2:wait()
assert(p2.stdout:read "a" == "ok")
```

批量等待（`test_subprocess:test_select`）：

```lua
while #progs > 0 do
    assert(subprocess.select(progs))          -- 等到任一进程结束
    local i = 1
    while i <= #progs do
        if progs[i]:is_running() then
            i = i + 1
        else
            assert(progs[i]:wait() == 0)
            progs[i]:detach()
            table.remove(progs, i)
        end
    end
end
```

## 其它工具

```lua
subprocess.peek(file)            --> 管道可读字节数 | nil, err
subprocess.get_id()              --> 当前进程 pid
subprocess.setenv(name, value)   -- 改**父进程**环境，后续 spawn 会继承（value 传 false 删除）
subprocess.quotearg(arg)         -- 处理空格/引号的命令行转义
```

## 注意事项

- `spawn` 失败返回 `nil, err`，用 `assert` 包装。
- `wait()` 之后 `is_running()` 为 `false`；被 kill 的进程返回码是平台相关的（Windows 上 `0x0F00`）。
- 用文件重定向时 `p.stdout` 就是传入的那个 `file*`（`assert(p.stdout == f)`），父进程 `close` 后子进程仍能写。
- Windows 下 `windows.filemode(io.stdin, "b")` 可关闭 CRT 的 CRLF 转换（`test_subprocess` 有覆盖）。
- 测试里统一通过 `shell:runlua(script, options)`（`test/shell.lua`）启动带正确 `package.cpath` 的 Lua 子进程，`options` 即上面的 spawn 表，`options[1]` 或 `"_"` 用于插入额外 argv。
- `cwd` 会真正切换子进程工作目录（`test_cwd` 用 `fs.current_path()` 验证）。
