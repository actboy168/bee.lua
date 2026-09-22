# bee.filesystem

`require "bee.filesystem"`，对应 `meta/filesystem.lua`、`test/test_filesystem.lua`。

## 路径对象 bee.fspath

`fs.path(p)` 创建；所有接受路径的接口同时接受字符串。

```lua
local fs = require "bee.filesystem"
local p = fs.path "a/b/c.ext"
p:string()            -- "a/b/c.ext"（Windows 下分隔符统一为 /）
p:filename()          --> bee.fspath "c.ext"
p:parent_path()       --> "a/b"
p:stem()              --> "c"
p:extension()         --> ".ext"
p:is_absolute() / p:is_relative()
p:remove_filename() / p:replace_filename(x) / p:replace_extension(".lua")
p:lexically_normal()
```

运算符：`a / b` 路径拼接（加分隔符），`a .. b` 直接拼接。

```lua
local root = fs.absolute("./temp/"):lexically_normal()
fs.create_directories(root / "dir")     -- temp/dir
```

## 查询与操作

```lua
fs.status(p) / fs.symlink_status(p)     --> bee.file_status（:type() / :exists() / :is_directory() / :is_regular_file()）
fs.exists / fs.is_directory / fs.is_regular_file / fs.file_size
fs.create_directory(p)                  -- 已存在返回 false
fs.create_directories(p)                -- 递归创建
fs.rename(from, to) / fs.remove(p)      -- remove 对不存在的路径返回 false
fs.remove_all(p)                        -- 递归删除，返回删除数量
fs.copy(from, to [, options]) / fs.copy_file(from, to [, options])
fs.absolute(p) / fs.canonical(p) / fs.relative(p [, base])
fs.current_path([p])                    -- 无参返回当前 CWD（fspath），有参则切换
fs.temp_directory_path()
fs.last_write_time(p [, t])             -- 秒级 Unix 时间戳，读写两用
fs.permissions(p [, perms, options])     -- 读写两用，位标志
fs.space(p)                             --> { capacity, free, available }（字节）
fs.create_symlink(target, link) / fs.create_directory_symlink / fs.create_hard_link
```

`file_status:type()` 取值：`"none"｜"not_found"｜"regular"｜"directory"｜"symlink"｜"block"｜"character"｜"fifo"｜"socket"｜"junction"｜"unknown"`。

## 目录遍历

`fs.pairs(dir)` 非递归、`fs.pairs_r(dir)` 递归。迭代产出 `(bee.fspath, bee.directory_entry)`；失败时**抛错**，目录不存在同样抛错。

```lua
for path, entry in fs.pairs(fs.path "temp") do
    print(path:string(), entry:type(), entry:file_size(), entry:last_write_time())
end
```

`directory_entry` 提供 `:path()`、`:refresh()`、`:status()`、`:symlink_status()`、`:type()`、`:exists()`、`:is_directory()`、`:is_regular_file()`、`:last_write_time()`、`:file_size()`。

递归累加（`test_fs:test_copy_dir` 模式）：

```lua
local function each_directory(dir, result)
    result = result or {}
    for path, status in fs.pairs(fs.path(dir)) do
        if status:is_directory() then each_directory(path, result) end
        result[path:string()] = true
    end
    return result
end
```

## 选项位标志

- `fs.copy_options.{none, skip_existing, overwrite_existing, update_existing, recursive, copy_symlinks, skip_symlinks, directories_only, create_symlinks, create_hard_links}`
- `fs.perm_options.{replace, add, remove, nofollow}`
- `fs.directory_options.{none, follow_directory_symlink, skip_permission_denied}`

```lua
fs.copy(fs.path "temp", fs.path "temp1",
        fs.copy_options.overwrite_existing | fs.copy_options.recursive)
```

## 注意事项

- 路径对象与字符串互转常见写法：`if type(filename) == "userdata" then filename = filename:string() end`。
- 测试里所有文件操作都在 `fs.temp_directory_path() / "test_bee"` 下进行（见 `test/test.lua`），临时目录用完 `pcall(fs.remove_all, dir)` 清理。
- 符号链接相关用例先 `if not supported "symlink" then return end`。
- Windows 上符号链接/hardlink 需权限，`supported.lua` 的探测方式即 `pcall(fs.create_symlink, ...)`。
