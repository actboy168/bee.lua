-- Optional chaining (?.) is a custom syntax enabled at build time by applying
-- the git patch in 3rd/lua-patch/optchain/ (luamake -optchain). The '?.' syntax
-- errors shown by LuaLS below are expected: the language server does not know
-- this extension. These tests only run when the interpreter was built with
-- the patch (see the loader in test/test.lua).
local lt = require "ltest"

local test_optchain = lt.test "optional_chain"

-- Codegen inspection: compile src (which must evaluate to a function), dump
-- and undump it, then decode the instruction stream. These pin the bytecode
-- the optchain patch emits (not just its runtime behavior).
-- Opcode numbers from lopcodes.h ORDER OP; identical for lua54/lua55 except
-- OP_SETTOP, which is appended after OP_EXTRAARG (number differs per version).
local OP_LOADNIL  = 8
local OP_JMP      = 56
local OP_EQ       = 57
local OP_EQK      = 60
local OP_CALL     = 68
local OP_TAILCALL = 69
local OP_SETTOP   = (_VERSION == "Lua 5.4") and 83 or 85

local function instructions(src)
    local f = assert(load(src))()
    local proto = lt.dump(f)
    local insns = {}
    for i = 1, proto.sizecode do
        local inst = proto.code[i] & 0xFFFFFFFF
        insns[i] = {
            op = inst & 0x7F,
            A  = (inst >> 7) & 0xFF,
            k  = (inst >> 15) & 1,
            B  = (inst >> 16) & 0xFF,
            C  = (inst >> 24) & 0xFF,
        }
    end
    return insns
end

local function count_op(insns, op)
    local n = 0
    for _, ins in ipairs(insns) do
        if ins.op == op then n = n + 1 end
    end
    return n
end

local function find_index(insns, op)
    for i, ins in ipairs(insns) do
        if ins.op == op then return i end
    end
    return nil
end

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

function test_optchain:test_method_nil_errors()
    -- '?:' guards the receiver only, not the method.
    local obj = {}
    lt.assertError(function () return obj?:missing() end)
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

function test_optchain:test_short_circuit_boundary()
    -- short-circuit covers only the chain, not across parentheses or operators
    local nothing
    lt.assertNil(nothing?.b)
    lt.assertError(function () return (nothing?.b).c end)
    lt.assertError(function () return nothing?.b + 1 end)
end

function test_optchain:test_false_not_short_circuit()
    -- only nil short-circuits; false errors as a normal non-nil value
    local f = false
    lt.assertError(function () return f?.a end)
    lt.assertError(function () return f?[1] end)
    lt.assertError(function () return f?() end)
    lt.assertError(function () return f?:m() end)
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
    -- single-value contexts still collapse to one value
    local obj = { getSize = function() return 100, 200 end }
    local s = obj?:getSize()
    lt.assertEquals(s, 100)
end

function test_optchain:test_multi_value_args()
    local function count(...) return select("#", ...) end
    local obj = { getSize = function() return 100, 200 end }
    lt.assertEquals(count(obj?:getSize()), 2)
    local nothing
    lt.assertEquals(count(nothing?:getSize()), 1)
end

function test_optchain:test_extra_values_discarded()
    -- 'nresults == 0' must leave OP_SETTOP's B field at 0 (a single nil).
    -- Regression: it used to store nresults-1 == -1 (255), overflowing the
    -- stack on short-circuit.
    local f
    local a = 1, f?()
    lt.assertEquals(a, 1)

    local g = function() return 10, 20 end
    local b = 1, g?()
    lt.assertEquals(b, 1)

    local h
    local c = 1
    c = 1, h?()
    lt.assertEquals(c, 1)
end

function test_optchain:test_multi_value_return_count()
    local obj = { getSize = function() return 7, 8 end }
    local function full()
        return obj?:getSize()
    end
    local function short()
        local n
        return n?:getSize()
    end
    lt.assertEquals(select("#", full()), 2)
    lt.assertEquals(select("#", short()), 1)
end

function test_optchain:test_multi_questionmark_chain_call()
    local obj = { a = { b = { c = function() return "deep" end } } }
    lt.assertEquals(obj?.a?.b?.c?(), "deep")
    local obj2 = { a = {} }
    lt.assertNil(obj2?.a?.b?.c?())
    local obj3 = { get = function() return { x = 7 } end }
    lt.assertEquals(obj3?:get()?.x, 7)
    local obj4 = { get = function() return nil end }
    lt.assertNil(obj4?:get()?.x)
end

-- Regression: with a count hook, luaG_traceexec resets L->top at the skip JMP
-- that follows an optional-chain open call, so the open consumer (OP_RETURN /
-- OP_SETLIST / OP_CALL with B==0) reads a wrong result count.
function test_optchain:test_hook_does_not_corrupt_multret()
    local f1 = function() return 42 end
    local f3 = function() return 10, 20, 30 end
    local function ret1() return f1?() end
    local function ret3() return f3?() end
    local function retnil() local n return n?() end
    debug.sethook(function() end, "", 1)
    local n1 = select("#", ret1())
    local n3 = select("#", ret3())
    local nn = select("#", retnil())
    debug.sethook()
    lt.assertEquals(n1, 1)
    lt.assertEquals(n3, 3)
    lt.assertEquals(nn, 1)
end

-- Call/return hooks go through luaG_callhook (a different path from the count
-- hook's luaG_traceexec).
function test_optchain:test_hook_call_return_does_not_corrupt_multret()
    local f1 = function() return 42 end
    local f3 = function() return 10, 20, 30 end
    local function ret1() return f1?() end
    local function ret3() return f3?() end
    local function retnil() local n return n?() end
    debug.sethook(function() end, "cr")
    local n1 = select("#", ret1())
    local n3 = select("#", ret3())
    local nn = select("#", retnil())
    debug.sethook()
    lt.assertEquals(n1, 1)
    lt.assertEquals(n3, 3)
    lt.assertEquals(nn, 1)
end

function test_optchain:test_codegen_nilcheck_eqk()
    -- a normal '?.' compiles the nil check to OP_EQK (small nil constant)
    local insns = instructions("return function(x) return x?.y end")
    lt.assertEquals(count_op(insns, OP_EQK), 1)
    lt.assertEquals(count_op(insns, OP_EQ), 0)
end

function test_optchain:test_codegen_nilcheck_eq_fallback()
    -- 300 constants before the first '?.' push the nil constant index past
    -- MAXARG_B, so luaK_jumpifnil falls back to OP_EQ against a temp nil.
    local parts = {}
    for i = 1, 300 do
        parts[#parts + 1] = ('"c%03d"'):format(i)
    end
    local src = "return function()\n"
        .. "  local _ = { " .. table.concat(parts, ", ") .. " }\n"
        .. "  local x\n"
        .. "  return x?.y\n"
        .. "end"
    local insns = instructions(src)
    lt.assertEquals(count_op(insns, OP_EQ), 1)
    lt.assertEquals(count_op(insns, OP_EQK), 0)
end

function test_optchain:test_codegen_settop_open_call()
    -- a chain ending in an open call fills via OP_SETTOP (fixes L->top);
    -- a chain ending in a field/index fills via OP_LOADNIL (single value).
    local insns = instructions("return function(f) return f?() end")
    lt.assertEquals(count_op(insns, OP_SETTOP), 1)
    lt.assertEquals(count_op(insns, OP_LOADNIL), 0)

    insns = instructions("return function(x) return x?.y end")
    lt.assertEquals(count_op(insns, OP_LOADNIL), 1)
    lt.assertEquals(count_op(insns, OP_SETTOP), 0)
end

function test_optchain:test_codegen_settop_layout()
    -- the fixed layout CALL / JMP / SETTOP; SETTOP is always at call+2
    local insns = instructions("return function(f) return f?() end")
    local i = find_index(insns, OP_CALL)
    lt.assertNotNil(i)
    lt.assertNotNil(insns[i + 1])
    lt.assertNotNil(insns[i + 2])
    lt.assertEquals(insns[i + 1].op, OP_JMP)
    lt.assertEquals(insns[i + 2].op, OP_SETTOP)
end

function test_optchain:test_codegen_settop_b_field()
    -- multi-value context widens OP_SETTOP's B to nresults-1; return context
    -- keeps it at 0 (one nil).
    local insns = instructions(
        "return function(o) local a, b, c = o?:m() return a, b, c end")
    local i = find_index(insns, OP_SETTOP)
    lt.assertNotNil(i)
    lt.assertEquals(insns[i].B, 2)  -- 3 nils

    insns = instructions("return function(f) return f?() end")
    i = find_index(insns, OP_SETTOP)
    lt.assertNotNil(i)
    lt.assertEquals(insns[i].B, 0)  -- 1 nil
end

function test_optchain:test_codegen_call_marked()
    -- the chain-call CALL carries the k flag and is not a tail call (the
    -- fixed layout needs it); a plain non-tail call has k=0.
    local insns = instructions("return function(f) return f?() end")
    local i = find_index(insns, OP_CALL)
    lt.assertNotNil(i)
    lt.assertEquals(insns[i].k, 1)
    lt.assertEquals(count_op(insns, OP_TAILCALL), 0)

    insns = instructions("return function(f) local x = f() return x end")
    i = find_index(insns, OP_CALL)
    lt.assertNotNil(i)
    lt.assertEquals(insns[i].k, 0)
end

function test_optchain:test_codegen_eqk_per_link()
    -- one nil check per '?' link
    local insns = instructions("return function(a) return a?.b?.c end")
    lt.assertEquals(count_op(insns, OP_EQK), 2)
end
