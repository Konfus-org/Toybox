#pragma once
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    class World;
    class WorldManager;

    /// @brief Applies every property connection in a world once: each connection copies its source
    /// entity's component property value onto its owner's target property, by serializing the source
    /// property and applying it to the target. One pass in entity order — a cycle settles over
    /// successive ticks rather than erroring, Dreams-style. A connection whose source entity or
    /// property is gone is skipped (the editor renders it broken). Exposed for tests; the system below
    /// is the per-frame driver.
    TBX_API void evaluate_property_connections(World& world);

    /// @brief
    /// Purpose: The per-frame driver of the property connections — the editor's value wires: each
    /// tick, before scripts run (so scripts observe driven values), every connection copies its
    /// source property onto its target.
    /// @details
    /// Runs in edit mode too (wiring gives immediate feedback), not just while playing — connection
    /// data is world content, so a shipped game evaluates identically. Main-thread only.
    class TBX_API PropertyConnectionSystem
    {
      public:
        explicit PropertyConnectionSystem(std::weak_ptr<WorldManager> world_manager);

        void update();

      private:
        std::weak_ptr<WorldManager> _world_manager;
    };
}
