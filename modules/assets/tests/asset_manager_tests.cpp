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

    struct TestAssetLoaderState
    {
        bool use_async = false;
        std::shared_ptr<TestAsset> asset;
        std::shared_ptr<std::promise<Result>> completion;
    };

    static TestAssetLoaderState& get_test_asset_loader_state()
    {
        static TestAssetLoaderState state = {};
        return state;
    }

    static void reset_test_asset_loader_state()
    {
        auto& state = get_test_asset_loader_state();
        state.use_async = false;
        state.asset.reset();
        state.completion.reset();
    }

    template <>
    struct AssetAsyncLoader<TestAsset>
    {
        static AssetPromise<TestAsset> load(const std::filesystem::path&)
        {
            auto& state = get_test_asset_loader_state();
            AssetPromise<TestAsset> promise = {};
            promise.asset = std::make_shared<TestAsset>();

            if (!state.use_async)
            {
                Result result;
                result.flag_success();
                std::promise<Result> completion;
                promise.promise = completion.get_future().share();
                completion.set_value(result);
                return promise;
            }

            state.asset = promise.asset;
            state.completion = std::make_shared<std::promise<Result>>();
            promise.promise = state.completion->get_future().share();
            return promise;
        }
    };
}

namespace tbx::tests::assets
{
    TEST(asset_manager, resolves_handle_by_path)
    {
        TestFileSystem file_system;
        file_system.assets_directory = "/virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("/virtual/assets/stone.asset", "");
        file_system.add_file(
            "/virtual/assets/stone.asset.meta",
            R"({ "id": "2f" })");

        reset_test_asset_loader_state();
        AssetManager manager(file_system);
        AssetHandle path_handle(std::filesystem::path("stone.asset"));
        auto asset = manager.request<TestAsset>(path_handle);

        ASSERT_NE(asset, nullptr);
        asset->value = 42;

        auto by_path = manager.get<TestAsset>(path_handle);
        ASSERT_NE(by_path, nullptr);
        EXPECT_EQ(by_path->value, 42);

        AssetUsage usage = manager.get_usage<TestAsset>(path_handle);
        EXPECT_EQ(usage.ref_count, 2U);
    }

    TEST(asset_manager, resolves_handle_by_id)
    {
        TestFileSystem file_system;
        file_system.assets_directory = "/virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("/virtual/assets/ore.asset", "");
        file_system.add_file(
            "/virtual/assets/ore.asset.meta",
            R"({ "id": "2f" })");

        reset_test_asset_loader_state();
        AssetManager manager(file_system);
        AssetHandle path_handle(std::filesystem::path("ore.asset"));
        auto asset = manager.request<TestAsset>(path_handle);

        ASSERT_NE(asset, nullptr);
        asset->value = 84;

        AssetHandle id_handle(Uuid(0x2fU));
        auto by_id = manager.get<TestAsset>(id_handle);
        ASSERT_NE(by_id, nullptr);
        EXPECT_EQ(by_id->value, 84);

        AssetUsage usage = manager.get_usage<TestAsset>(id_handle);
        EXPECT_EQ(usage.ref_count, 2U);
    }

    TEST(asset_manager, supports_stream_in_out_and_pin)
    {
        TestFileSystem file_system;
        file_system.assets_directory = "/virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("/virtual/assets/crate.asset", "");
        file_system.add_file(
            "/virtual/assets/crate.asset.meta",
            R"({ "id": "30" })");

        reset_test_asset_loader_state();
        AssetManager manager(file_system);
        AssetHandle handle(std::filesystem::path("crate.asset"));
        auto streamed_in = manager.stream_in<TestAsset>(handle);

        ASSERT_NE(streamed_in, nullptr);
        manager.set_pinned<TestAsset>(handle, true);
        EXPECT_FALSE(manager.stream_out<TestAsset>(handle));

        manager.set_pinned<TestAsset>(handle, false);
        streamed_in.reset();

        EXPECT_TRUE(manager.stream_out<TestAsset>(handle));
    }

    TEST(asset_manager, streams_in_async_and_updates_payload)
    {
        TestFileSystem file_system;
        file_system.assets_directory = "/virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file("/virtual/assets/async.asset", "");
        file_system.add_file(
            "/virtual/assets/async.asset.meta",
            R"({ "id": "40" })");

        reset_test_asset_loader_state();
        auto& loader_state = get_test_asset_loader_state();
        loader_state.use_async = true;

        AssetManager manager(file_system);
        AssetHandle handle(std::filesystem::path("async.asset"));
        auto streamed_in = manager.stream_in<TestAsset>(handle);

        ASSERT_NE(streamed_in, nullptr);
        EXPECT_EQ(streamed_in->value, 0);

        loader_state.asset->value = 99;
        Result result;
        result.flag_success();
        loader_state.completion->set_value(result);

        AssetUsage usage = manager.get_usage<TestAsset>(handle);
        EXPECT_EQ(usage.stream_state, AssetStreamState::Loaded);
        EXPECT_EQ(streamed_in->value, 99);
    }
}
