#include <bee/async/async.h>
#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/file.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/dynarray.h>
#include <bee/utility/span.h>

namespace bee::lua_socket {
    net::fd_t& newfd(lua_State* L, net::fd_t fd);
    net::fd_t& checkfd(lua_State* L, int idx);
}

namespace bee::lua_async {
    struct lua_async {
        std::unique_ptr<async::async> handle;
        int i = 0;
        int n = 0;
        dynarray<async::io_completion> completions;
        lua_async(size_t max_completions)
            : completions(max_completions) {
        }
        ~lua_async() = default;
    };

    static file_handle::value_type tofilefd(lua_State* L, int idx) {
        luaL_Stream* p = lua::tofile(L, idx);
        return file_handle::from_file(p->f).value();
    }

    struct read_buf {
        void*     ud;
        lua_Alloc allocf;
        char*     buf;
        size_t    len;

        static read_buf* create(lua_State* L, size_t len) {
            lua_Alloc allocf;
            void* ud       = nullptr;
            allocf         = lua_getallocf(L, &ud);
            read_buf* self = static_cast<read_buf*>(allocf(ud, NULL, 0, sizeof(read_buf)));
            self->ud       = ud;
            self->allocf   = allocf;
            self->len      = len;
            self->buf      = static_cast<char*>(allocf(ud, NULL, 0, len));
            return self;
        }
        void push_string(lua_State* L, size_t bytes) {
#if LUA_VERSION_NUM >= 505
            lua_pushexternalstring(L, buf, bytes, allocf, ud);
#else
            lua_pushlstring(L, buf, bytes);
            allocf(ud, buf, len, 0);
#endif
            buf = nullptr;
            allocf(ud, this, sizeof(read_buf), 0);
        }
        void destroy() {
            if (buf) {
                allocf(ud, buf, len, 0);
            }
            allocf(ud, this, sizeof(read_buf), 0);
        }
    };

    static int async_completions(lua_State* L) {
        auto& as = *(lua_async*)lua_touserdata(L, lua_upvalueindex(1));
        if (as.i >= as.n) {
            return 0;
        }
        const auto& c = as.completions[as.i];
        lua_pushinteger(L, static_cast<lua_Integer>(c.request_id));
        lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(c.status)));
        if (c.op == async::async_op::accept) {
            if (c.status == async::async_status::success) {
                auto newfd = static_cast<net::fd_t>(c.bytes_transferred);
                lua_socket::newfd(L, newfd);
            } else {
                lua_pushinteger(L, 0);
            }
        } else if (c.op == async::async_op::read || c.op == async::async_op::file_read) {
            lua_getiuservalue(L, lua_upvalueindex(1), 1);  // buf table
            lua_pushinteger(L, static_cast<lua_Integer>(c.request_id));
            lua_gettable(L, -2);  // buf table[reqid] -> rb userdata
            read_buf* rb = static_cast<read_buf*>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            lua_pushinteger(L, static_cast<lua_Integer>(c.request_id));
            lua_pushnil(L);
            lua_settable(L, -3);  // remove entry
            lua_pop(L, 1);  // pop buf table
            if (c.status == async::async_status::success && rb) {
                rb->push_string(L, c.bytes_transferred);
            } else {
                if (rb) rb->destroy();
                lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
            }
        } else {
            lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
        }
        lua_pushinteger(L, static_cast<lua_Integer>(c.error_code));
        as.i++;
        return 4;
    }

    static char* alloc_read_buf(lua_State* L, int async_idx, lua_Integer len, lua_Integer reqid) {
        read_buf* rb = read_buf::create(L, static_cast<size_t>(len));
        lua_getiuservalue(L, async_idx, 1);  // buf table
        lua_pushinteger(L, reqid);
        lua_pushlightuserdata(L, rb);
        lua_settable(L, -3);
        lua_pop(L, 1);  // pop buf table
        return rb->buf;
    }

    static void free_read_buf(lua_State* L, int async_idx, lua_Integer reqid) {
        lua_getiuservalue(L, async_idx, 1);
        lua_pushinteger(L, reqid);
        lua_gettable(L, -2);
        read_buf* rb = static_cast<read_buf*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        lua_pushinteger(L, reqid);
        lua_pushnil(L);
        lua_settable(L, -3);
        lua_pop(L, 1);
        if (rb) rb->destroy();
    }

    static int async_submit_read(lua_State* L) {
        auto& as          = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd      = lua_socket::checkfd(L, 2);
        lua_Integer len   = luaL_checkinteger(L, 3);
        lua_Integer reqid = luaL_checkinteger(L, 4);
        if (len <= 0) {
            return luaL_error(L, "buffer size must be positive");
        }
        void* buffer = alloc_read_buf(L, 1, len, reqid);
        if (!as.handle->submit_read(fd, buffer, static_cast<size_t>(len), static_cast<uint64_t>(reqid))) {
            free_read_buf(L, 1, reqid);
            return lua::return_net_error(L, "submit_read");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_write(lua_State* L) {
        auto& as          = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd      = lua_socket::checkfd(L, 2);
        size_t len        = 0;
        const char* data  = luaL_checklstring(L, 3, &len);
        lua_Integer reqid = luaL_checkinteger(L, 4);
        if (!as.handle->submit_write(fd, data, len, static_cast<uint64_t>(reqid))) {
            return lua::return_net_error(L, "submit_write");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_accept(lua_State* L) {
        auto& as          = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd      = lua_socket::checkfd(L, 2);
        lua_Integer reqid = luaL_checkinteger(L, 3);
        if (!as.handle->submit_accept(fd, static_cast<uint64_t>(reqid))) {
            return lua::return_net_error(L, "submit_accept");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_connect(lua_State* L) {
        auto& as          = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd      = lua_socket::checkfd(L, 2);
        const char* host  = luaL_checkstring(L, 3);
        lua_Integer port  = luaL_checkinteger(L, 4);
        lua_Integer reqid = luaL_checkinteger(L, 5);
        net::endpoint ep;
        if (!net::endpoint::ctor_hostname(ep, host, static_cast<uint16_t>(port))) {
            return lua::return_error(L, "invalid endpoint");
        }
        if (!as.handle->submit_connect(fd, ep, static_cast<uint64_t>(reqid))) {
            return lua::return_net_error(L, "submit_connect");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_file_read(lua_State* L) {
        auto& as                   = lua::checkudata<lua_async>(L, 1);
        file_handle::value_type fd = tofilefd(L, 2);
        lua_Integer len            = luaL_checkinteger(L, 3);
        lua_Integer offset         = luaL_optinteger(L, 4, 0);
        lua_Integer reqid          = luaL_checkinteger(L, 5);
        if (len <= 0) {
            return luaL_error(L, "buffer size must be positive");
        }
        void* buffer = alloc_read_buf(L, 1, len, reqid);
        if (!as.handle->submit_file_read(fd, buffer, static_cast<size_t>(len), static_cast<int64_t>(offset), static_cast<uint64_t>(reqid))) {
            free_read_buf(L, 1, reqid);
            return lua::return_net_error(L, "submit_file_read");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_file_write(lua_State* L) {
        auto& as                   = lua::checkudata<lua_async>(L, 1);
        file_handle::value_type fd = tofilefd(L, 2);
        size_t len                 = 0;
        const char* data           = luaL_checklstring(L, 3, &len);
        lua_Integer offset         = luaL_optinteger(L, 4, 0);
        lua_Integer reqid          = luaL_checkinteger(L, 5);
        if (!as.handle->submit_file_write(fd, data, len, static_cast<int64_t>(offset), static_cast<uint64_t>(reqid))) {
            return lua::return_net_error(L, "submit_file_write");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_poll(lua_State* L) {
        auto& as = lua::checkudata<lua_async>(L, 1);
        as.i     = 0;
        as.n     = as.handle->poll(span<async::io_completion>(as.completions.data(), as.completions.size()));
        lua_getiuservalue(L, 1, 2);
        return 1;
    }

    static int async_wait(lua_State* L) {
        auto& as    = lua::checkudata<lua_async>(L, 1);
        int timeout = lua::optinteger<int, -1>(L, 2);
        as.i        = 0;
        as.n        = as.handle->wait(span<async::io_completion>(as.completions.data(), as.completions.size()), timeout);
        lua_getiuservalue(L, 1, 2);
        return 1;
    }

    static int async_stop(lua_State* L) {
        auto& as = lua::checkudata<lua_async>(L, 1);
        as.handle->stop();
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_mt_close(lua_State* L) {
        auto& as = lua::checkudata<lua_async>(L, 1);
        as.handle->stop();
        return 0;
    }

    static void metatable(lua_State* L) {
        static luaL_Reg lib[] = {
            { "submit_read", async_submit_read },
            { "submit_write", async_submit_write },
            { "submit_accept", async_submit_accept },
            { "submit_connect", async_submit_connect },
            { "submit_file_read", async_submit_file_read },
            { "submit_file_write", async_submit_file_write },
            { "poll", async_poll },
            { "wait", async_wait },
            { "stop", async_stop },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        static luaL_Reg mt[] = {
            { "__close", async_mt_close },
            { NULL, NULL }
        };
        luaL_setfuncs(L, mt, 0);
    }

    static int async_create(lua_State* L) {
        lua_Integer max_completions = luaL_optinteger(L, 1, 64);
        if (max_completions <= 0) {
            return lua::return_error(L, "max_completions is less than or equal to zero.");
        }
        auto handle = async::create();
        if (!handle) {
            return lua::return_error(L, "failed to create async backend");
        }
        lua::newudata<lua_async>(L, static_cast<size_t>(max_completions));
        auto& as  = lua::checkudata<lua_async>(L, -1);
        as.handle = std::move(handle);
        lua_newtable(L);
        lua_setiuservalue(L, -2, 1);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, async_completions, 1);
        lua_setiuservalue(L, -2, 2);
        return 1;
    }

    static int luaopen(lua_State* L) {
        struct luaL_Reg l[] = {
            { "create", async_create },
            { NULL, NULL },
        };
        luaL_newlib(L, l);

#define SETENUM(E, V)                                                    \
    lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(V))); \
    lua_setfield(L, -2, #E)

        SETENUM(SUCCESS, async::async_status::success);
        SETENUM(CLOSE, async::async_status::close);
        SETENUM(ERROR, async::async_status::error);
        SETENUM(CANCEL, async::async_status::cancel);
#undef SETENUM
        return 1;
    }
}

DEFINE_LUAOPEN(async)

namespace bee::lua {
    template <>
    struct udata<lua_async::lua_async> {
        static inline int nupvalue   = 2;
        static inline auto metatable = bee::lua_async::metatable;
    };
}
