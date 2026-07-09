#include "sync_event_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "sync_event_state.h"
#include "wire.h"
#include <algorithm>
#include <string>

namespace tbx::studio_bridge
{
    Result subscribe_sync_event(SyncEventState& events, const tbx::Json& params)
    {
        const auto address = params.value(Wire::ADDRESS, std::string());
        const auto key = params.value(Wire::KEY, std::string());
        if (address.empty() || key.empty())
            return Result(false, "sync.subscribe: missing 'address' or 'key'.");

        const auto exists = std::ranges::any_of(
            events.subscriptions,
            [&](const EventSubscription& row) { return row.address == address && row.key == key; });
        if (exists)
            return Result::OK;

        events.subscriptions.push_back(
            EventSubscription {
                .address = address,
                .key = key,
                .entity_id = parse_address_entity(address),
            });
        return Result::OK;
    }

    Result unsubscribe_sync_event(SyncEventState& events, const tbx::Json& params)
    {
        const auto address = params.value(Wire::ADDRESS, std::string());
        const auto key = params.value(Wire::KEY, std::string());
        std::erase_if(
            events.subscriptions,
            [&](const EventSubscription& row) { return row.address == address && row.key == key; });
        return Result::OK;
    }

    void raise_sync_event_for_entity(
        const SyncEventState& events,
        const EngineServices& services,
        uint64 entity_id,
        std::string_view key,
        const tbx::Json& args)
    {
        const auto host = services.rpc_host.lock();
        if (!host)
            return;

        for (const auto& subscription : events.subscriptions)
        {
            if (subscription.entity_id != entity_id || subscription.key != key)
                continue;

            auto params = tbx::Json::object();
            params[Wire::ADDRESS] = subscription.address;
            params[Wire::KEY] = subscription.key;
            params[Wire::ARGS] = args;
            host->send_notification(Wire::SYNC_EVENT, params);
        }
    }
}
