#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/ecs/entity.h"
#include "tbx/input/input_manager.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct AppSettings;

    /// @brief
    /// Purpose: Defines the host interface provided to runtime plugins.
    /// @details
    /// Ownership: Implementations retain ownership of returned references.
    /// Thread Safety: Not thread-safe; expected to be used on the main thread.

    class TBX_API IPluginHost
    {
      public:
        virtual ~IPluginHost() noexcept = default;

        /// @brief
        /// Purpose: Returns the host name.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual const std::string& get_name() const = 0;

        /// @brief
        /// Purpose: Returns the immutable startup icon handle for native windows.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual const Handle& get_icon_handle() const = 0;

        /// @brief
        /// Purpose: Returns the host settings instance.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual AppSettings& get_settings() = 0;

        /// @brief
        /// Purpose: Returns the host message coordinator.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual IMessageCoordinator& get_message_coordinator() = 0;

        /// @brief
        /// Purpose: Returns the host input manager.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual InputManager& get_input_manager() = 0;

        /// @brief
        /// Purpose: Returns the host entity manager.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual EntityRegistry& get_entity_registry() = 0;

        /// @brief
        /// Purpose: Returns the host asset manager.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        virtual AssetManager& get_asset_manager() = 0;

        /// @brief
        /// Purpose: Returns the host job manager for scheduling asynchronous work.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Thread-safe according to JobSystem guarantees.

        virtual JobSystem& get_job_system() = 0;

        /// @brief
        /// Purpose: Returns the host thread manager for dedicated lane scheduling.
        /// @details
        /// Ownership: Returns a reference owned by the host.
        /// Thread Safety: Thread-safe according to ThreadManager guarantees.

        virtual ThreadManager& get_thread_manager() = 0;
    };
}
