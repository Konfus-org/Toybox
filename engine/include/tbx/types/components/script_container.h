#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/components/component.h"
#include "tbx/types/uuid.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes one script asset bound to an entity.
    /// @details
    /// Ownership: Holds serialized binding data only; runtime script instances live in
    /// ScriptSystem.
    [[tbx::serializable]];
    struct TBX_API ScriptBinding
    {
        [[tbx::prop]]
        Uuid script = {};

        [[tbx::prop]]
        bool enabled = true;

        [[tbx::prop]]
        Uuid binding_id = {};

        [[tbx::prop]]
        Json overrides = Json::object();
    };

    /// @brief
    /// Purpose: Stores script asset bindings attached to an entity.
    [[tbx::serializable]];
    struct TBX_API ScriptContainer : Component
    {
        [[tbx::prop]]
        std::vector<ScriptBinding> scripts = {};
    };
}

#include "tbx/types/components/script_container.generated.h"
