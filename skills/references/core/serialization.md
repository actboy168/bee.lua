# bee.serialization

跨线程/跨通道传值的序列化。签名见 `meta/serialization.lua`，行为契约见 `test/test_serialization.lua`。

## 要点

- 支持的类型：`nil`、`boolean`、`number`、`string`、`table`、**light C function**（如 `require`、`os.clock`）。
- **引用共享会保留**：同一张表被多处引用，反序列化后仍指向同一份。
- `pack` 返回 lightuserdata（需自行管理生命周期）；跨线程传值用 `packstring` 更安全。

```lua
local seri = require "bee.serialization"

local data = seri.packstring(1, { A = { B = "C" } }, true)
local a, t, b = seri.unpack(data)
```

引用共享（`test_seri:test_ref`）：

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
seri.pack(function () end)         -- error: Only light C function can be serialized
seri.pack(coroutine.create(f))     -- error: Unsupport type thread to serialize
seri.pack(io.stdout)               -- error: Unsupport type userdata to serialize
```

## 注意事项

- 这是 `bee.thread` 参数传递和 `bee.channel` push/pop 的底层实现，限制完全一致。
- `pack` 与 `packstring` 对不支持类型的报错行为一致（测试对两者都断言）。
