#include "luau_bindings.h"
#include "tbx/core/log.h"
#include "tbx/platform/input.h"
#include <lualib.h>
#include <cstring>

namespace tbx
{
    static constexpr const char* TOY_METATABLE = "tbx.Toy";
    static constexpr const char* BLOCK_METATABLE = "tbx.Block";

    /// @brief
    /// Purpose: Payload of a Toy userdata. Raw pointer is deliberate at the C boundary; the
    /// Sandbox outlives the VM (Scripts is constructed after it and destroyed first).
    struct ToyUserdata
    {
        Sandbox* sandbox = nullptr;
        ToyId entity = NULL_TOY;
    };

    /// @brief
    /// Purpose: Payload of a Block userdata: enough to re-fetch the live block every access, so
    /// stale pointers cannot exist.
    struct BlockUserdata
    {
        Sandbox* sandbox = nullptr;
        ToyId entity = NULL_TOY;
        uint64 type_hash = 0;
    };

    //// HELPERS ////

    static ToyUserdata& check_toy(lua_State* lua, int index)
    {
        return *static_cast<ToyUserdata*>(luaL_checkudata(lua, index, TOY_METATABLE));
    }

    static BlockUserdata& check_block(lua_State* lua, int index)
    {
        return *static_cast<BlockUserdata*>(luaL_checkudata(lua, index, BLOCK_METATABLE));
    }

    static void push_vector_table(lua_State* lua, const float* values, const char* const* keys, int count)
    {
        lua_createtable(lua, 0, count);
        for (int i = 0; i < count; ++i)
        {
            lua_pushnumber(lua, values[i]);
            lua_setfield(lua, -2, keys[i]);
        }
    }

    static void read_vector_table(lua_State* lua, int index, float* values, const char* const* keys, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            lua_getfield(lua, index, keys[i]);
            if (lua_isnumber(lua, -1))
                values[i] = static_cast<float>(lua_tonumber(lua, -1));
            lua_pop(lua, 1);
        }
    }

    static const char* const XYZW_KEYS[] = {"x", "y", "z", "w"};
    static const char* const RGBA_KEYS[] = {"r", "g", "b", "a"};

    static int push_field_value(lua_State* lua, const FieldInfo& field, const std::byte* block)
    {
        const std::byte* at = block + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                lua_pushboolean(lua, *reinterpret_cast<const bool*>(at));
                return 1;
            case FieldKind::INT32:
                lua_pushnumber(lua, *reinterpret_cast<const int32*>(at));
                return 1;
            case FieldKind::UINT32:
                lua_pushnumber(lua, *reinterpret_cast<const uint32*>(at));
                return 1;
            case FieldKind::INT64:
                lua_pushnumber(lua, static_cast<double>(*reinterpret_cast<const int64*>(at)));
                return 1;
            case FieldKind::UINT64:
                lua_pushnumber(lua, static_cast<double>(*reinterpret_cast<const uint64*>(at)));
                return 1;
            case FieldKind::FLOAT:
                lua_pushnumber(lua, *reinterpret_cast<const float*>(at));
                return 1;
            case FieldKind::DOUBLE:
                lua_pushnumber(lua, *reinterpret_cast<const double*>(at));
                return 1;
            case FieldKind::STRING:
                lua_pushstring(lua, reinterpret_cast<const std::string*>(at)->c_str());
                return 1;
            case FieldKind::VEC2:
                push_vector_table(lua, &reinterpret_cast<const Vec2*>(at)->x, XYZW_KEYS, 2);
                return 1;
            case FieldKind::VEC3:
                push_vector_table(lua, &reinterpret_cast<const Vec3*>(at)->x, XYZW_KEYS, 3);
                return 1;
            case FieldKind::VEC4:
                push_vector_table(lua, &reinterpret_cast<const Vec4*>(at)->x, XYZW_KEYS, 4);
                return 1;
            case FieldKind::QUAT:
                push_vector_table(lua, &reinterpret_cast<const Quat*>(at)->x, XYZW_KEYS, 4);
                return 1;
            case FieldKind::COLOR:
                push_vector_table(lua, &reinterpret_cast<const Color*>(at)->r, RGBA_KEYS, 4);
                return 1;
            case FieldKind::UUID:
            case FieldKind::ASSET:
                lua_pushstring(lua, reinterpret_cast<const Uuid*>(at)->to_string().c_str());
                return 1;
            case FieldKind::ENUM:
            {
                auto raw = uint64(0);
                std::memcpy(&raw, at, field.size_bytes);
                lua_pushnumber(lua, static_cast<double>(raw));
                return 1;
            }
            case FieldKind::TYPE:
                log_warn("nested block field '{}' is not scriptable yet", field.name);
                lua_pushnil(lua);
                return 1;
        }
        lua_pushnil(lua);
        return 1;
    }

    static void write_field_value(lua_State* lua, const int value_index, const FieldInfo& field, std::byte* block)
    {
        std::byte* at = block + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                *reinterpret_cast<bool*>(at) = lua_toboolean(lua, value_index) != 0;
                return;
            case FieldKind::INT32:
                *reinterpret_cast<int32*>(at) = static_cast<int32>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::UINT32:
                *reinterpret_cast<uint32*>(at) = static_cast<uint32>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::INT64:
                *reinterpret_cast<int64*>(at) = static_cast<int64>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::UINT64:
                *reinterpret_cast<uint64*>(at) = static_cast<uint64>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::FLOAT:
                *reinterpret_cast<float*>(at) = static_cast<float>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::DOUBLE:
                *reinterpret_cast<double*>(at) = luaL_checknumber(lua, value_index);
                return;
            case FieldKind::STRING:
                *reinterpret_cast<std::string*>(at) = luaL_checkstring(lua, value_index);
                return;
            case FieldKind::VEC2:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec2*>(at)->x, XYZW_KEYS, 2);
                return;
            case FieldKind::VEC3:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec3*>(at)->x, XYZW_KEYS, 3);
                return;
            case FieldKind::VEC4:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec4*>(at)->x, XYZW_KEYS, 4);
                return;
            case FieldKind::QUAT:
                read_vector_table(lua, value_index, &reinterpret_cast<Quat*>(at)->x, XYZW_KEYS, 4);
                return;
            case FieldKind::COLOR:
                read_vector_table(lua, value_index, &reinterpret_cast<Color*>(at)->r, RGBA_KEYS, 4);
                return;
            case FieldKind::UUID:
            case FieldKind::ASSET:
                *reinterpret_cast<Uuid*>(at) = Uuid::parse(luaL_checkstring(lua, value_index));
                return;
            case FieldKind::ENUM:
            {
                const auto raw = static_cast<uint64>(luaL_checknumber(lua, value_index));
                std::memcpy(at, &raw, field.size_bytes);
                return;
            }
            case FieldKind::TYPE:
                log_warn("nested block field '{}' is not scriptable yet", field.name);
                return;
        }
    }

    //// BLOCK METATABLE ////

    static std::byte* fetch_block(const BlockUserdata& data)
    {
        const auto operations = get_block_registry().find(data.type_hash);
        if (!operations)
            return nullptr;
        return operations->get(data.sandbox->get_registry(), data.entity);
    }

    static int block_index(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        const char* field_name = luaL_checkstring(lua, 2);
        const auto type = get_type_registry().find(data.type_hash);
        std::byte* block = fetch_block(data);
        if (!type || !block)
            luaL_error(lua, "block is gone");
        for (const FieldInfo& field : type->get().fields)
            if (field.name == field_name)
                return push_field_value(lua, field, block);
        luaL_error(lua, "block '%s' has no field '%s'", type->get().name.c_str(), field_name);
        return 0;
    }

    static int block_newindex(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        const char* field_name = luaL_checkstring(lua, 2);
        const auto type = get_type_registry().find(data.type_hash);
        std::byte* block = fetch_block(data);
        if (!type || !block)
            luaL_error(lua, "block is gone");
        for (const FieldInfo& field : type->get().fields)
        {
            if (field.name == field_name)
            {
                write_field_value(lua, 3, field, block);
                return 0;
            }
        }
        luaL_error(lua, "block '%s' has no field '%s'", type->get().name.c_str(), field_name);
        return 0;
    }

    //// TOY METHODS ////

    static int toy_get(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        const char* block_name = luaL_checkstring(lua, 2);
        const uint64 hash = hash_name(block_name);
        const auto operations = get_block_registry().find(hash);
        if (!operations)
            luaL_error(lua, "unknown block type '%s'", block_name);
        operations->add_default(data.sandbox->get_registry(), data.entity);

        auto* block = static_cast<BlockUserdata*>(lua_newuserdata(lua, sizeof(BlockUserdata)));
        *block = BlockUserdata {.sandbox = data.sandbox, .entity = data.entity, .type_hash = hash};
        luaL_getmetatable(lua, BLOCK_METATABLE);
        lua_setmetatable(lua, -2);
        return 1;
    }

    static int toy_has(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        const auto operations = get_block_registry().find(hash_name(luaL_checkstring(lua, 2)));
        lua_pushboolean(
            lua,
            operations && operations->has(data.sandbox->get_registry(), data.entity));
        return 1;
    }

    static int toy_get_name(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        lua_pushstring(lua, Toy(*data.sandbox, data.entity).get_name().c_str());
        return 1;
    }

    static int toy_set_name(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).set_name(luaL_checkstring(lua, 2));
        return 0;
    }

    static int toy_sticker(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).sticker(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_has_sticker(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, Toy(*data.sandbox, data.entity).has_sticker(luaL_checkstring(lua, 2)));
        return 1;
    }

    static int toy_remove_sticker(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).remove_sticker(luaL_checkstring(lua, 2));
        return 0;
    }

    static int toy_is_alive(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, data.sandbox->get_registry().valid(data.entity));
        return 1;
    }

    //// TBX TABLE ////

    static Sandbox& bound_sandbox(lua_State* lua)
    {
        return *static_cast<Sandbox*>(lua_tolightuserdata(lua, lua_upvalueindex(1)));
    }

    static int sandbox_spawn(lua_State* lua)
    {
        Sandbox& sandbox = bound_sandbox(lua);
        const Toy toy = sandbox.spawn(luaL_checkstring(lua, 1));
        push_toy(lua, sandbox, toy.get_id());
        return 1;
    }

    static int sandbox_find(lua_State* lua)
    {
        Sandbox& sandbox = bound_sandbox(lua);
        const auto toy = sandbox.find(std::string_view(luaL_checkstring(lua, 1)));
        if (!toy)
        {
            lua_pushnil(lua);
            return 1;
        }
        push_toy(lua, sandbox, toy->get_id());
        return 1;
    }

    static int sandbox_despawn(lua_State* lua)
    {
        ToyUserdata& data = check_toy(lua, 1);
        data.sandbox->despawn(Toy(*data.sandbox, data.entity));
        return 0;
    }

    static Key key_from_string(const char* name)
    {
        // Hand-written map: enum name arrays are deliberately not generated.
        const uint64 hash = hash_name(name);
        if (std::strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z')
            return static_cast<Key>(static_cast<int>(Key::A) + (name[0] - 'A'));
        if (hash == hash_name("SPACE"))
            return Key::SPACE;
        if (hash == hash_name("ESCAPE"))
            return Key::ESCAPE;
        if (hash == hash_name("ENTER"))
            return Key::ENTER;
        if (hash == hash_name("LEFT"))
            return Key::LEFT;
        if (hash == hash_name("RIGHT"))
            return Key::RIGHT;
        if (hash == hash_name("UP"))
            return Key::UP;
        if (hash == hash_name("DOWN"))
            return Key::DOWN;
        if (hash == hash_name("LEFT_SHIFT"))
            return Key::LEFT_SHIFT;
        if (hash == hash_name("LEFT_CTRL"))
            return Key::LEFT_CTRL;
        if (hash == hash_name("TAB"))
            return Key::TAB;
        return Key::UNKNOWN;
    }

    static int input_is_down(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_down(key_from_string(luaL_checkstring(lua, 1))));
        return 1;
    }

    static int input_is_pressed(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_pressed(key_from_string(luaL_checkstring(lua, 1))));
        return 1;
    }

    //// OPEN ////

    void push_toy(lua_State* lua, Sandbox& sandbox, const ToyId entity)
    {
        auto* data = static_cast<ToyUserdata*>(lua_newuserdata(lua, sizeof(ToyUserdata)));
        *data = ToyUserdata {.sandbox = &sandbox, .entity = entity};
        luaL_getmetatable(lua, TOY_METATABLE);
        lua_setmetatable(lua, -2);
    }

    void open_tbx_bindings(lua_State* lua, Sandbox& sandbox)
    {
        // Toy metatable: __index = method table.
        luaL_newmetatable(lua, TOY_METATABLE);
        lua_createtable(lua, 0, 8);
        const luaL_Reg toy_methods[] = {
            {"get", toy_get},
            {"has", toy_has},
            {"get_name", toy_get_name},
            {"set_name", toy_set_name},
            {"sticker", toy_sticker},
            {"has_sticker", toy_has_sticker},
            {"remove_sticker", toy_remove_sticker},
            {"is_alive", toy_is_alive},
            {nullptr, nullptr}};
        luaL_register(lua, nullptr, toy_methods);
        lua_setfield(lua, -2, "__index");
        lua_pop(lua, 1);

        // Block metatable: field access straight through TypeInfo.
        luaL_newmetatable(lua, BLOCK_METATABLE);
        lua_pushcfunction(lua, block_index, "block_index");
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, block_newindex, "block_newindex");
        lua_setfield(lua, -2, "__newindex");
        lua_pop(lua, 1);

        // Global tbx table.
        lua_createtable(lua, 0, 2);

        lua_createtable(lua, 0, 3);
        lua_pushlightuserdata(lua, &sandbox);
        lua_pushcclosure(lua, sandbox_spawn, "sandbox_spawn", 1);
        lua_setfield(lua, -2, "spawn");
        lua_pushlightuserdata(lua, &sandbox);
        lua_pushcclosure(lua, sandbox_find, "sandbox_find", 1);
        lua_setfield(lua, -2, "find");
        lua_pushcfunction(lua, sandbox_despawn, "sandbox_despawn");
        lua_setfield(lua, -2, "despawn");
        lua_setfield(lua, -2, "sandbox");

        lua_createtable(lua, 0, 2);
        lua_pushcfunction(lua, input_is_down, "input_is_down");
        lua_setfield(lua, -2, "is_down");
        lua_pushcfunction(lua, input_is_pressed, "input_is_pressed");
        lua_setfield(lua, -2, "is_pressed");
        lua_setfield(lua, -2, "input");

        lua_setglobal(lua, "tbx");
    }
}
