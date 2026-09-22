# bee.platform

平台信息模块。返回的是**普通表**（非类），无需 `<close>`。

## API

| 字段 | 类型 | 说明 |
|------|------|------|
| `os` | `"windows"｜"android"｜"linux"｜"netbsd"｜"freebsd"｜"openbsd"｜"ios"｜"macos"｜"unknown"` | 操作系统 |
| `Arch` | `"x86"｜"x86_64"｜"arm"｜"arm64"｜"riscv"｜"wasm32"｜"wasm64"｜"mips64el"｜"loongarch64"｜"ppc"｜"ppc64"｜"unknown"` | 目标架构 |
| `Compiler` | `"clang"｜"msvc"｜"gcc"｜"unknown"` | 编译器 |
| `CompilerVersion` | `string` | 编译器版本 |
| `CRT` | `"msvc"｜"libstdc++"｜"libc++"｜"bionic"｜"unknown"` | C 运行时库 |
| `CRTVersion` | `string` | CRT 版本 |
| `DEBUG` | `boolean` | 是否 Debug 构建 |
| `os_version` | `{ major: integer, minor: integer, revision: integer }` | 系统版本号 |

## 用法

```lua
local platform = require "bee.platform"

local isWindows = platform.os == "windows"
local isMinGW   = isWindows and platform.CRT == "libstdc++"

if platform.DEBUG then ... end
```

`test/test.lua` 在启动时打印环境信息，是标准用法：

```lua
local v = platform.os_version
print(("OS:       %s %d.%d.%d"):format(platform.os, v.major, v.minor, v.revision))
print("Arch:     ", platform.Arch)
print("Compiler: ", platform.CompilerVersion)
print("CRT:      ", platform.CRTVersion)
print("DEBUG:    ", platform.DEBUG)
```

## 注意事项

- 测试中按平台跳过用例请用 `lt.skip "module.test_name"`（`test/test_skip.lua`），按特性探测用 `supported "symlink"`（`test/supported.lua`）。
- `supported "hardlink"` 的判定就是 `platform.os ~= "android"`。
