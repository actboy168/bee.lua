# bee.filesystem

文件系统与路径操作。签名见 `meta/filesystem.lua`，行为契约见 `test/test_filesystem.lua`。

## 要点

- 路径对象是 `bee.fspath`，`fs.path(p)` 创建；所有接口同时接受字符串。
- `a / b` 是路径拼接（会补分隔符），`a .. b` 是直接拼接；取字符串用 `:string()`。
- `fs.pairs` 非递归 / `fs.pairs_r` 递归，迭代产出 `(bee.fspath, bee.directory_entry)`；**目录不可遍历时抛错**，不是返回 `nil, err`。
- 选项是位标志：`fs.copy_options` / `fs.perm_options` / `fs.directory_options`，用 `|` 组合。
- `fs.current_path()` 无参读 CWD、有参切换；`fs.last_write_time` 与 `fs.permissions` 都是读写两用。

```lua
local fs = require "bee.filesystem"
local root = fs.absolute("./temp/"):lexically_normal()
fs.create_directories(root / "dir")

fs.copy(fs.path "temp", fs.path "temp1",
        fs.copy_options.overwrite_existing | fs.copy_options.recursive)

for path, entry in fs.pairs(fs.path "temp") do
    print(path:string(), entry:type(), entry:file_size(), entry:last_write_time())
end
```

递归累加（`test_fs:test_copy_dir` 的骨架）：

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

## 注意事项

- 路径对象与字符串互转的常见写法：`if type(filename) == "userdata" then filename = filename:string() end`。
- `fs.remove` 对不存在的路径返回 `false`，递归删除要用 `fs.remove_all`。
- 测试里所有文件操作都在 `fs.temp_directory_path() / "test_bee"` 下进行（`test/test.lua`），临时目录用完 `pcall(fs.remove_all, dir)` 清理。
- 符号链接相关用例先 `if not supported "symlink" then return end`；Windows 上 symlink / hardlink 需要权限，`supported.lua` 的探测方式就是 `pcall(fs.create_symlink, ...)`。
