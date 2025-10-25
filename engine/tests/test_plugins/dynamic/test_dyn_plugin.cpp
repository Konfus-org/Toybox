#include "tbx/plugin_api/plugin_loader.h" // for export macro
#include "tbx/plugin_api/plugin.h"

class TestDynamicPlugin : public ::tbx::Plugin
{
public:
    void on_attach(const ::tbx::ApplicationContext&, ::tbx::MessageDispatcher&) override {}
    void on_detach() override {}
    void on_update(const ::tbx::DeltaTime&) override {}
    void on_message(const ::tbx::Message&) override {}
};

TBX_DEFINE_DYNAMIC_PLUGIN(CreateTestDynamicPlugin, TestDynamicPlugin)
