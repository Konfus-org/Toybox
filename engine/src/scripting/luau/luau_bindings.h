#pragma once
#include "tbx/ecs/sandbox.h"
#include "tbx/runtime.h"
#include <lua.h>

// Private glue between the Luau VM and the engine. Bindings are generic over reflection::TypeInfo — never
// per-type code — plus a thin hand-written tbx.* service API. Other language backends
// (scripting/csharp/...) follow the same rule inside their own folders.
namespace tbx::scripts
{
    /// @brief
    /// Purpose: Installs the global `tbx` table (sandbox/input) and the ecs::Toy/ecs::Block metatables;
    /// service closures capture the runtime as their upvalue.
    void open_tbx_bindings(lua_State* lua, RuntimeState& runtime);

    /// @brief
    /// Purpose: Pushes a ecs::Toy userdata for the given entity onto the Lua stack.
    void push_toy(lua_State* lua, ecs::Sandbox& sandbox, ecs::ToyId entity);
}
