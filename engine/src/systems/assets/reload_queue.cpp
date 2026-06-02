#include "tbx/systems/assets/reload_queue.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include <algorithm>
#include <memory>
#include <utility>

namespace tbx
{
    //// INTERNAL ////

    struct RegisteredAssetReloadHandler
    {
        Uuid token = {};
        AssetReloadHandler handler = {};
    };

    struct AssetReloadQueue::State
    {
        State(std::weak_ptr<IMessageCoordinator> coordinator)
            : message_coordinator(std::move(coordinator))
        {
        }

        std::weak_ptr<IMessageCoordinator> message_coordinator = {};
        Uuid message_handler = {};
        std::vector<AssetReloadedEvent> pending = {};
        std::vector<RegisteredAssetReloadHandler> handlers = {};
    };

    static AssetReloadContext make_reload_context(const AssetReloadedEvent& event)
    {
        return AssetReloadContext {
            .affected_asset = event.affected_asset,
            .succeeded = event.succeeded,
            .revision = event.revision,
            .report = event.report,
        };
    }

    //// ASSET RELOAD QUEUE IMPL ////

    AssetReloadQueue::AssetReloadQueue(std::weak_ptr<IMessageCoordinator> message_coordinator)
        : _state(std::make_unique<State>(std::move(message_coordinator)))
    {
        if (const auto coordinator = _state->message_coordinator.lock())
        {
            _state->message_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                    {
                        push(reloaded->get());
                    }
                });
        }
    }

    AssetReloadQueue::~AssetReloadQueue() noexcept
    {
        if (!_state || !_state->message_handler.is_valid())
            return;

        if (const auto coordinator = _state->message_coordinator.lock())
            coordinator->deregister_handler(_state->message_handler);
    }

    void AssetReloadQueue::deregister_handler(const Uuid token)
    {
        std::erase_if(
            _state->handlers,
            [token](const RegisteredAssetReloadHandler& entry)
            {
                return entry.token == token;
            });
    }

    void AssetReloadQueue::flush()
    {
        auto pending = std::vector<AssetReloadedEvent> {};
        pending.swap(_state->pending);

        for (const auto& event : pending)
        {
            const auto context = make_reload_context(event);
            for (const auto& entry : _state->handlers)
            {
                if (entry.handler)
                    entry.handler(context);
            }
        }
    }

    void AssetReloadQueue::push(const AssetReloadedEvent& event)
    {
        if (!event.affected_asset.id.is_valid())
            return;

        _state->pending.push_back(event);
    }

    Uuid AssetReloadQueue::register_handler(AssetReloadHandler handler)
    {
        if (!handler)
            return {};

        auto token = Uuid::generate();
        _state->handlers.push_back(
            RegisteredAssetReloadHandler {
                .token = token,
                .handler = std::move(handler),
            });
        return token;
    }
}
