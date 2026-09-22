---
name: bee-sys
description: 用 bee.sys 获取可执行文件/动态库路径、解析文件完整路径、创建进程级文件锁。当需要定位自身可执行文件、防重入单实例锁或规范化路径时使用。
---

# bee.sys

`require "bee.sys"`，对应 `meta/sys.lua`、`test/test_sys.lua`。

## API

```lua
local sys = require "bee.sys"

sys.exe_path()        --> bee.fspath | nil, err   -- 当前可执行文件路径
sys.dll_path()        --> bee.fspath | nil, err   -- 当前动态库(bee.dll/so)路径
sys.fullpath(path)    --> bee.fspath | nil, err   -- 解析符号链接后的完整路径
sys.filelock(path)    --> file* | nil, err        -- 独占文件锁；句柄即锁，close 即解锁
```

## 文件锁

跨进程互斥（`test_sys:test_filelock_1`）：

```lua
local sys = require "bee.sys"
local fs = require "bee.filesystem"

local f1 = assert(sys.filelock "temp.lock")   -- 拿到锁
assert(sys.filelock "temp.lock" == nil)       -- 同进程/其他进程再取都是 nil（不是 error）
f1:close()                                    -- 关闭句柄 = 释放锁
local f2 = assert(sys.filelock "temp.lock")
f2:close()
fs.remove "temp.lock"
```

跨进程验证（`test_sys:test_filelock_2` 用 `shell:runlua` 起子进程）：子进程拿锁后，父进程 `sys.filelock` 返回 `nil`；子进程退出（句柄关闭）后父进程即可获取。

## 用法：定位自身与路径规范化

```lua
local exe = sys.exe_path():string()
local dll = sys.dll_path()
local real = sys.fullpath("some/rel/path"):string()
```

`test/shell.lua` 里定位当前 Lua 解释器即用 `fs.absolute(fs.path(arg[i+1]))`（配合 `arg` 负数索引），可作为参考。

## 注意事项

- 三个路径函数返回的是 `bee.fspath`，需要字符串时 `:string()`。
- 失败返回 `nil, err`，不要用 `assert` 之外的方式跳过错误。
- 文件锁是**独占**语义，同进程重复加锁同样返回 `nil`（测试明确断言），别用它做可重入锁。
- 锁文件本身会被创建，用完自行 `fs.remove`。
