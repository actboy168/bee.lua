local lu = require 'luaunit'
local ls = require 'bee.socket'

test_socket = {}

function test_socket:test_bind()
    os.remove('test.unixsock')
    lu.assertUserdata(ls.bind('tcp', '127.0.0.1', 0))
    lu.assertUserdata(ls.bind('udp', '127.0.0.1', 0))
    lu.assertUserdata(ls.bind('unix', 'test.unixsock'))
    lu.assertErrorMsgEquals('invalid protocol `icmp`.', ls.bind, 'icmp', '127.0.0.1', 0)
end
