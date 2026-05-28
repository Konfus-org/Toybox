#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/world.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Streams world chunks around active cameras with separate visual and simulation
    /// radii.
    class TBX_API EntityStreamer final
    {
      public:
        EntityStreamer(std::weak_ptr<AssetManager> asset_manager);
        ~EntityStreamer() noexcept;

      public:
        EntityStreamer(const EntityStreamer&) = delete;
        EntityStreamer& operator=(const EntityStreamer&) = delete;
        EntityStreamer(EntityStreamer&&) noexcept = delete;
        EntityStreamer& operator=(EntityStreamer&&) noexcept = delete;

      public:
        void update(const DeltaTime& dt);

      private:
        void update_world(AssetManager& asset_manager, World& world);

      private:
        struct Impl;
        std::unique_ptr<Impl> _impl = {};
    };
}
