# bee.sys

系统工具：自身路径与文件锁。签名见 `meta/sys.lua`，行为契约见 `test/test_sys.lua`。

## 要点

- `exe_path` / `dll_path` / `fullpath` 返回的是 **`bee.fspath`**，要字符串时 `:string()`；失败返回 `nil, err`。
- `filelock(path)` 是**独占**语义：拿不到锁返回 `nil`（不是 `error`）；返回的 `file*` 既是句柄也是锁，`close()` 即解锁。

## 用法

跨进程互斥（`test_sys:test_filelock_1`）：

```lua
local sys = require "bee.sys"
local fs = require "bee.filesystem"

local f1 = assert(sys.filelock "temp.lock")   -- 拿到锁
assert(sys.filelock "temp.lock" == nil)       -- 同进程再取也是 nil
f1:close()                                    -- 关闭句柄 = 释放锁
local f2 = assert(sys.filelock "temp.lock")
f2:close()
fs.remove "temp.lock"
```

跨进程验证见 `test_sys:test_filelock_2`：用 `shell:runlua` 起子进程拿锁，父进程随即返回 `nil`；子进程退出后父进程即可获取。

定位自身与路径规范化：

```lua
local exe = sys.exe_path():string()
local dll = sys.dll_path()
local real = sys.fullpath("some/rel/path"):string()
```

## 注意事项

- 文件锁是**独占**的，同进程重复加锁同样返回 `nil`，别拿它当可重入锁。
- 锁文件用完自行 `fs.remove`。
- `test/shell.lua` 里定位当前 Lua 解释器用的是 `fs.absolute(fs.path(arg[i+1]))`（配合 `arg` 负数索引），需要类似逻辑时可以参考。
