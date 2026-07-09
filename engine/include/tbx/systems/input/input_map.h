#pragma once
#include "tbx/systems/input/input_map.generated.h"
#include "tbx/systems/input/scheme.h"
#include "tbx/types/assets/asset.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores input schemes as a standalone asset so keybindings are data instead of
    /// code. Apps reference maps from AppSettings::input_maps and the application feeds their
    /// schemes into the InputManager at startup; editors edit the same asset to rebind actions.
    /// @details
    /// Ownership: Owns its schemes by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API InputMap : Asset
    {
        std::vector<InputScheme> schemes = {};
    };
}
