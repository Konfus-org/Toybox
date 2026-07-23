#pragma once
#include "tbx/math/math.h"
#include "tbx/utils/color.h"
#include <lua.h>
#include <lualib.h>
#include <string>
#include <type_traits>
#include <utility>

// The Luau backend's marshalling core: convert one value between the Lua stack and C++ (LuaConvert<T>)
// and turn any plain free function into a lua_CFunction (bind<Fn>) with no per-function stack code. This
// is backend glue (it lives with the VM); the GENERATED bindings only reference bind<&fn> + LuaConvert,
// so adding a scriptable free function needs no hand-written marshalling. Bespoke surfaces (Toy userdata,
// events, input) are NOT expressed here — they stay in the backend's hand-written glue.
namespace tbx
{
    /// @brief
    /// Purpose: Reads/pushes one marshallable value; specialized per supported type. Vectors are
    /// {x,y,z(,w)} tables, colors {r,g,b,a}, matching the block field marshalling.
    template <typename T>
    struct LuaConvert;

    namespace luau_detail
    {
        inline void push_components(lua_State* lua, const float* values, const char* const* keys, int count)
        {
            lua_createtable(lua, 0, count);
            for (int index = 0; index < count; ++index)
            {
                lua_pushnumber(lua, values[index]);
                lua_setfield(lua, -2, keys[index]);
            }
        }

        inline void read_components(lua_State* lua, int at, float* values, const char* const* keys, int count)
        {
            luaL_checktype(lua, at, LUA_TTABLE);
            for (int index = 0; index < count; ++index)
            {
                lua_getfield(lua, at, keys[index]);
                if (lua_isnumber(lua, -1))
                    values[index] = static_cast<float>(lua_tonumber(lua, -1));
                lua_pop(lua, 1);
            }
        }

        inline constexpr const char* XYZW[] = {"x", "y", "z", "w"};
        inline constexpr const char* RGBA[] = {"r", "g", "b", "a"};
    }

    template <>
    struct LuaConvert<float>
    {
        static float read(lua_State* lua, int at) { return static_cast<float>(luaL_checknumber(lua, at)); }
        static void push(lua_State* lua, float value) { lua_pushnumber(lua, value); }
    };

    template <>
    struct LuaConvert<double>
    {
        static double read(lua_State* lua, int at) { return luaL_checknumber(lua, at); }
        static void push(lua_State* lua, double value) { lua_pushnumber(lua, value); }
    };

    template <>
    struct LuaConvert<int>
    {
        static int read(lua_State* lua, int at) { return static_cast<int>(luaL_checkinteger(lua, at)); }
        static void push(lua_State* lua, int value) { lua_pushinteger(lua, value); }
    };

    template <>
    struct LuaConvert<bool>
    {
        static bool read(lua_State* lua, int at) { return lua_toboolean(lua, at) != 0; }
        static void push(lua_State* lua, bool value) { lua_pushboolean(lua, value); }
    };

    template <>
    struct LuaConvert<std::string>
    {
        static std::string read(lua_State* lua, int at) { return luaL_checkstring(lua, at); }
        static void push(lua_State* lua, const std::string& value) { lua_pushstring(lua, value.c_str()); }
    };

    template <>
    struct LuaConvert<Vec2>
    {
        static Vec2 read(lua_State* lua, int at)
        {
            auto value = Vec2(0.0f, 0.0f);
            luau_detail::read_components(lua, at, &value.x, luau_detail::XYZW, 2);
            return value;
        }
        static void push(lua_State* lua, const Vec2& value)
        {
            luau_detail::push_components(lua, &value.x, luau_detail::XYZW, 2);
        }
    };

    template <>
    struct LuaConvert<Vec3>
    {
        static Vec3 read(lua_State* lua, int at)
        {
            auto value = Vec3(0.0f, 0.0f, 0.0f);
            luau_detail::read_components(lua, at, &value.x, luau_detail::XYZW, 3);
            return value;
        }
        static void push(lua_State* lua, const Vec3& value)
        {
            luau_detail::push_components(lua, &value.x, luau_detail::XYZW, 3);
        }
    };

    template <>
    struct LuaConvert<Vec4>
    {
        static Vec4 read(lua_State* lua, int at)
        {
            auto value = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            luau_detail::read_components(lua, at, &value.x, luau_detail::XYZW, 4);
            return value;
        }
        static void push(lua_State* lua, const Vec4& value)
        {
            luau_detail::push_components(lua, &value.x, luau_detail::XYZW, 4);
        }
    };

    template <>
    struct LuaConvert<Quat>
    {
        // Quat stores (w,x,y,z); the script table is {x,y,z,w}.
        static Quat read(lua_State* lua, int at)
        {
            auto values = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            luau_detail::read_components(lua, at, &values.x, luau_detail::XYZW, 4);
            return Quat(values.w, values.x, values.y, values.z);
        }
        static void push(lua_State* lua, const Quat& value)
        {
            const float components[4] = {value.x, value.y, value.z, value.w};
            luau_detail::push_components(lua, components, luau_detail::XYZW, 4);
        }
    };

    template <>
    struct LuaConvert<Color>
    {
        static Color read(lua_State* lua, int at)
        {
            auto value = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
            luau_detail::read_components(lua, at, &value.r, luau_detail::RGBA, 4);
            return value;
        }
        static void push(lua_State* lua, const Color& value)
        {
            luau_detail::push_components(lua, &value.r, luau_detail::RGBA, 4);
        }
    };

    namespace luau_detail
    {
        template <auto Fn, typename R, typename... Args, std::size_t... Indices>
        int call_impl(lua_State* lua, std::index_sequence<Indices...>)
        {
            if constexpr (std::is_void_v<R>)
            {
                Fn(LuaConvert<std::remove_cvref_t<Args>>::read(lua, static_cast<int>(Indices) + 1)...);
                return 0;
            }
            else
            {
                LuaConvert<std::remove_cvref_t<R>>::push(
                    lua,
                    Fn(LuaConvert<std::remove_cvref_t<Args>>::read(lua, static_cast<int>(Indices) + 1)...));
                return 1;
            }
        }

        template <auto Fn, typename R, typename... Args>
        int call_free(lua_State* lua, R (*)(Args...))
        {
            return call_impl<Fn, R, Args...>(lua, std::index_sequence_for<Args...>());
        }
    }

    /// @brief
    /// Purpose: The lua_CFunction that marshals a call to the plain free function Fn — reads each
    /// argument via LuaConvert, invokes Fn, pushes the result. Overloaded targets need a cast:
    /// bind<static_cast<Vec3 (*)(const Vec3&, const Vec3&)>(&lerp)>.
    template <auto Fn>
    int bind(lua_State* lua)
    {
        return luau_detail::call_free<Fn>(lua, Fn);
    }
}
