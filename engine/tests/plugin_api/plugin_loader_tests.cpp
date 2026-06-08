#include "in_memory_file_ops.h"
#include "plugin_loader.h"
#include "plugin_loader_discovery.h"
#include "plugin_unloader.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/plugin_api/plugin_manager.h"

namespace tbx::tests::plugin_loader
{
    struct DummyPlugin final : Plugin
    {
    };

    struct DummyScriptAsset
    {
    };

    struct PluginDetachedEvent : Event
    {
    };

    struct DetachCountingPlugin final : Plugin
    {
        explicit DetachCountingPlugin(uint& detach_count)
            : _detach_count(detach_count)
        {
        }

      protected:
        void on_detach() override
        {
            _detach_count += 1U;
        }

      private:
        uint& _detach_count;
    };

    struct DetachPostingPlugin final : Plugin
    {
      protected:
        void on_detach() override
        {
            post_message<PluginDetachedEvent>();
        }
    };

    static void register_dummy_script_asset(Plugin*, ServiceProvider*)
    {
        register_asset_type_entry(
            AssetTypeRegistration {
                .type_name = "dummy_script_asset",
                .type = std::type_index(typeid(DummyScriptAsset)),
            });
    }

    static LoadedPlugins make_loaded_plugin(std::unique_ptr<Plugin> plugin)
    {
        auto meta = PluginMeta {};
        meta.name = "DummyPlugin";
        meta.version = "1.0.0";
        auto plugin_deleter =
            [](Plugin* plugin_ptr)
        {
            delete plugin_ptr;
        };
        auto loaded_plugins = LoadedPlugins {};
        auto& loaded_plugin = loaded_plugins.emplace_back(
            meta,
            nullptr,
            std::unique_ptr<Plugin, PluginDeleter>(plugin.release(), plugin_deleter));
        loaded_plugin.set_id(Uuid(123U));
        return loaded_plugins;
    }

    TEST(plugin_loader, resolves_requested_plugin_to_debug_library_without_scanning_siblings)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("C:/virtual/plugins");
        auto file_ops = InMemoryFileOps(working_directory);
        file_ops.set_binary("ExampleAppd.dll", {0x01});
        file_ops.set_binary("TwoDExampleAppd.dll", {0x02});

        // Act
        const auto library_path =
            resolve_requested_plugin_library_path(working_directory, "ExampleApp", file_ops);

        // Assert
        EXPECT_EQ(library_path, (working_directory / "ExampleAppd.dll").lexically_normal());
    }

    TEST(plugin_loader, returns_empty_path_when_requested_plugin_library_is_missing)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("C:/virtual/plugins");
        auto file_ops = InMemoryFileOps(working_directory);
        file_ops.set_binary("TwoDExampleAppd.dll", {0x02});

        // Act
        const auto library_path =
            resolve_requested_plugin_library_path(working_directory, "ExampleApp", file_ops);

        // Assert
        EXPECT_TRUE(library_path.empty());
    }

    TEST(plugin_loader, resolves_requested_plugin_to_meta_file_without_scanning_sibling_libraries)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("C:/virtual/plugins");
        auto file_ops = InMemoryFileOps(working_directory);
        file_ops.set_binary("MetaOnlyPlugind.dll", {0x01});
        file_ops.set_binary("NeighborPlugind.dll", {0x02});
        file_ops.set_text("MetaOnlyPlugind.dll.meta", "{ \"name\": \"MetaOnlyPlugin\" }");

        // Act
        const auto meta_path =
            resolve_requested_plugin_meta_path(working_directory, "MetaOnlyPlugin", file_ops);

        // Assert
        EXPECT_EQ(meta_path, (working_directory / "MetaOnlyPlugind.dll.meta").lexically_normal());
    }

    TEST(plugin_loader, reads_plugin_meta_with_serialized_enum_names)
    {
        // Arrange
        constexpr auto meta_json = R"({
            "name": "SdlWindowing",
            "version": "1.0.0",
            "description": "",
            "dependencies": [],
            "resource_directory": "",
            "abi_version": 1,
            "category": "input",
            "linkage": "dynamic",
            "priority": 0
        })";
        auto meta = PluginMeta {};

        // Act
        const auto read = read_json_serializable_value(meta_json, meta);

        // Assert
        ASSERT_TRUE(read);
        EXPECT_EQ(meta.name, "SdlWindowing");
        EXPECT_EQ(meta.category, PluginCategory::INPUT);
        EXPECT_EQ(meta.linkage, PluginLinkage::DYNAMIC);
    }

    TEST(plugin_loader, supplied_file_ops_overload_returns_empty_for_missing_requested_plugin)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("C:/virtual/plugins");
        auto file_ops = InMemoryFileOps(working_directory);
        auto loader = PluginLoader();

        // Act
        const auto loaded_plugins = loader.load(working_directory, {"MissingPlugin"}, file_ops);

        // Assert
        EXPECT_TRUE(loaded_plugins.empty());
    }

    TEST(plugin_unloader, detach_overload_without_coordinator_detaches_plugins)
    {
        // Arrange
        auto service_provider = std::make_shared<ServiceProvider>();
        service_provider->register_service<IMessageCoordinator>(
            std::make_shared<MessageCoordinator>());
        uint detach_count = 0U;
        auto loaded_plugins =
            make_loaded_plugin(std::make_unique<DetachCountingPlugin>(detach_count));
        loaded_plugins.front().attach(service_provider);
        auto unloader = PluginUnloader();

        // Act
        unloader.detach(loaded_plugins, *service_provider);

        // Assert
        EXPECT_EQ(detach_count, 1U);
    }

    TEST(plugin_unloader, detach_overload_with_coordinator_flushes_posted_detach_messages)
    {
        // Arrange
        auto service_provider = std::make_shared<ServiceProvider>();
        auto coordinator = std::make_shared<MessageCoordinator>();
        service_provider->register_service<IMessageCoordinator>(coordinator);
        uint handled_count = 0U;
        coordinator->register_handler(
            [&handled_count](Message& message)
            {
                if (handle_message<PluginDetachedEvent>(message).has_value())
                    handled_count += 1U;
            });
        auto loaded_plugins = make_loaded_plugin(std::make_unique<DetachPostingPlugin>());
        loaded_plugins.front().attach(service_provider);
        auto unloader = PluginUnloader();

        // Act
        unloader.detach(loaded_plugins, *service_provider, *coordinator);

        // Assert
        EXPECT_EQ(handled_count, 1U);
    }

    TEST(plugin_manager, unload_removes_plugin_owned_script_asset_registrations)
    {
        // Arrange
        auto service_provider = std::make_shared<ServiceProvider>();
        service_provider->register_service<IMessageCoordinator>(
            std::make_shared<MessageCoordinator>());
        auto file_ops =
            std::make_shared<InMemoryFileOps>(std::filesystem::path("C:/virtual/plugins"));
        auto manager = PluginManager(service_provider, file_ops);

        auto meta = PluginMeta {};
        meta.name = "DummyPlugin";
        meta.version = "1.0.0";
        auto plugin = std::unique_ptr<Plugin, PluginDeleter>(
            new DummyPlugin(),
            [](Plugin* plugin)
            {
                delete plugin;
            });

        auto loaded_plugins = LoadedPlugins {};
        auto& loaded_plugin = loaded_plugins.emplace_back(
            meta,
            nullptr,
            std::move(plugin),
            register_dummy_script_asset);
        loaded_plugin.set_id(Uuid(123U));

        manager.add(std::move(loaded_plugins));

        ASSERT_TRUE(
            get_asset_type_registration(std::type_index(typeid(DummyScriptAsset))).has_value());

        // Act
        manager.unload("DummyPlugin");

        // Assert
        EXPECT_FALSE(
            get_asset_type_registration(std::type_index(typeid(DummyScriptAsset))).has_value());
    }
}
