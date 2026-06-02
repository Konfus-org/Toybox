#pragma once
#include "tbx/systems/assets/messages.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    class IMessageCoordinator;

    /// @brief
    /// Purpose: Describes one frame-safe reload reaction for the changed asset.
    /// @details
    /// Ownership: Value type copied to reload handlers.
    struct TBX_API AssetReloadContext
    {
        Handle affected_asset = {};
        bool succeeded = false;
        uint64 revision = 0U;
        std::string report = {};
    };

    using AssetReloadHandler = std::function<void(const AssetReloadContext&)>;

    /// @brief
    /// Purpose: Queues asset reload events and dispatches frame-safe reload reactions.
    /// @details
    /// Ownership: Owns registered handlers. Thread Safety: Not inherently thread-safe; flush and
    /// handler registration are expected on the main thread.
    class TBX_API AssetReloadQueue final
    {
      public:
        AssetReloadQueue(std::weak_ptr<IMessageCoordinator> message_coordinator = {});
        ~AssetReloadQueue() noexcept;

      public:
        AssetReloadQueue(const AssetReloadQueue&) = delete;
        AssetReloadQueue& operator=(const AssetReloadQueue&) = delete;
        AssetReloadQueue(AssetReloadQueue&&) noexcept = delete;
        AssetReloadQueue& operator=(AssetReloadQueue&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Removes a reload handler by token.
        void deregister_handler(Uuid token);

        /// @brief
        /// Purpose: Dispatches pending reload reactions and clears the queue.
        void flush();

        /// @brief
        /// Purpose: Adds a reload event to be processed on the next flush.
        void push(const AssetReloadedEvent& event);

        /// @brief
        /// Purpose: Registers a handler invoked for each flushed reload reaction.
        Uuid register_handler(AssetReloadHandler handler);

      private:
        struct State;
        std::unique_ptr<State> _state = {};
    };
}
