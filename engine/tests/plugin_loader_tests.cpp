#include "pch.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin.h"
#include <filesystem>
#include <fstream>

namespace tbx::tests::plugin_loader
{
    // Testing plugin
    class TestDynamicPlugin : public ::tbx::Plugin
    {
    public:
        void on_attach(const ::tbx::ApplicationContext&, ::tbx::IMessageDispatcher&) override {}
        void on_detach() override {}
        void on_update(const ::tbx::DeltaTime&) override {}
        void on_message(const ::tbx::Message&) override {}
    };
    TBX_REGISTER_PLUGIN(CreateTestDynamicPlugin, TestDynamicPlugin)

    // Dynamic plugin test: load directly from in-memory PluginMeta, no IO.
    TEST(plugin_loader_dynamic, loads_dynamic_plugin_from_manifest)
    {
#ifdef TBX_TEST_DYN_PLUGIN_PATH
        const std::filesystem::path module_path_raw = TBX_TEST_DYN_PLUGIN_PATH;
        const std::string module_path = module_path_raw.generic_string();

        ::tbx::PluginMeta meta;
        meta.id = "Test.Dynamic.Plugin";
        meta.name = "Dynamic Plugin";
        meta.version = "1.0.0";
        meta.entry_point = "CreateTestDynamicPlugin";
        meta.type = "plugin";
        meta.module_path = module_path;

        auto loaded = ::tbx::load_plugins(std::vector<::tbx::PluginMeta>{meta});
        ASSERT_EQ(loaded.size(), 1u);
        EXPECT_NE(loaded[0].instance.get(), nullptr);
#else
        GTEST_SKIP() << "TBX_TEST_DYN_PLUGIN_PATH is not defined; dynamic plugin test skipped.";
#endif
    }
}
