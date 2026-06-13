#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/script_container.generated.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes one script asset bound to an entity.
    /// @details
    /// Ownership: Holds serialized binding data only; runtime script instances live in
    /// ScriptSystem.
    [[serializable]];
    struct TBX_API ScriptContainerBinding
    {
        [[prop]]
        [[description("The script asset this binding runs.")]]
        Handle script = {};

        [[prop]]
        bool enabled = true;

        [[prop]]
        [[readonly]]
        [[hidden]]
        Uuid binding_id = {};

        [[prop]]
        Json overrides = Json::object();
    };

    /// @brief
    /// Purpose: Stores script asset bindings attached to an entity.
    [[serializable]];
    [[icon("ScrollText", Color::GREEN)]];
    struct TBX_API ScriptContainer : Component
    {
        [[prop]]
        std::vector<ScriptContainerBinding> scripts = {};
    };
}
