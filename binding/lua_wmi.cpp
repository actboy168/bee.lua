#include <bee/lua/binding.h>
#include <bee/utility/unicode_win.h>
#include <wrl/client.h>
#include <wbemidl.h>
#include <comdef.h>
#include <string>
#include <memory>

namespace bee::lua_wmi {
    typedef Microsoft::WRL::ComPtr<IWbemServices>    services_t;
    typedef Microsoft::WRL::ComPtr<IWbemClassObject> object_t;

    class scoped_bstr {
    public:
        scoped_bstr() : bstr_(NULL) { }
        scoped_bstr(const wchar_t* bstr) : bstr_(SysAllocString(bstr)) { }
        ~scoped_bstr() { SysFreeString(bstr_); }
        BSTR* operator&() { return &bstr_; }
        operator BSTR() const { return bstr_; }
    protected:
        BSTR bstr_;
    };

    class scoped_variant {
    public:
        scoped_variant() { var_.vt = VT_EMPTY; }
        ~scoped_variant() { ::VariantClear(&var_); }
        VARIANT* operator&() { return &var_; }
        operator VARIANT() const { return var_; }
    protected:
        VARIANT var_;
    };

    static void check(lua_State* L, const char* name, HRESULT hres) {
        if (FAILED(hres)) {
            luaL_error(L, "%s failed:\n\t%s.", name, _com_error(hres).ErrorMessage());
        }
    }

    static void check_wbem(lua_State* L, const char* name, HRESULT hres) {
        if (hres != WBEM_S_NO_ERROR) {
            luaL_error(L, "%s failed:\n\t%s.", name, _com_error(hres).ErrorMessage());
        }
    }

    namespace array {
        static VARTYPE get_vartype(lua_State* L, LPSAFEARRAY array) {
            VARTYPE type;
            check(L, "SafeArrayGetVartype", SafeArrayGetVartype(array, &type));
            return type;
        }
        static LONG get_lbound(lua_State* L, LPSAFEARRAY array, int dim) {
            LONG lbound;
            check(L, "SafeArrayGetLBound", SafeArrayGetLBound(array, dim, &lbound));
            return lbound;
        }
        static LONG get_ubound(lua_State* L, LPSAFEARRAY array, int dim) {
            LONG ubound;
            check(L, "SafeArrayGetUBound", SafeArrayGetUBound(array, dim, &ubound));
            return ubound;
        }
        template <typename T>
        class view {
        public:
            view(lua_State* L, LPSAFEARRAY array)
                : l(L)
                , ary(array)
                , data(access_data(l, ary))
            { }
            ~view() {
                unaccess_data(l, ary);
            }
            T operator[](LONG pos) {
                return ((T*)(data))[pos];
            }
        private:
            static void* access_data(lua_State* L, LPSAFEARRAY array) {
                void* data;
                check(L, "SafeArrayAccessData", SafeArrayAccessData(array, &data));
                return data;
            }
            static void unaccess_data(lua_State* L, LPSAFEARRAY array) {
                check(L, "SafeArrayUnaccessData", SafeArrayUnaccessData(array));
            }
            lua_State* l;
            LPSAFEARRAY ary;
            void* data;
        };
    }

    template <typename T>
    void push_value(lua_State* L);

    template <typename T>
    void push_value(lua_State* L, T v, typename std::enable_if<!std::is_integral<T>::value>::type* = 0);

    template <typename T>
    void push_value(lua_State* L, T v, typename std::enable_if<std::is_integral<T>::value>::type* = 0) {
        lua_pushinteger(L, v);
    }

    template <>
    void push_value<BSTR>(lua_State* L, BSTR v, void*) {
        auto str = w2u(v);
        lua_pushlstring(L, str.data(), str.size());
    }

    template <>
    void push_value<bool>(lua_State* L, bool v, void*) {
        lua_pushboolean(L, v);
    }

    template <>
    void push_value<void>(lua_State* L) {
        lua_pushnil(L);
    }

    template <typename T>
    static void push_array(lua_State* L, LPSAFEARRAY array, LONG lbound, LONG ubound) {
        array::view<T> view(L, array);
        lua_createtable(L, (int)(ubound - lbound + 1), 0);
        lua_Integer n = 0;
        for (LONG i = lbound; i <= ubound; ++i) {
            push_value(L, view[i]);
            lua_seti(L, -2, ++n);
        }
    }

    static void push_variant(lua_State* L, const VARIANT& value) {
        VARTYPE type = value.vt;
        if (type & VT_ARRAY) {
            LPSAFEARRAY array = V_ARRAY(&value);
            if (SafeArrayGetDim(array) != 1) {
                luaL_error(L, "Only supports one-dimensional array.");
                return;
            }
            LONG lbound = array::get_lbound(L, array, 1);
            LONG ubound = array::get_ubound(L, array, 1);
            VARTYPE vt = array::get_vartype(L, array);
            switch (vt) {
            case VT_BSTR: push_array<BSTR>(L, array, lbound, ubound); break;
            case VT_UI1: push_array<uint8_t>(L, array, lbound, ubound); break;
            default: luaL_error(L, "unknown array type = %d", vt); break;
            }
            return;
        }
        switch (value.vt) {
        case VT_BSTR: push_value<BSTR>(L, V_BSTR(&value)); break;
        case VT_BOOL: push_value<bool>(L, V_BOOL(&value)); break;
        case VT_I1:   push_value<int8_t>(L, V_I1(&value)); break;
        case VT_I2:   push_value<int16_t>(L, V_I2(&value)); break;
        case VT_I4:   push_value<int32_t>(L, V_I4(&value)); break;
        case VT_I8:   push_value<int64_t>(L, V_I8(&value)); break;
        case VT_INT:  push_value<int>(L, V_INT(&value)); break;
        case VT_UI1:  push_value<uint8_t>(L, V_UI1(&value)); break;
        case VT_UI2:  push_value<uint16_t>(L, V_UI2(&value)); break;
        case VT_UI4:  push_value<uint32_t>(L, V_UI4(&value)); break;
        case VT_UI8:  push_value<uint64_t>(L, V_UI8(&value)); break;
        case VT_UINT: push_value<unsigned int>(L, V_UINT(&value)); break;
        case VT_NULL: push_value<void>(L); break;
        default: luaL_error(L, "unknown type = %d", value.vt); break;
        }
    }

    static int object_get(lua_State* L) {
        object_t& o = *(object_t*)lua_touserdata(L, 1);
        std::wstring name = lua::checkstring(L, 2);
        scoped_variant value;
        check_wbem(L, "IWbemClassObject::Get", o->Get(name.c_str(), 0, &value, 0, 0));
        push_variant(L, value);
        return 1;
    }

    static int object_next(lua_State* L) {
        object_t& o = *(object_t*)lua_touserdata(L, 1);
        scoped_bstr name;
        scoped_variant value;
        HRESULT hres = o->Next(0, &name, &value, 0, NULL);
        if (WBEM_S_NO_ERROR != hres) {
            o->EndEnumeration();
            return 0;
        }
        push_value<BSTR>(L, name);
        push_variant(L, value);
        return 2;
    }

    static int object_pairs(lua_State* L) {
        object_t& o = *(object_t*)lua_touserdata(L, 1);
        o->BeginEnumeration(WBEM_FLAG_LOCAL_ONLY);
        lua_pushcfunction(L, object_next);
        lua_pushvalue(L, 1);
        return 2;
    }

    static void create_object(lua_State* L, IWbemClassObject* object) {
        void* storage = lua_newuserdata(L, sizeof(object_t));
        new (storage) object_t(object);
        if (luaL_newmetatable(L, "wmi::object")) {
            static luaL_Reg mt[] = {
                {"__index", object_get},
                {"__pairs", object_pairs},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
        }
        lua_setmetatable(L, -2);
    }

    static int query(lua_State* L) {
        services_t& s = *(services_t*)lua_touserdata(L, lua_upvalueindex(1));
        std::wstring query_str = lua::checkstring(L, 1);
        Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
        check_wbem(L, "IEnumWbemClassObject::ExecQuery", s->ExecQuery(BSTR(L"WQL"), BSTR(query_str.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, enumerator.GetAddressOf()));
        lua_newtable(L);
        for (lua_Integer n = 1;;++n) {
            object_t object;
            ULONG returned = 0;
            HRESULT hres = enumerator->Next(WBEM_INFINITE, 1, object.GetAddressOf(), &returned);
            if (hres != WBEM_S_NO_ERROR || returned != 1) {
                break;
            }
            create_object(L, object.Detach());
            lua_seti(L, -2, n);
        }
        return 1;
    }

    static void create_services(lua_State* L, IWbemServices* services) {
        static luaL_Reg lib[] = {
            { "query", query },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        void* storage = lua_newuserdata(L, sizeof(services_t));
        new (storage) services_t(services);
        luaL_setfuncs(L, lib, 1);
    }

    static int luaopen(lua_State* L) {
        check(L, "CoInitializeEx", CoInitializeEx(0, COINIT_MULTITHREADED));
        Microsoft::WRL::ComPtr<IWbemLocator> locator;
        check(L, "CoCreateInstance", CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator)));
        services_t services;
        check_wbem(L, "IWbemLocator::ConnectServer", locator->ConnectServer(BSTR(L"ROOT\\CIMV2"), NULL, NULL, NULL, 0, NULL, NULL, services.GetAddressOf()));
        check(L, "CoSetProxyBlanket", CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE));
        create_services(L, services.Detach());
        return 1;
    }
}

DEFINE_LUAOPEN(wmi)
