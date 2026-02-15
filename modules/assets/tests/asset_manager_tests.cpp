#include "tbx/assets/asset_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/result.h"
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>

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
    struct AssetLoader<TestAsset>
    {
        static AssetPromise<TestAsset> load_async(const std::filesystem::path&)
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

        static std::shared_ptr<TestAsset> load(const std::filesystem::path&)
        {
            return std::make_shared<TestAsset>();
        }
    };
}

namespace tbx::tests::assets
{
    struct InMemoryHandleSource final
    {
        std::unordered_map<std::string, Uuid> id_by_path = {};

        void add(const std::filesystem::path& asset_path, Uuid id)
        {
            auto key = asset_path.lexically_normal().generic_string();
            id_by_path.emplace(key, id);
        }

        bool try_get(const std::filesystem::path& asset_path, Handle& out_handle) const
        {
            auto key = asset_path.lexically_normal().generic_string();
            auto iterator = id_by_path.find(key);
            if (iterator == id_by_path.end())
                return false;

            auto handle = Handle(asset_path.lexically_normal().generic_string());
            handle.id = iterator->second;
            out_handle = std::move(handle);
            return true;
        }
    };

    static AssetManager make_manager(
        const std::filesystem::path& working_directory,
        const InMemoryHandleSource& handle_source = {})
    {
        auto provider = [handle_source](const std::filesystem::path& asset_path, Handle& out_handle)
        {
            return handle_source.try_get(asset_path, out_handle);
        };
        return AssetManager(working_directory, {}, provider, false);
    }

    static AssetManager make_disk_backed_manager(const std::filesystem::path& working_directory)
    {
        return AssetManager(working_directory, {}, {}, false);
    }

    /// <summary>
    /// Verifies loading by path reuses the same tracked asset instance.
    /// </summary>
    TEST(asset_manager, resolves_handle_by_path)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle path_handle("stone.asset");

        // Act
        reset_test_asset_loader_state();
        auto asset = manager.load<TestAsset>(path_handle);

        // Assert
        ASSERT_NE(asset, nullptr);
        asset->value = 42;

        auto by_path = manager.load<TestAsset>(path_handle);
        ASSERT_NE(by_path, nullptr);
        EXPECT_EQ(by_path->value, 42);

        AssetUsage usage = manager.get_usage<TestAsset>(path_handle);
        EXPECT_EQ(usage.ref_count, 2U);
    }

    /// <summary>
    /// Confirms handle resolution by id uses metadata-supplied UUIDs.
    /// </summary>
    TEST(asset_manager, resolves_handle_by_id)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        InMemoryHandleSource handle_source = {};
        handle_source.add(working_directory / "ore.asset", Uuid(0x2fU));
        AssetManager manager = make_manager(working_directory, handle_source);
        Handle path_handle("ore.asset");

        // Act
        reset_test_asset_loader_state();
        auto asset = manager.load<TestAsset>(path_handle);

        // Assert
        ASSERT_NE(asset, nullptr);
        asset->value = 84;

        Handle id_handle(Uuid(0x2fU));
        auto by_id = manager.load<TestAsset>(id_handle);
        ASSERT_NE(by_id, nullptr);
        EXPECT_EQ(by_id->value, 84);

        AssetUsage usage = manager.get_usage<TestAsset>(id_handle);
        EXPECT_EQ(usage.ref_count, 2U);
    }

    /// <summary>
    /// Ensures pinning prevents unload and unpinning allows eviction.
    /// </summary>
    TEST(asset_manager, supports_stream_in_out_and_pin)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("crate.asset");

        // Act
        reset_test_asset_loader_state();
        auto streamed_in = manager.load_async<TestAsset>(handle);

        // Assert
        ASSERT_NE(streamed_in.asset, nullptr);
        manager.set_pinned(handle, true);
        EXPECT_FALSE(manager.unload<TestAsset>(handle));

        manager.set_pinned(handle, false);
        streamed_in.asset.reset();

        EXPECT_TRUE(manager.unload<TestAsset>(handle));
    }

    /// <summary>
    /// Verifies asynchronous loading updates the streamed asset payload.
    /// </summary>
    TEST(asset_manager, streams_in_async_and_updates_payload)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("async.asset");

        // Act
        reset_test_asset_loader_state();
        auto& loader_state = get_test_asset_loader_state();
        loader_state.use_async = true;
        auto streamed_in = manager.load_async<TestAsset>(handle);

        // Assert
        ASSERT_NE(streamed_in.asset, nullptr);
        EXPECT_EQ(streamed_in.asset->value, 0);

        loader_state.asset->value = 99;
        Result result;
        result.flag_success();
        loader_state.completion->set_value(result);

        AssetUsage usage = manager.get_usage<TestAsset>(handle);
        EXPECT_EQ(usage.stream_state, AssetStreamState::LOADED);
        EXPECT_EQ(streamed_in.asset->value, 99);
    }

    /// <summary>
    /// Confirms unreferenced assets are released during cleanup.
    /// </summary>
    TEST(asset_manager, unloads_unreferenced_assets)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle keep_handle("keep.asset");
        Handle drop_handle("drop.asset");

        // Act
        reset_test_asset_loader_state();
        auto keep_asset = manager.load<TestAsset>(keep_handle);
        auto drop_asset = manager.load<TestAsset>(drop_handle);

        ASSERT_NE(keep_asset, nullptr);
        ASSERT_NE(drop_asset, nullptr);

        drop_asset.reset();
        manager.unload_unreferenced();

        // Assert
        AssetUsage keep_usage = manager.get_usage<TestAsset>(keep_handle);
        EXPECT_EQ(keep_usage.stream_state, AssetStreamState::LOADED);
        EXPECT_EQ(keep_usage.ref_count, 1U);

        AssetUsage drop_usage = manager.get_usage<TestAsset>(drop_handle);
        EXPECT_EQ(drop_usage.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(drop_usage.ref_count, 0U);

        keep_asset.reset();
        manager.unload_unreferenced();

        AssetUsage keep_usage_after = manager.get_usage<TestAsset>(keep_handle);
        EXPECT_EQ(keep_usage_after.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(keep_usage_after.ref_count, 0U);
    }

    /// <summary>
    /// Verifies the manager resolves asset ids from metadata when provided.
    /// </summary>
    TEST(asset_manager, resolves_asset_id_from_handle_source)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        InMemoryHandleSource handle_source = {};
        handle_source.add(working_directory / "id.asset", Uuid(0x7aU));
        AssetManager manager = make_manager(working_directory, handle_source);
        Handle handle("id.asset");

        // Act
        auto resolved_id = manager.resolve_asset_id(handle);

        // Assert
        EXPECT_EQ(resolved_id.value, 0x7aU);
    }

    /// <summary>
    /// Verifies missing disk metadata generates a stable runtime id for discovery.
    /// </summary>
    TEST(asset_manager, generates_runtime_id_when_meta_is_missing)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_disk_backed_manager(working_directory);
        Handle handle("missing_meta.asset");

        // Act
        auto first = manager.resolve_asset_id(handle);
        auto second = manager.resolve_asset_id(handle);

        // Assert
        EXPECT_TRUE(first.is_valid());
        EXPECT_EQ(first, second);
    }

    /// <summary>
    /// Ensures invalid ids from metadata providers fall back to a generated registry id.
    /// </summary>
    TEST(asset_manager, falls_back_when_meta_provider_returns_invalid_id)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        InMemoryHandleSource handle_source = {};
        handle_source.add(working_directory / "invalid_id.asset", Uuid());
        AssetManager manager = make_manager(working_directory, handle_source);
        Handle handle("invalid_id.asset");

        // Act
        auto resolved_id = manager.resolve_asset_id(handle);

        // Assert
        EXPECT_TRUE(resolved_id.is_valid());
    }

    /// <summary>
    /// Verifies duplicate metadata ids reject the conflicting asset registration.
    /// </summary>
    TEST(asset_manager, rejects_conflicting_asset_when_meta_ids_collide)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        InMemoryHandleSource handle_source = {};
        handle_source.add(working_directory / "alpha.asset", Uuid(0x44U));
        handle_source.add(working_directory / "beta.asset", Uuid(0x44U));
        AssetManager manager = make_manager(working_directory, handle_source);
        Handle first_path("alpha.asset");
        Handle second_path("beta.asset");
        Handle shared_id(Uuid(0x44U));

        // Act
        reset_test_asset_loader_state();
        auto first_asset = manager.load<TestAsset>(first_path);
        auto conflicting_asset = manager.load<TestAsset>(second_path);
        auto conflicting_id = manager.resolve_asset_id(second_path);
        auto by_id_asset = manager.load<TestAsset>(shared_id);

        // Assert
        ASSERT_NE(first_asset, nullptr);
        EXPECT_EQ(conflicting_asset, nullptr);
        EXPECT_FALSE(conflicting_id.is_valid());
        EXPECT_EQ(by_id_asset, first_asset);
    }

    /// <summary>
    /// Ensures asset paths resolve against the working directory when no roots are configured.
    /// </summary>
    TEST(asset_manager, resolves_relative_paths_without_search_roots)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        std::filesystem::path relative_path = "relative.asset";

        // Act
        auto resolved = manager.resolve_asset_path(relative_path);

        // Assert
        EXPECT_EQ(resolved, working_directory / relative_path);
    }

    /// <summary>
    /// Validates adding asset directories stores resolved roots.
    /// </summary>
    TEST(asset_manager, tracks_asset_directories)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);

        // Act
        manager.add_asset_directory("content");
        auto directories = manager.get_asset_directories();

        // Assert
        ASSERT_EQ(directories.size(), 1U);
        EXPECT_EQ(directories[0], working_directory / "content");
    }

    /// <summary>
    /// Confirms unload_all clears tracked assets across types.
    /// </summary>
    TEST(asset_manager, unloads_all_assets)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("cleanup.asset");

        // Act
        reset_test_asset_loader_state();
        auto asset = manager.load<TestAsset>(handle);
        manager.unload_all();

        // Assert
        ASSERT_NE(asset, nullptr);
        AssetUsage usage = manager.get_usage<TestAsset>(handle);
        EXPECT_EQ(usage.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(usage.ref_count, 0U);
    }

    /// <summary>
    /// Verifies reload replaces the asset payload when invoked.
    /// </summary>
    TEST(asset_manager, reloads_assets)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("reload.asset");

        // Act
        reset_test_asset_loader_state();
        auto asset = manager.load<TestAsset>(handle);
        bool reloaded = manager.reload<TestAsset>(handle);

        // Assert
        ASSERT_NE(asset, nullptr);
        EXPECT_TRUE(reloaded);
    }
}
