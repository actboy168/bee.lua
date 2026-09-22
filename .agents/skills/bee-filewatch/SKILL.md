---
name: bee-filewatch
description: 用 bee.filewatch 监控文件系统变化（create、add 路径、set_recursive/set_follow_symlinks/set_filter、select 轮询 modify/rename 事件）。当需要实现热重载、构建监听或检测目录变更时使用。
---

# bee.filewatch

`require "bee.filewatch"`，对应 `meta/filewatch.lua`、`test/test_filewatch.lua`。底层：inotify / FSEvents / ReadDirectoryChangesW。

## API

```lua
local filewatch = require "bee.filewatch"

local fw = filewatch.create()
fw:add(path)                     -- 自动转绝对路径；可多次调用添加多个根
fw:set_recursive(enable)         --> boolean
fw:set_follow_symlinks(enable)   --> boolean   -- 某些平台可能不支持
fw:set_filter(fn|nil)            --> boolean   -- fn 接收路径字符串，返回 true 表示接受该事件
fw:select()                      --> type, path  -- type: "modify" | "rename"；无事件时 type == nil
```

`select()` 是非阻塞的：没有事件时立即返回 `nil`，需要自己轮询 + 睡眠。

## 用法

```lua
local filewatch = require "bee.filewatch"
local fs = require "bee.filesystem"
local thread = require "bee.thread"

local root = fs.absolute("./temp"):lexically_normal()
local fw = filewatch.create()
fw:set_recursive(true)
fw:set_follow_symlinks(true)
fw:set_filter(function (path) return true end)
fw:add(root:string())                 -- add 接收 string

while true do
    local kind, path = fw:select()
    if kind then
        print(kind, path)             -- "modify"/"rename" + 变更路径
    else
        thread.sleep(20)              -- 空转等待，测试里用重试计数退出
    end
end
```

测试里的收事件循环（`test_filewatch:test_2`）值得参考——`select` 返回 `nil` 时重试若干次即认为事件收完：

```lua
local retry = 5
local n = retry
local list = {}
while true do
    local w, v = fw:select()
    if w then
        n = retry
        list[#list+1] = v
    else
        n = n - 1
        if n < 0 then break end
        thread.sleep(20)
    end
end
```

## 注意事项

- `add` 只接受字符串路径，`fs.path` 需要先 `:string()`；路径会自动转绝对路径。
- 事件类型只有 `"modify"` 与 `"rename"` 两种（创建/删除/重命名都落在 `rename` 上）。
- 事件可能重复或漏报（平台差异），测试里用 `has(list, v)` 去重并允许重试。
- 目录符号链接、指向自身的符号链接是已知边界情况，`test_symlink` 只验证不崩溃。
- FreeBSD/OpenBSD/NetBSD 上整个 filewatch 测试组被 `lt.skip "filewatch"` 跳过。
- 需要“等事件”而不是“轮询”时，把 `select` 放进 `thread.sleep` 循环或与 `bee.epoll`/`bee.async` 的事件循环结合。
