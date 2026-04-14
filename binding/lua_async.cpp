#include <bee/async/async.h>
#include <bee/async/ring_buf.h>
#include <bee/async/write_buf.h>
#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/file.h>
#include <bee/lua/luaref.h>
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
    net::endpoint& new_endpoint(lua_State* L);
    net::endpoint& to_endpoint(lua_State* L, int idx, net::endpoint& ep);
}

namespace bee::lua_async {

    struct lua_async {
        std::unique_ptr<async::async> handle;
        luaref refs = nullptr;
        int i = 0;
        int n = 0;
        dynarray<async::io_completion> completions;
        lua_async(size_t max_completions)
            : completions(max_completions) {}
        ~lua_async() {
            if (refs) luaref_close(refs);
        }
    };

    static file_handle::value_type tofilefd(lua_State* L, int idx) {
        luaL_Stream* p = lua::tofile(L, idx);
        return file_handle::from_file(p->f).value();
    }

    // ---- request_id packing ----
    // high 32 bits = buf_ref, low 32 bits = udata_ref

    static uint64_t make_request_id(int buf_ref, int udata_ref) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(buf_ref)) << 32)
             | static_cast<uint64_t>(static_cast<uint32_t>(udata_ref));
    }

    static int get_buf_ref(uint64_t id) {
        return static_cast<int>(static_cast<int32_t>(id >> 32));
    }

    static int get_udata_ref(uint64_t id) {
        return static_cast<int>(static_cast<int32_t>(id & 0xFFFFFFFF));
    }

    static void unref_buf(lua_async& as, int ref) {
        if (ref) luaref_unref(as.refs, ref);
    }

    // ---- read_buf ----

    struct read_buf {
        void* ud;
        lua_Alloc allocf;
        char* buf;
        size_t len;

        static read_buf* create(lua_State* L, size_t len) {
            lua_Alloc allocf;
            void* ud       = nullptr;
            allocf         = lua_getallocf(L, &ud);
            read_buf* self = static_cast<read_buf*>(allocf(ud, NULL, 0, sizeof(read_buf)));
            self->ud       = ud;
            self->allocf   = allocf;
            self->len      = len;
            self->buf      = static_cast<char*>(allocf(ud, NULL, 0, len + 1));
            self->buf[len] = '\0';
            return self;
        }
        void push_string(lua_State* L, size_t bytes) {
#if LUA_VERSION_NUM >= 505
            buf[bytes] = '\0';
            lua_pushexternalstring(L, buf, bytes, allocf, ud);
#else
            lua_pushlstring(L, buf, bytes);
            allocf(ud, buf, len + 1, 0);
#endif
            buf = nullptr;
            allocf(ud, this, sizeof(read_buf), 0);
        }
        void destroy() {
            if (buf) allocf(ud, buf, len + 1, 0);
            allocf(ud, this, sizeof(read_buf), 0);
        }
    };

    static char* alloc_read_buf(lua_State* L, lua_async& as, int udata_idx, lua_Integer len, uint64_t& out_id) {
        read_buf* rb = read_buf::create(L, static_cast<size_t>(len));
        lua_pushlightuserdata(L, rb);
        int buf_r = luaref_ref(as.refs, L);
        lua_pushvalue(L, udata_idx);
        int udata_r = luaref_ref(as.refs, L);
        out_id      = make_request_id(buf_r, udata_r);
        return rb->buf;
    }

    // ---- write_buf drain helpers ----

    // Submit the front queue entry.  wb.fd and wb.lua_reqid must already be set.
    static bool wb_submit_next(lua_async& as, async::write_buf& wb) {
        if (wb.q.empty()) return true;
        async::write_buf::entry& entry = wb.q.front();
        net::socket::iobuf buf;
        buf.set(entry.data + entry.offset, entry.len - entry.offset);
        if (!as.handle->submit_writev(wb.fd, span<const net::socket::iobuf>(&buf, 1), wb.lua_reqid)) {
            return false;
        }
        wb.in_flight = true;
        return true;
    }

    // Handle writev completion for a write_buf-managed packed id.
    // Returns true  → emit the Lua-visible completion.
    // Returns false → still draining, no Lua completion yet.
    static bool wb_on_completion(lua_State* L, lua_async& as, uint64_t packed_id, async::write_buf& wb, async::async_status status, size_t bytes) {
        wb.in_flight   = false;
        int buf_r      = get_buf_ref(packed_id);
        int udata_r    = get_udata_ref(packed_id);

        if (status != async::async_status::success) {
            for (auto& e : wb.q) luaL_unref(L, LUA_REGISTRYINDEX, e.str_ref);
            wb.q.clear();
            wb.buffered = 0;
            unref_buf(as, buf_r);
            return true;
        }

        if (wb.q.empty()) {
            unref_buf(as, buf_r);
            return true;
        }

        async::write_buf::entry& front = wb.q.front();
        front.offset += bytes;

        if (front.offset < front.len) {
            if (!wb_submit_next(as, wb)) {
                unref_buf(as, buf_r);
                return true;
            }
            // Still draining: wb.lua_reqid already holds the packed id.
            return false;
        }

        luaL_unref(L, LUA_REGISTRYINDEX, front.str_ref);
        wb.buffered -= front.len;
        wb.q.pop_front();

        if (!wb.q.empty()) {
            if (!wb_submit_next(as, wb)) {
                unref_buf(as, buf_r);
                return true;
            }
            // Still draining: wb.lua_reqid already holds the packed id.
            return false;
        }

        unref_buf(as, buf_r);
        return true;
    }

    // udata_r is consumed (unref'd) by this call.
    static void push_udata(lua_State* L, lua_async& as, int udata_r) {
        if (udata_r) {
            luaref_get(as.refs, L, udata_r);
            luaref_unref(as.refs, udata_r);
        } else {
            lua_pushnil(L);
        }
    }

    // ---- completion iterator ----

    static int async_completions(lua_State* L) {
        auto& as = *(lua_async*)lua_touserdata(L, lua_upvalueindex(1));
    again:
        if (as.i >= as.n) return 0;

        const auto& c = as.completions[as.i];
        as.i++;
        int buf_r   = get_buf_ref(c.request_id);
        int udata_r = get_udata_ref(c.request_id);

        if (c.op == async::async_op::writev) {
            async::write_buf* wb = nullptr;
            if (buf_r) {
                luaref_get(as.refs, L, buf_r);
                if (lua_type(L, -1) == LUA_TUSERDATA) {
                    void* p = luaL_testudata(L, -1, reflection::name_v<async::write_buf>.data());
                    if (p) wb = lua::udata_align<async::write_buf>(p);
                }
                lua_pop(L, 1);
            }
            if (wb) {
                async::async_status st = c.status;
                size_t bytes           = c.bytes_transferred;
                bool done              = wb_on_completion(L, as, c.request_id, *wb, st, bytes);
                if (!done) goto again;
                lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(c.op)));
                push_udata(L, as, udata_r);
                lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(st)));
                lua_pushinteger(L, 0);
                lua_pushinteger(L, static_cast<lua_Integer>(c.error_code));
                return 5;
            }
        }

        lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(c.op)));
        push_udata(L, as, udata_r);
        lua_pushinteger(L, static_cast<lua_Integer>(std::to_underlying(c.status)));

        switch (c.op) {
        case async::async_op::accept:
            if (c.status == async::async_status::success) {
                lua_socket::newfd(L, static_cast<net::fd_t>(c.bytes_transferred));
            } else {
                lua_pushinteger(L, 0);
            }
            break;
        case async::async_op::connect:
            unref_buf(as, buf_r);
            lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
            break;
        case async::async_op::read: {
            async::ring_buf* rb = nullptr;
            if (buf_r) {
                luaref_get(as.refs, L, buf_r);
                if (lua_type(L, -1) == LUA_TUSERDATA) {
                    void* p = luaL_testudata(L, -1, reflection::name_v<async::ring_buf>.data());
                    if (p) rb = lua::udata_align<async::ring_buf>(p);
                }
                lua_pop(L, 1);
                luaref_unref(as.refs, buf_r);
            }
            if (rb && c.status == async::async_status::success) rb->commit(c.bytes_transferred);
            lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
            break;
        }
        case async::async_op::file_read: {
            read_buf* rb = nullptr;
            if (buf_r) {
                luaref_get(as.refs, L, buf_r);
                rb = static_cast<read_buf*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                luaref_unref(as.refs, buf_r);
            }
            if (c.status == async::async_status::success && rb) {
                rb->push_string(L, c.bytes_transferred);
            } else {
                if (rb) rb->destroy();
                lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
            }
            break;
        }
        default:
            unref_buf(as, buf_r);
            lua_pushinteger(L, static_cast<lua_Integer>(c.bytes_transferred));
            break;
        }
        lua_pushinteger(L, static_cast<lua_Integer>(c.error_code));
        return 5;
    }

    // ---- fd helpers ----

    // Accept both socket userdata (from bee.socket) and light userdata (from channel:fd()).
    static net::fd_t checkfd_any(lua_State* L, int idx) {
        if (lua_type(L, idx) == LUA_TLIGHTUSERDATA) {
            return static_cast<net::fd_t>(reinterpret_cast<intptr_t>(lua_touserdata(L, idx)));
        }
        return lua_socket::checkfd(L, idx);
    }

    // Make a packed request_id and pin both values.
    static uint64_t pin(lua_State* L, lua_async& as, int buf_idx, int udata_idx) {
        lua_pushvalue(L, buf_idx);
        int buf_r = luaref_ref(as.refs, L);
        lua_pushvalue(L, udata_idx);
        int udata_r = luaref_ref(as.refs, L);
        return make_request_id(buf_r, udata_r);
    }

    // Make a packed request_id with no buffer (buf_ref=0).
    static uint64_t pin_udata(lua_State* L, lua_async& as, int udata_idx) {
        lua_pushvalue(L, udata_idx);
        int udata_r = luaref_ref(as.refs, L);
        return make_request_id(0, udata_r);
    }

    // Unref both refs from a packed id (used on submit failure).
    static void pin_release(lua_async& as, uint64_t id) {
        unref_buf(as, get_buf_ref(id));
        unref_buf(as, get_udata_ref(id));
    }

    // submit_read(asfd, rb, fd, udata)
    static int async_submit_read(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        auto& rb     = lua::checkudata<async::ring_buf>(L, 2);
        net::fd_t fd = lua_socket::checkfd(L, 3);
        luaL_checkany(L, 4);
        size_t len = rb.write_len();
        if (len == 0) {
            lua_pushboolean(L, 0);
            return 1;
        }
        void* ptr    = rb.write_ptr();
        uint64_t id  = pin(L, as, 2, 4);
        if (!as.handle->submit_read(fd, ptr, len, id)) {
            pin_release(as, id);
            return lua::return_net_error(L, "submit_read");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    // submit_write(asfd, wb, fd, udata)
    // Drains wb's queue. C layer auto-drains until empty, then emits one Lua completion.
    static int async_submit_write(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        auto& wb     = lua::checkudata<async::write_buf>(L, 2);
        net::fd_t fd = lua_socket::checkfd(L, 3);
        luaL_checkany(L, 4);

        if (wb.in_flight || wb.q.empty()) {
            lua_pushboolean(L, 1);
            return 1;
        }

        uint64_t id  = pin(L, as, 2, 4);
        wb.fd        = fd;
        wb.lua_reqid = id;

        if (!wb_submit_next(as, wb)) {
            pin_release(as, id);
            return lua::return_net_error(L, "submit_write");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_accept(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd = lua_socket::checkfd(L, 2);
        luaL_checkany(L, 3);
        uint64_t id = pin_udata(L, as, 3);
        if (!as.handle->submit_accept(fd, id)) {
            pin_release(as, id);
            return lua::return_net_error(L, "submit_accept");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_submit_connect(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd = lua_socket::checkfd(L, 2);
        net::endpoint stack_ep;
        const net::endpoint* ep_ptr;
        int udata_idx;
        if (lua_type(L, 3) == LUA_TUSERDATA) {
            ep_ptr    = &lua_socket::to_endpoint(L, 3, stack_ep);
            udata_idx = 4;
            lua_pushvalue(L, 3);
        } else {
            auto name = lua::checkstrview(L, 3);
            auto port = lua::checkinteger<uint16_t>(L, 4);
            udata_idx = 5;
            auto& ep  = lua_socket::new_endpoint(L);
            if (!net::endpoint::ctor_hostname(ep, name, port))
                return lua::return_error(L, "invalid endpoint");
            ep_ptr = &ep;
        }
        luaL_checkany(L, udata_idx);
        uint64_t id = pin(L, as, lua_gettop(L), udata_idx);
        lua_pop(L, 1);
        if (!as.handle->submit_connect(fd, *ep_ptr, id)) {
            pin_release(as, id);
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
        luaL_checkany(L, 5);
        if (len <= 0) return luaL_error(L, "buffer size must be positive");
        uint64_t id  = 0;
        void* buffer = alloc_read_buf(L, as, 5, len, id);
        if (!as.handle->submit_file_read(fd, buffer, static_cast<size_t>(len), static_cast<int64_t>(offset), id)) {
            int buf_r = get_buf_ref(id);
            if (buf_r) {
                luaref_get(as.refs, L, buf_r);
                read_buf* rb = static_cast<read_buf*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                if (rb) rb->destroy();
            }
            pin_release(as, id);
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
        luaL_checkany(L, 5);
        uint64_t id = pin(L, as, 3, 5);
        if (!as.handle->submit_file_write(fd, data, len, static_cast<int64_t>(offset), id)) {
            pin_release(as, id);
            return lua::return_net_error(L, "submit_file_write");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    // submit_poll(asfd, fd, udata)
    static int async_submit_poll(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd = checkfd_any(L, 2);
        luaL_checkany(L, 3);
        uint64_t id = pin_udata(L, as, 3);
        if (!as.handle->submit_poll(fd, id)) {
            pin_release(as, id);
            return lua::return_net_error(L, "submit_poll");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int async_poll(lua_State* L) {
        auto& as = lua::checkudata<lua_async>(L, 1);
        as.i     = 0;
        as.n     = as.handle->poll(span<async::io_completion>(as.completions.data(), as.completions.size()));
        lua_getiuservalue(L, 1, 1);
        return 1;
    }

    static int async_wait(lua_State* L) {
        auto& as    = lua::checkudata<lua_async>(L, 1);
        int timeout = lua::optinteger<int, -1>(L, 2);
        as.i        = 0;
        as.n        = as.handle->wait(span<async::io_completion>(as.completions.data(), as.completions.size()), timeout);
        lua_getiuservalue(L, 1, 1);
        return 1;
    }

    static int async_associate(lua_State* L) {
#if defined(_WIN32)
        auto& as     = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd = checkfd_any(L, 2);
        lua_pushboolean(L, as.handle->associate(fd) ? 1 : 0);
#else
        lua_pushboolean(L, 1);
#endif
        return 1;
    }

    static int async_associate_file(lua_State* L) {
#if defined(_WIN32)
        auto& as                   = lua::checkudata<lua_async>(L, 1);
        file_handle::value_type fd = tofilefd(L, 2);
        auto [ov_fh, writable]     = as.handle->associate_file(fd);
        if (!ov_fh)
            return lua::return_net_error(L, "associate_file");
        auto fm  = writable ? file_handle::mode::write : file_handle::mode::read;
        FILE* fp = ov_fh.to_file(fm);
        if (!fp)
            return lua::return_net_error(L, "associate_file");
        luaL_Stream* stream = lua::tofile(L, 2);
        fclose(stream->f);
        stream->f = fp;
        lua_pushboolean(L, 1);
#else
        lua_pushboolean(L, 1);
#endif
        return 1;
    }

    static int async_cancel(lua_State* L) {
        auto& as     = lua::checkudata<lua_async>(L, 1);
        net::fd_t fd = lua_socket::checkfd(L, 2);
        as.handle->cancel(fd);
        return 0;
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

    // ---- ring_buf (readbuf) methods ----

    static int rb_read(lua_State* L) {
        auto& rb = lua::checkudata<async::ring_buf>(L, 1);
        if (lua_isnoneornil(L, 2)) {
            size_t n = rb.size();
            if (n == 0) {
                lua_pushnil(L);
                return 1;
            }
            luaL_Buffer b;
            char* dst = luaL_buffinitsize(L, &b, n);
            rb.consume(dst, n);
            luaL_pushresultsize(&b, n);
            return 1;
        }
        lua_Integer n = luaL_checkinteger(L, 2);
        if (n <= 0) return luaL_error(L, "n must be positive");
        size_t ulen = static_cast<size_t>(n);
        if (rb.size() < ulen) {
            lua_pushnil(L);
            return 1;
        }
        luaL_Buffer b;
        char* dst = luaL_buffinitsize(L, &b, ulen);
        rb.consume(dst, ulen);
        luaL_pushresultsize(&b, ulen);
        return 1;
    }

    static int rb_readline(lua_State* L) {
        auto& rb        = lua::checkudata<async::ring_buf>(L, 1);
        size_t seplen   = 0;
        const char* sep = luaL_optlstring(L, 2, "\r\n", &seplen);
        if (seplen == 0) return luaL_error(L, "separator must not be empty");
        size_t n = rb.find(sep, seplen);
        if (n == 0) {
            lua_pushnil(L);
            return 1;
        }
        luaL_Buffer b;
        char* dst = luaL_buffinitsize(L, &b, n);
        rb.consume(dst, n);
        luaL_pushresultsize(&b, n);
        return 1;
    }

    static int async_readbuf_create(lua_State* L) {
        lua_Integer bufsize = luaL_checkinteger(L, 1);
        if (bufsize <= 0) return luaL_error(L, "bufsize must be positive");
        lua::newudata<async::ring_buf>(L, static_cast<size_t>(bufsize));
        return 1;
    }

    // ---- write_buf (writebuf) methods ----

    // async.writebuf(hwm) -> write_buf userdata
    static int async_writebuf_create(lua_State* L) {
        lua_Integer hwm = luaL_optinteger(L, 1, 64 * 1024);
        if (hwm <= 0) return luaL_error(L, "hwm must be positive");
        auto& wb = lua::newudata<async::write_buf>(L);
        wb.hwm   = static_cast<size_t>(hwm);
        return 1;
    }

    // wb:write(data) -> bool  (true = buffered >= hwm after enqueue)
    static int wb_write(lua_State* L) {
        auto& wb         = lua::checkudata<async::write_buf>(L, 1);
        size_t len       = 0;
        const char* data = luaL_checklstring(L, 2, &len);
        if (len == 0) {
            lua_pushboolean(L, 0);
            return 1;
        }
        lua_pushvalue(L, 2);
        int str_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        async::write_buf::entry e;
        e.data    = data;
        e.len     = len;
        e.offset  = 0;
        e.str_ref = str_ref;
        wb.q.push_back(e);
        wb.buffered += len;
        lua_pushboolean(L, wb.buffered >= wb.hwm ? 1 : 0);
        return 1;
    }

    // wb:buffered() -> integer
    static int wb_buffered(lua_State* L) {
        auto& wb = lua::checkudata<async::write_buf>(L, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(wb.buffered));
        return 1;
    }

    // wb:close() -- release all queued strings (called on stream close)
    static int wb_close(lua_State* L) {
        auto& wb = lua::checkudata<async::write_buf>(L, 1);
        for (auto& e : wb.q) luaL_unref(L, LUA_REGISTRYINDEX, e.str_ref);
        wb.q.clear();
        wb.buffered = 0;
        return 0;
    }

    // ---- metatable / module ----

    static void metatable(lua_State* L) {
        static luaL_Reg lib[] = {
            { "submit_write", async_submit_write },
            { "submit_accept", async_submit_accept },
            { "submit_connect", async_submit_connect },
            { "submit_file_read", async_submit_file_read },
            { "submit_file_write", async_submit_file_write },
            { "submit_read", async_submit_read },
            { "submit_poll", async_submit_poll },
            { "associate", async_associate },
            { "associate_file", async_associate_file },
            { "cancel", async_cancel },
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
        if (max_completions <= 0)
            return lua::return_error(L, "max_completions is less than or equal to zero.");
        auto handle = async::create();
        if (!handle)
            return lua::return_error(L, "failed to create async backend");
        lua::newudata<lua_async>(L, static_cast<size_t>(max_completions));
        auto& as  = lua::checkudata<lua_async>(L, -1);
        as.handle = std::move(handle);
        as.refs   = luaref_init(L);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, async_completions, 1);
        lua_setiuservalue(L, -2, 1);
        return 1;
    }

    static int luaopen(lua_State* L) {
        struct luaL_Reg l[] = {
            { "create", async_create },
            { "readbuf", async_readbuf_create },
            { "writebuf", async_writebuf_create },
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

        SETENUM(OP_READ, async::async_op::read);
        SETENUM(OP_WRITE, async::async_op::write);
        SETENUM(OP_WRITEV, async::async_op::writev);
        SETENUM(OP_ACCEPT, async::async_op::accept);
        SETENUM(OP_CONNECT, async::async_op::connect);
        SETENUM(OP_FILE_READ, async::async_op::file_read);
        SETENUM(OP_FILE_WRITE, async::async_op::file_write);
        SETENUM(OP_POLL, async::async_op::fd_poll);
#undef SETENUM
        return 1;
    }
}

DEFINE_LUAOPEN(async)

namespace bee::lua {
    template <>
    struct udata<lua_async::lua_async> {
        static inline int nupvalue   = 1;
        static inline auto metatable = bee::lua_async::metatable;
    };
    template <>
    struct udata<async::ring_buf> {
        static inline auto metatable = [](lua_State* L) {
            static luaL_Reg lib[] = {
                { "read", lua_async::rb_read },
                { "readline", lua_async::rb_readline },
                { NULL, NULL }
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
        };
    };
    template <>
    struct udata<async::write_buf> {
        static inline auto metatable = [](lua_State* L) {
            static luaL_Reg lib[] = {
                { "write", lua_async::wb_write },
                { "buffered", lua_async::wb_buffered },
                { "close", lua_async::wb_close },
                { NULL, NULL }
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
        };
    };
}
