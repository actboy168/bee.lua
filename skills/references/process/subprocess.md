# bee.subprocess

子进程与管道。签名见 `meta/subprocess.lua`，行为契约见 `test/test_subprocess.lua`，测试脚手架见 `test/shell.lua`。

## 要点

- `spawn(args)` 的 `args[1]` 是程序路径，其后为参数（数组可嵌套，会展平）；`spawn` 失败返回 `nil, err`，用 `assert` 包装。
- 只有请求了对应管道的句柄才存在：`p.stdin` / `p.stdout` / `p.stderr`，都是标准 `file*`。
- `stdin`/`stdout`/`stderr` 传 `true` 建管道、传 `file*` 直接接文件；`stderr = "stdout"` 表示共享标准输出。
- `env` 是**覆盖**语义：缺省继承父进程，键值为 `false` 表示删除该变量。
- `wait()` 之后 `is_running()` 为 `false`；用完记得 `detach()` 收尾。
- `kill(0)` 只探测存活、不真杀；被 kill 的返回码平台相关（Windows 上是 `0x0F00`）。
- 切换 `cwd` 会真正改子进程工作目录（`test_cwd` 用 `fs.current_path()` 验证）。
- `setenv` 改的是**父进程**环境，后续 `spawn` 会继承。

## 用法

```lua
local subprocess = require "bee.subprocess"

local p <close> = assert(subprocess.spawn {
    "lua", "-e", "io.write(io.read 'a')",    -- 后续元素是参数
    cwd = "some/dir",
    stdin = true, stdout = true, stderr = "stdout",
    env = { BEE_TEST = "ok", OTHER = false },
    suspended = false, detached = false,
    console = "new", hideWindow = false, searchPath = false,   -- 后三个是 Windows 专有
})
local out = p.stdout:read "a"
assert(p:wait() == 0)
assert(p:detach() == true)                   -- 测试里每个进程用完都 detach
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

## 注意事项

- 用文件重定向时 `p.stdout` 就是传入的那个 `file*`（`assert(p.stdout == f)`），父进程 `close` 后子进程仍能写。
- `subprocess.peek(file)` 探测管道可读字节数；`subprocess.quotearg(arg)` 处理空格/引号转义；`subprocess.get_id()` 取当前 pid。
- Windows 下 `windows.filemode(io.stdin, "b")` 可关掉 CRT 的 CRLF 转换。
- 测试统一用 `shell:runlua(script, options)`（`test/shell.lua`）起带正确 `package.cpath` 的 Lua 子进程，`options` 就是上面的 spawn 表，`options[1]` 或 `"_"` 用于插入额外 argv。
