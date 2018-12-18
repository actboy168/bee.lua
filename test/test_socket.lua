local lu = require 'luaunit'
local ls = require 'bee.socket'
local platform = require 'bee.platform'

local function file_exists(filename)
    local f, _, errno = io.open(filename, 'r')
    if f then
        f:close()
        return true
    end
    return errno ~= 2
end

test_socket = {}

function test_socket:test_bind()
    os.remove('test.unixsock')
    local function assert_ok(fd)
        lu.assertUserdata(fd)
        fd:close()
    end
    assert_ok(ls.bind('tcp', '127.0.0.1', 0))
    assert_ok(ls.bind('udp', '127.0.0.1', 0))
    lu.assertErrorMsgEquals('invalid protocol `icmp`.', ls.bind, 'icmp', '127.0.0.1', 0)

    local fd = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(fd)
    lu.assertIsTrue(file_exists('test.unixsock'))
    fd:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

function test_socket:test_tcp_connect()
    local server = ls.bind('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(server)
    local client = ls.connect('tcp', '127.0.0.1', 31234)
    lu.assertUserdata(client)
    client:close()
    server:close()
end

function test_socket:test_unix_connect()
    os.remove('test.unixsock')
    lu.assertIsNil(ls.connect('unix', 'test.unixsock'))
    if platform.OS == 'Windows' then
        -- Windows的bug？
        os.remove('test.unixsock')
    end
    lu.assertIsFalse(file_exists('test.unixsock'))

    local server = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(server)
    lu.assertIsTrue(file_exists('test.unixsock'))
    local client = ls.connect('unix', 'test.unixsock')
    lu.assertUserdata(client)
    client:close()
    server:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end

function test_socket:test_unix_accept()
    os.remove('test.unixsock')
    local server = ls.bind('unix', 'test.unixsock')
    lu.assertUserdata(server)
    lu.assertIsTrue(file_exists('test.unixsock'))
    local client = ls.connect('unix', 'test.unixsock')
    lu.assertUserdata(client)
    local rd, wr = ls.select({server}, {client})
    lu.assertTable(rd)
    lu.assertTable(wr)
    lu.assertEquals(rd[1], server)
    lu.assertEquals(wr[1], client)
    local session = server:accept()
    lu.assertUserdata(session)
    lu.assertIsTrue(client:status())
    lu.assertIsTrue(session:status())
    session:close()
    client:close()
    server:close()
    lu.assertIsFalse(file_exists('test.unixsock'))
end
