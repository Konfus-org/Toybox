#include "runtime.h"
#include <memory>

namespace three_d_example
{
    void ThreeDExampleRuntimePlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto& entity_registry = service_provider.get_service<tbx::EntityRegistry>();
        auto& input_manager = service_provider.get_service<tbx::IInputManager>();

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
