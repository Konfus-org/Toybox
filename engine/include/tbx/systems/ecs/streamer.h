#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/world.h"

namespace tbx
{
    struct AppSettings;
    struct Message;

    /// @brief
    /// Purpose: Streams world chunks around active cameras with separate visual and simulation
    /// radii.
    class TBX_API EntityStreamer final
    {
      public:
        EntityStreamer(
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<AppSettings> settings = {});
        ~EntityStreamer() noexcept;

      public:
        EntityStreamer(const EntityStreamer&) = delete;
        EntityStreamer& operator=(const EntityStreamer&) = delete;
        EntityStreamer(EntityStreamer&&) noexcept = delete;
        EntityStreamer& operator=(EntityStreamer&&) noexcept = delete;

      public:
        void update(const DeltaTime& dt);
        void receive_message(Message& msg);

      private:
        void update_world(
            AssetManager& asset_manager,
            World& world,
            float chunk_size,
            uint32 unload_radius_chunks);

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        WorldSettings _settings = {};

        struct State;
        std::unique_ptr<State> _state = {};
    };
}
