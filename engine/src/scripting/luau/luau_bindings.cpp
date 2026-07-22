#include "luau_bindings.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/utils/hash.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/runtime.h"
#include "tbx/ui/ui.h"
#include <cstring>
#include <lualib.h>

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
    /// Purpose: Payload of a Block userdata: enough to re-fetch the live block every access,
    /// so stale pointers cannot exist.
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

    static void push_vector_table(
        lua_State* lua,
        const float* values,
        const char* const* keys,
        const int count)
    {
        lua_createtable(lua, 0, count);
        for (int i = 0; i < count; ++i)
        {
            lua_pushnumber(lua, values[i]);
            lua_setfield(lua, -2, keys[i]);
        }
    }

    static void read_vector_table(
        lua_State* lua,
        const int index,
        float* values,
        const char* const* keys,
        const int count)
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

    static int push_field_value(
        lua_State* lua,
        const FieldInfo& field,
        const std::byte* block)
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
                lua_pushstring(lua, reinterpret_cast<const Uuid*>(at)->to_string().c_str());
                return 1;
            case FieldKind::ASSET:
            {
                const auto [id, asset_path] = field.read_asset(block);
                if (!id.is_valid() && !asset_path.empty())
                    lua_pushstring(lua, asset_path.c_str());
                else
                    lua_pushstring(lua, id.to_string().c_str());
                return 1;
            }
            case FieldKind::ENUM:
            {
                auto raw = uint64(0);
                std::memcpy(&raw, at, field.size_bytes);
                lua_pushnumber(lua, static_cast<double>(raw));
                return 1;
            }
            case FieldKind::ASSET_LIST:
            {
                const auto ids = field.read_asset_list(block);
                lua_createtable(lua, static_cast<int>(ids.size()), 0);
                for (size i = 0; i < ids.size(); ++i)
                {
                    lua_pushstring(lua, ids[i].to_string().c_str());
                    lua_rawseti(lua, -2, static_cast<int>(i) + 1);
                }
                return 1;
            }
            case FieldKind::TYPE:
            case FieldKind::TYPE_LIST:
                TBX_WARN("nested block field '{}' is not scriptable yet", field.name);
                lua_pushnil(lua);
                return 1;
        }
        lua_pushnil(lua);
        return 1;
    }

    static void write_field_value(
        lua_State* lua,
        const int value_index,
        const FieldInfo& field,
        std::byte* block)
    {
        std::byte* at = block + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                *reinterpret_cast<bool*>(at) = lua_toboolean(lua, value_index) != 0;
                return;
            case FieldKind::INT32:
                *reinterpret_cast<int32*>(at) =
                    static_cast<int32>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::UINT32:
                *reinterpret_cast<uint32*>(at) =
                    static_cast<uint32>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::INT64:
                *reinterpret_cast<int64*>(at) =
                    static_cast<int64>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::UINT64:
                *reinterpret_cast<uint64*>(at) =
                    static_cast<uint64>(luaL_checknumber(lua, value_index));
                return;
            case FieldKind::FLOAT:
                *reinterpret_cast<float*>(at) =
                    static_cast<float>(luaL_checknumber(lua, value_index));
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
                *reinterpret_cast<Uuid*>(at) = Uuid::parse(luaL_checkstring(lua, value_index));
                return;
            case FieldKind::ASSET:
            {
                auto text = std::string(luaL_checkstring(lua, value_index));
                auto stripped = text;
                std::erase(stripped, '-');
                const Uuid id = Uuid::parse(stripped);
                if (!id.is_valid())
                    field.write_asset(block, Uuid {}, std::move(text));
                else
                    field.write_asset(block, id, std::string());
                return;
            }
            case FieldKind::ENUM:
            {
                const auto raw = static_cast<uint64>(luaL_checknumber(lua, value_index));
                std::memcpy(at, &raw, field.size_bytes);
                return;
            }
            case FieldKind::ASSET_LIST:
            {
                luaL_checktype(lua, value_index, LUA_TTABLE);
                auto ids = std::vector<Uuid>();
                const int count = lua_objlen(lua, value_index);
                for (int i = 1; i <= count; ++i)
                {
                    lua_rawgeti(lua, value_index, i);
                    ids.push_back(Uuid::parse(luaL_checkstring(lua, -1)));
                    lua_pop(lua, 1);
                }
                field.write_asset_list(block, ids);
                return;
            }
            case FieldKind::TYPE:
            case FieldKind::TYPE_LIST:
                TBX_WARN("nested block field '{}' is not scriptable yet", field.name);
                return;
        }
    }

    //// BLOCK METATABLE ////

    static std::byte* fetch_block(const BlockUserdata& data)
    {
        const auto type = describe_type(data.type_hash);
        if (!type || !type->get().get_block)
            return nullptr;
        return Toy(*data.sandbox, data.entity).get_block_bytes(data.type_hash);
    }

    /// @brief
    /// Purpose: ui:bind(slot, getter) — links a document slot on THIS UI block to a live Lua
    /// getter, evaluated each frame the ui pass draws the block. The value lives on the block
    /// (UI::bindings), so two UIs never collide on a slot name. Only valid on a UI component.
    static int block_bind(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        if (data.type_hash != hash("UI"))
            luaL_error(lua, "bind is only valid on a UI component");
        const auto slot = std::string(luaL_checkstring(lua, 2));
        luaL_checktype(lua, 3, LUA_TFUNCTION);
        const int getter_ref = lua_ref(lua, 3); // refs the getter in place (Luau: no pop)
        UI* ui = Toy(*data.sandbox, data.entity).try_block<UI>();
        if (!ui)
            luaL_error(lua, "bind: the UI component is gone");
        ui->bindings[slot] = [lua, getter_ref]() -> std::string
        {
            lua_getref(lua, getter_ref);
            if (!lua_isfunction(lua, -1))
            {
                lua_pop(lua, 1);
                return {};
            }
            auto value = std::string();
            if (lua_pcall(lua, 0, 1, 0) == 0)
            {
                if (const char* text = lua_tostring(lua, -1))
                    value = text;
            }
            else
            {
                TBX_ERROR("ui bind getter error: {}", lua_tostring(lua, -1));
            }
            lua_pop(lua, 1);
            return value;
        };
        return 0;
    }

    static int block_index(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        const char* field_name = luaL_checkstring(lua, 2);
        // Methods first (blocks currently expose just bind), then reflected fields.
        if (std::strcmp(field_name, "bind") == 0)
        {
            lua_pushcfunction(lua, block_bind, "bind");
            return 1;
        }
        const auto type = describe_type(data.type_hash);
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
        const auto type = describe_type(data.type_hash);
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

    static void push_block(lua_State* lua, const ToyUserdata& data, const uint64 type_hash)
    {
        auto* block = static_cast<BlockUserdata*>(lua_newuserdata(lua, sizeof(BlockUserdata)));
        *block =
            BlockUserdata {.sandbox = data.sandbox, .entity = data.entity, .type_hash = type_hash};
        luaL_getmetatable(lua, BLOCK_METATABLE);
        lua_setmetatable(lua, -2);
    }

    /// @brief
    /// Purpose: toy.<Method> resolves methods, toy.<BlockType> resolves attached blocks by
    /// their registered name (nil when absent, so `if toy.Health then` is the has-check) — no
    /// string-based get() anywhere.
    static int toy_index(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const char* key = luaL_checkstring(lua, 2);

        // Methods first (the table sits in this closure's upvalue).
        lua_getfield(lua, lua_upvalueindex(1), key);
        if (!lua_isnil(lua, -1))
            return 1;
        lua_pop(lua, 1);

        const auto type = describe_type(hash(key));
        if (!type || !type->get().has_block
            || !Toy(*data.sandbox, data.entity).has_block_named(hash(key)))
        {
            lua_pushnil(lua);
            return 1;
        }
        push_block(lua, data, hash(key));
        return 1;
    }

    /// @brief
    /// Purpose: toy.<BlockType> = { field = value, ... } attaches the block (default-built)
    /// and writes the given fields — the add-and-populate path.
    static int toy_newindex(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const char* key = luaL_checkstring(lua, 2);
        const uint64 hashed = hash(key);
        const auto type = describe_type(hashed);
        if (!type || !type->get().add_block)
        {
            luaL_error(lua, "'%s' is not a registered block type", key);
            return 0;
        }
        if (!lua_istable(lua, 3))
        {
            luaL_error(lua, "assign a table of fields to toy.%s", key);
            return 0;
        }
        std::byte* block = Toy(*data.sandbox, data.entity).add_block_bytes(hashed);
        for (const FieldInfo& field : type->get().fields)
        {
            lua_getfield(lua, 3, field.name.c_str());
            if (!lua_isnil(lua, -1))
                write_field_value(lua, lua_gettop(lua), field, block);
            lua_pop(lua, 1);
        }
        return 0;
    }

    static int toy_get_name(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushstring(lua, Toy(*data.sandbox, data.entity).get_name().c_str());
        return 1;
    }

    static int toy_set_name(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).set_name(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_is_enabled(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, Toy(*data.sandbox, data.entity).is_enabled());
        return 1;
    }

    static int toy_set_enabled(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).set_enabled(lua_toboolean(lua, 2) != 0);
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    /// @brief
    /// Purpose: toy:with("BlockType", { field = value, ... }) attaches (default-builds) the block
    /// and writes the given fields, then returns the toy — the chainable form of
    /// toy.BlockType = {...}, so a whole toy builds in one expression. The fields table is
    /// optional (toy:with("RigidBody") attaches a default block).
    static int toy_with(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const char* key = luaL_checkstring(lua, 2);
        const auto type = describe_type(hash(key));
        if (!type || !type->get().add_block)
        {
            luaL_error(lua, "'%s' is not a registered block type", key);
            return 0;
        }
        std::byte* block = Toy(*data.sandbox, data.entity).add_block_bytes(hash(key));
        if (lua_istable(lua, 3))
        {
            for (const FieldInfo& field : type->get().fields)
            {
                lua_getfield(lua, 3, field.name.c_str());
                if (!lua_isnil(lua, -1))
                    write_field_value(lua, lua_gettop(lua), field, block);
                lua_pop(lua, 1);
            }
        }
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_remove_block(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).remove_block_named(hash(luaL_checkstring(lua, 2)));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_set_parent(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        // A nil/absent parent clears the link; otherwise reparent to the given toy.
        auto parent = Toy();
        if (!lua_isnoneornil(lua, 2))
        {
            const ToyUserdata& parent_data = check_toy(lua, 2);
            parent = Toy(*parent_data.sandbox, parent_data.entity);
        }
        Toy(*data.sandbox, data.entity).set_parent(parent);
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_despawn(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        data.sandbox->despawn(Toy(*data.sandbox, data.entity));
        return 0;
    }

    static int toy_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).sticker(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_has_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(
            lua,
            Toy(*data.sandbox, data.entity).has_sticker(luaL_checkstring(lua, 2)));
        return 1;
    }

    static int toy_remove_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).remove_sticker(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_is_alive(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, Toy(*data.sandbox, data.entity).is_alive());
        return 1;
    }

    static int toy_get_parent(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const auto parent = Toy(*data.sandbox, data.entity).get_parent();
        if (!parent)
        {
            lua_pushnil(lua);
            return 1;
        }
        push_toy(lua, *data.sandbox, parent->get_id());
        return 1;
    }

    static int toy_get_children(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const auto children = Toy(*data.sandbox, data.entity).get_children();
        lua_createtable(lua, static_cast<int>(children.size()), 0);
        int index = 1;
        for (const Toy& child : children)
        {
            push_toy(lua, *data.sandbox, child.get_id());
            lua_rawseti(lua, -2, index++); // append: children[index] = child
        }
        return 1;
    }

    //// TBX TABLE ////

    // The closure upvalue is the heap-stable RuntimeState (raw pointer only at this Lua C
    // boundary); the scripts module owning the VM dies before the runtime it points at.
    static RuntimeState& bound_runtime(lua_State* lua)
    {
        return *static_cast<RuntimeState*>(lua_tolightuserdata(lua, lua_upvalueindex(1)));
    }

    // tbx.sandbox is the scene object: its queries are called with `:` (self = the sandbox
    // table at index 1, runtime from the upvalue), so real arguments start at index 2.
    static int sandbox_spawn(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        const Toy toy = sandbox.spawn(luaL_checkstring(lua, 2));
        push_toy(lua, sandbox, toy.get_id());
        return 1;
    }

    static int sandbox_find(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        const auto toy = sandbox.find(std::string_view(luaL_checkstring(lua, 2)));
        if (!toy)
        {
            lua_pushnil(lua);
            return 1;
        }
        push_toy(lua, sandbox, toy->get_id());
        return 1;
    }

    static int sandbox_find_with_sticker(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        const auto sticker = std::string(luaL_checkstring(lua, 2));
        auto found = std::optional<ToyId>();
        sandbox.for_each_sticker(
            sticker,
            [&](Toy toy)
            {
                if (!found)
                    found = toy.get_id();
            });
        if (!found)
        {
            lua_pushnil(lua);
            return 1;
        }
        push_toy(lua, sandbox, *found);
        return 1;
    }

    static int sandbox_find_all_with_sticker(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        const auto sticker = std::string(luaL_checkstring(lua, 2));
        lua_newtable(lua);
        int index = 1;
        sandbox.for_each_sticker(
            sticker,
            [&](Toy toy)
            {
                push_toy(lua, sandbox, toy.get_id());
                lua_rawseti(lua, -2, index++); // append into the result array
            });
        return 1;
    }

    static int sandbox_get_toys(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        const auto toys = sandbox.get_toys();
        lua_createtable(lua, static_cast<int>(toys.size()), 0);
        int index = 1;
        for (const Toy& toy : toys)
        {
            push_toy(lua, sandbox, toy.get_id());
            lua_rawseti(lua, -2, index++);
        }
        return 1;
    }

    static int sandbox_despawn(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 2);
        data.sandbox->despawn(Toy(*data.sandbox, data.entity));
        return 0;
    }

    static Vec3 check_vector3(lua_State* lua, const int index)
    {
        luaL_checktype(lua, index, LUA_TTABLE);
        auto vector = Vec3(0.0f, 0.0f, 0.0f);
        read_vector_table(lua, index, &vector.x, XYZW_KEYS, 3);
        return vector;
    }

    static int sandbox_spawn_kit(lua_State* lua)
    {
        RuntimeState& state = bound_runtime(lua);
        const char* reference = luaL_checkstring(lua, 2);
        const Vec3 position = lua_istable(lua, 3) ? check_vector3(lua, 3) : Vec3(0.0f, 0.0f, 0.0f);
        const auto spawned = spawn(
            state.sandbox,
            state.assets,
            state.events,
            AssetHandle<Kit>(reference),
            position);
        if (!spawned)
        {
            luaL_error(lua, "kit '%s': %s", reference, spawned.error().c_str());
            return 0;
        }
        push_toy(lua, state.sandbox, spawned->get_id()); // the instance's root toy
        return 1;
    }

    static int sandbox_despawn_kit(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 2);
        data.sandbox->despawn_subtree(Toy(*data.sandbox, data.entity));
        return 0;
    }

    static int physics_raycast(lua_State* lua)
    {
        RuntimeState& state = bound_runtime(lua);
        const Vec3 origin = check_vector3(lua, 1);
        const Vec3 direction = check_vector3(lua, 2);
        const auto max_distance = static_cast<float>(luaL_optnumber(lua, 3, 1000.0));
        const auto hit = raycast(state.physics, origin, direction, max_distance);
        if (!hit)
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_createtable(lua, 0, 3);
        push_toy(lua, state.sandbox, hit->toy);
        lua_setfield(lua, -2, "toy");
        push_vector_table(lua, &hit->position.x, XYZW_KEYS, 3);
        lua_setfield(lua, -2, "position");
        lua_pushnumber(lua, hit->distance);
        lua_setfield(lua, -2, "distance");
        return 1;
    }

    //// MATH ////
    // tbx.math mirrors tbx::math so scripts lean on the C++ library instead of hand-rolled
    // trig: vectors are {x,y,z} tables, rotations are {x,y,z,w} tables.

    static Quat check_quat(lua_State* lua, const int index)
    {
        luaL_checktype(lua, index, LUA_TTABLE);
        auto values = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        read_vector_table(lua, index, &values.x, XYZW_KEYS, 4);
        return Quat(values.w, values.x, values.y, values.z);
    }

    static int push_vector3(lua_State* lua, const Vec3& vector)
    {
        push_vector_table(lua, &vector.x, XYZW_KEYS, 3);
        return 1;
    }

    static int push_quat(lua_State* lua, const Quat& rotation)
    {
        const float values[4] = {rotation.x, rotation.y, rotation.z, rotation.w};
        push_vector_table(lua, values, XYZW_KEYS, 4);
        return 1;
    }

    static int math_add(lua_State* lua)
    {
        return push_vector3(lua, check_vector3(lua, 1) + check_vector3(lua, 2));
    }

    static int math_subtract(lua_State* lua)
    {
        return push_vector3(lua, check_vector3(lua, 1) - check_vector3(lua, 2));
    }

    static int math_scale(lua_State* lua)
    {
        return push_vector3(
            lua,
            check_vector3(lua, 1) * static_cast<float>(luaL_checknumber(lua, 2)));
    }

    static int math_dot(lua_State* lua)
    {
        lua_pushnumber(lua, dot(check_vector3(lua, 1), check_vector3(lua, 2)));
        return 1;
    }

    static int math_cross(lua_State* lua)
    {
        return push_vector3(lua, cross(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int math_length(lua_State* lua)
    {
        lua_pushnumber(lua, length(check_vector3(lua, 1)));
        return 1;
    }

    static int math_distance(lua_State* lua)
    {
        lua_pushnumber(lua, distance(check_vector3(lua, 1), check_vector3(lua, 2)));
        return 1;
    }

    static int math_normalize(lua_State* lua)
    {
        return push_vector3(lua, normalize(check_vector3(lua, 1)));
    }

    static int math_lerp(lua_State* lua)
    {
        const auto t = static_cast<float>(luaL_checknumber(lua, 3));
        if (lua_istable(lua, 1))
            return push_vector3(lua, lerp(check_vector3(lua, 1), check_vector3(lua, 2), t));
        lua_pushnumber(
            lua,
            lerp(
                static_cast<float>(luaL_checknumber(lua, 1)),
                static_cast<float>(luaL_checknumber(lua, 2)),
                t));
        return 1;
    }

    static int math_move_toward(lua_State* lua)
    {
        return push_vector3(
            lua,
            move_toward(
                check_vector3(lua, 1),
                check_vector3(lua, 2),
                static_cast<float>(luaL_checknumber(lua, 3))));
    }

    static int math_reflect(lua_State* lua)
    {
        return push_vector3(lua, reflect(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int math_angle_axis(lua_State* lua)
    {
        return push_quat(
            lua,
            angle_axis(static_cast<float>(luaL_checknumber(lua, 1)), check_vector3(lua, 2)));
    }

    static int math_multiply(lua_State* lua)
    {
        return push_quat(lua, multiply(check_quat(lua, 1), check_quat(lua, 2)));
    }

    static int math_rotate(lua_State* lua)
    {
        return push_vector3(lua, rotate(check_quat(lua, 1), check_vector3(lua, 2)));
    }

    static int math_slerp(lua_State* lua)
    {
        return push_quat(
            lua,
            slerp(
                check_quat(lua, 1),
                check_quat(lua, 2),
                static_cast<float>(luaL_checknumber(lua, 3))));
    }

    static int math_from_euler(lua_State* lua)
    {
        return push_quat(lua, from_euler(check_vector3(lua, 1)));
    }

    static int math_to_euler(lua_State* lua)
    {
        return push_vector3(lua, to_euler(check_quat(lua, 1)));
    }

    static int math_quat_look_at(lua_State* lua)
    {
        return push_quat(lua, quat_look_at(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int ui_set_string(lua_State* lua)
    {
        // Numbers coerce to strings; documents bind via data-text / data-style attributes.
        bound_runtime(lua).ui.bindings[luaL_checkstring(lua, 1)] = luaL_checkstring(lua, 2);
        return 0;
    }

    static int tbx_quit(lua_State* lua)
    {
        // tbx::quit takes the Runtime handle, which async work (this VM) never holds;
        // requesting the exit is one status write on the bound state.
        App& app = bound_runtime(lua).app;
        if (app.status != AppStatus::STOPPED)
            app.status = AppStatus::QUIT_REQUESTED;
        return 0;
    }

    /// @brief
    /// Purpose: One row of the script-facing key enum table (tbx.Key.NAME).
    struct KeyEntry
    {
        const char* name;
        Key key;
    };

#define TBX_KEY_ENTRY(name)                                                                        \
    KeyEntry                                                                                       \
    {                                                                                              \
        #name, Key::name                                                                    \
    }
    static constexpr KeyEntry KEY_TABLE[] = {
        TBX_KEY_ENTRY(A),
        TBX_KEY_ENTRY(B),
        TBX_KEY_ENTRY(C),
        TBX_KEY_ENTRY(D),
        TBX_KEY_ENTRY(E),
        TBX_KEY_ENTRY(F),
        TBX_KEY_ENTRY(G),
        TBX_KEY_ENTRY(H),
        TBX_KEY_ENTRY(I),
        TBX_KEY_ENTRY(J),
        TBX_KEY_ENTRY(K),
        TBX_KEY_ENTRY(L),
        TBX_KEY_ENTRY(M),
        TBX_KEY_ENTRY(N),
        TBX_KEY_ENTRY(O),
        TBX_KEY_ENTRY(P),
        TBX_KEY_ENTRY(Q),
        TBX_KEY_ENTRY(R),
        TBX_KEY_ENTRY(S),
        TBX_KEY_ENTRY(T),
        TBX_KEY_ENTRY(U),
        TBX_KEY_ENTRY(V),
        TBX_KEY_ENTRY(W),
        TBX_KEY_ENTRY(X),
        TBX_KEY_ENTRY(Y),
        TBX_KEY_ENTRY(Z),
        TBX_KEY_ENTRY(NUM_0),
        TBX_KEY_ENTRY(NUM_1),
        TBX_KEY_ENTRY(NUM_2),
        TBX_KEY_ENTRY(NUM_3),
        TBX_KEY_ENTRY(NUM_4),
        TBX_KEY_ENTRY(NUM_5),
        TBX_KEY_ENTRY(NUM_6),
        TBX_KEY_ENTRY(NUM_7),
        TBX_KEY_ENTRY(NUM_8),
        TBX_KEY_ENTRY(NUM_9),
        TBX_KEY_ENTRY(F1),
        TBX_KEY_ENTRY(F2),
        TBX_KEY_ENTRY(F3),
        TBX_KEY_ENTRY(F4),
        TBX_KEY_ENTRY(F5),
        TBX_KEY_ENTRY(F6),
        TBX_KEY_ENTRY(F7),
        TBX_KEY_ENTRY(F8),
        TBX_KEY_ENTRY(F9),
        TBX_KEY_ENTRY(F10),
        TBX_KEY_ENTRY(F11),
        TBX_KEY_ENTRY(F12),
        TBX_KEY_ENTRY(ESCAPE),
        TBX_KEY_ENTRY(TAB),
        TBX_KEY_ENTRY(CAPS_LOCK),
        TBX_KEY_ENTRY(SPACE),
        TBX_KEY_ENTRY(ENTER),
        TBX_KEY_ENTRY(BACKSPACE),
        TBX_KEY_ENTRY(DEL),
        TBX_KEY_ENTRY(INSERT),
        TBX_KEY_ENTRY(HOME),
        TBX_KEY_ENTRY(END),
        TBX_KEY_ENTRY(PAGE_UP),
        TBX_KEY_ENTRY(PAGE_DOWN),
        TBX_KEY_ENTRY(LEFT),
        TBX_KEY_ENTRY(RIGHT),
        TBX_KEY_ENTRY(UP),
        TBX_KEY_ENTRY(DOWN),
        TBX_KEY_ENTRY(LEFT_SHIFT),
        TBX_KEY_ENTRY(RIGHT_SHIFT),
        TBX_KEY_ENTRY(LEFT_CTRL),
        TBX_KEY_ENTRY(RIGHT_CTRL),
        TBX_KEY_ENTRY(LEFT_ALT),
        TBX_KEY_ENTRY(RIGHT_ALT),
        TBX_KEY_ENTRY(MINUS),
        TBX_KEY_ENTRY(EQUALS),
        TBX_KEY_ENTRY(LEFT_BRACKET),
        TBX_KEY_ENTRY(RIGHT_BRACKET),
        TBX_KEY_ENTRY(BACKSLASH),
        TBX_KEY_ENTRY(SEMICOLON),
        TBX_KEY_ENTRY(APOSTROPHE),
        TBX_KEY_ENTRY(GRAVE),
        TBX_KEY_ENTRY(COMMA),
        TBX_KEY_ENTRY(PERIOD),
        TBX_KEY_ENTRY(SLASH)};
#undef TBX_KEY_ENTRY

    /// @brief
    /// Purpose: One row of the script-facing controller enum tables (tbx.GamepadButton.NAME,
    /// tbx.GamepadAxis.NAME).
    struct GamepadButtonEntry
    {
        const char* name;
        GamepadButton button;
    };

    struct GamepadAxisEntry
    {
        const char* name;
        GamepadAxis axis;
    };

#define TBX_GAMEPAD_BUTTON_ENTRY(name)                                                             \
    GamepadButtonEntry                                                                             \
    {                                                                                              \
        #name, GamepadButton::name                                                                 \
    }
    static constexpr GamepadButtonEntry GAMEPAD_BUTTON_TABLE[] = {
        TBX_GAMEPAD_BUTTON_ENTRY(SOUTH),
        TBX_GAMEPAD_BUTTON_ENTRY(EAST),
        TBX_GAMEPAD_BUTTON_ENTRY(WEST),
        TBX_GAMEPAD_BUTTON_ENTRY(NORTH),
        TBX_GAMEPAD_BUTTON_ENTRY(BACK),
        TBX_GAMEPAD_BUTTON_ENTRY(GUIDE),
        TBX_GAMEPAD_BUTTON_ENTRY(START),
        TBX_GAMEPAD_BUTTON_ENTRY(LEFT_STICK),
        TBX_GAMEPAD_BUTTON_ENTRY(RIGHT_STICK),
        TBX_GAMEPAD_BUTTON_ENTRY(LEFT_SHOULDER),
        TBX_GAMEPAD_BUTTON_ENTRY(RIGHT_SHOULDER),
        TBX_GAMEPAD_BUTTON_ENTRY(DPAD_UP),
        TBX_GAMEPAD_BUTTON_ENTRY(DPAD_DOWN),
        TBX_GAMEPAD_BUTTON_ENTRY(DPAD_LEFT),
        TBX_GAMEPAD_BUTTON_ENTRY(DPAD_RIGHT)};
#undef TBX_GAMEPAD_BUTTON_ENTRY

#define TBX_GAMEPAD_AXIS_ENTRY(name)                                                               \
    GamepadAxisEntry                                                                               \
    {                                                                                              \
        #name, GamepadAxis::name                                                                   \
    }
    static constexpr GamepadAxisEntry GAMEPAD_AXIS_TABLE[] = {
        TBX_GAMEPAD_AXIS_ENTRY(LEFT_X),
        TBX_GAMEPAD_AXIS_ENTRY(LEFT_Y),
        TBX_GAMEPAD_AXIS_ENTRY(RIGHT_X),
        TBX_GAMEPAD_AXIS_ENTRY(RIGHT_Y),
        TBX_GAMEPAD_AXIS_ENTRY(LEFT_TRIGGER),
        TBX_GAMEPAD_AXIS_ENTRY(RIGHT_TRIGGER)};
#undef TBX_GAMEPAD_AXIS_ENTRY

    /// @brief
    /// Purpose: Input takes tbx.Key/tbx.MouseButton enum values — strongly typed, no strings.
    static Key check_key(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value <= 0 || value >= static_cast<int>(Key::COUNT))
            luaL_error(lua, "expected a tbx.Key value");
        return static_cast<Key>(value);
    }

    static MouseButton check_mouse_button(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(MouseButton::COUNT))
            luaL_error(lua, "expected a tbx.MouseButton value");
        return static_cast<MouseButton>(value);
    }

    static int input_is_down(lua_State* lua)
    {
        lua_pushboolean(lua, is_down(bound_runtime(lua).input, check_key(lua, 1)));
        return 1;
    }

    static int input_is_pressed(lua_State* lua)
    {
        lua_pushboolean(lua, is_pressed(bound_runtime(lua).input, check_key(lua, 1)));
        return 1;
    }

    static int input_is_mouse_down(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_mouse_down(bound_runtime(lua).input, check_mouse_button(lua, 1)));
        return 1;
    }

    static int input_is_mouse_pressed(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_mouse_pressed(bound_runtime(lua).input, check_mouse_button(lua, 1)));
        return 1;
    }

    static int input_get_mouse_delta(lua_State* lua)
    {
        const Vec2 delta = get_mouse_delta(bound_runtime(lua).input);
        push_vector_table(lua, &delta.x, XYZW_KEYS, 2);
        return 1;
    }

    static CursorMode check_cursor_mode(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(CursorMode::COUNT))
            luaL_error(lua, "expected a tbx.CursorMode value");
        return static_cast<CursorMode>(value);
    }

    static int input_get_cursor_mode(lua_State* lua)
    {
        lua_pushinteger(lua, static_cast<int>(get_cursor_mode(bound_runtime(lua).input)));
        return 1;
    }

    static int input_set_cursor_mode(lua_State* lua)
    {
        set_cursor_mode(bound_runtime(lua).input, check_cursor_mode(lua, 1));
        return 0;
    }

    static GamepadButton check_gamepad_button(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(GamepadButton::COUNT))
            luaL_error(lua, "expected a tbx.GamepadButton value");
        return static_cast<GamepadButton>(value);
    }

    static GamepadAxis check_gamepad_axis(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(GamepadAxis::COUNT))
            luaL_error(lua, "expected a tbx.GamepadAxis value");
        return static_cast<GamepadAxis>(value);
    }

    // Gamepad queries take the controller slot (0-based) first, then the button/axis enum.
    static int input_is_gamepad_connected(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_gamepad_connected(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1))));
        return 1;
    }

    static int input_is_gamepad_down(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_gamepad_down(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1)),
                check_gamepad_button(lua, 2)));
        return 1;
    }

    static int input_is_gamepad_pressed(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_gamepad_pressed(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1)),
                check_gamepad_button(lua, 2)));
        return 1;
    }

    static int input_is_gamepad_released(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_gamepad_released(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1)),
                check_gamepad_button(lua, 2)));
        return 1;
    }

    static int input_get_gamepad_axis(lua_State* lua)
    {
        lua_pushnumber(
            lua,
            get_gamepad_axis(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1)),
                check_gamepad_axis(lua, 2)));
        return 1;
    }

    //// EVENTS ////

    // Each event kind gets a pusher turning its POD payload into the Lua table the handler
    // receives — the mirror of the push_field_value work, but for the fixed event structs.
    static void push_key_event(lua_State* lua, RuntimeState&, const KeyEvent& event)
    {
        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, static_cast<int>(event.key));
        lua_setfield(lua, -2, "key");
        lua_pushboolean(lua, event.is_down);
        lua_setfield(lua, -2, "is_down");
        lua_pushboolean(lua, event.is_repeat);
        lua_setfield(lua, -2, "is_repeat");
    }

    static void push_window_resized(lua_State* lua, RuntimeState&, const WindowResized& event)
    {
        lua_createtable(lua, 0, 2);
        lua_pushinteger(lua, event.width);
        lua_setfield(lua, -2, "width");
        lua_pushinteger(lua, event.height);
        lua_setfield(lua, -2, "height");
    }

    static void push_asset_reloaded(lua_State* lua, RuntimeState&, const AssetReloaded& event)
    {
        lua_createtable(lua, 0, 2);
        lua_pushstring(lua, event.id.to_string().c_str());
        lua_setfield(lua, -2, "id");
        lua_pushstring(lua, event.extension.data());
        lua_setfield(lua, -2, "extension");
    }

    static void push_asset_unloaded(lua_State* lua, RuntimeState&, const AssetUnloaded& event)
    {
        lua_createtable(lua, 0, 2);
        lua_pushstring(lua, event.id.to_string().c_str());
        lua_setfield(lua, -2, "id");
        lua_pushstring(lua, event.extension.data());
        lua_setfield(lua, -2, "extension");
    }

    static void push_script_reloaded(lua_State* lua, RuntimeState&, const ScriptReloaded& event)
    {
        lua_createtable(lua, 0, 1);
        lua_pushstring(lua, event.id.to_string().c_str());
        lua_setfield(lua, -2, "id");
    }

    static void push_collision_event(lua_State* lua, RuntimeState& runtime, const CollisionEvent& event)
    {
        lua_createtable(lua, 0, 2);
        push_toy(lua, runtime.sandbox, static_cast<ToyId>(event.toy_a));
        lua_setfield(lua, -2, "toy_a");
        push_toy(lua, runtime.sandbox, static_cast<ToyId>(event.toy_b));
        lua_setfield(lua, -2, "toy_b");
    }

    static void push_input_device_connected(
        lua_State* lua,
        RuntimeState&,
        const InputDeviceConnected& event)
    {
        lua_createtable(lua, 0, 1);
        lua_pushinteger(lua, event.index);
        lua_setfield(lua, -2, "index");
    }

    static void push_input_device_disconnected(
        lua_State* lua,
        RuntimeState&,
        const InputDeviceDisconnected& event)
    {
        lua_createtable(lua, 0, 1);
        lua_pushinteger(lua, event.index);
        lua_setfield(lua, -2, "index");
    }

    /// @brief
    /// Purpose: The shared body of every tbx.events.on_*(fn): refs the Lua handler and subscribes
    /// a C++ bridge that, at dispatch, builds the event table and calls it. The owner tag is the
    /// lua_State so the backend bulk-purges every handler before it closes the VM.
    template <typename TEvent>
    static int subscribe_lua_event(
        lua_State* lua,
        Signal<TEvent>& signal,
        void (*push_event)(lua_State*, RuntimeState&, const TEvent&))
    {
        luaL_checktype(lua, 1, LUA_TFUNCTION);
        const int callback_ref = lua_ref(lua, 1); // refs the function in place (Luau: no pop)
        RuntimeState* runtime = &bound_runtime(lua);
        signal.subscribe(
            lua,
            [lua, callback_ref, push_event, runtime](const TEvent& event)
            {
                lua_getref(lua, callback_ref);
                if (!lua_isfunction(lua, -1))
                {
                    lua_pop(lua, 1);
                    return;
                }
                push_event(lua, *runtime, event);
                if (lua_pcall(lua, 1, 0, 0) != 0)
                {
                    TBX_ERROR("event handler error: {}", lua_tostring(lua, -1));
                    lua_pop(lua, 1);
                }
            });
        return 0;
    }

    static int events_on_key(lua_State* lua)
    {
        return subscribe_lua_event<KeyEvent>(lua, bound_runtime(lua).events.key, push_key_event);
    }

    static int events_on_window_resized(lua_State* lua)
    {
        return subscribe_lua_event<WindowResized>(
            lua,
            bound_runtime(lua).events.window_resized,
            push_window_resized);
    }

    static int events_on_asset_reloaded(lua_State* lua)
    {
        return subscribe_lua_event<AssetReloaded>(
            lua,
            bound_runtime(lua).events.asset_reloaded,
            push_asset_reloaded);
    }

    static int events_on_asset_unloaded(lua_State* lua)
    {
        return subscribe_lua_event<AssetUnloaded>(
            lua,
            bound_runtime(lua).events.asset_unloaded,
            push_asset_unloaded);
    }

    static int events_on_script_reloaded(lua_State* lua)
    {
        return subscribe_lua_event<ScriptReloaded>(
            lua,
            bound_runtime(lua).events.script_reloaded,
            push_script_reloaded);
    }

    static int events_on_collision(lua_State* lua)
    {
        return subscribe_lua_event<CollisionEvent>(
            lua,
            bound_runtime(lua).events.collision,
            push_collision_event);
    }

    static int events_on_input_device_connected(lua_State* lua)
    {
        return subscribe_lua_event<InputDeviceConnected>(
            lua,
            bound_runtime(lua).events.input_device_connected,
            push_input_device_connected);
    }

    static int events_on_input_device_disconnected(lua_State* lua)
    {
        return subscribe_lua_event<InputDeviceDisconnected>(
            lua,
            bound_runtime(lua).events.input_device_disconnected,
            push_input_device_disconnected);
    }

    //// OPEN ////

    void push_toy(lua_State* lua, Sandbox& sandbox, const ToyId entity)
    {
        auto* data = static_cast<ToyUserdata*>(lua_newuserdata(lua, sizeof(ToyUserdata)));
        *data = ToyUserdata {.sandbox = &sandbox, .entity = entity};
        luaL_getmetatable(lua, TOY_METATABLE);
        lua_setmetatable(lua, -2);
    }

    /// @brief
    /// Purpose: Registers one tbx.* binding on the table at the top of the stack — the
    /// closure carries the runtime as its light-userdata upvalue.
    /// @brief
    /// Purpose: Replaces Lua's stdout print — routes script output through the engine log so it
    /// carries the same "[file:line]" source stamp as engine logs. Level 1 is the script frame
    /// that called print; the '@'-prefixed chunk name makes short_src the clean script filename.
    static int lua_print(lua_State* lua)
    {
        const int count = lua_gettop(lua);
        std::string message;
        for (int i = 1; i <= count; ++i)
        {
            if (i > 1)
                message += ' ';
            if (const char* text = lua_tostring(lua, i))
                message += text;
            else
                message += luaL_typename(lua, i);
        }
        lua_Debug info = {};
        if (lua_getinfo(lua, 1, "sl", &info))
            write_log(LogLevel::INFO, message, info.short_src, info.currentline);
        else
            write_log(LogLevel::INFO, message, "luau", 0);
        return 0;
    }

    static void register_runtime_closure(
        lua_State* lua,
        RuntimeState& runtime,
        const lua_CFunction function,
        const char* debug_name,
        const char* field)
    {
        lua_pushlightuserdata(lua, &runtime);
        lua_pushcclosure(lua, function, debug_name, 1);
        lua_setfield(lua, -2, field);
    }

    void open_tbx_bindings(lua_State* lua, RuntimeState& runtime)
    {
        // Toy metatable: __index is a closure over the method table so unknown keys fall
        // through to typed block lookup; __newindex is add-and-populate.
        luaL_newmetatable(lua, TOY_METATABLE);
        lua_createtable(lua, 0, 13);
        const luaL_Reg toy_methods[] = {
            {"getName", toy_get_name},
            {"setName", toy_set_name},
            {"isEnabled", toy_is_enabled},
            {"setEnabled", toy_set_enabled},
            {"with", toy_with},
            {"removeBlock", toy_remove_block},
            {"sticker", toy_sticker},
            {"hasSticker", toy_has_sticker},
            {"removeSticker", toy_remove_sticker},
            {"setParent", toy_set_parent},
            {"getParent", toy_get_parent},
            {"getChildren", toy_get_children},
            {"despawn", toy_despawn},
            {"isAlive", toy_is_alive},
            {nullptr, nullptr}};
        luaL_register(lua, nullptr, toy_methods);
        lua_pushcclosure(lua, toy_index, "toy_index", 1);
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, toy_newindex, "toy_newindex");
        lua_setfield(lua, -2, "__newindex");
        lua_pop(lua, 1);

        // Block metatable: field access straight through TypeInfo.
        luaL_newmetatable(lua, BLOCK_METATABLE);
        lua_pushcfunction(lua, block_index, "block_index");
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, block_newindex, "block_newindex");
        lua_setfield(lua, -2, "__newindex");
        lua_pop(lua, 1);

        // Route print() through the engine log so script output is source-stamped like the rest.
        lua_pushcfunction(lua, lua_print, "print");
        lua_setglobal(lua, "print");

        // Global tbx table.
        lua_createtable(lua, 0, 5);

        // tbx.sandbox is the scene object — its queries are called with `:` (methods).
        lua_createtable(lua, 0, 8);
        register_runtime_closure(lua, runtime, sandbox_spawn, "sandbox_spawn", "spawn");
        register_runtime_closure(lua, runtime, sandbox_find, "sandbox_find", "find");
        register_runtime_closure(
            lua,
            runtime,
            sandbox_find_with_sticker,
            "sandbox_find_with_sticker",
            "findWithSticker");
        register_runtime_closure(
            lua,
            runtime,
            sandbox_find_all_with_sticker,
            "sandbox_find_all_with_sticker",
            "findAllWithSticker");
        register_runtime_closure(lua, runtime, sandbox_get_toys, "sandbox_get_toys", "getToys");
        lua_pushcfunction(lua, sandbox_despawn, "sandbox_despawn");
        lua_setfield(lua, -2, "despawn");
        register_runtime_closure(lua, runtime, sandbox_spawn_kit, "sandbox_spawn_kit", "spawnKit");
        register_runtime_closure(
            lua,
            runtime,
            sandbox_despawn_kit,
            "sandbox_despawn_kit",
            "despawnKit");
        // Streaming is engine-pulled from the scene's cameras — scripts never push a focus.
        lua_setfield(lua, -2, "sandbox");

        lua_createtable(lua, 0, 12);
        register_runtime_closure(lua, runtime, input_is_down, "input_is_down", "isDown");
        register_runtime_closure(lua, runtime, input_is_pressed, "input_is_pressed", "isPressed");
        register_runtime_closure(
            lua,
            runtime,
            input_is_mouse_down,
            "input_is_mouse_down",
            "isMouseDown");
        register_runtime_closure(
            lua,
            runtime,
            input_is_mouse_pressed,
            "input_is_mouse_pressed",
            "isMousePressed");
        register_runtime_closure(
            lua,
            runtime,
            input_get_mouse_delta,
            "input_get_mouse_delta",
            "getMouseDelta");
        register_runtime_closure(
            lua,
            runtime,
            input_get_cursor_mode,
            "input_get_cursor_mode",
            "getCursorMode");
        register_runtime_closure(
            lua,
            runtime,
            input_set_cursor_mode,
            "input_set_cursor_mode",
            "setCursorMode");
        register_runtime_closure(
            lua,
            runtime,
            input_is_gamepad_connected,
            "input_is_gamepad_connected",
            "isGamepadConnected");
        register_runtime_closure(
            lua,
            runtime,
            input_is_gamepad_down,
            "input_is_gamepad_down",
            "isGamepadDown");
        register_runtime_closure(
            lua,
            runtime,
            input_is_gamepad_pressed,
            "input_is_gamepad_pressed",
            "isGamepadPressed");
        register_runtime_closure(
            lua,
            runtime,
            input_is_gamepad_released,
            "input_is_gamepad_released",
            "isGamepadReleased");
        register_runtime_closure(
            lua,
            runtime,
            input_get_gamepad_axis,
            "input_get_gamepad_axis",
            "getGamepadAxis");
        lua_setfield(lua, -2, "input");

        lua_createtable(lua, 0, 1);
        register_runtime_closure(lua, runtime, physics_raycast, "physics_raycast", "raycast");
        lua_setfield(lua, -2, "physics");

        // ui:bind lives on the UI component now; tbx.ui keeps only the global one-shot setter.
        lua_createtable(lua, 0, 1);
        register_runtime_closure(lua, runtime, ui_set_string, "ui_set_string", "setString");
        lua_setfield(lua, -2, "ui");

        // tbx.events.on<Name>(fn): subscribe a script handler to an engine event. Naming mirrors
        // the EventsState signals (asset_reloaded -> onAssetReloaded).
        lua_createtable(lua, 0, 8);
        register_runtime_closure(lua, runtime, events_on_key, "events_on_key", "onKey");
        register_runtime_closure(
            lua,
            runtime,
            events_on_window_resized,
            "events_on_window_resized",
            "onWindowResized");
        register_runtime_closure(
            lua,
            runtime,
            events_on_asset_reloaded,
            "events_on_asset_reloaded",
            "onAssetReloaded");
        register_runtime_closure(
            lua,
            runtime,
            events_on_asset_unloaded,
            "events_on_asset_unloaded",
            "onAssetUnloaded");
        register_runtime_closure(
            lua,
            runtime,
            events_on_script_reloaded,
            "events_on_script_reloaded",
            "onScriptReloaded");
        register_runtime_closure(
            lua,
            runtime,
            events_on_collision,
            "events_on_collision",
            "onCollision");
        register_runtime_closure(
            lua,
            runtime,
            events_on_input_device_connected,
            "events_on_input_device_connected",
            "onInputDeviceConnected");
        register_runtime_closure(
            lua,
            runtime,
            events_on_input_device_disconnected,
            "events_on_input_device_disconnected",
            "onInputDeviceDisconnected");
        lua_setfield(lua, -2, "events");

        // Strongly typed input enums: tbx.Key.W, tbx.MouseButton.LEFT.
        lua_createtable(lua, 0, static_cast<int>(std::size(KEY_TABLE)));
        for (const KeyEntry& entry : KEY_TABLE)
        {
            lua_pushinteger(lua, static_cast<int>(entry.key));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "Key");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, static_cast<int>(MouseButton::LEFT));
        lua_setfield(lua, -2, "LEFT");
        lua_pushinteger(lua, static_cast<int>(MouseButton::RIGHT));
        lua_setfield(lua, -2, "RIGHT");
        lua_pushinteger(lua, static_cast<int>(MouseButton::MIDDLE));
        lua_setfield(lua, -2, "MIDDLE");
        lua_setfield(lua, -2, "MouseButton");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, static_cast<int>(CursorMode::NORMAL));
        lua_setfield(lua, -2, "NORMAL");
        lua_pushinteger(lua, static_cast<int>(CursorMode::HIDDEN));
        lua_setfield(lua, -2, "HIDDEN");
        lua_pushinteger(lua, static_cast<int>(CursorMode::LOCKED));
        lua_setfield(lua, -2, "LOCKED");
        lua_setfield(lua, -2, "CursorMode");

        // Strongly typed controller enums: tbx.GamepadButton.SOUTH, tbx.GamepadAxis.LEFT_X.
        lua_createtable(lua, 0, static_cast<int>(std::size(GAMEPAD_BUTTON_TABLE)));
        for (const GamepadButtonEntry& entry : GAMEPAD_BUTTON_TABLE)
        {
            lua_pushinteger(lua, static_cast<int>(entry.button));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "GamepadButton");

        lua_createtable(lua, 0, static_cast<int>(std::size(GAMEPAD_AXIS_TABLE)));
        for (const GamepadAxisEntry& entry : GAMEPAD_AXIS_TABLE)
        {
            lua_pushinteger(lua, static_cast<int>(entry.axis));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "GamepadAxis");

        lua_createtable(lua, 0, 18);
        const luaL_Reg math_functions[] = {
            {"add", math_add},
            {"subtract", math_subtract},
            {"scale", math_scale},
            {"dot", math_dot},
            {"cross", math_cross},
            {"length", math_length},
            {"distance", math_distance},
            {"normalize", math_normalize},
            {"lerp", math_lerp},
            {"moveToward", math_move_toward},
            {"reflect", math_reflect},
            {"angleAxis", math_angle_axis},
            {"multiply", math_multiply},
            {"rotate", math_rotate},
            {"slerp", math_slerp},
            {"fromEuler", math_from_euler},
            {"toEuler", math_to_euler},
            {"quatLookAt", math_quat_look_at},
            {nullptr, nullptr}};
        luaL_register(lua, nullptr, math_functions);
        lua_setfield(lua, -2, "math");

        register_runtime_closure(lua, runtime, tbx_quit, "tbx_quit", "quit");

        lua_setglobal(lua, "tbx");
    }
}
