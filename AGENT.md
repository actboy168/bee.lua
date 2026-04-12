# AGENT.md

本文件为 AI 编码助手在此仓库中工作时提供指引。

## 项目简介

**bee.lua** 是一个跨平台 Lua 扩展库，为 Lua 5.4 和 5.5 提供系统级原生绑定，封装了异步 I/O、网络、子进程管理、多线程、文件监控等操作系统 API，以统一的 Lua 接口对外暴露。

## 构建命令

项目使用 **luamake**（基于 Lua 的构建系统），底层依赖 **Ninja**。

```bash
luamake                    # 编译 + 测试
luamake -notest            # 只编译，跳过测试
luamake -EXE lua           # 同时构建 Lua 可执行文件
luamake -arch x86_64       # 指定目标架构
luamake -mode debug        # Debug 构建（默认 release）
luamake -sanitize          # 开启 address sanitizer
luamake --analyze          # 静态分析
```

## 测试命令

```bash
luamake test               # 只测试（不重新编译）
luamake test -v            # 只测试，详细输出
luamake test -v <pattern>  # 只运行名称匹配 pattern 的测试用例
```

测试文件位于 `test/`，使用 **ltest** 框架。入口 `test/test.lua` 会按平台加载对应的测试文件。

## 测试规范（必须遵守）

- **每次完成代码修改后，必须先 `luamake -notest` 编译，再 `luamake test -v` 运行全量测试，确保全部通过后才能结束任务。**
- **新增或修改 API 时，必须同步更新 `meta/` 目录下对应的 LuaLS 类型注解文件。**
- **新增 API 时，必须同步在 `test/` 目录下对应的测试文件中增加测试用例。**
- 调试单个测试用例时，可用 `luamake test -v <pattern>` 快速定位，最终仍须全量测试通过。
- 测试失败时，必须修复代码直至全部通过，不得跳过或忽略失败的测试。

## 代码格式化

```bash
luamake lua compile/clang-format.lua   # 用 clang-format 格式化 C++ 代码
```

## 架构

### 层次结构

```
Lua 用户代码
    └── Lua 运行时 (3rd/lua54/ 或 3rd/lua55/)
        └── binding/lua_*.cpp        ← C++ ↔ Lua FFI 桥接层（每模块一个文件）
            └── bee/*.cpp/h           ← 核心 C++ 库（平台无关抽象）
                └── 操作系统 API      ← IOCP / io_uring / GCD / kqueue / epoll
```

### 关键目录

- **`bee/`** — 核心 C++ 库，每个子目录对应一个功能域：
  - `bee/net/` — Socket、网络端点、IP 工具
  - `bee/async/` — 平台原生异步 I/O（Windows: IOCP，Linux: io_uring/epoll，macOS: GCD）
  - `bee/subprocess/` — 子进程管理，支持 stdin/stdout/stderr 管道
  - `bee/filewatch/` — 文件系统监控（inotify / FSEvents / ReadDirectoryChangesW）
  - `bee/thread/` — 线程、自旋锁、原子操作、信号量
  - `bee/lua/` — Lua 绑定基础设施（binding.h、module.h、错误处理）
  - `bee/sys/` — 路径、文件句柄、错误码的平台抽象
  - `bee/crash/` — 栈展开与崩溃报告

- **`binding/`** — 每个模块对应一个 `lua_*.cpp`，负责注册 Lua 模块、类型转换和 userdata 生命周期管理。

- **`compile/`** — 用 Lua 编写的构建配置。`common.lua` 包含编译器标志和 Lua 版本选择；`make.lua` 是入口。

- **`meta/`** — EmmyLua 类型注解，供 IDE 使用，记录每个 `bee.*` 模块的公开 API。

- **`test/`** — 每个模块一个测试文件，均为 Lua 脚本。

- **`3rd/`** — 第三方依赖：`lua54/`、`lua55/`、`fmt/`（格式化）、`filesystem.h`（backport）、`lua-seri`（序列化）。

- **`bootstrap/`** — 预加载 bee 库的独立启动器可执行文件。

### 平台特定代码模式

平台差异尽量通过文件命名而非 `#ifdef` 来区分。例如异步 I/O 拆分为独立文件：
- `async_uring_linux.cpp` — Linux (io_uring)
- `async_epoll_linux.cpp` — Linux (epoll 回退)
- `async_macos.cpp` — macOS (GCD)
- `async_win.cpp` — Windows (IOCP)

### Lua 模块（均在 `bee.*` 命名空间下）

`socket`、`subprocess`、`thread`、`async`、`filesystem`、`filewatch`、`channel`、`epoll`、`select`、`serialization`、`time`、`crash`、`debugging`、`platform`、`sys`、`windows`

### 自定义 Lua 补丁

vendored 的 Lua 源码已打补丁（见 `3rd/lua-patch/`），Lua 5.4 和 5.5 均适用的补丁包括：
- Windows 上支持 ANSI 转义码
- Windows 上使用 UTF-8 字符串编码
- Windows 上使用快速 setjmp
- 错误/resume/yield 钩子（供调试器使用）
- Debug 模式下禁用尾调用
- Debug 构建中启用 `lua_assert`

## CI 矩阵

测试平台覆盖：Windows（x86、x86_64、Clang、MinGW）、macOS（多版本，Intel+ARM）、Linux（Ubuntu 22.04/24.04、ARM）、FreeBSD、OpenBSD、NetBSD，以及通过 QEMU 运行的 ARMv7/RISC-V。详见 `.github/workflows/test.yml`。
