#include <binding/binding.h>
#include <binding/udata.h>
#include <binding/file.h>
#include <bee/error.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/file_handle.h>
#include <bee/utility/path_helper.h>
#include <utility>

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#define BEE_DISABLE_FULLPATH
#endif

namespace bee::lua {
    template <>
    struct udata<fs::file_status> {
        static inline auto name = "bee::file_status";
    };
    template <>
    struct udata<fs::directory_entry> {
        static inline auto name = "bee::directory_entry";
    };
    template <>
    struct udata<fs::recursive_directory_iterator> {
        static inline auto name = "bee::pairs_r";
    };
    template <>
    struct udata<fs::directory_iterator> {
        static inline auto name = "bee::pairs";
    };

}

namespace bee::lua_filesystem {
#if defined(__cpp_lib_char8_t)
    static std::string_view u8tostrview(std::u8string const& u8str) {
        return { reinterpret_cast<const char*>(u8str.data()), u8str.size() };
    }
#else
    static std::string_view u8tostrview(std::string const& u8str) {
        return { u8str.data(), u8str.size() };
    }
#endif

    static int pusherror(
        lua_State* L,
        const char* op,
        std::error_code ec
    ) {
        {
            auto errmsg = std::format("{}: {}",
                op,
                ec.message()
            );
            lua_pushlstring(L, errmsg.data(), errmsg.size());
        }
        return lua_error(L);
    }

    static int pusherror(
        lua_State* L,
        const char* op,
        std::error_code ec,
        const fs::path& path1
    ) {
        {
            auto errmsg = std::format("{}: {}: \"{}\"",
                op,
                ec.message(),
                u8tostrview(path1.generic_u8string())
            );
            lua_pushlstring(L, errmsg.data(), errmsg.size());
        }
        return lua_error(L);
    }

    static int pusherror(
        lua_State* L,
        const char* op,
        std::error_code ec,
        const fs::path& path1,
        const fs::path& path2
    ) {
        {
            auto errmsg = std::format("{}: {}: \"{}\", \"{}\"",
                op,
                ec.message(),
                u8tostrview(path1.generic_u8string()),
                u8tostrview(path2.generic_u8string())
            );
            lua_pushlstring(L, errmsg.data(), errmsg.size());
        }
        return lua_error(L);
    }

    template <typename T>
    class constptr {
    public:
        using value_type = T;
        ~constptr() {
            if (has_val) {
                val.~value_type();
            }
        }
        constptr(value_type&& v)
            : has_val {true} {
            new (&val) value_type(std::move(v));
        }
        constptr(const value_type* p)
            : has_val {false}
            , ptr {p}
        { }
        const value_type* operator->() const {
            if (has_val) {
                return &val;
            }
            else {
                return ptr;
            }
        }
        const value_type& operator*() const {
            if (has_val) {
                return val;
            }
            else {
                return *ptr;
            }
        }
        operator const value_type& () const& {
            if (has_val) {
                return val;
            }
            else {
                return *ptr;
            }
        }
    private:
        bool has_val;
        union {
            value_type        val;
            const value_type* ptr;
        };
    };
    using path_ptr = constptr<fs::path>;

    static fs::path& getpath(lua_State* L, int idx) {
        return lua::checkudata<fs::path>(L, idx);
    }

    static path_ptr getpathptr(lua_State* L, int idx) {
        if (lua_type(L, idx) == LUA_TSTRING) {
            return fs::path { lua::checkstring(L, idx) };
        }
        return &getpath(L, idx);
    }

    namespace path {
        static void push(lua_State* L);
        static void push(lua_State* L, const fs::path& path);
        static void push(lua_State* L, fs::path&& path);

        static int  constructor(lua_State* L) {
            if (lua_gettop(L) == 0) {
                push(L);
            }
            else {
                push(L, getpathptr(L, 1));
            }
            return 1;
        }

        static int filename(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            push(L, self->filename());
            return 1;
        }

        static int parent_path(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            push(L, self->parent_path());
            return 1;
        }

        static int stem(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            push(L, self->stem());
            return 1;
        }

        static int extension(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            push(L, self->extension());
            return 1;
        }

        static int is_absolute(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            lua_pushboolean(L, self->is_absolute());
            return 1;
        }

        static int is_relative(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            lua_pushboolean(L, self->is_relative());
            return 1;
        }

        static int remove_filename(lua_State* L) {
            fs::path& self = getpath(L, 1);
            self.remove_filename();
            return 1;
        }

        static int replace_filename(lua_State* L) {
            fs::path& self = getpath(L, 1);
            path_ptr path = getpathptr(L, 2);
            self.replace_filename(path);
            lua_settop(L, 1);
            return 1;
        }

        static int replace_extension(lua_State* L) {
            fs::path& self = getpath(L, 1);
            path_ptr path = getpathptr(L, 2);
            self.replace_extension(path);
            lua_settop(L, 1);
            return 1;
        }

        static int equal_extension(lua_State* L, const fs::path& self, const fs::path::string_type& ext) {
            auto const& selfext = self.extension();
            if (selfext.empty()) {
                lua_pushboolean(L, ext.empty());
                return 1;
            }
            if (ext[0] != '.') {
                lua_pushboolean(L, path_helper::equal(selfext, fs::path::string_type{'.'} + ext));
                return 1;
            }
            lua_pushboolean(L, path_helper::equal(selfext, ext));
            return 1;
        }

        static int equal_extension(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            path_ptr path = getpathptr(L, 2);
            return equal_extension(L, self, *path);
        }

        static int lexically_normal(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            push(L, self->lexically_normal());
            return 1;
        }

        static int mt_div(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            path_ptr path = getpathptr(L, 2);
            push(L, self / path);
            return 1;
        }

        static int mt_concat(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            path_ptr path = getpathptr(L, 2);
            push(L, self->native() + path->native());
            return 1;
        }

        static int mt_eq(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            path_ptr rht = getpathptr(L, 2);
            lua_pushboolean(L, path_helper::equal(self, rht));
            return 1;
        }

        static int mt_tostring(lua_State* L) {
            path_ptr self = getpathptr(L, 1);
            auto u8str = self->generic_u8string();
            auto str = u8tostrview(u8str);
            lua_pushlstring(L, str.data(), str.size());
            return 1;
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                {"string", mt_tostring},
                {"filename", filename},
                {"parent_path", parent_path},
                {"stem", stem},
                {"extension", extension},
                {"is_absolute", is_absolute},
                {"is_relative", is_relative},
                {"remove_filename", remove_filename},
                {"replace_filename", replace_filename},
                {"replace_extension", replace_extension},
                {"equal_extension", equal_extension},
                {"lexically_normal", lexically_normal},
                {NULL, NULL},
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                {"__div", mt_div},
                {"__concat", mt_concat},
                {"__eq", mt_eq},
                {"__tostring", mt_tostring},
                {"__debugger_tostring", mt_tostring},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
        }
        static void push(lua_State* L) {
            lua::newudata<fs::path>(L, metatable);
        }
        static void push(lua_State* L, const fs::path& path) {
            lua::newudata<fs::path>(L, metatable, path);
        }
        static void push(lua_State* L, fs::path&& path) {
            lua::newudata<fs::path>(L, metatable, std::forward<fs::path>(path));
        }
    }

    namespace file_status {
        static void push(lua_State* L, fs::file_status&& status);

        static fs::file_status& to(lua_State* L, int idx) {
            return lua::checkudata<fs::file_status>(L, idx);
        }

        static const char* filetypename(fs::file_type type) {
            switch (type) {
            case fs::file_type::none:      return "none";
            case fs::file_type::not_found: return "not_found";
            case fs::file_type::regular:   return "regular";
            case fs::file_type::directory: return "directory";
            case fs::file_type::symlink:   return "symlink";
            case fs::file_type::block:     return "block";
            case fs::file_type::character: return "character";
            case fs::file_type::fifo:      return "fifo";
            case fs::file_type::socket:    return "socket";
    #if defined(BEE_ENABLE_FILESYSTEM) && defined(_MSC_VER)
            case fs::file_type::junction:  return "junction";
    #endif
            default:
            case fs::file_type::unknown:   return "unknown";
            }
        }

        static int type(lua_State* L) {
            auto const& status = to(L, 1);
            lua_pushstring(L, filetypename(status.type()));
            return 1;
        }

        static int exists(lua_State* L) {
            auto const& status = to(L, 1);
            lua_pushboolean(L, fs::exists(status));
            return 1;
        }

        static int is_directory(lua_State* L) {
            auto const& status = to(L, 1);
            lua_pushboolean(L, fs::is_directory(status));
            return 1;
        }

        static int is_regular_file(lua_State* L) {
            auto const& status = to(L, 1);
            lua_pushboolean(L, fs::is_regular_file(status));
            return 1;
        }

        static int mt_eq(lua_State* L) {
            auto const& l = to(L, 1);
            auto const& r = to(L, 2);
            lua_pushboolean(L, l.type() == r.type() && l.permissions() == r.permissions());
            return 1;
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                {"type", type},
                {"exists", exists},
                {"is_directory", is_directory},
                {"is_regular_file", is_regular_file},
                {NULL, NULL},
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                {"__eq", mt_eq},
                {"__tostring", type},
                {"__debugger_tostring", type},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
        }
        static void push(lua_State* L, fs::file_status&& status) {
            lua::newudata<fs::file_status>(L, metatable, std::forward<fs::file_status>(status));
        }
    }

    namespace directory_entry {
        static fs::directory_entry& to(lua_State* L, int idx) {
            return lua::checkudata<fs::directory_entry>(L, idx);
        }

        static int path(lua_State* L) {
            auto const& entry = to(L, 1);
            path::push(L, entry.path());
            return 1;
        }

        static int status(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            file_status::push(L, entry.status(ec));
            return 1;
        }

        static int symlink_status(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            file_status::push(L, entry.symlink_status(ec));
            return 1;
        }

        static int type(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            lua_pushstring(L, file_status::filetypename(entry.status(ec).type()));
            return 1;
        }

        static int exists(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.exists(ec));
            return 1;
        }

        static int is_directory(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.is_directory(ec));
            return 1;
        }

        static int is_regular_file(lua_State* L) {
            auto const& entry = to(L, 1);
            std::error_code ec;
            lua_pushboolean(L, entry.is_regular_file(ec));
            return 1;
        }

        static void metatable(lua_State* L) {
            static luaL_Reg lib[] = {
                {"path", path},
                {"status", status},
                {"symlink_status", symlink_status},
                {"type", type},
                {"exists", exists},
                {"is_directory", is_directory},
                {"is_regular_file", is_regular_file},
                {NULL, NULL},
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            static luaL_Reg mt[] = {
                {"__tostring", path},
                {"__debugger_tostring", path},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
        }
        static void push(lua_State* L, fs::directory_entry const& entry) {
            lua::newudata<fs::directory_entry>(L, metatable, entry);
        }
    }

    static int status(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        file_status::push(L, fs::status(p, ec));
        return 1;
    }
    
    static int symlink_status(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        file_status::push(L, fs::symlink_status(p, ec));
        return 1;
    }

    static int exists(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::exists(status));
        return 1;
    }

    static int is_directory(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::is_directory(status));
        return 1;
    }

    static int is_regular_file(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        auto status = fs::status(p, ec);
        lua_pushboolean(L, fs::is_regular_file(status));
        return 1;
    }

    static int create_directory(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        bool r = fs::create_directory(p, ec);
        if (ec) {
            return pusherror(L, "create_directory", ec, p); 
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static int create_directories(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        bool r = fs::create_directories(p, ec);
        if (ec) {
            return pusherror(L, "create_directories", ec, p); 
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static int rename(lua_State* L) {
        path_ptr from = getpathptr(L, 1);
        path_ptr to = getpathptr(L, 2);
        std::error_code ec;
        fs::rename(from, to, ec);
        if (ec) {
            return pusherror(L, "rename", ec, from, to); 
        }
        return 0;
    }

    static int remove(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        bool r = fs::remove(p, ec);
        if (ec) {
            return pusherror(L, "remove", ec, p); 
        }
        lua_pushboolean(L, r);
        return 1;
    }

    static int remove_all(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        uintmax_t r = fs::remove_all(p, ec);
        if (ec) {
            return pusherror(L, "remove_all", ec, p); 
        }
        lua_pushinteger(L, static_cast<lua_Integer>(r));
        return 1;
    }

    static int current_path(lua_State* L) {
        std::error_code ec;
        if (lua_gettop(L) == 0) {
            fs::path r = fs::current_path(ec);
            if (ec) {
                return pusherror(L, "current_path()", ec); 
            }
            path::push(L, r);
            return 1;
        }
        path_ptr p = getpathptr(L, 1);
        fs::current_path(p, ec);
        if (ec) {
            return pusherror(L, "current_path(path)", ec, p); 
        }
        return 0;
    }

    static int copy(lua_State* L) {
        path_ptr from = getpathptr(L, 1);
        path_ptr to = getpathptr(L, 2);
        fs::copy_options options = fs::copy_options::none;
        if (lua_gettop(L) > 2) {
            options = lua::checkinteger<fs::copy_options>(L, 3, "options");
        }
        std::error_code ec;
        fs::copy(from, to, options, ec);
        if (ec) {
            return pusherror(L, "copy", ec, from, to); 
        }
        return 0;
    }

#if defined(__MINGW32__)
    static bool mingw_copy_file(const fs::path& from, const fs::path& to, fs::copy_options options, std::error_code& ec) {
        try {
            if (fs::exists(from) && fs::exists(to)) {
                if ((options & fs::copy_options::overwrite_existing) != fs::copy_options::none) {
                    fs::remove(to);
                }
                else if ((options & fs::copy_options::update_existing) != fs::copy_options::none) {
                    if (fs::last_write_time(from) > fs::last_write_time(to)) {
                        fs::remove(to);
                    }
                    else {
                        return false;
                    }
                }
                else if ((options & fs::copy_options::skip_existing) != fs::copy_options::none) {
                    return false;
                }
            }
            return fs::copy_file(from, to, options);
        }
        catch(const fs::filesystem_error& e) {
            ec = e.code();
            return false;
        }
    }
#endif

    static int copy_file(lua_State* L) {
        path_ptr from = getpathptr(L, 1);
        path_ptr to = getpathptr(L, 2);
        fs::copy_options options = fs::copy_options::none;
        if (lua_gettop(L) > 2) {
            options = lua::checkinteger<fs::copy_options>(L, 3, "options");
        }
        std::error_code ec;
#if defined(__MINGW32__)
        bool ok = mingw_copy_file(from, to, options, ec);
#else
        bool ok = fs::copy_file(from, to, options, ec);
#endif
        if (ec) {
            return pusherror(L, "copy_file", ec, from, to); 
        }
        lua_pushboolean(L, ok);
        return 1;
    }

    static int absolute(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        fs::path r = fs::absolute(p, ec);
        if (ec) {
            return pusherror(L, "absolute", ec, p); 
        }
        path::push(L, r);
        return 1;
    }

    static int canonical(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        fs::path r = fs::canonical(p, ec);
        if (ec) {
            return pusherror(L, "canonical", ec, p); 
        }
        path::push(L, r);
        return 1;
    }
 
    static int relative(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        if (lua_gettop(L) == 1) {
            fs::path r = fs::relative(p, ec);
            if (ec) {
                return pusherror(L, "relative", ec, p); 
            }
            path::push(L, r);
            return 1;
        }
        path_ptr base = getpathptr(L, 2);
        fs::path r = fs::relative(p, base, ec);
        if (ec) {
            return pusherror(L, "relative", ec, p, base); 
        }
        path::push(L, r);
        return 1;
    }

#if !defined(__cpp_lib_chrono) || __cpp_lib_chrono < 201907
    template <class DestClock, class SourceClock, class Duration>
    static auto clock_cast(const std::chrono::time_point<SourceClock, Duration>& t) {
        return DestClock::now() + (t - SourceClock::now());
    }
#endif

    static int last_write_time(lua_State* L) {
        using namespace std::chrono;
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        if (lua_gettop(L) == 1) {
            auto time = fs::last_write_time(p, ec);
            if (ec) {
                return pusherror(L, "last_write_time", ec, p); 
            }
            auto system_time = clock_cast<system_clock>(time);
            lua_pushinteger(L, duration_cast<seconds>(system_time.time_since_epoch()).count());
            return 1;
        }
        auto file_time = clock_cast<fs::file_time_type::clock>(system_clock::time_point() + seconds(luaL_checkinteger(L, 2)));
        fs::last_write_time(p, file_time, ec);
        if (ec) {
            return pusherror(L, "last_write_time", ec, p); 
        }
        return 0;
    }
    
    static int permissions(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        std::error_code ec;
        switch (lua_gettop(L)) {
        case 1: {
            auto status = fs::status(p, ec);
            if (!fs::status_known(status) || status.type() == fs::file_type::not_found) {
                return pusherror(L, "status", ec, p); 
            }
            lua_pushinteger(L, lua_Integer(status.permissions()));
            return 1;
        }
        case 2: {
            auto perms = fs::perms::mask & lua::checkinteger<fs::perms>(L, 2, "perms");
            fs::permissions(p, perms, ec);
            if (ec) {
                return pusherror(L, "permissions", ec, p); 
            }
            return 0;
        }
        default: {
            auto perms = fs::perms::mask & lua::checkinteger<fs::perms>(L, 2, "perms");
            auto options = lua::checkinteger<fs::perm_options>(L, 3, "options");
            fs::permissions(p, perms, options, ec);
            if (ec) {
                return pusherror(L, "permissions", ec, p); 
            }
            return 0;
        }
        }
    }
    
    static int create_symlink(lua_State* L) {
        path_ptr target = getpathptr(L, 1);
        path_ptr link = getpathptr(L, 2);
        std::error_code ec;
        fs::create_symlink(target, link, ec);
        if (ec) {
            return pusherror(L, "create_symlink", ec, target, link); 
        }
        return 0;
    }

    static int create_directory_symlink(lua_State* L) {
        path_ptr target = getpathptr(L, 1);
        path_ptr link = getpathptr(L, 2);
        std::error_code ec;
        fs::create_directory_symlink(target, link, ec);
        if (ec) {
            return pusherror(L, "create_directory_symlink", ec, target, link); 
        }
        return 0;
    }

    static int create_hard_link(lua_State* L) {
        path_ptr target = getpathptr(L, 1);
        path_ptr link = getpathptr(L, 2);
        std::error_code ec;
        fs::create_hard_link(target, link, ec);
        if (ec) {
            return pusherror(L, "create_hard_link", ec, target, link); 
        }
        return 0;
    }

    static int temp_directory_path(lua_State* L) {
        std::error_code ec;
        fs::path r = fs::temp_directory_path(ec);
        if (ec) {
            return pusherror(L, "temp_directory_path", ec); 
        }
        path::push(L, r);
        return 1;
    }

    template <typename T>
    struct pairs_directory {
        static int next(lua_State* L) {
            auto& iter = lua::toudata<T>(L, lua_upvalueindex(1));
            if (iter == T {}) {
                lua_pushnil(L);
                return 1;
            }
            path::push(L, iter->path());
            directory_entry::push(L, *iter);
            std::error_code ec;
            iter.increment(ec);
            if (ec) {
                return pusherror(L, "directory_iterator::operator++", ec); 
            }
            return 2;
        }
        static int mt_close(lua_State* L) {
            auto& iter = lua::checkudata<T>(L, 1);
            iter = {};
            return 0;
        }
        static void metatable(lua_State* L) {
            static luaL_Reg mt[] = {
                {"__close", mt_close},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
        }
        static int constructor(lua_State* L, const fs::path& path) {
            std::error_code ec;
            lua::newudata<T>(L, metatable, path, ec);
            if (ec) {
                return pusherror(L, "directory_iterator::directory_iterator", ec, path); 
            }
            lua_pushvalue(L, -1);
            lua_pushcclosure(L, next, 1);
            return 2;
        }
    };

    static int pairs(lua_State* L) {
        path_ptr p = getpathptr(L, 1);
        const char* flags = luaL_optstring(L, 2, "");
        luaL_argcheck(L, (flags[0] == '\0' || (flags[0] == 'r' && flags[1] == '\0')), 2, "invalid flags");
        if (flags[0] == 'r') {
            pairs_directory<fs::recursive_directory_iterator>::constructor(L, p);
        }
        else {
            pairs_directory<fs::directory_iterator>::constructor(L, p);
        }
        lua_pushnil(L);
        lua_pushnil(L);
        lua_rotate(L, -4, -1);
        return 4;
    }

    static int exe_path(lua_State* L) {
        auto r = path_helper::exe_path();
        if (!r) {
            lua_pushnil(L);
            lua_pushstring(L,  r.error().c_str());
            return 2;
        }
        path::push(L, r.value());
        return 1;
    }

    static int dll_path(lua_State* L) {
        auto r = path_helper::dll_path();
        if (!r) {
            lua_pushnil(L);
            lua_pushstring(L,  r.error().c_str());
            return 2;
        }
        path::push(L, r.value());
        return 1;
    }

    static int filelock(lua_State* L) {
        path_ptr self = getpathptr(L, 1);
        file_handle fd = file_handle::lock(self);
        if (!fd) {
            lua_pushnil(L);
            lua_pushstring(L, make_syserror("filelock").what());
            return 2;
        }
        FILE* f = fd.to_file(file_handle::mode::write);
        if (!f) {
            lua_pushnil(L);
            lua_pushstring(L, make_crterror("filelock").what());
            fd.close();
            return 2;
        }
        lua::newfile(L, f);
        return 1;
    }

#if !defined(BEE_DISABLE_FULLPATH)
    static int fullpath(lua_State* L) {
        path_ptr path = getpathptr(L, 1);
        file_handle fd = file_handle::open_link(path);
        if (!fd) {
            lua_pushnil(L);
            lua_pushstring(L, make_syserror("fullpath").what());
            return 2;
        }
        auto fullpath = fd.path();
        fd.close();
        if (!fullpath) {
            lua_pushnil(L);
            lua_pushstring(L, make_syserror("fullpath").what());
            return 2;
        }
        path::push(L, *fullpath);
        return 1;
    }
#endif

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            {"path", path::constructor},
            {"status", status},
            {"symlink_status", symlink_status},
            {"exists", exists},
            {"is_directory", is_directory},
            {"is_regular_file", is_regular_file},
            {"create_directory", create_directory},
            {"create_directories", create_directories},
            {"rename", rename},
            {"remove", remove},
            {"remove_all", remove_all},
            {"current_path", current_path},
            {"copy", copy},
            {"copy_file", copy_file},
            {"absolute", absolute},
            {"canonical", canonical},
            {"relative", relative},
            {"last_write_time", last_write_time},
            {"permissions", permissions},
            {"create_symlink", create_symlink},
            {"create_directory_symlink", create_directory_symlink},
            {"create_hard_link", create_hard_link},
            {"temp_directory_path", temp_directory_path},
            {"pairs", pairs},
            {"exe_path", exe_path},
            {"dll_path", dll_path},
            {"filelock", filelock},
#if !defined(BEE_DISABLE_FULLPATH)
            {"fullpath", fullpath},
#endif
            {"copy_options", NULL},
            {"perm_options", NULL},
            {NULL, NULL},
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);

#define DEF_ENUM(CLASS, MEMBER) \
    lua_pushinteger(L, static_cast<lua_Integer>(fs::CLASS::MEMBER)); \
    lua_setfield(L, -2, #MEMBER);

        lua_newtable(L);
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

        lua_newtable(L);
        DEF_ENUM(perm_options, replace);
        DEF_ENUM(perm_options, add);
        DEF_ENUM(perm_options, remove);
        DEF_ENUM(perm_options, nofollow);
        lua_setfield(L, -2, "perm_options");
        return 1;
    }
}

DEFINE_LUAOPEN(filesystem)
