#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/pipeline/context/frame_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Snapshots ECS renderables and turns them into backend-neutral frame commands.
    /// @details
    /// Ownership: Borrows services and writes value-owned FrameData for a single render frame.
    /// Thread Safety: Call on the render lane while the resource manager is owned by that lane.
    class TBX_API FrameDataFactory final
    {
      public:
        FrameDataFactory(
            std::weak_ptr<EntityRegistry> entity_registry,
            std::weak_ptr<IWindowManager> window_manager
            const GraphicsSettings& settings);

        Result create(
            GraphicsResourceManager& resource_manager,
            uint64 frame_index,
            FrameData& out_frame_data) const;

      private:
        std::weak_ptr<EntityRegistry> _entity_registry = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        Size _configured_resolution = {};
    };
}
