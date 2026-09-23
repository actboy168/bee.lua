# bee.platform

平台信息。签名见 `meta/platform.lua`。模块返回的是**普通表**（非类），无需 `<close>`。

## 要点

- 常用来分支的字段：`os`、`Arch`、`Compiler`、`CRT`、`DEBUG`。
- 版本号在 `os_version` 里（`{ major, minor, revision }`），另有 `CompilerVersion` / `CRTVersion` 字符串。

## 用法

```lua
local platform = require "bee.platform"

local isWindows = platform.os == "windows"
local isMinGW   = isWindows and platform.CRT == "libstdc++"

if platform.DEBUG then ... end
```

`test/test.lua` 启动时打印环境信息，是标准用法：

```lua
local v = platform.os_version
print(("OS:       %s %d.%d.%d"):format(platform.os, v.major, v.minor, v.revision))
print("Arch:     ", platform.Arch)
print("Compiler: ", platform.CompilerVersion)
print("CRT:      ", platform.CRTVersion)
print("DEBUG:    ", platform.DEBUG)
```

## 注意事项

- 测试里按平台跳过用例用 `lt.skip "module.test_name"`（`test/test_skip.lua`），按**特性**探测用 `supported "symlink"`（`test/supported.lua`，结果会缓存）——能力探测优先于平台判断。
- `supported "hardlink"` 的判定就是 `platform.os ~= "android"`。
