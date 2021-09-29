local lt = require 'ltest'

local time = require "bee.time"

local test_time = lt.test "time"

function test_time:test_now()
    local t1 = os.time() * 1000
    local t2 = time.time()
    lt.assertEquals(t1 <= t2, true)
    lt.assertEquals(t1 + 2000 >= t2, true)
end

function test_time:test_monotonic()
    local t1 = time.monotonic()
    lt.assertEquals(t1 > 0, true)
end
