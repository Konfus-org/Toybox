#include "tbx/assets/asset_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/result.h"
#include "tbx/files/tests/in_memory_file_ops.h"
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

    struct TestAssetLoadParameters
    {
        int value = 0;

        bool operator==(const TestAssetLoadParameters& other) const = default;
    };

    struct TestAssetLoaderState
    {
        int async_load_count = 0;
        int sync_load_count = 0;
        TestAssetLoadParameters last_async_parameters = {};
        TestAssetLoadParameters last_sync_parameters = {};
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
        state.async_load_count = 0;
        state.sync_load_count = 0;
        state.last_async_parameters = {};
        state.last_sync_parameters = {};
        state.use_async = false;
        state.asset.reset();
        state.completion.reset();
    }

    template <>
    struct AssetLoader<TestAsset>
    {
        using Parameters = TestAssetLoadParameters;

        static AssetPromise<TestAsset> load_async(
            const std::filesystem::path&,
            const Parameters& parameters = {})
        {
            auto& state = get_test_asset_loader_state();
            AssetPromise<TestAsset> promise = {};
            promise.asset = std::make_shared<TestAsset>();
            promise.asset->value = parameters.value;
            state.async_load_count += 1;
            state.last_async_parameters = parameters;

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

        static std::shared_ptr<TestAsset> load(
            const std::filesystem::path&,
            const Parameters& parameters = {})
        {
            auto& state = get_test_asset_loader_state();
            auto asset = std::make_shared<TestAsset>();
            asset->value = parameters.value;
            state.sync_load_count += 1;
            state.last_sync_parameters = parameters;
            return asset;
        }
    };
}

namespace tbx::tests::assets
{
    using InMemoryFileOps = ::tbx::tests::file_system::InMemoryFileOps;

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

    struct TrackingHandleSerializerState
    {
        int read_count = 0;
        int write_count = 0;
        bool write_succeeds = true;
        std::unordered_map<std::filesystem::path, Handle> stored_handles = {};
    };

    class TrackingHandleSerializer final : public IAssetHandleSerializer
    {
      public:
        TrackingHandleSerializer(std::shared_ptr<TrackingHandleSerializerState> state)
            : _state(std::move(state))
        {
        }

      public:
        std::unique_ptr<Handle> read_from_disk(
            const std::filesystem::path&,
            const std::filesystem::path& asset_path) const override
        {
            _state->read_count += 1;
            auto iterator = _state->stored_handles.find(asset_path.lexically_normal());
            if (iterator == _state->stored_handles.end())
                return nullptr;

            return std::make_unique<Handle>(iterator->second);
        }

        std::unique_ptr<Handle> read_from_disk(
            const IFileOps& file_ops,
            const std::filesystem::path& asset_path) const override
        {
            _state->read_count += 1;
            auto iterator = _state->stored_handles.find(file_ops.resolve(asset_path));
            if (iterator == _state->stored_handles.end())
                return nullptr;

            return std::make_unique<Handle>(iterator->second);
        }

        std::unique_ptr<Handle> read_from_source(std::string_view, const std::filesystem::path&)
            const override
        {
            return nullptr;
        }

        bool try_write_to_disk(
            const std::filesystem::path&,
            const std::filesystem::path& asset_path,
            const Handle& handle) const override
        {
            _state->write_count += 1;
            if (!_state->write_succeeds)
                return false;

            _state->stored_handles[asset_path.lexically_normal()] = handle;
            return true;
        }

        bool try_write_to_disk(
            IFileOps& file_ops,
            const std::filesystem::path& asset_path,
            const Handle& handle) const override
        {
            _state->write_count += 1;
            if (!_state->write_succeeds)
                return false;

            auto resolved = file_ops.resolve(asset_path);
            _state->stored_handles[resolved] = handle;
            return file_ops.write_file(
                resolved.string() + ".meta",
                FileDataFormat::UTF8_TEXT,
                "{ \"id\": \"tracked\" }\n");
        }

      private:
        std::shared_ptr<TrackingHandleSerializerState> _state = {};
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

    TEST(asset_manager, forwards_sync_load_parameters)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("sync_params.asset");
        TestAssetLoadParameters parameters = {.value = 17};

        // Act
        reset_test_asset_loader_state();
        auto asset = manager.load<TestAsset>(handle, parameters);

        // Assert
        ASSERT_NE(asset, nullptr);
        EXPECT_EQ(asset->value, 17);

        const auto& loader_state = get_test_asset_loader_state();
        EXPECT_EQ(loader_state.sync_load_count, 1);
        EXPECT_EQ(loader_state.last_sync_parameters, parameters);
    }

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

    TEST(asset_manager, forwards_async_load_parameters)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        AssetManager manager = make_manager(working_directory);
        Handle handle("async_params.asset");
        TestAssetLoadParameters parameters = {.value = 29};

        // Act
        reset_test_asset_loader_state();
        auto streamed_in = manager.load_async<TestAsset>(handle, parameters);

        // Assert
        ASSERT_NE(streamed_in.asset, nullptr);
        EXPECT_EQ(streamed_in.asset->value, 29);

        const auto& loader_state = get_test_asset_loader_state();
        EXPECT_EQ(loader_state.async_load_count, 1);
        EXPECT_EQ(loader_state.last_async_parameters, parameters);
    }

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
    TEST(asset_manager, constructor_keeps_explicit_directories_when_defaults_are_disabled)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);

        // Act
        AssetManager manager(working_directory, {"content"}, {}, false, {}, file_ops);
        auto directories = manager.get_asset_directories();

        // Assert
        ASSERT_EQ(directories.size(), 1U);
        EXPECT_EQ(directories[0], working_directory / "content");
    }

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

    TEST(asset_manager, discovery_does_not_write_meta_until_asset_id_is_ensured)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->write_file("content/stone.asset", FileDataFormat::UTF8_TEXT, "x");
        auto serializer_state = std::make_shared<TrackingHandleSerializerState>();

        // Act
        AssetManager manager(
            working_directory,
            {"content"},
            {},
            false,
            std::make_unique<TrackingHandleSerializer>(serializer_state),
            file_ops);
        auto discovered_write_count = serializer_state->write_count;
        auto ensured_id = manager.ensure_asset_id(Handle("stone.asset"));
        auto resolved_id = manager.resolve_asset_id(Handle("stone.asset"));

        // Assert
        EXPECT_EQ(discovered_write_count, 0);
        EXPECT_TRUE(ensured_id.is_valid());
        EXPECT_EQ(serializer_state->write_count, 1);
        EXPECT_EQ(resolved_id, ensured_id);
    }

    TEST(asset_manager, discovery_indexes_existing_meta_ids_for_id_only_lookup)
    {
        // Arrange
        std::filesystem::path working_directory = "/virtual/asset_manager";
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->write_file("content/material.mat", FileDataFormat::UTF8_TEXT, "material");
        file_ops->write_file(
            "content/material.mat.meta",
            FileDataFormat::UTF8_TEXT,
            "{ \"id\": \"2\" }\n");
        auto serializer_state = std::make_shared<TrackingHandleSerializerState>();
        serializer_state->stored_handles[working_directory / "content" / "material.mat"] =
            Handle(Uuid(0x2U));

        // Act
        AssetManager manager(
            working_directory,
            {"content"},
            {},
            false,
            std::make_unique<TrackingHandleSerializer>(serializer_state),
            file_ops);
        auto asset = manager.load<TestAsset>(Handle(Uuid(0x2U)));

        // Assert
        ASSERT_NE(asset, nullptr);
        EXPECT_EQ(serializer_state->write_count, 0);
    }

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
