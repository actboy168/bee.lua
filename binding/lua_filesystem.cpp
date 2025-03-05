#include <bee/lua/binding.h>
#include <bee/lua/cxx_status.h>
#include <bee/lua/error.h>
#include <bee/lua/module.h>
#include <bee/lua/narrow.h>
#include <bee/lua/udata.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>

#include <chrono>
#include <utility>

#if defined(_WIN32)
#    include <Windows.h>
#    include <bee/win/wtf8.h>
#endif

namespace bee::lua_filesystem {
    namespace workaround {
#if defined(__MINGW32__)
        static bool copy_file(const fs::path& from, const fs::path& to, fs::copy_options options, std::error_code& ec) {
            try {
                if (fs::exists(from) && fs::exists(to)) {
                    if ((options & fs::copy_options::overwrite_existing) != fs::copy_options::none) {
                        fs::remove(to);
                    } else if ((options & fs::copy_options::update_existing) != fs::copy_options::none) {
                        if (fs::last_write_time(from) > fs::last_write_time(to)) {
                            fs::remove(to);
                        } else {
                            return false;
                        }
                    } else if ((options & fs::copy_options::skip_existing) != fs::copy_options::none) {
                        return false;
                    }
                }
                return fs::copy_file(from, to, options);
            } catch (const fs::filesystem_error& e) {
                ec = e.code();
                return false;
            }
        }
#endif
#if defined(_WIN32)
        static bool remove_can_retry(uint32_t retry) noexcept {
            constexpr uint32_t max_retry   = 10;
            constexpr uint32_t retry_delay = 25;
            if (retry >= max_retry) {
                return false;
            }
            DWORD e = ::GetLastError();
            if (e != ERROR_SHARING_VIOLATION) {
                return false;
            }
            ::Sleep(retry_delay * (1 + retry));
            return true;
        }
        static bool remove(const fs::path& path, std::error_code& ec) {
            for (uint32_t retry = 0;; ++retry) {
                auto r = fs::remove(path, ec);
                if (!ec || !remove_can_retry(retry)) {
                    return r;
                }
            }
        }
        static uintmax_t remove_all(const fs::path& path, std::error_code& ec) {
            for (uint32_t retry = 0;; ++retry) {
                auto r = fs::remove_all(path, ec);
                if (!ec || !remove_can_retry(retry)) {
                    return r;
                }
            }
        }
#endif
    }

    static std::string tostring(const fs::path& path) {
#if defined(_WIN32)
        auto wstr = path.generic_wstring();
        return wtf8::w2u(wstr);
#else
        return path.generic_string();
#endif
    }

    static bool path_equal(const fs::path& lft, const fs::path& rht) noexcept {
        fs::path lpath = lft.lexically_normal();
        fs::path rpath = rht.lexically_normal();
#if defined(_WIN32)
        auto l = lpath.c_str();
        auto r = rpath.c_str();
        while ((towlower(*l) == towlower(*r) || (*l == L'\\' && *r == L'/') || (*l == L'/' && *r == L'\\')) && *l) {
            ++l;
            ++r;
        }
        return *l == *r;
#elif defined(__APPLE__)
        auto l = lpath.c_str();
        auto r = rpath.c_str();
        while (towlower(*l) == towlower(*r) && *l) {
            ++l;
            ++r;
        }
        return *l == *r;
#else
        return lft.lexically_normal() == rht.lexically_normal();
#endif
    }

    template <typename... Args>
    static void lua_pusherrmsg(lua_State* L, std::error_code ec, std::format_string<Args...> fmt, Args... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        lua::push_sys_error(L, msg, ec.value());
    }

    [[nodiscard]] static lua::cxx::status pusherror(lua_State* L, std::string_view op, std::error_code ec) {
        lua_pusherrmsg(L, ec, "{}", op);
        return lua::cxx::error;
    }

    [[nodiscard]] static lua::cxx::status pusherror(lua_State* L, std::string_view op, std::error_code ec, const fs::path& path1) {
        lua_pusherrmsg(L, ec, "{}: \"{}\"", op, tostring(path1));
        return lua::cxx::error;
    }

    [[nodiscard]] static lua::cxx::status pusherror(lua_State* L, std::string_view op, std::error_code ec, const fs::path& path1, const fs::path& path2) {
        lua_pusherrmsg(L, ec, "{}: \"{}\", \"{}\"", op, tostring(path1), tostring(path2));
        return lua::cxx::error;
    }

    class path_ref {
    public:
        path_ref(lua_State* L, int idx) {
            if (lua_type(L, idx) == LUA_TSTRING) {
                auto str = lua::checkstrview(L, idx);
#if defined(_WIN32)
                val.assign(wtf8::u2w(str));
#else
                val.assign(std::string { str.data(), str.size() });
#endif
                ptr = &val;
            } else {
                ptr = &lua::checkudata<fs::path>(L, idx);
            }
        }
        const fs::path* operator->() {
            return ptr;
        }
        const fs::path& operator*() {
            return *ptr;
        }
        operator const fs::path&() {
            return *ptr;
        }

    private:
        const fs::path* ptr;
        fs::path val;
    };

    namespace path {
        static int constructor(lua_State* L) {
            if (lua_gettop(L) == 0) {
                lua::newudata<fs::path>(L);
            } else if (lua_type(L, 1) == LUA_TSTRING) {
                auto str = lua::checkstrview(L, 1);
#if defined(_WIN32)
                lua::newudata<fs::path>(L, wtf8::u2w(str));
#else
                lua::newudata<fs::path>(L, std::string { str.data(), str.size() });
#endif
            } else {
                lua::newudata<fs::path>(L, lua::checkudata<fs::path>(L, 1));
            }
            return 1;
        }

        static int filename(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua::newudata<fs::path>(L, self.filename());
            return 1;
        }

        static int parent_path(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua::newudata<fs::path>(L, self.parent_path());
            return 1;
        }

        static int stem(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua::newudata<fs::path>(L, self.stem());
            return 1;
        }

        static int extension(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            auto str         = tostring(self.extension());
            lua_pushlstring(L, str.data(), str.size());
            return 1;
        }

        static int is_absolute(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua_pushboolean(L, self.is_absolute());
            return 1;
        }

        static int is_relative(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua_pushboolean(L, self.is_relative());
            return 1;
        }

        static int remove_filename(lua_State* L) {
            auto& self = lua::checkudata<fs::path>(L, 1);
            self.remove_filename();
            lua_settop(L, 1);
            return 1;
        }

        static int replace_filename(lua_State* L) {
            auto& self = lua::checkudata<fs::path>(L, 1);
            path_ref path(L, 2);
            self.replace_filename(path);
            lua_settop(L, 1);
            return 1;
        }

        static int replace_extension(lua_State* L) {
            auto& self = lua::checkudata<fs::path>(L, 1);
            path_ref path(L, 2);
            self.replace_extension(path);
            lua_settop(L, 1);
            return 1;
        }

        static int lexically_normal(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            lua::newudata<fs::path>(L, self.lexically_normal());
            return 1;
        }

        static int mt_div(lua_State* L) {
            path_ref lft(L, 1);
            path_ref rht(L, 2);
            lua::newudata<fs::path>(L, (*lft) / (*rht));
            return 1;
        }

        static int mt_concat(lua_State* L) {
            path_ref lft(L, 1);
            path_ref rht(L, 2);
            lua::newudata<fs::path>(L, lft->native() + rht->native());
            return 1;
        }

        static int mt_eq(lua_State* L) {
            const auto& lft = lua::checkudata<fs::path>(L, 1);
            const auto& rht = lua::checkudata<fs::path>(L, 2);
            lua_pushboolean(L, path_equal(lft, rht));
            return 1;
        }

        static int mt_tostring(lua_State* L) {
            const auto& self = lua::checkudata<fs::path>(L, 1);
            auto str         = tostring(self);
            lua_pushlstring(L, str.data(), str.size());
            return 1;
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                { "string", mt_tostring },
                { "filename", filename },
                { "parent_path", parent_path },
                { "stem", stem },
                { "extension", extension },
                { "is_absolute", is_absolute },
                { "is_relative", is_relative },
                { "remove_filename", remove_filename },
                { "replace_filename", replace_filename },
                { "replace_extension", replace_extension },
                { "lexically_normal", lexically_normal },
                { NULL, NULL },
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                { "__div", mt_div },
                { "__concat", mt_concat },
                { "__eq", mt_eq },
                { "__tostring", mt_tostring },
                { "__debugger_tostring", mt_tostring },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
    }

    namespace file_status {
        static fs::file_status& to(lua_State* L, int idx) {
            return lua::checkudata<fs::file_status>(L, idx);
        }

        static const char* filetypename(fs::file_type type) {
            switch (type) {
            case fs::file_type::none:
                return "none";
            case fs::file_type::not_found:
                return "not_found";
            case fs::file_type::regular:
                return "regular";
            case fs::file_type::directory:
                return "directory";
            case fs::file_type::symlink:
                return "symlink";
            case fs::file_type::block:
                return "block";
            case fs::file_type::character:
                return "character";
            case fs::file_type::fifo:
                return "fifo";
            case fs::file_type::socket:
                return "socket";
#if defined(BEE_ENABLE_FILESYSTEM) && defined(_MSC_VER)
            case fs::file_type::junction:
                return "junction";
#endif
            default:
            case fs::file_type::unknown:
                return "unknown";
            }
        }

        static int type(lua_State* L) {
            const auto& status = to(L, 1);
            lua_pushstring(L, filetypename(status.type()));
            return 1;
        }

        static int exists(lua_State* L) {
            const auto& status = to(L, 1);
            lua_pushboolean(L, fs::exists(status));
            return 1;
        }

        static int is_directory(lua_State* L) {
            const auto& status = to(L, 1);
            lua_pushboolean(L, fs::is_directory(status));
            return 1;
        }

        static int is_regular_file(lua_State* L) {
            const auto& status = to(L, 1);
            lua_pushboolean(L, fs::is_regular_file(status));
            return 1;
        }

        static int mt_eq(lua_State* L) {
            const auto& l = to(L, 1);
            const auto& r = to(L, 2);
            lua_pushboolean(L, l.type() == r.type() && l.permissions() == r.permissions());
            return 1;
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                { "type", type },
                { "exists", exists },
                { "is_directory", is_directory },
                { "is_regular_file", is_regular_file },
                { NULL, NULL },
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                { "__eq", mt_eq },
                { "__tostring", type },
                { "__debugger_tostring", type },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
    }

    namespace directory_entry {
        static fs::directory_entry& to(lua_State* L, int idx) {
            return lua::checkudata<fs::directory_entry>(L, idx);
        }

        static int path(lua_State* L) {
            const auto& entry = to(L, 1);
            lua::newudata<fs::path>(L, entry.path());
            return 1;
        }

        static lua::cxx::status refresh(lua_State* L) {
            auto& entry = to(L, 1);
            std::error_code ec;
            entry.refresh(ec);
            if (ec) {
                return pusherror(L, "directory_entry::refresh", ec);
            }
            return 0;
        }

        static int status(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua::newudata<fs::file_status>(L, entry.status(ec));
            return 1;
        }

        static int symlink_status(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua::newudata<fs::file_status>(L, entry.symlink_status(ec));
            return 1;
        }

        static int type(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua_pushstring(L, file_status::filetypename(entry.status(ec).type()));
            return 1;
        }

        static int exists(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.exists(ec));
            return 1;
        }

        static int is_directory(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.is_directory(ec));
            return 1;
        }

        static int is_regular_file(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.is_regular_file(ec));
            return 1;
        }

        static lua::cxx::status last_write_time(lua_State* L) {
            static_assert(std::is_integral_v<fs::file_time_type::duration::rep>);
            const auto& entry = to(L, 1);
            std::error_code ec;
            auto time = entry.last_write_time(ec);
            if (ec) {
                return pusherror(L, "directory_entry::last_write_time", ec);
            }
            auto time_count = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
            return lua::narrow_pushinteger(L, time_count);
        }

        static lua::cxx::status file_size(lua_State* L) {
            const auto& entry = to(L, 1);
            std::error_code ec;
            auto size = entry.file_size(ec);
            if (ec) {
                return pusherror(L, "directory_entry::file_size", ec);
            }
            return lua::narrow_pushinteger(L, size);
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                { "path", path },
                { "refresh", lua::cxx::cfunc<refresh> },
                { "status", status },
                { "symlink_status", symlink_status },
                { "type", type },
                { "exists", exists },
                { "is_directory", is_directory },
                { "is_regular_file", is_regular_file },
                { "last_write_time", lua::cxx::cfunc<last_write_time> },
                { "file_size", lua::cxx::cfunc<file_size> },
                { NULL, NULL },
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                { "__tostring", path },
                { "__debugger_tostring", path },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
    }

    static int status(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        lua::newudata<fs::file_status>(L, fs::status(p, ec));
        return 1;
    }

    static int symlink_status(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        lua::newudata<fs::file_status>(L, fs::symlink_status(p, ec));
        return 1;
    }

    static int exists(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::exists(status));
        return 1;
    }

    static int is_directory(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::is_directory(status));
        return 1;
    }

    static int is_regular_file(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::is_regular_file(status));
        return 1;
    }

    static lua::cxx::status file_size(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto size = fs::file_size(p, ec);
        if (ec) {
            return pusherror(L, "file_size", ec, p);
        }
        return lua::narrow_pushinteger(L, size);
    }

    static lua::cxx::status create_directory(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto r = fs::create_directory(p, ec);
        if (ec) {
            return pusherror(L, "create_directory", ec, p);
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static lua::cxx::status create_directories(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto r = fs::create_directories(p, ec);
        if (ec) {
            return pusherror(L, "create_directories", ec, p);
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static lua::cxx::status rename(lua_State* L) {
        path_ref from(L, 1);
        path_ref to(L, 2);
        std::error_code ec;
        fs::rename(from, to, ec);
        if (ec) {
            return pusherror(L, "rename", ec, from, to);
        }
        return 0;
    }

    static lua::cxx::status remove(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
#if defined(_WIN32)
        auto r = workaround::remove(p, ec);
#else
        auto r = fs::remove(p, ec);
#endif
        if (ec) {
            return pusherror(L, "remove", ec, p);
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static lua::cxx::status remove_all(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
#if defined(_WIN32)
        auto r = workaround::remove_all(p, ec);
#else
        auto r = fs::remove_all(p, ec);
#endif
        if (ec) {
            return pusherror(L, "remove_all", ec, p);
        }
        return lua::narrow_pushinteger(L, r);
    }

    static lua::cxx::status current_path(lua_State* L) {
        std::error_code ec;
        if (lua_gettop(L) == 0) {
            fs::path r = fs::current_path(ec);
            if (ec) {
                return pusherror(L, "current_path()", ec);
            }
            lua::newudata<fs::path>(L, std::move(r));
            return 1;
        }
        path_ref p(L, 1);
        fs::current_path(p, ec);
        if (ec) {
            return pusherror(L, "current_path(path)", ec, p);
        }
        return 0;
    }

    static lua::cxx::status copy(lua_State* L) {
        path_ref from(L, 1);
        path_ref to(L, 2);
        auto options = lua::optinteger<fs::copy_options, fs::copy_options::none>(L, 3);
        std::error_code ec;
        fs::copy(from, to, options, ec);
        if (ec) {
            return pusherror(L, "copy", ec, from, to);
        }
        return 0;
    }

    static lua::cxx::status copy_file(lua_State* L) {
        path_ref from(L, 1);
        path_ref to(L, 2);
        auto options = lua::optinteger<fs::copy_options, fs::copy_options::none>(L, 3);
        std::error_code ec;
#if defined(__MINGW32__)
        auto ok = workaround::copy_file(from, to, options, ec);
#else
        auto ok = fs::copy_file(from, to, options, ec);
#endif
        if (ec) {
            return pusherror(L, "copy_file", ec, from, to);
        }
        lua_pushboolean(L, ok);
        return 1;
    }

    static lua::cxx::status absolute(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto r = fs::absolute(p, ec);
        if (ec) {
            return pusherror(L, "absolute", ec, p);
        }
        lua::newudata<fs::path>(L, std::move(r));
        return 1;
    }

    static lua::cxx::status canonical(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        auto r = fs::canonical(p, ec);
        if (ec) {
            return pusherror(L, "canonical", ec, p);
        }
        lua::newudata<fs::path>(L, std::move(r));
        return 1;
    }

    static lua::cxx::status relative(lua_State* L) {
        path_ref p(L, 1);
        std::error_code ec;
        if (lua_gettop(L) == 1) {
            fs::path r = fs::relative(p, ec);
            if (ec) {
                return pusherror(L, "relative", ec, p);
            }
            lua::newudata<fs::path>(L, std::move(r));
            return 1;
        }
        path_ref base(L, 2);
        auto r = fs::relative(p, base, ec);
        if (ec) {
            return pusherror(L, "relative", ec, p, base);
        }
        lua::newudata<fs::path>(L, std::move(r));
        return 1;
    }

    static lua::cxx::status last_write_time(lua_State* L) {
        static_assert(std::is_integral_v<fs::file_time_type::duration::rep>);
        path_ref p(L, 1);
        if (lua_gettop(L) == 1) {
            std::error_code ec;
            auto time = fs::last_write_time(p, ec);
            if (ec) {
                return pusherror(L, "last_write_time", ec, p);
            }
            auto time_count = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
            return lua::narrow_pushinteger(L, time_count);
        }
        auto sec = std::chrono::seconds(lua::checkinteger<std::chrono::seconds::rep>(L, 2));
        std::error_code ec;
        fs::last_write_time(p, fs::file_time_type(sec), ec);
        if (ec) {
            return pusherror(L, "last_write_time", ec, p);
        }
        return 0;
    }

    static lua::cxx::status permissions(lua_State* L) {
        path_ref p(L, 1);
        switch (lua_gettop(L)) {
        case 1: {
            std::error_code ec;
            auto status = fs::status(p, ec);
            if (!fs::status_known(status) || status.type() == fs::file_type::not_found) {
                return pusherror(L, "status", ec, p);
            }
            lua_pushinteger(L, lua_Integer(status.permissions()));
            return 1;
        }
        case 2: {
            auto perms = fs::perms::mask & lua::checkinteger<fs::perms>(L, 2);
            std::error_code ec;
            fs::permissions(p, perms, ec);
            if (ec) {
                return pusherror(L, "permissions", ec, p);
            }
            return 0;
        }
        default: {
            auto perms   = fs::perms::mask & lua::checkinteger<fs::perms>(L, 2);
            auto options = lua::checkinteger<fs::perm_options>(L, 3);
            std::error_code ec;
            fs::permissions(p, perms, options, ec);
            if (ec) {
                return pusherror(L, "permissions", ec, p);
            }
            return 0;
        }
        }
    }

    static lua::cxx::status create_symlink(lua_State* L) {
        path_ref target(L, 1);
        path_ref link(L, 2);
        std::error_code ec;
        fs::create_symlink(target, link, ec);
        if (ec) {
            return pusherror(L, "create_symlink", ec, target, link);
        }
        return 0;
    }

    static lua::cxx::status create_directory_symlink(lua_State* L) {
        path_ref target(L, 1);
        path_ref link(L, 2);
        std::error_code ec;
        fs::create_directory_symlink(target, link, ec);
        if (ec) {
            return pusherror(L, "create_directory_symlink", ec, target, link);
        }
        return 0;
    }

    static lua::cxx::status create_hard_link(lua_State* L) {
        path_ref target(L, 1);
        path_ref link(L, 2);
        std::error_code ec;
        fs::create_hard_link(target, link, ec);
        if (ec) {
            return pusherror(L, "create_hard_link", ec, target, link);
        }
        return 0;
    }

    static lua::cxx::status temp_directory_path(lua_State* L) {
        std::error_code ec;
        auto r = fs::temp_directory_path(ec);
        if (ec) {
            return pusherror(L, "temp_directory_path", ec);
        }
        lua::newudata<fs::path>(L, std::move(r));
        return 1;
    }

    template <typename T>
    struct pairs_directory {
        static lua::cxx::status next(lua_State* L) {
            auto& iter = lua::toudata<T>(L, lua_upvalueindex(1));
            if (iter == T {}) {
                return 0;
            }
            lua::newudata<fs::path>(L, iter->path());
            lua::newudata<fs::directory_entry>(L, *iter);
            std::error_code ec;
            iter.increment(ec);
            if (ec) {
                return pusherror(L, "directory_iterator::operator++", ec);
            }
            return 2;
        }
        static int mt_close(lua_State* L) {
            auto& iter = lua::checkudata<T>(L, 1);
            iter       = {};
            return 0;
        }
        static void metatable(lua_State* L) {
            static luaL_Reg mt[] = {
                { "__close", mt_close },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
        static lua::cxx::status ctor(lua_State* L) {
            path_ref path(L, 1);
            auto options = lua::optinteger<fs::directory_options, fs::directory_options::none>(L, 2);
            std::error_code ec;
            lua::newudata<T>(L, path, options, ec);
            if (ec) {
                return pusherror(L, "directory_iterator::directory_iterator", ec, path);
            }
            lua_pushvalue(L, -1);
            lua_pushcclosure(L, lua::cxx::cfunc<next>, 1);
            lua_pushnil(L);
            lua_pushnil(L);
            lua_rotate(L, -4, -1);
            return 4;
        }
    };

    using pairs   = pairs_directory<fs::directory_iterator>;
    using pairs_r = pairs_directory<fs::recursive_directory_iterator>;

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            { "path", path::constructor },
            { "status", status },
            { "symlink_status", symlink_status },
            { "exists", exists },
            { "is_directory", is_directory },
            { "is_regular_file", is_regular_file },
            { "file_size", lua::cxx::cfunc<file_size> },
            { "create_directory", lua::cxx::cfunc<create_directory> },
            { "create_directories", lua::cxx::cfunc<create_directories> },
            { "rename", lua::cxx::cfunc<rename> },
            { "remove", lua::cxx::cfunc<remove> },
            { "remove_all", lua::cxx::cfunc<remove_all> },
            { "current_path", lua::cxx::cfunc<current_path> },
            { "copy", lua::cxx::cfunc<copy> },
            { "copy_file", lua::cxx::cfunc<copy_file> },
            { "absolute", lua::cxx::cfunc<absolute> },
            { "canonical", lua::cxx::cfunc<canonical> },
            { "relative", lua::cxx::cfunc<relative> },
            { "last_write_time", lua::cxx::cfunc<last_write_time> },
            { "permissions", lua::cxx::cfunc<permissions> },
            { "create_symlink", lua::cxx::cfunc<create_symlink> },
            { "create_directory_symlink", lua::cxx::cfunc<create_directory_symlink> },
            { "create_hard_link", lua::cxx::cfunc<create_hard_link> },
            { "temp_directory_path", lua::cxx::cfunc<temp_directory_path> },
            { "pairs", lua::cxx::cfunc<pairs::ctor> },
            { "pairs_r", lua::cxx::cfunc<pairs_r::ctor> },
            { "copy_options", NULL },
            { "perm_options", NULL },
            { "directory_options", NULL },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);

#define DEF_ENUM(CLASS, MEMBER)                                      \
    lua_pushinteger(L, static_cast<lua_Integer>(fs::CLASS::MEMBER)); \
    lua_setfield(L, -2, #MEMBER);

        lua_createtable(L, 0, 16);
        DEF_ENUM(copy_options, none);
        DEF_ENUM(copy_options, skip_existing);
        DEF_ENUM(copy_options, overwrite_existing);
        DEF_ENUM(copy_options, update_existing);
        DEF_ENUM(copy_options, recursive);
        DEF_ENUM(copy_options, copy_symlinks);
        DEF_ENUM(copy_options, skip_symlinks);
        DEF_ENUM(copy_options, directories_only);
        DEF_ENUM(copy_options, create_symlinks);
        DEF_ENUM(copy_options, create_hard_links);
        lua_setfield(L, -2, "copy_options");

        lua_createtable(L, 0, 4);
        DEF_ENUM(perm_options, replace);
        DEF_ENUM(perm_options, add);
        DEF_ENUM(perm_options, remove);
        DEF_ENUM(perm_options, nofollow);
        lua_setfield(L, -2, "perm_options");

        lua_createtable(L, 0, 3);
        DEF_ENUM(directory_options, none);
        DEF_ENUM(directory_options, follow_directory_symlink);
        DEF_ENUM(directory_options, skip_permission_denied);
        lua_setfield(L, -2, "directory_options");
#undef DEF_ENUM
        return 1;
    }
}

DEFINE_LUAOPEN(filesystem)

namespace bee::lua {
    fs::path& new_path(lua_State* L, fs::path&& path) {
        return lua::newudata<fs::path>(L, std::forward<fs::path>(path));
    }

    template <>
    struct udata<fs::path> {
        static inline auto metatable = bee::lua_filesystem::path::metatable;
    };
    template <>
    struct udata<fs::file_status> {
        static inline auto metatable = bee::lua_filesystem::file_status::metatable;
    };
    template <>
    struct udata<fs::directory_entry> {
        static inline auto metatable = bee::lua_filesystem::directory_entry::metatable;
    };
    template <>
    struct udata<fs::recursive_directory_iterator> {
        static inline auto metatable = bee::lua_filesystem::pairs_r::metatable;
    };
    template <>
    struct udata<fs::directory_iterator> {
        static inline auto metatable = bee::lua_filesystem::pairs::metatable;
    };
}
