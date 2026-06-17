#pragma once
#include "tbx/types/uuid.h"
#include "systems/scripting/script_system_state_key.generated.h"

namespace tbx
{
    [[hash(world, entity, script, binding_id)]];
    struct ScriptSystemStateKey
    {
        Uuid world = {};
        Uuid entity = {};
        Uuid script = {};
        Uuid binding_id = {};
    };
}
