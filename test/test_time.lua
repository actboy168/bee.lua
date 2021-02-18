local lu = require 'ltest'

local time = require "bee.time"

local test_time = lu.test "time"

function test_time:test_now()
    local t1 = os.time() * 1000
    local t2 = time.time()
    lu.assertEquals(t1 <= t2, true)
    lu.assertEquals(t1 + 2000 >= t2, true)
end

function test_time:test_monotonic()
    local t1 = time.monotonic()
    lu.assertEquals(t1 > 0, true)
end
