#include "luau_bindings.h"
#include "tbx/runtime.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/utils/hash.h"
#include "tbx/ui/ui.h"
#include <lualib.h>
#include <cstring>

namespace tbx::scripts
{
    static constexpr const char* TOY_METATABLE = "tbx.Toy";
    static constexpr const char* BLOCK_METATABLE = "tbx.Block";

    /// @brief
    /// Purpose: Payload of a ecs::Toy userdata. Raw pointer is deliberate at the C boundary; the
    /// ecs::Sandbox outlives the VM (Scripts is constructed after it and destroyed first).
    struct ToyUserdata
    {
        ecs::Sandbox* sandbox = nullptr;
        ecs::ToyId entity = ecs::NULL_TOY;
    };

    /// @brief
    /// Purpose: Payload of a ecs::Block userdata: enough to re-fetch the live block every access, so
    /// stale pointers cannot exist.
    struct BlockUserdata
    {
        ecs::Sandbox* sandbox = nullptr;
        ecs::ToyId entity = ecs::NULL_TOY;
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

    static void push_vector_table(lua_State* lua, const float* values, const char* const* keys, const int count)
    {
        lua_createtable(lua, 0, count);
        for (int i = 0; i < count; ++i)
        {
            lua_pushnumber(lua, values[i]);
            lua_setfield(lua, -2, keys[i]);
        }
    }

    static void read_vector_table(lua_State* lua, const int index, float* values, const char* const* keys, const int count)
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

    static int push_field_value(lua_State* lua, const reflection::FieldInfo& field, const std::byte* block)
    {
        const std::byte* at = block + field.offset;
        switch (field.kind)
        {
            case reflection::FieldKind::BOOL:
                lua_pushboolean(lua, *reinterpret_cast<const bool*>(at));
                return 1;
            case reflection::FieldKind::INT32:
                lua_pushnumber(lua, *reinterpret_cast<const int32*>(at));
                return 1;
            case reflection::FieldKind::UINT32:
                lua_pushnumber(lua, *reinterpret_cast<const uint32*>(at));
                return 1;
            case reflection::FieldKind::INT64:
                lua_pushnumber(lua, static_cast<double>(*reinterpret_cast<const int64*>(at)));
                return 1;
            case reflection::FieldKind::UINT64:
                lua_pushnumber(lua, static_cast<double>(*reinterpret_cast<const uint64*>(at)));
                return 1;
            case reflection::FieldKind::FLOAT:
                lua_pushnumber(lua, *reinterpret_cast<const float*>(at));
                return 1;
            case reflection::FieldKind::DOUBLE:
                lua_pushnumber(lua, *reinterpret_cast<const double*>(at));
                return 1;
            case reflection::FieldKind::STRING:
                lua_pushstring(lua, reinterpret_cast<const std::string*>(at)->c_str());
                return 1;
            case reflection::FieldKind::VEC2:
                push_vector_table(lua, &reinterpret_cast<const Vec2*>(at)->x, XYZW_KEYS, 2);
                return 1;
            case reflection::FieldKind::VEC3:
                push_vector_table(lua, &reinterpret_cast<const Vec3*>(at)->x, XYZW_KEYS, 3);
                return 1;
            case reflection::FieldKind::VEC4:
                push_vector_table(lua, &reinterpret_cast<const Vec4*>(at)->x, XYZW_KEYS, 4);
                return 1;
            case reflection::FieldKind::QUAT:
                push_vector_table(lua, &reinterpret_cast<const Quat*>(at)->x, XYZW_KEYS, 4);
                return 1;
            case reflection::FieldKind::COLOR:
                push_vector_table(lua, &reinterpret_cast<const Color*>(at)->r, RGBA_KEYS, 4);
                return 1;
            case reflection::FieldKind::UUID:
                lua_pushstring(lua, reinterpret_cast<const Uuid*>(at)->to_string().c_str());
                return 1;
            case reflection::FieldKind::ASSET:
            {
                const auto [id, asset_path] = field.read_asset(block);
                if (!id.is_valid() && !asset_path.empty())
                    lua_pushstring(lua, asset_path.c_str());
                else
                    lua_pushstring(lua, id.to_string().c_str());
                return 1;
            }
            case reflection::FieldKind::ENUM:
            {
                auto raw = uint64(0);
                std::memcpy(&raw, at, field.size_bytes);
                lua_pushnumber(lua, static_cast<double>(raw));
                return 1;
            }
            case reflection::FieldKind::ASSET_LIST:
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
            case reflection::FieldKind::TYPE:
            case reflection::FieldKind::TYPE_LIST:
                TBX_WARN("nested block field '{}' is not scriptable yet", field.name);
                lua_pushnil(lua);
                return 1;
        }
        lua_pushnil(lua);
        return 1;
    }

    static void write_field_value(lua_State* lua, const int value_index, const reflection::FieldInfo& field, std::byte* block)
    {
        std::byte* at = block + field.offset;
        switch (field.kind)
        {
            case reflection::FieldKind::BOOL:
                *reinterpret_cast<bool*>(at) = lua_toboolean(lua, value_index) != 0;
                return;
            case reflection::FieldKind::INT32:
                *reinterpret_cast<int32*>(at) = static_cast<int32>(luaL_checknumber(lua, value_index));
                return;
            case reflection::FieldKind::UINT32:
                *reinterpret_cast<uint32*>(at) = static_cast<uint32>(luaL_checknumber(lua, value_index));
                return;
            case reflection::FieldKind::INT64:
                *reinterpret_cast<int64*>(at) = static_cast<int64>(luaL_checknumber(lua, value_index));
                return;
            case reflection::FieldKind::UINT64:
                *reinterpret_cast<uint64*>(at) = static_cast<uint64>(luaL_checknumber(lua, value_index));
                return;
            case reflection::FieldKind::FLOAT:
                *reinterpret_cast<float*>(at) = static_cast<float>(luaL_checknumber(lua, value_index));
                return;
            case reflection::FieldKind::DOUBLE:
                *reinterpret_cast<double*>(at) = luaL_checknumber(lua, value_index);
                return;
            case reflection::FieldKind::STRING:
                *reinterpret_cast<std::string*>(at) = luaL_checkstring(lua, value_index);
                return;
            case reflection::FieldKind::VEC2:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec2*>(at)->x, XYZW_KEYS, 2);
                return;
            case reflection::FieldKind::VEC3:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec3*>(at)->x, XYZW_KEYS, 3);
                return;
            case reflection::FieldKind::VEC4:
                read_vector_table(lua, value_index, &reinterpret_cast<Vec4*>(at)->x, XYZW_KEYS, 4);
                return;
            case reflection::FieldKind::QUAT:
                read_vector_table(lua, value_index, &reinterpret_cast<Quat*>(at)->x, XYZW_KEYS, 4);
                return;
            case reflection::FieldKind::COLOR:
                read_vector_table(lua, value_index, &reinterpret_cast<Color*>(at)->r, RGBA_KEYS, 4);
                return;
            case reflection::FieldKind::UUID:
                *reinterpret_cast<Uuid*>(at) = Uuid::parse(luaL_checkstring(lua, value_index));
                return;
            case reflection::FieldKind::ASSET:
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
            case reflection::FieldKind::ENUM:
            {
                const auto raw = static_cast<uint64>(luaL_checknumber(lua, value_index));
                std::memcpy(at, &raw, field.size_bytes);
                return;
            }
            case reflection::FieldKind::ASSET_LIST:
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
            case reflection::FieldKind::TYPE:
            case reflection::FieldKind::TYPE_LIST:
                TBX_WARN("nested block field '{}' is not scriptable yet", field.name);
                return;
        }
    }

    //// BLOCK METATABLE ////

    static std::byte* fetch_block(const BlockUserdata& data)
    {
        const auto type = reflection::describe(data.type_hash);
        if (!type || !type->get().get_block)
            return nullptr;
        return type->get().get_block(data.sandbox->get_registry(), data.entity);
    }

    static int block_index(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        const char* field_name = luaL_checkstring(lua, 2);
        const auto type = reflection::describe(data.type_hash);
        std::byte* block = fetch_block(data);
        if (!type || !block)
            luaL_error(lua, "block is gone");
        for (const reflection::FieldInfo& field : type->get().fields)
            if (field.name == field_name)
                return push_field_value(lua, field, block);
        luaL_error(lua, "block '%s' has no field '%s'", type->get().name.c_str(), field_name);
        return 0;
    }

    static int block_newindex(lua_State* lua)
    {
        const BlockUserdata& data = check_block(lua, 1);
        const char* field_name = luaL_checkstring(lua, 2);
        const auto type = reflection::describe(data.type_hash);
        std::byte* block = fetch_block(data);
        if (!type || !block)
            luaL_error(lua, "block is gone");
        for (const reflection::FieldInfo& field : type->get().fields)
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

        const auto type = reflection::describe(hash(key));
        if (!type || !type->get().has_block
            || !type->get().has_block(data.sandbox->get_registry(), data.entity))
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
        const auto type = reflection::describe(hashed);
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
        std::byte* block = type->get().add_block(data.sandbox->get_registry(), data.entity);
        for (const reflection::FieldInfo& field : type->get().fields)
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
        lua_pushstring(lua, ecs::Toy(*data.sandbox, data.entity).get_name().c_str());
        return 1;
    }

    static int toy_set_name(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        ecs::Toy(*data.sandbox, data.entity).set_name(luaL_checkstring(lua, 2));
        return 0;
    }

    static int toy_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        ecs::Toy(*data.sandbox, data.entity).sticker(luaL_checkstring(lua, 2));
        lua_pushvalue(lua, 1); // fluent: return the toy
        return 1;
    }

    static int toy_has_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, ecs::Toy(*data.sandbox, data.entity).has_sticker(luaL_checkstring(lua, 2)));
        return 1;
    }

    static int toy_remove_sticker(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        ecs::Toy(*data.sandbox, data.entity).remove_sticker(luaL_checkstring(lua, 2));
        return 0;
    }

    static int toy_is_alive(lua_State* lua)
    {
        const ToyUserdata& data = check_toy(lua, 1);
        lua_pushboolean(lua, data.sandbox->get_registry().valid(data.entity));
        return 1;
    }

    //// TBX TABLE ////

    // The closure upvalue is the heap-stable RuntimeState (raw pointer only at this Lua C
    // boundary); the scripts module owning the VM dies before the runtime it points at.
    static RuntimeState& bound_runtime(lua_State* lua)
    {
        return *static_cast<RuntimeState*>(lua_tolightuserdata(lua, lua_upvalueindex(1)));
    }

    static int sandbox_spawn(lua_State* lua)
    {
        ecs::Sandbox& sandbox = bound_runtime(lua).sandbox;
        const ecs::Toy toy = sandbox.spawn(luaL_checkstring(lua, 1));
        push_toy(lua, sandbox, toy.get_id());
        return 1;
    }

    static int sandbox_find(lua_State* lua)
    {
        ecs::Sandbox& sandbox = bound_runtime(lua).sandbox;
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
        const ToyUserdata& data = check_toy(lua, 1);
        data.sandbox->despawn(ecs::Toy(*data.sandbox, data.entity));
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
        const char* reference = luaL_checkstring(lua, 1);
        const Vec3 position = lua_istable(lua, 2) ? check_vector3(lua, 2) : Vec3(0.0f, 0.0f, 0.0f);
        const auto spawned =
            state.sandbox.spawn(state.assets, state.events, assets::AssetHandle<ecs::Kit>(reference), position);
        if (!spawned)
        {
            luaL_error(lua, "kit '%s': %s", reference, spawned.error().c_str());
            return 0;
        }
        lua_pushnumber(lua, static_cast<double>(spawned->id));
        return 1;
    }

    static int sandbox_despawn_kit(lua_State* lua)
    {
        ecs::Sandbox& sandbox = bound_runtime(lua).sandbox;
        sandbox.despawn(ecs::KitInstance {.id = static_cast<uint64>(luaL_checknumber(lua, 1))});
        return 0;
    }

    static int sandbox_stream(lua_State* lua)
    {
        RuntimeState& state = bound_runtime(lua);
        state.sandbox.stream(state.assets, state.events, state.jobs, check_vector3(lua, 1));
        return 0;
    }

    static int physics_raycast(lua_State* lua)
    {
        RuntimeState& state = bound_runtime(lua);
        const Vec3 origin = check_vector3(lua, 1);
        const Vec3 direction = check_vector3(lua, 2);
        const auto max_distance = static_cast<float>(luaL_optnumber(lua, 3, 1000.0));
        const auto hit = physics::raycast(state.physics, origin, direction, max_distance);
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
            lua, check_vector3(lua, 1) * static_cast<float>(luaL_checknumber(lua, 2)));
    }

    static int math_dot(lua_State* lua)
    {
        lua_pushnumber(lua, math::dot(check_vector3(lua, 1), check_vector3(lua, 2)));
        return 1;
    }

    static int math_cross(lua_State* lua)
    {
        return push_vector3(lua, math::cross(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int math_length(lua_State* lua)
    {
        lua_pushnumber(lua, math::length(check_vector3(lua, 1)));
        return 1;
    }

    static int math_distance(lua_State* lua)
    {
        lua_pushnumber(lua, math::distance(check_vector3(lua, 1), check_vector3(lua, 2)));
        return 1;
    }

    static int math_normalize(lua_State* lua)
    {
        return push_vector3(lua, math::normalize(check_vector3(lua, 1)));
    }

    static int math_lerp(lua_State* lua)
    {
        const auto t = static_cast<float>(luaL_checknumber(lua, 3));
        if (lua_istable(lua, 1))
            return push_vector3(lua, math::lerp(check_vector3(lua, 1), check_vector3(lua, 2), t));
        lua_pushnumber(
            lua,
            math::lerp(
                static_cast<float>(luaL_checknumber(lua, 1)),
                static_cast<float>(luaL_checknumber(lua, 2)),
                t));
        return 1;
    }

    static int math_move_toward(lua_State* lua)
    {
        return push_vector3(
            lua,
            math::move_toward(
                check_vector3(lua, 1),
                check_vector3(lua, 2),
                static_cast<float>(luaL_checknumber(lua, 3))));
    }

    static int math_reflect(lua_State* lua)
    {
        return push_vector3(lua, math::reflect(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int math_angle_axis(lua_State* lua)
    {
        return push_quat(
            lua,
            math::angle_axis(
                static_cast<float>(luaL_checknumber(lua, 1)), check_vector3(lua, 2)));
    }

    static int math_multiply(lua_State* lua)
    {
        return push_quat(lua, math::multiply(check_quat(lua, 1), check_quat(lua, 2)));
    }

    static int math_rotate(lua_State* lua)
    {
        return push_vector3(lua, math::rotate(check_quat(lua, 1), check_vector3(lua, 2)));
    }

    static int math_slerp(lua_State* lua)
    {
        return push_quat(
            lua,
            math::slerp(
                check_quat(lua, 1),
                check_quat(lua, 2),
                static_cast<float>(luaL_checknumber(lua, 3))));
    }

    static int math_from_euler(lua_State* lua)
    {
        return push_quat(lua, math::from_euler(check_vector3(lua, 1)));
    }

    static int math_to_euler(lua_State* lua)
    {
        return push_vector3(lua, math::to_euler(check_quat(lua, 1)));
    }

    static int math_quat_look_at(lua_State* lua)
    {
        return push_quat(lua, math::quat_look_at(check_vector3(lua, 1), check_vector3(lua, 2)));
    }

    static int ui_bind(lua_State* lua)
    {
        // tbx.ui.bind(hud, "kills", "kills"): the property on the left (hud.kills — Lua
        // cannot pass scalars by reference, so the table+key pair IS the property), the
        // target tag on the right. Scripts just mutate the table and the UI follows. The
        // table pins in the VM registry; the VM outlives the UI (ui::reset clears bindings
        // before scripts tear down).
        luaL_checktype(lua, 1, LUA_TTABLE);
        const auto key = std::string(luaL_checkstring(lua, 2));
        const auto name = std::string(luaL_checkstring(lua, 3));
        lua_pushvalue(lua, 1);
        const int table_ref = lua_ref(lua, -1);
        lua_pop(lua, 1);
        ui::bind(
            bound_runtime(lua).ui,
            {.name = name,
             .source =
                 [lua, table_ref, key]() -> std::string
             {
                 lua_getref(lua, table_ref);
                 lua_getfield(lua, -1, key.c_str());
                 auto value = std::string();
                 if (const char* text = lua_tostring(lua, -1))
                     value = text;
                 lua_pop(lua, 2);
                 return value;
             }});
        return 0;
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
        input::Key key;
    };

#define TBX_KEY_ENTRY(name) KeyEntry {#name, input::Key::name}
    static constexpr KeyEntry KEY_TABLE[] = {
        TBX_KEY_ENTRY(A), TBX_KEY_ENTRY(B), TBX_KEY_ENTRY(C), TBX_KEY_ENTRY(D),
        TBX_KEY_ENTRY(E), TBX_KEY_ENTRY(F), TBX_KEY_ENTRY(G), TBX_KEY_ENTRY(H),
        TBX_KEY_ENTRY(I), TBX_KEY_ENTRY(J), TBX_KEY_ENTRY(K), TBX_KEY_ENTRY(L),
        TBX_KEY_ENTRY(M), TBX_KEY_ENTRY(N), TBX_KEY_ENTRY(O), TBX_KEY_ENTRY(P),
        TBX_KEY_ENTRY(Q), TBX_KEY_ENTRY(R), TBX_KEY_ENTRY(S), TBX_KEY_ENTRY(T),
        TBX_KEY_ENTRY(U), TBX_KEY_ENTRY(V), TBX_KEY_ENTRY(W), TBX_KEY_ENTRY(X),
        TBX_KEY_ENTRY(Y), TBX_KEY_ENTRY(Z),
        TBX_KEY_ENTRY(NUM_0), TBX_KEY_ENTRY(NUM_1), TBX_KEY_ENTRY(NUM_2),
        TBX_KEY_ENTRY(NUM_3), TBX_KEY_ENTRY(NUM_4), TBX_KEY_ENTRY(NUM_5),
        TBX_KEY_ENTRY(NUM_6), TBX_KEY_ENTRY(NUM_7), TBX_KEY_ENTRY(NUM_8),
        TBX_KEY_ENTRY(NUM_9),
        TBX_KEY_ENTRY(F1), TBX_KEY_ENTRY(F2), TBX_KEY_ENTRY(F3), TBX_KEY_ENTRY(F4),
        TBX_KEY_ENTRY(F5), TBX_KEY_ENTRY(F6), TBX_KEY_ENTRY(F7), TBX_KEY_ENTRY(F8),
        TBX_KEY_ENTRY(F9), TBX_KEY_ENTRY(F10), TBX_KEY_ENTRY(F11), TBX_KEY_ENTRY(F12),
        TBX_KEY_ENTRY(ESCAPE), TBX_KEY_ENTRY(TAB), TBX_KEY_ENTRY(CAPS_LOCK),
        TBX_KEY_ENTRY(SPACE), TBX_KEY_ENTRY(ENTER), TBX_KEY_ENTRY(BACKSPACE),
        TBX_KEY_ENTRY(DEL), TBX_KEY_ENTRY(INSERT), TBX_KEY_ENTRY(HOME),
        TBX_KEY_ENTRY(END), TBX_KEY_ENTRY(PAGE_UP), TBX_KEY_ENTRY(PAGE_DOWN),
        TBX_KEY_ENTRY(LEFT), TBX_KEY_ENTRY(RIGHT), TBX_KEY_ENTRY(UP),
        TBX_KEY_ENTRY(DOWN), TBX_KEY_ENTRY(LEFT_SHIFT), TBX_KEY_ENTRY(RIGHT_SHIFT),
        TBX_KEY_ENTRY(LEFT_CTRL), TBX_KEY_ENTRY(RIGHT_CTRL), TBX_KEY_ENTRY(LEFT_ALT),
        TBX_KEY_ENTRY(RIGHT_ALT), TBX_KEY_ENTRY(MINUS), TBX_KEY_ENTRY(EQUALS),
        TBX_KEY_ENTRY(LEFT_BRACKET), TBX_KEY_ENTRY(RIGHT_BRACKET),
        TBX_KEY_ENTRY(BACKSLASH), TBX_KEY_ENTRY(SEMICOLON), TBX_KEY_ENTRY(APOSTROPHE),
        TBX_KEY_ENTRY(GRAVE), TBX_KEY_ENTRY(COMMA), TBX_KEY_ENTRY(PERIOD),
        TBX_KEY_ENTRY(SLASH)};
#undef TBX_KEY_ENTRY

    /// @brief
    /// Purpose: Input takes tbx.Key/tbx.MouseButton enum values — strongly typed, no strings.
    static input::Key check_key(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value <= 0 || value >= static_cast<int>(input::Key::COUNT))
            luaL_error(lua, "expected a tbx.Key value");
        return static_cast<input::Key>(value);
    }

    static input::MouseButton check_mouse_button(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(input::MouseButton::COUNT))
            luaL_error(lua, "expected a tbx.MouseButton value");
        return static_cast<input::MouseButton>(value);
    }

    static int input_is_down(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_down(bound_runtime(lua).input, check_key(lua, 1)));
        return 1;
    }

    static int input_is_pressed(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_pressed(bound_runtime(lua).input, check_key(lua, 1)));
        return 1;
    }

    static int input_is_mouse_down(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_mouse_down(bound_runtime(lua).input, check_mouse_button(lua, 1)));
        return 1;
    }

    static int input_is_mouse_pressed(lua_State* lua)
    {
        lua_pushboolean(lua, input::is_mouse_pressed(bound_runtime(lua).input, check_mouse_button(lua, 1)));
        return 1;
    }

    static int input_get_mouse_delta(lua_State* lua)
    {
        const Vec2 delta = input::get_mouse_delta(bound_runtime(lua).input);
        push_vector_table(lua, &delta.x, XYZW_KEYS, 2);
        return 1;
    }

    static input::CursorMode check_cursor_mode(lua_State* lua, const int index)
    {
        const auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value >= static_cast<int>(input::CursorMode::COUNT))
            luaL_error(lua, "expected a tbx.CursorMode value");
        return static_cast<input::CursorMode>(value);
    }

    static int input_get_cursor_mode(lua_State* lua)
    {
        lua_pushinteger(lua, static_cast<int>(input::get_cursor_mode(bound_runtime(lua).input)));
        return 1;
    }

    static int input_set_cursor_mode(lua_State* lua)
    {
        input::set_cursor_mode(bound_runtime(lua).input, check_cursor_mode(lua, 1));
        return 0;
    }

    //// OPEN ////

    void push_toy(lua_State* lua, ecs::Sandbox& sandbox, const ecs::ToyId entity)
    {
        auto* data = static_cast<ToyUserdata*>(lua_newuserdata(lua, sizeof(ToyUserdata)));
        *data = ToyUserdata {.sandbox = &sandbox, .entity = entity};
        luaL_getmetatable(lua, TOY_METATABLE);
        lua_setmetatable(lua, -2);
    }

    /// @brief
    /// Purpose: Registers one tbx.* binding on the table at the top of the stack — the
    /// closure carries the runtime as its light-userdata upvalue.
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
        // ecs::Toy metatable: __index is a closure over the method table so unknown keys fall
        // through to typed block lookup; __newindex is add-and-populate.
        luaL_newmetatable(lua, TOY_METATABLE);
        lua_createtable(lua, 0, 8);
        const luaL_Reg toy_methods[] = {
            {"get_name", toy_get_name},
            {"set_name", toy_set_name},
            {"sticker", toy_sticker},
            {"has_sticker", toy_has_sticker},
            {"remove_sticker", toy_remove_sticker},
            {"is_alive", toy_is_alive},
            {nullptr, nullptr}};
        luaL_register(lua, nullptr, toy_methods);
        lua_pushcclosure(lua, toy_index, "toy_index", 1);
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, toy_newindex, "toy_newindex");
        lua_setfield(lua, -2, "__newindex");
        lua_pop(lua, 1);

        // ecs::Block metatable: field access straight through reflection::TypeInfo.
        luaL_newmetatable(lua, BLOCK_METATABLE);
        lua_pushcfunction(lua, block_index, "block_index");
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, block_newindex, "block_newindex");
        lua_setfield(lua, -2, "__newindex");
        lua_pop(lua, 1);

        // Global tbx table.
        lua_createtable(lua, 0, 5);

        lua_createtable(lua, 0, 6);
        register_runtime_closure(lua, runtime, sandbox_spawn, "sandbox_spawn", "spawn");
        register_runtime_closure(lua, runtime, sandbox_find, "sandbox_find", "find");
        lua_pushcfunction(lua, sandbox_despawn, "sandbox_despawn");
        lua_setfield(lua, -2, "despawn");
        register_runtime_closure(lua, runtime, sandbox_spawn_kit, "sandbox_spawn_kit", "spawn_kit");
        register_runtime_closure(
            lua, runtime, sandbox_despawn_kit, "sandbox_despawn_kit", "despawn_kit");
        register_runtime_closure(lua, runtime, sandbox_stream, "sandbox_stream", "stream");
        lua_setfield(lua, -2, "sandbox");

        lua_createtable(lua, 0, 5);
        register_runtime_closure(lua, runtime, input_is_down, "input_is_down", "is_down");
        register_runtime_closure(lua, runtime, input_is_pressed, "input_is_pressed", "is_pressed");
        register_runtime_closure(
            lua, runtime, input_is_mouse_down, "input_is_mouse_down", "is_mouse_down");
        register_runtime_closure(
            lua, runtime, input_is_mouse_pressed, "input_is_mouse_pressed", "is_mouse_pressed");
        register_runtime_closure(
            lua, runtime, input_get_mouse_delta, "input_get_mouse_delta", "get_mouse_delta");
        register_runtime_closure(
            lua, runtime, input_get_cursor_mode, "input_get_cursor_mode", "get_cursor_mode");
        register_runtime_closure(
            lua, runtime, input_set_cursor_mode, "input_set_cursor_mode", "set_cursor_mode");
        lua_setfield(lua, -2, "input");

        lua_createtable(lua, 0, 1);
        register_runtime_closure(lua, runtime, physics_raycast, "physics_raycast", "raycast");
        lua_setfield(lua, -2, "physics");

        lua_createtable(lua, 0, 2);
        register_runtime_closure(lua, runtime, ui_bind, "ui_bind", "bind");
        register_runtime_closure(lua, runtime, ui_set_string, "ui_set_string", "set_string");
        lua_setfield(lua, -2, "ui");

        // Strongly typed input enums: tbx.Key.W, tbx.MouseButton.LEFT.
        lua_createtable(lua, 0, static_cast<int>(std::size(KEY_TABLE)));
        for (const KeyEntry& entry : KEY_TABLE)
        {
            lua_pushinteger(lua, static_cast<int>(entry.key));
            lua_setfield(lua, -2, entry.name);
        }
        lua_setfield(lua, -2, "Key");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, static_cast<int>(input::MouseButton::LEFT));
        lua_setfield(lua, -2, "LEFT");
        lua_pushinteger(lua, static_cast<int>(input::MouseButton::RIGHT));
        lua_setfield(lua, -2, "RIGHT");
        lua_pushinteger(lua, static_cast<int>(input::MouseButton::MIDDLE));
        lua_setfield(lua, -2, "MIDDLE");
        lua_setfield(lua, -2, "MouseButton");

        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, static_cast<int>(input::CursorMode::NORMAL));
        lua_setfield(lua, -2, "NORMAL");
        lua_pushinteger(lua, static_cast<int>(input::CursorMode::HIDDEN));
        lua_setfield(lua, -2, "HIDDEN");
        lua_pushinteger(lua, static_cast<int>(input::CursorMode::LOCKED));
        lua_setfield(lua, -2, "LOCKED");
        lua_setfield(lua, -2, "CursorMode");

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
            {"move_toward", math_move_toward},
            {"reflect", math_reflect},
            {"angle_axis", math_angle_axis},
            {"multiply", math_multiply},
            {"rotate", math_rotate},
            {"slerp", math_slerp},
            {"from_euler", math_from_euler},
            {"to_euler", math_to_euler},
            {"quat_look_at", math_quat_look_at},
            {nullptr, nullptr}};
        luaL_register(lua, nullptr, math_functions);
        lua_setfield(lua, -2, "math");

        register_runtime_closure(lua, runtime, tbx_quit, "tbx_quit", "quit");

        lua_setglobal(lua, "tbx");
    }
}
