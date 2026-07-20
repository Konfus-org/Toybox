#pragma once
#include "tbx/ecs/sandbox.h"
#include <lua.h>

// Private glue between the Luau VM and the engine. Bindings are generic over TypeInfo — never
// per-type code — plus a thin hand-written tbx.* service API. Future language surfaces
// (bindings/csharp/...) follow the same rule.
namespace tbx
{
    /// @brief
    /// Purpose: Installs the global `tbx` table (sandbox/input) and the Toy/Block metatables.
    void open_tbx_bindings(lua_State* lua, Sandbox& sandbox);

    /// @brief
    /// Purpose: Pushes a Toy userdata for the given entity onto the Lua stack.
    void push_toy(lua_State* lua, Sandbox& sandbox, entt::entity entity);
}
