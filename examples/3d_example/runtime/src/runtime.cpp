#include "runtime.h"
#include "tbx/systems/assets/manager.h"
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

    void ThreeDExampleRuntimePlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto& asset_manager = service_provider.get_service<tbx::AssetManager>();
        auto& entity_registry = service_provider.get_service<tbx::EntityRegistry>();
        auto& input_manager = service_provider.get_service<tbx::IInputManager>();

        asset_manager.add_directory(get_three_d_example_asset_directory());
        _scene = std::make_unique<DemoScene>(entity_registry, input_manager);
    }

    void ThreeDExampleRuntimePlugin::on_detach()
    {
        _scene.reset();
    }

    void ThreeDExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        if (!_scene)
            return;

        _scene->update(dt);
    }
}
