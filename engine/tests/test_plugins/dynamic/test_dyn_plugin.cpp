#include "tbx/plugin_api/plugin_loader.h" // for export macro
#include "tbx/plugin_api/plugin.h"

class TestDynamicPlugin : public ::tbx::Plugin
{
public:
    void on_attach(const ::tbx::ApplicationContext&) override {}
    void on_detach() override {}
    void on_update(const ::tbx::DeltaTime&) override {}
    void on_message(const ::tbx::Message&) override {}
};

#ifndef TBX_TEST_DYN_PLUGIN_PATH
TBX_REGISTER_PLUGIN(CreateTestDynamicPlugin, TestDynamicPlugin)
#endif
