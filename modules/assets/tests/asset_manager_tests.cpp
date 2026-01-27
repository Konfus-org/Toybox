#include "pch.h"
#include "test_filesystem.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/result.h"
#include <future>

namespace tbx
{
    struct TestAsset
    {
        int value = 0;
    };

    template <>
    struct AssetAsyncLoader<TestAsset>
    {
        static AssetPromise<TestAsset> load(const std::filesystem::path&)
        {
            AssetPromise<TestAsset> promise = {};
            promise.asset = std::make_shared<TestAsset>();
            Result result;
            result.flag_success();
            std::promise<Result> completion;
            promise.promise = completion.get_future().share();
            completion.set_value(result);
            return promise;
        }
    };
}

namespace tbx::tests::assets
{
    TEST(asset_manager, resolves_handle_by_path_and_id)
    {
        TestFileSystem file_system;
        file_system.working_directory = "virtual";
        file_system.assets_directory = "virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("virtual/assets/stone.asset", "");
        file_system.add_file(
            "virtual/assets/stone.asset.meta",
            R"({ "id": "2f" })");

        AssetManager manager(file_system);
        AssetHandle path_handle(std::filesystem::path("stone.asset"));
        auto asset = manager.request<TestAsset>(path_handle);

        ASSERT_NE(asset, nullptr);
        asset->value = 42;

        AssetHandle id_handle(Uuid(0x2fU));
        auto by_id = manager.get<TestAsset>(id_handle);
        ASSERT_NE(by_id, nullptr);
        EXPECT_EQ(by_id->value, 42);

        AssetUsage usage = manager.get_usage<TestAsset>(id_handle);
        EXPECT_EQ(usage.ref_count, 1U);
    }

    TEST(asset_manager, streams_out_unreferenced_assets)
    {
        TestFileSystem file_system;
        file_system.working_directory = "virtual";
        file_system.assets_directory = "virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("virtual/assets/crate.asset", "");
        file_system.add_file(
            "virtual/assets/crate.asset.meta",
            R"({ "id": "30" })");

        AssetManager manager(file_system);
        AssetHandle handle(std::filesystem::path("crate.asset"));
        auto asset = manager.request<TestAsset>(handle);

        ASSERT_NE(asset, nullptr);
        asset.reset();

        EXPECT_TRUE(manager.stream_out<TestAsset>(handle));
    }
}
