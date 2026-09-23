# bee.filewatch

文件系统监控，底层 inotify / FSEvents / ReadDirectoryChangesW。签名见 `meta/filewatch.lua`，行为契约见 `test/test_filewatch.lua`。

## 要点

- `select()` 是**非阻塞轮询**：没有事件时立即返回 `nil`，需要自己 `thread.sleep` 再试。
- 事件类型只有 `"modify"` 和 `"rename"` 两种（创建、删除、重命名都落在 `rename` 上）。
- `add(path)` 只接受字符串（`fs.path` 要先 `:string()`），内部会转绝对路径；可多次调用添加多个根。
- `set_filter(fn)` 的 `fn` 收到路径字符串、返回 `true` 表示接受该事件；传 `nil` 清除过滤器。

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
fw:add(root:string())

while true do
    local kind, path = fw:select()
    if kind then
        print(kind, path)             -- "modify"/"rename" + 变更路径
    else
        thread.sleep(20)              -- 空转等待
    end
end
```

「收到一批事件就停」的写法（`test_filewatch:test_2`）——`select` 连续返回若干次 `nil` 即认为这一轮事件收完：

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

- 事件可能重复或漏报（平台差异），消费端要去重并允许重试；测试里用 `has(list, v)` 去重。
- 目录符号链接、指向自身的符号链接是已知边界情况，`test_symlink` 只验证不崩溃。
- FreeBSD / OpenBSD / NetBSD 上整个 filewatch 测试组被 `lt.skip "filewatch"` 跳过。
- 要「等事件」而不是轮询时，把它接到 `bee.epoll` / `bee.async` 的事件循环上，避免忙等。
