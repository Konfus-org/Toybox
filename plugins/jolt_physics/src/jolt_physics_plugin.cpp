#include "tbx/plugins/jolt_physics/jolt_physics_plugin.h"
#include "jolt_physics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <memory>

namespace jolt_physics
{
    void JoltPhysicsPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = std::ref(service_provider);
        service_provider.register_service<tbx::IPhysicsBackend>(
            std::make_unique<JoltPhysicsBackend>());
    }

    void JoltPhysicsPlugin::on_detach()
    {
        if (_service_provider.has_value() && _service_provider->get().has_service<tbx::Physics>())
            _service_provider->get().deregister_service<tbx::Physics>();

        if (_service_provider.has_value()
            && _service_provider->get().has_service<tbx::IPhysicsBackend>())
            _service_provider->get().deregister_service<tbx::IPhysicsBackend>();

        _service_provider = std::nullopt;
    }
}
