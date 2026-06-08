#include "tbx/systems/assets/messages.h"
#include "tbx/systems/messaging/message_coordinator.h"

namespace tbx::tests::assets
{
    TEST(asset_reload_events, posted_reload_event_reaches_handlers_once)
    {
        MessageCoordinator coordinator;
        const auto asset = Uuid(0x30U);
        auto delivered = 0U;
        auto last_event = AssetReloadedEvent();
        coordinator.register_handler(
            [&delivered, &last_event](Message& message)
            {
                if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                {
                    ++delivered;
                    last_event = reloaded->get();
                }
            });

        coordinator.post<AssetReloadedEvent>(Handle(asset), true, 7U, "ok");
        coordinator.flush();
        coordinator.flush();

        ASSERT_EQ(delivered, 1U);
        EXPECT_EQ(last_event.affected_asset.id, asset);
        EXPECT_TRUE(last_event.succeeded);
        EXPECT_EQ(last_event.revision, 7U);
        EXPECT_EQ(last_event.report, "ok");
    }

    TEST(asset_reload_events, failed_reload_event_reaches_handlers)
    {
        MessageCoordinator coordinator;
        const auto asset = Uuid(0x40U);
        auto delivered = 0U;
        auto last_event = AssetReloadedEvent();
        coordinator.register_handler(
            [&delivered, &last_event](Message& message)
            {
                if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                {
                    ++delivered;
                    last_event = reloaded->get();
                }
            });

        coordinator.post<AssetReloadedEvent>(Handle(asset), false, 3U, "parse failed");
        coordinator.flush();

        ASSERT_EQ(delivered, 1U);
        EXPECT_EQ(last_event.affected_asset.id, asset);
        EXPECT_FALSE(last_event.succeeded);
        EXPECT_EQ(last_event.revision, 3U);
        EXPECT_EQ(last_event.report, "parse failed");
    }
}
