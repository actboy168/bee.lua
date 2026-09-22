# bee.serialization

`require "bee.serialization"`，对应 `meta/serialization.lua`、`test/test_serialization.lua`。

## API

```lua
local seri = require "bee.serialization"

seri.pack(...)        --> lightuserdata   -- 需要 unpack 释放
seri.packstring(...)  --> string
seri.unpack(data)     --> ...             -- 接受 lightuserdata | string | userdata | function
seri.lightuserdata(ud) --> lightuserdata
```

```lua
local data = seri.packstring(1, { A = { B = "C" } }, true)
local a, t, b = seri.unpack(data)
```

## 支持的类型

`nil`、`boolean`、`number`、`string`、`table`、**light C function**。

**引用共享会保留**（`test_seri:test_ref`）：同一张表被多处引用，反序列化后仍共享同一份：

```lua
local N = 10
local t = {}
for i = 1, N do t[i] = {} end
for i = 1, N do for j = 1, N do t[i][j] = t[j] end end
local newt = seri.unpack(seri.pack(t))
for i = 1, N do
    for j = 1, N do
        assert(newt[i][j] == newt[j])      -- 解出来仍指向同一张表
    end
end
```

## 不支持的类型与报错文案（固定字符串，测试逐字断言）

| 输入 | 错误消息 |
|------|----------|
| 普通 Lua function | `Only light C function can be serialized` |
| coroutine（thread） | `Unsupport type thread to serialize` |
| userdata（如 `io.stdout`） | `Unsupport type userdata to serialize` |

```lua
seri.pack(require)                 -- OK：require 是 light C function
seri.pack(os.clock)                -- OK
seri.pack(function () end)         -- error: Only light C function can be serialized
seri.pack(coroutine.create(f))     -- error: Unsupport type thread to serialize
seri.pack(io.stdout)               -- error: Unsupport type userdata to serialize
```

## 注意事项

- 这是 `bee.thread` 参数传递和 `bee.channel` push/pop 的底层实现，限制完全一致。
- `pack` 返回 lightuserdata，注意生命周期；只要跨线程传值用 `packstring` 更安全。
- 不支持的类型在 `pack` 与 `packstring` 上行为一致（测试对两者都断言）。
