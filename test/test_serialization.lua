local seri = require 'bee.serialization'

local function TABLE_EQ(a, b)
	for k, v in pairs(a) do
		if type(v) == 'table' then
			TABLE_EQ(v, b[k])
		else
			assert(v == b[k])
		end
	end
end

local function VARGS_EQ(a, b)
    assert(a.n == b.n)
	for i = 1, a.n do
		if type(a[i]) == 'table' then
			TABLE_EQ(a[i], b[i])
			TABLE_EQ(b[i], a[i])
		else
			assert(a[i] == b[i])
		end
	end
end

local function TEST_LUD(...)
    VARGS_EQ(
        table.pack(seri.unpack(seri.pack(...))),
        table.pack(...)
    )
end

local function TEST_STR(...)
    VARGS_EQ(
        table.pack(seri.unpack(seri.packstring(...))),
        table.pack(...)
    )
end

local function TEST(...)
    TEST_LUD(...)
    TEST_STR(...)
end

TEST(1)
TEST(0.0001)
TEST('TEST')
TEST(true)
TEST(false)
TEST({})
TEST({1, 2})
TEST(1, {1, 2})
TEST(1, 2, {A={B={C='D'}}})
TEST(1, nil, 2)
