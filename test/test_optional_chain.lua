-- Optional chaining (?.) is a custom syntax enabled at build time by applying
-- the git patch in 3rd/lua-patch/optchain/ (luamake -optchain). The '?.' syntax
-- errors shown by LuaLS below are expected: the language server does not know
-- this extension. These tests only run when the interpreter was built with
-- the patch (see the loader in test/test.lua).
local lt = require "ltest"

local test_optchain = lt.test "optional_chain"

function test_optchain:test_field()
    local obj = { a = { b = 42 } }
    lt.assertEquals(obj?.a?.b, 42)
    local nothing
    lt.assertNil(nothing?.a?.b)
    lt.assertNil(obj?.x?.y)
    lt.assertEquals((nil)?.a, nil)
end

function test_optchain:test_field_chain()
    local obj = { a = { b = { c = 1 } } }
    lt.assertEquals(obj?.a.b.c, 1)
    local nothing
    lt.assertNil(nothing?.a.b.c)
    lt.assertEquals(obj?.a?.b.c, 1)
    lt.assertNil(obj?.x?.y?.z)
    lt.assertEquals(obj?.a?.b?.c, 1)
end

function test_optchain:test_index()
    local t = { [1] = { [2] = "hi" } }
    lt.assertEquals(t?[1]?[2], "hi")
    local nothing
    lt.assertNil(nothing?[1])
    lt.assertNil(t?[2]?[1])
    lt.assertEquals(t?[1][2], "hi")
end

function test_optchain:test_method()
    local obj = { x = 1, get = function(self) return self.x end }
    lt.assertEquals(obj?:get(), 1)
    local nothing
    lt.assertNil(nothing?:get())
end

function test_optchain:test_call()
    local f = function() return "ok" end
    lt.assertEquals(f?(), "ok")
    local nothing
    lt.assertNil(nothing?())
end

function test_optchain:test_call_args()
    local f = function(a, b, c) return a + b + c end
    lt.assertEquals(f?(1, 2, 3), 6)
    local nothing
    lt.assertNil(nothing?(1, 2, 3))
end

function test_optchain:test_call_args_short_circuit()
    -- short-circuit must not evaluate the arguments (no side effects)
    local calls = 0
    local function arg(v) calls = calls + 1; return v end
    local f = function(a, b, c) return a + b + c end
    lt.assertEquals(f?(arg(1), arg(2), arg(3)), 6)
    lt.assertEquals(calls, 3)
    local nothing
    lt.assertNil(nothing?(arg(1), arg(2), arg(3)))
    lt.assertEquals(calls, 3)  -- args not evaluated on short-circuit
end

function test_optchain:test_call_args_multi()
    local g = function(a, b) return a, b end
    local x, y = g?(10, 20)
    lt.assertEquals(x, 10)
    lt.assertEquals(y, 20)
    local nothing
    local n1, n2 = nothing?(10, 20)
    lt.assertNil(n1)
    lt.assertNil(n2)
end

function test_optchain:test_short_circuit_key()
    local calls = 0
    local function key() calls = calls + 1; return 1 end
    local nothing
    local t = { [1] = "v" }
    lt.assertEquals(nothing?[key()], nil)
    lt.assertEquals(calls, 0)
    lt.assertEquals(t?[key()], "v")
    lt.assertEquals(calls, 1)
end

function test_optchain:test_short_circuit_args()
    local calls = 0
    local function arg() calls = calls + 1; return 1 end
    local obj = { f = function(self, x) return x end }
    local nothing
    lt.assertEquals(nothing?:f(arg()), nil)
    lt.assertEquals(calls, 0)
    lt.assertEquals(obj?:f(arg()), 1)
    lt.assertEquals(calls, 1)
end

function test_optchain:test_eval_once()
    local calls = 0
    local function recv() calls = calls + 1; return { a = { b = 1 } } end
    lt.assertEquals(recv()?.a?.b, 1)
    lt.assertEquals(calls, 1)
    lt.assertNil(recv()?.x?.y)
    lt.assertEquals(calls, 2)
end

function test_optchain:test_false_not_short_circuit()
    local f = false
    lt.assertError(function () return f?.a end)
    lt.assertError(function () return f?[1] end)
end

function test_optchain:test_not_assignable()
    local ok
    ok, _ = load("obj?.a = 1")
    lt.assertTrue(not ok)
    ok, _ = load("obj?[1] = 2")
    lt.assertTrue(not ok)
    ok, _ = load("obj?:f = 3")
    lt.assertTrue(not ok)
end

function test_optchain:test_bad_syntax()
    local ok
    ok, _ = load("local a; return a?")
    lt.assertTrue(not ok)
    ok, _ = load("local a; return a ?? 1")
    lt.assertTrue(not ok)
    ok, _ = load("local a; return a?b")
    lt.assertTrue(not ok)
end

-- Multiple results: a chain ending in a call can yield several values
-- (the short-circuit path fills the whole result range with nils).

function test_optchain:test_multi_value_assign()
    local obj = { getSize = function() return 100, 200 end }
    local w, h = obj?:getSize()
    lt.assertEquals(w, 100)
    lt.assertEquals(h, 200)
    local nothing
    local a, b, c = nothing?:getSize()
    lt.assertNil(a)
    lt.assertNil(b)
    lt.assertNil(c)
end

function test_optchain:test_multi_value_method()
    local o = { pair = function(self) return 1, 2, 3 end }
    local x, y, z = o?:pair()
    lt.assertEquals(x, 1)
    lt.assertEquals(y, 2)
    lt.assertEquals(z, 3)
end

function test_optchain:test_multi_value_return()
    local obj = { getSize = function() return 7, 8 end }
    local function f()
        return obj?:getSize()
    end
    local r1, r2 = f()
    lt.assertEquals(r1, 7)
    lt.assertEquals(r2, 8)
    local function g()
        local n
        return n?:getSize()
    end
    local s1, s2 = g()
    lt.assertNil(s1)
    lt.assertNil(s2)
end

function test_optchain:test_multi_value_table()
    local obj = { getSize = function() return 5, 6 end }
    local t = { obj?:getSize() }
    lt.assertEquals(t[1], 5)
    lt.assertEquals(t[2], 6)
    local nothing
    local tn = { nothing?:getSize() }
    lt.assertEquals(#tn, 0)  -- one nil element; trailing nils don't count for #
end

function test_optchain:test_multi_value_single()
    -- Single-value contexts still collapse to one value.
    local obj = { getSize = function() return 100, 200 end }
    local s = obj?:getSize()
    lt.assertEquals(s, 100)
end

function test_optchain:test_multi_value_args()
    -- A chain ending in a call, used as call arguments, yields exactly
    -- the produced values (short-circuit: exactly one nil argument).
    local function count(...) return select("#", ...) end
    local obj = { getSize = function() return 100, 200 end }
    lt.assertEquals(count(obj?:getSize()), 2)
    local nothing
    lt.assertEquals(count(nothing?:getSize()), 1)
end

function test_optchain:test_extra_values_discarded()
    -- More expressions than variables: the last multi-return optional-chain
    -- call is discarded. This path passes 'nresults == 0' to
    -- luaK_setreturns_optchain, which must leave OP_SETTOP's B field at 0
    -- (a single nil). Regression: it used to store nresults-1 == -1 (255),
    -- overflowing the stack on short-circuit.
    local f
    local a = 1, f?()   -- f is nil → short-circuit, result discarded
    lt.assertEquals(a, 1)

    local g = function() return 10, 20 end
    local b = 1, g?()   -- non-short-circuit path (control)
    lt.assertEquals(b, 1)

    local h
    local c = 1
    c = 1, h?()         -- assignment-statement form (same adjust_assign)
    lt.assertEquals(c, 1)
end
