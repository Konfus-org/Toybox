#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"

namespace tbx::tests::plugin_loader
{
    struct DummyPlugin final : Plugin
    {
    };

    struct DummyScriptAsset
    {
    };

    static void register_dummy_script_asset(Plugin*, ServiceProvider*)
    {
        register_asset_type_entry(
            AssetTypeRegistration {
                .type_name = "dummy_script_asset",
                .type = std::type_index(typeid(DummyScriptAsset)),
            });
    }

    TEST(plugin_manager, unload_removes_plugin_owned_script_asset_registrations)
    {
        // Arrange
        auto service_provider = std::make_shared<ServiceProvider>();
        service_provider->register_service<IMessageCoordinator>(
            std::make_shared<MessageCoordinator>());
        service_provider->register_service<PluginOwnershipTracker>(
            std::make_shared<PluginOwnershipTracker>());
        bind_plugin_ownership_tracker(service_provider->get_service<PluginOwnershipTracker>());
        auto manager = PluginManager(service_provider);

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
        bind_plugin_ownership_tracker({});

        // Assert
        EXPECT_FALSE(
            get_asset_type_registration(std::type_index(typeid(DummyScriptAsset))).has_value());
    }
}
