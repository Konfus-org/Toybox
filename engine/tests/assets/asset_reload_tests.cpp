#include "tbx/systems/assets/reload_queue.h"
#include <vector>

namespace tbx::tests::assets
{
    TEST(asset_reload_queue, dispatches_direct_reload_once_per_flush)
    {
        AssetReloadQueue queue;
        const auto asset = Uuid(0x30U);
        auto contexts = std::vector<AssetReloadContext>();
        queue.register_handler(
            [&contexts](const AssetReloadContext& context)
            {
                contexts.push_back(context);
            });

        queue.push(AssetReloadedEvent(Handle(asset), true, 7U, "ok"));
        queue.flush();
        queue.flush();

        ASSERT_EQ(contexts.size(), 1U);
        EXPECT_EQ(contexts[0].affected_asset.id, asset);
        EXPECT_TRUE(contexts[0].succeeded);
        EXPECT_EQ(contexts[0].revision, 7U);
        EXPECT_EQ(contexts[0].report, "ok");
    }

    TEST(asset_reload_queue, failed_reload_reaches_handlers)
    {
        AssetReloadQueue queue;
        const auto asset = Uuid(0x40U);
        auto contexts = std::vector<AssetReloadContext>();
        queue.register_handler(
            [&contexts](const AssetReloadContext& context)
            {
                contexts.push_back(context);
            });

        queue.push(AssetReloadedEvent(Handle(asset), false, 3U, "parse failed"));
        queue.flush();

        ASSERT_EQ(contexts.size(), 1U);
        EXPECT_EQ(contexts[0].affected_asset.id, asset);
        EXPECT_FALSE(contexts[0].succeeded);
        EXPECT_EQ(contexts[0].revision, 3U);
        EXPECT_EQ(contexts[0].report, "parse failed");
    }
}
