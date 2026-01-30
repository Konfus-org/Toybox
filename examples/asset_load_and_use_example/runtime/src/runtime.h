#pragma once
#include "tbx/ecs/entities.h"
#include "tbx/graphics/texture.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>

namespace tbx::examples
{
    /// <summary>
    /// Runtime plugin that loads a texture asset and applies it to a quad model.
    /// </summary>
    /// <remarks>
    /// Purpose: Drives the asset load example logic.
    /// Ownership: Owns only transient pointers to ECS and shared texture data.
    /// Thread Safety: Expects to run on the main thread where the ECS is updated.
    /// </remarks>
    class AssetLoadAndUseExampleRuntimePlugin final : public Plugin
    {
      public:
        /// <summary>
        /// Initializes the example entities and loads the Smily texture.
        /// </summary>
        /// <remarks>
        /// Purpose: Creates the quad entity and loads its texture.
        /// Ownership: Does not take ownership of the host Application.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_attach(IPluginHost& host) override;

        /// <summary>
        /// Releases cached example data.
        /// </summary>
        /// <remarks>
        /// Purpose: Clears cached pointers and shared assets.
        /// Ownership: Releases any owned shared texture references.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Updates the example simulation.
        /// </summary>
        /// <remarks>
        /// Purpose: Keeps the plugin up to date each frame.
        /// Ownership: Does not own the provided DeltaTime instance.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_update(const DeltaTime& dt) override;

        /// <summary>
        /// Handles incoming messages routed through the dispatcher.
        /// </summary>
        /// <remarks>
        /// Purpose: Responds to runtime messages if needed.
        /// Ownership: Does not take ownership of the message.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_recieve_message(Message& msg) override;

      private:
        EntityManager* _entity_manager = nullptr;
        std::shared_ptr<Texture> _smily_texture = {};
    };
}
