#include "runtime.h"
#include <filesystem>
#include <memory>

namespace three_d_example
{
    static std::filesystem::path get_three_d_example_asset_directory()
    {
        return (std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                / "assets")
            .lexically_normal();
    }

    void ThreeDExampleRuntimePlugin::on_attach(tbx::IPluginHost& host)
    {
        host.get_asset_manager().add_directory(get_three_d_example_asset_directory());
        _scene = std::make_unique<DemoScene>(host.get_entity_registry(), host.get_input_manager());
    }

    void ThreeDExampleRuntimePlugin::on_detach()
    {
        _scene.reset();
    }

    void ThreeDExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        _scene->update(dt);
    }
}
