#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entities.h"
#include "tbx/files/filesystem.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct AppSettings;

    /// <summary>
    /// Purpose: Defines the host interface provided to runtime plugins.
    /// </summary>
    /// <remarks>
    /// Ownership: Implementations retain ownership of returned references.
    /// Thread Safety: Not thread-safe; expected to be used on the main thread.
    /// </remarks>
    class TBX_API IPluginHost
    {
      public:
        virtual ~IPluginHost() noexcept = default;

        /// <summary>
        /// Purpose: Returns the host name.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual const std::string& get_name() const = 0;

        /// <summary>
        /// Purpose: Returns the host settings instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual AppSettings& get_settings() = 0;

        /// <summary>
        /// Purpose: Returns the host message coordinator.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual IMessageCoordinator& get_message_coordinator() = 0;

        /// <summary>
        /// Purpose: Returns the host filesystem service.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual IFileSystem& get_filesystem() = 0;

        /// <summary>
        /// Purpose: Returns the host entity manager.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual EntityRegistry& get_entity_registry() = 0;

        /// <summary>
        /// Purpose: Returns the host asset manager.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        virtual AssetManager& get_asset_manager() = 0;
    };
}
