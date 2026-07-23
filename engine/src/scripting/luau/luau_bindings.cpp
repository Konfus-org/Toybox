#include "luau_bindings.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/math/math.h"
#include "tbx/math/transform.h"
#include "tbx/reflection/type_registry.h"
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
        // get_block_bytes already does the describe_type + get_block-null guard and returns
        // nullptr for an unregistered/non-block type or an unattached block — no need to repeat it.
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
        UI* ui = Toy(*data.sandbox, data.entity).try_get_block<UI>();
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

    // Defined further down (near the math bindings); the toy metatable needs them up here.
    static int push_vector3(lua_State* lua, const Vec3& vector);
    static int push_quat(lua_State* lua, const Quat& rotation);
    static Vec3 check_vector3(lua_State* lua, int index);
    static Quat check_quat(lua_State* lua, int index);

    /// @brief
    /// Purpose: A block token (tbx.blocks.<Type>) is a small table carrying the block's registered
    /// name under "__name". Returns the name hash, or 0 when the value isn't a block token.
    static uint64 block_token_hash(lua_State* lua, const int index)
    {
        if (!lua_istable(lua, index))
            return 0;
        lua_getfield(lua, index, "__name");
        const char* name = lua_tostring(lua, -1);
        const uint64 result = name ? hash(name) : 0;
        lua_pop(lua, 1);
        return result;
    }

    /// @brief
    /// Purpose: Attaches (default-builds) the block with this type hash, then writes any fields
    /// present in the table at fields_index — the shared body of toy:add / toy:with for blocks.
    static void add_and_populate(
        lua_State* lua,
        const ToyUserdata& data,
        const uint64 type_hash,
        const int fields_index)
    {
        const auto type = describe_type(type_hash);
        std::byte* block = Toy(*data.sandbox, data.entity).add_block_bytes(type_hash);
        if (!block || !type || !lua_istable(lua, fields_index))
            return;
        for (const FieldInfo& field : type->get().fields)
        {
            lua_getfield(lua, fields_index, field.name.c_str());
            if (!lua_isnil(lua, -1))
                write_field_value(lua, lua_gettop(lua), field, block);
            lua_pop(lua, 1);
        }
    }

    /// @brief
    /// Purpose: toy.<Method> resolves methods; otherwise a fixed set of Roblox-style convenience
    /// properties (Name/Enabled/Parent/Children/Transform/Position/... /World*). Blocks are NOT
    /// toy fields — reach them with toy:get(tbx.blocks.<Type>).
    static int toy_index(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const char* key = luaL_checkstring(lua, 2);

        // Methods first (the method table sits in this closure's upvalue).
        lua_getfield(lua, lua_upvalueindex(1), key);
        if (!lua_isnil(lua, -1))
            return 1;
        lua_pop(lua, 1);

        Toy toy(*data.sandbox, data.entity);
        if (std::strcmp(key, "Alive") == 0)
        {
            lua_pushboolean(lua, toy.is_alive());
            return 1;
        }
        if (std::strcmp(key, "Name") == 0)
        {
            lua_pushstring(lua, toy.get_name().c_str());
            return 1;
        }
        if (std::strcmp(key, "Enabled") == 0)
        {
            lua_pushboolean(lua, toy.is_enabled());
            return 1;
        }
        if (std::strcmp(key, "Parent") == 0)
        {
            const auto parent = toy.get_parent();
            if (parent)
                push_toy(lua, *data.sandbox, parent->get_id());
            else
                lua_pushnil(lua);
            return 1;
        }
        if (std::strcmp(key, "Children") == 0)
        {
            const auto children = toy.get_children();
            lua_createtable(lua, static_cast<int>(children.size()), 0);
            int index = 1;
            for (const Toy& child : children)
            {
                push_toy(lua, *data.sandbox, child.get_id());
                lua_rawseti(lua, -2, index++);
            }
            return 1;
        }
        if (std::strcmp(key, "Transform") == 0)
        {
            if (toy.has_block_named(hash("Transform")))
                push_block(lua, data, hash("Transform"));
            else
                lua_pushnil(lua);
            return 1;
        }
        if (std::strcmp(key, "Position") == 0)
            return push_vector3(lua, toy.get_transform().position);
        if (std::strcmp(key, "Rotation") == 0)
            return push_quat(lua, toy.get_transform().rotation);
        if (std::strcmp(key, "Scale") == 0)
            return push_vector3(lua, toy.get_transform().scale);
        if (std::strcmp(key, "WorldPosition") == 0)
            return push_vector3(lua, decompose(toy.get_world_transform()).position);
        if (std::strcmp(key, "WorldRotation") == 0)
            return push_quat(lua, decompose(toy.get_world_transform()).rotation);
        if (std::strcmp(key, "WorldScale") == 0)
            return push_vector3(lua, decompose(toy.get_world_transform()).scale);

        lua_pushnil(lua);
        return 1;
    }

    /// @brief
    /// Purpose: Writes a convenience property (Name/Enabled/Parent/Position/Rotation/Scale).
    /// Blocks and stickers are added through toy:add, not assignment; read-only properties
    /// (Alive/Children/World*) reject writes.
    static int toy_newindex(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const char* key = luaL_checkstring(lua, 2);
        Toy toy(*data.sandbox, data.entity);

        if (std::strcmp(key, "Name") == 0)
        {
            toy.set_name(luaL_checkstring(lua, 3));
            return 0;
        }
        if (std::strcmp(key, "Enabled") == 0)
        {
            toy.set_enabled(lua_toboolean(lua, 3) != 0);
            return 0;
        }
        if (std::strcmp(key, "Parent") == 0)
        {
            // nil clears the parent; a toy reparents.
            auto parent = Toy();
            if (!lua_isnoneornil(lua, 3))
            {
                const ToyUserdata& parent_data = check_toy(lua, 3);
                parent = Toy(*parent_data.sandbox, parent_data.entity);
            }
            toy.set_parent(parent);
            return 0;
        }
        if (std::strcmp(key, "Position") == 0)
        {
            toy.get_transform().position = check_vector3(lua, 3);
            return 0;
        }
        if (std::strcmp(key, "Rotation") == 0)
        {
            toy.get_transform().rotation = check_quat(lua, 3);
            return 0;
        }
        if (std::strcmp(key, "Scale") == 0)
        {
            toy.get_transform().scale = check_vector3(lua, 3);
            return 0;
        }
        luaL_error(
            lua,
            "'%s' is not an assignable toy property (add blocks/stickers with toy:add)",
            key);
        return 0;
    }

    /// @brief
    /// Purpose: toy:get(tbx.blocks.<Type>) — the generic component getter. The token carries the
    /// block's name; the .d.luau types it as BlockType<T, P>, so toy:get(tbx.blocks.UI) infers
    /// UI?. Returns the live block, or nil when it isn't attached.
    static int toy_get(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        const uint64 hashed = block_token_hash(lua, 2);
        if (hashed == 0 || !Toy(*data.sandbox, data.entity).has_block_named(hashed))
        {
            lua_pushnil(lua);
            return 1;
        }
        push_block(lua, data, hashed);
        return 1;
    }

    /// @brief
    /// Purpose: toy:add(tbx.blocks.<Type>, fields?) attaches a block and returns it (typed);
    /// toy:add(name) adds a sticker and returns the toy (fluent).
    static int toy_add(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        if (lua_type(lua, 2) == LUA_TSTRING)
        {
            Toy(*data.sandbox, data.entity).sticker(lua_tostring(lua, 2));
            lua_pushvalue(lua, 1); // fluent: return the toy
            return 1;
        }
        const uint64 hashed = block_token_hash(lua, 2);
        if (hashed == 0)
            luaL_error(lua, "toy:add expects a block token (tbx.blocks.X) or a sticker name");
        add_and_populate(lua, data, hashed, 3);
        push_block(lua, data, hashed); // return the added block
        return 1;
    }

    /// @brief
    /// Purpose: toy:has(tbx.blocks.<Type>) tests for a block; toy:has(name) tests for a sticker.
    static int toy_has(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy toy(*data.sandbox, data.entity);
        if (lua_type(lua, 2) == LUA_TSTRING)
        {
            lua_pushboolean(lua, toy.has_sticker(lua_tostring(lua, 2)));
            return 1;
        }
        const uint64 hashed = block_token_hash(lua, 2);
        lua_pushboolean(lua, hashed != 0 && toy.has_block_named(hashed));
        return 1;
    }

    /// @brief
    /// Purpose: toy:remove(tbx.blocks.<Type>) detaches a block; toy:remove(name) peels a sticker.
    /// Chainable (returns the toy). To remove the toy itself, call sandbox:remove(toy).
    static int toy_remove(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy toy(*data.sandbox, data.entity);
        if (lua_type(lua, 2) == LUA_TSTRING)
        {
            toy.remove_sticker(lua_tostring(lua, 2));
        }
        else
        {
            const uint64 hashed = block_token_hash(lua, 2);
            if (hashed == 0)
                luaL_error(
                    lua,
                    "toy:remove expects a block token (tbx.blocks.X) or a sticker name");
            toy.remove_block_named(hashed);
        }
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    /// @brief
    /// Purpose: The fluent builder, always returns the toy — toy:with(sticker) /
    /// toy:with(parentToy) / toy:with(tbx.blocks.<Type>, fields?).
    static int toy_with(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy toy(*data.sandbox, data.entity);
        switch (lua_type(lua, 2))
        {
            case LUA_TSTRING: // sticker
                toy.sticker(lua_tostring(lua, 2));
                break;
            case LUA_TUSERDATA: // parent toy
            {
                const ToyUserdata& parent_data = check_toy(lua, 2);
                toy.set_parent(Toy(*parent_data.sandbox, parent_data.entity));
                break;
            }
            case LUA_TTABLE: // block token (+ optional fields)
            {
                const uint64 hashed = block_token_hash(lua, 2);
                if (hashed == 0)
                    luaL_error(
                        lua,
                        "toy:with got a table that isn't a block token (tbx.blocks.X)");
                add_and_populate(lua, data, hashed, 3);
                break;
            }
            default:
                luaL_error(lua, "toy:with expects a sticker name, a block token, or a parent toy");
        }
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    //// TOY FLUENT VERBS (each returns the toy, for chaining) ////

    static int toy_parent(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        auto parent = Toy();
        if (!lua_isnoneornil(lua, 2))
        {
            const ToyUserdata& parent_data = check_toy(lua, 2);
            parent = Toy(*parent_data.sandbox, parent_data.entity);
        }
        Toy(*data.sandbox, data.entity).set_parent(parent);
        lua_pushvalue(lua, 1);
        return 1;
    }

    static int toy_move(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).get_transform().position = check_vector3(lua, 2);
        lua_pushvalue(lua, 1);
        return 1;
    }

    static int toy_rotate(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).get_transform().rotation = check_quat(lua, 2);
        lua_pushvalue(lua, 1);
        return 1;
    }

    static int toy_resize(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).get_transform().scale = check_vector3(lua, 2);
        lua_pushvalue(lua, 1);
        return 1;
    }

    static int toy_rename(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        Toy(*data.sandbox, data.entity).set_name(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1);
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
    // A kit is referenced by an asset path (it has a folder or a file extension); a plain toy
    // name has neither. That's how sandbox:add tells "add a kit" from "add an empty toy".
    static bool looks_like_asset_reference(const char* name)
    {
        return std::strchr(name, '/') != nullptr || std::strchr(name, '.') != nullptr;
    }

    static int sandbox_add(lua_State* lua)
    {
        RuntimeState& state = bound_runtime(lua);
        const char* name = luaL_checkstring(lua, 2);
        if (looks_like_asset_reference(name))
        {
            // Given an asset path → instantiate the kit it references (optional add position).
            const Vec3 position =
                lua_istable(lua, 3) ? check_vector3(lua, 3) : Vec3(0.0f, 0.0f, 0.0f);
            const auto spawned = add(
                state.sandbox,
                state.assets,
                state.events,
                AssetHandle<Kit>(name),
                position);
            if (!spawned)
            {
                luaL_error(lua, "kit '%s': %s", name, spawned.error().c_str());
                return 0;
            }
            push_toy(lua, state.sandbox, spawned->get_id()); // the instance's root toy
            return 1;
        }
        const Toy toy = state.sandbox.add(name);
        push_toy(lua, state.sandbox, toy.get_id());
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

    // findWith(sticker) returns every toy wearing the sticker; findWith(tbx.blocks.X) returns
    // every toy carrying that block. Both return a list.
    static int sandbox_find_with(lua_State* lua)
    {
        Sandbox& sandbox = bound_runtime(lua).sandbox;
        lua_newtable(lua);
        int index = 1;
        if (lua_type(lua, 2) == LUA_TSTRING)
        {
            sandbox.for_each_with(
                std::string(lua_tostring(lua, 2)),
                [&](Toy toy)
                {
                    push_toy(lua, sandbox, toy.get_id());
                    lua_rawseti(lua, -2, index++); // append into the result array
                });
            return 1;
        }
        const uint64 hashed = block_token_hash(lua, 2);
        if (hashed == 0)
            luaL_error(lua, "sandbox:findWith expects a sticker name or a block token (tbx.blocks.X)");
        for (const Toy& toy : sandbox.get_toys())
            if (toy.has_block_named(hashed))
            {
                push_toy(lua, sandbox, toy.get_id());
                lua_rawseti(lua, -2, index++);
            }
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
        // sandbox:remove(toy) removes the toy and its whole subtree.
        const ToyUserdata& data = check_toy(lua, 2);
        data.sandbox->remove_subtree(Toy(*data.sandbox, data.entity));
        return 0;
    }

    static Vec3 check_vector3(lua_State* lua, const int index)
    {
        luaL_checktype(lua, index, LUA_TTABLE);
        auto vector = Vec3(0.0f, 0.0f, 0.0f);
        read_vector_table(lua, index, &vector.x, XYZW_KEYS, 3);
        return vector;
    }

    static int sandbox_despawn_kit(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 2);
        data.sandbox->remove_subtree(Toy(*data.sandbox, data.entity));
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

    // The Luau enum tables (tbx.Key/MouseButton/GamepadButton/GamepadAxis/MouseAxis) occupy
    // distinct value ranges so one overloaded isDown/getAxis can tell them apart from the bare
    // integer Lua passes. These bases offset each kind; keys stay 0-based (< the first base).
    static constexpr int MOUSE_BUTTON_INPUT_BASE = 1000;
    static constexpr int GAMEPAD_BUTTON_INPUT_BASE = 2000;
    static constexpr int GAMEPAD_AXIS_INPUT_BASE = 3000;
    static constexpr int MOUSE_AXIS_INPUT_BASE = 4000;

    // Gamepad queries take an optional controller slot as the second arg (default 0).
    static int input_gamepad_slot(lua_State* lua)
    {
        return static_cast<int>(luaL_optinteger(lua, 2, 0));
    }

    static int input_is_down(lua_State* lua)
    {
        const InputState& input = bound_runtime(lua).input;
        const int code = static_cast<int>(luaL_checkinteger(lua, 1));
        bool held;
        if (code >= GAMEPAD_BUTTON_INPUT_BASE && code < GAMEPAD_AXIS_INPUT_BASE)
            held = is_down(
                input,
                static_cast<GamepadButton>(code - GAMEPAD_BUTTON_INPUT_BASE),
                input_gamepad_slot(lua));
        else if (code >= MOUSE_BUTTON_INPUT_BASE && code < GAMEPAD_BUTTON_INPUT_BASE)
            held = is_down(input, static_cast<MouseButton>(code - MOUSE_BUTTON_INPUT_BASE));
        else
            held = is_down(input, static_cast<Key>(code));
        lua_pushboolean(lua, held);
        return 1;
    }

    static int input_is_pressed(lua_State* lua)
    {
        const InputState& input = bound_runtime(lua).input;
        const int code = static_cast<int>(luaL_checkinteger(lua, 1));
        bool pressed;
        if (code >= GAMEPAD_BUTTON_INPUT_BASE && code < GAMEPAD_AXIS_INPUT_BASE)
            pressed = is_pressed(
                input,
                static_cast<GamepadButton>(code - GAMEPAD_BUTTON_INPUT_BASE),
                input_gamepad_slot(lua));
        else if (code >= MOUSE_BUTTON_INPUT_BASE && code < GAMEPAD_BUTTON_INPUT_BASE)
            pressed = is_pressed(input, static_cast<MouseButton>(code - MOUSE_BUTTON_INPUT_BASE));
        else
            pressed = is_pressed(input, static_cast<Key>(code));
        lua_pushboolean(lua, pressed);
        return 1;
    }

    static int input_is_released(lua_State* lua)
    {
        const InputState& input = bound_runtime(lua).input;
        const int code = static_cast<int>(luaL_checkinteger(lua, 1));
        bool released;
        if (code >= GAMEPAD_BUTTON_INPUT_BASE && code < GAMEPAD_AXIS_INPUT_BASE)
            released = is_released(
                input,
                static_cast<GamepadButton>(code - GAMEPAD_BUTTON_INPUT_BASE),
                input_gamepad_slot(lua));
        else if (code >= MOUSE_BUTTON_INPUT_BASE && code < GAMEPAD_BUTTON_INPUT_BASE)
            released = is_released(input, static_cast<MouseButton>(code - MOUSE_BUTTON_INPUT_BASE));
        else
            released = is_released(input, static_cast<Key>(code));
        lua_pushboolean(lua, released);
        return 1;
    }

    static int input_get_axis(lua_State* lua)
    {
        const InputState& input = bound_runtime(lua).input;
        const int code = static_cast<int>(luaL_checkinteger(lua, 1));
        float value;
        if (code >= MOUSE_AXIS_INPUT_BASE)
            value = get_axis(input, static_cast<MouseAxis>(code - MOUSE_AXIS_INPUT_BASE));
        else
            value = get_axis(
                input,
                static_cast<GamepadAxis>(code - GAMEPAD_AXIS_INPUT_BASE),
                input_gamepad_slot(lua));
        lua_pushnumber(lua, value);
        return 1;
    }

    static int input_get_axis_delta(lua_State* lua)
    {
        const int code = static_cast<int>(luaL_checkinteger(lua, 1));
        lua_pushnumber(
            lua,
            get_axis_delta(
                bound_runtime(lua).input,
                static_cast<MouseAxis>(code - MOUSE_AXIS_INPUT_BASE)));
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

    // isGamepadConnected(slot) is the one input query that takes a bare slot, not an enum value.
    static int input_is_gamepad_connected(lua_State* lua)
    {
        lua_pushboolean(
            lua,
            is_gamepad_connected(
                bound_runtime(lua).input,
                static_cast<int>(luaL_checkinteger(lua, 1))));
        return 1;
    }

    //// EVENTS ////

    // Each event kind gets a pusher turning its POD payload into the Lua table the handler
    // receives — the mirror of the push_field_value work, but for the fixed event structs.
    static void push_input_event(lua_State* lua, RuntimeState&, const InputEvent& event)
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

    static void push_asset_loaded(lua_State* lua, RuntimeState&, const AssetLoaded& event)
    {
        lua_createtable(lua, 0, 2);
        lua_pushstring(lua, event.id.to_string().c_str());
        lua_setfield(lua, -2, "id");
        lua_pushstring(lua, event.extension.data());
        lua_setfield(lua, -2, "extension");
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

    static int events_on_input(lua_State* lua)
    {
        return subscribe_lua_event<InputEvent>(
            lua,
            bound_runtime(lua).events.input,
            push_input_event);
    }

    static int events_on_window_resized(lua_State* lua)
    {
        return subscribe_lua_event<WindowResized>(
            lua,
            bound_runtime(lua).events.window_resized,
            push_window_resized);
    }

    static int events_on_asset_loaded(lua_State* lua)
    {
        return subscribe_lua_event<AssetLoaded>(
            lua,
            bound_runtime(lua).events.asset_loaded,
            push_asset_loaded);
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
        lua_createtable(lua, 0, 10);
        const luaL_Reg toy_methods[] = {
            {"get", toy_get},
            {"add", toy_add},
            {"has", toy_has},
            {"remove", toy_remove},
            {"with", toy_with},
            {"parent", toy_parent},
            {"move", toy_move},
            {"rotate", toy_rotate},
            {"resize", toy_resize},
            {"rename", toy_rename},
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
        lua_createtable(lua, 0, 6);

        // tbx.sandbox is the scene object — its queries are called with `:` (methods).
        lua_createtable(lua, 0, 8);
        register_runtime_closure(lua, runtime, sandbox_add, "sandbox_add", "add");
        register_runtime_closure(lua, runtime, sandbox_find, "sandbox_find", "find");
        register_runtime_closure(
            lua,
            runtime,
            sandbox_find_with,
            "sandbox_find_with",
            "findWith");
        register_runtime_closure(lua, runtime, sandbox_get_toys, "sandbox_get_toys", "getToys");
        lua_pushcfunction(lua, sandbox_despawn, "sandbox_despawn");
        lua_setfield(lua, -2, "remove");
        // Streaming is engine-pulled from the scene's cameras — scripts never push a focus.
        lua_setfield(lua, -2, "sandbox");

        lua_createtable(lua, 0, 8);
        register_runtime_closure(lua, runtime, input_is_down, "input_is_down", "isDown");
        register_runtime_closure(lua, runtime, input_is_pressed, "input_is_pressed", "isPressed");
        register_runtime_closure(
            lua,
            runtime,
            input_is_released,
            "input_is_released",
            "isReleased");
        register_runtime_closure(lua, runtime, input_get_axis, "input_get_axis", "getAxis");
        register_runtime_closure(
            lua,
            runtime,
            input_get_axis_delta,
            "input_get_axis_delta",
            "getAxisDelta");
        register_runtime_closure(
            lua,
            runtime,
            input_is_gamepad_connected,
            "input_is_gamepad_connected",
            "isGamepadConnected");
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
        register_runtime_closure(lua, runtime, events_on_input, "events_on_input", "onInput");
        register_runtime_closure(
            lua,
            runtime,
            events_on_window_resized,
            "events_on_window_resized",
            "onWindowResized");
        register_runtime_closure(
            lua,
            runtime,
            events_on_asset_loaded,
            "events_on_asset_loaded",
            "onAssetLoaded");
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

        // tbx.blocks: opaque tokens for the generic block methods (get/add/has/remove/with). Each
        // token is a small table { __name = "<Type>" } — a table (not a bare string) so the
        // block methods can tell a token from a sticker name. The .d.luau types them as
        // BlockType<T, P>. Every registered block facet (has_block) gets an entry.
        lua_newtable(lua);
        for (const TypeInfo& type : get_type_registry().get_all())
        {
            if (!type.has_block)
                continue;
            lua_createtable(lua, 0, 1);
            lua_pushstring(lua, type.name.c_str());
            lua_setfield(lua, -2, "__name");
            lua_setfield(lua, -2, type.name.c_str());
        }
        lua_setfield(lua, -2, "blocks");

        // Strongly typed input enums. Each kind is offset into its own value range (keys stay
        // 0-based) so the overloaded isDown/getAxis can dispatch on the bare number Lua passes.
        lua_createtable(lua, 0, static_cast<int>(std::size(KEY_TABLE)));
        for (const KeyEntry& entry : KEY_TABLE)
        {
            lua_pushinteger(lua, static_cast<int>(entry.key));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "Key");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, MOUSE_BUTTON_INPUT_BASE + static_cast<int>(MouseButton::LEFT));
        lua_setfield(lua, -2, "LEFT");
        lua_pushinteger(lua, MOUSE_BUTTON_INPUT_BASE + static_cast<int>(MouseButton::RIGHT));
        lua_setfield(lua, -2, "RIGHT");
        lua_pushinteger(lua, MOUSE_BUTTON_INPUT_BASE + static_cast<int>(MouseButton::MIDDLE));
        lua_setfield(lua, -2, "MIDDLE");
        lua_setfield(lua, -2, "MouseButton");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, MOUSE_AXIS_INPUT_BASE + static_cast<int>(MouseAxis::X));
        lua_setfield(lua, -2, "X");
        lua_pushinteger(lua, MOUSE_AXIS_INPUT_BASE + static_cast<int>(MouseAxis::Y));
        lua_setfield(lua, -2, "Y");
        lua_pushinteger(lua, MOUSE_AXIS_INPUT_BASE + static_cast<int>(MouseAxis::SCROLL));
        lua_setfield(lua, -2, "SCROLL");
        lua_setfield(lua, -2, "MouseAxis");

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
            lua_pushinteger(lua, GAMEPAD_BUTTON_INPUT_BASE + static_cast<int>(entry.button));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "GamepadButton");

        lua_createtable(lua, 0, static_cast<int>(std::size(GAMEPAD_AXIS_TABLE)));
        for (const GamepadAxisEntry& entry : GAMEPAD_AXIS_TABLE)
        {
            lua_pushinteger(lua, GAMEPAD_AXIS_INPUT_BASE + static_cast<int>(entry.axis));
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
