#include "tbx/plugins/jolt_physics/jolt_physics_plugin.h"
#include "jolt_physics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/service_provider.h"

namespace jolt_physics
{
    void JoltPhysicsPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        service_provider.register_service<tbx::IPhysicsBackend>(
            std::make_unique<JoltPhysicsBackend>());
    }

    void JoltPhysicsPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (service_provider.has_service<tbx::Physics>())
            service_provider.deregister_service<tbx::Physics>();

        if (service_provider.has_service<tbx::IPhysicsBackend>())
            service_provider.deregister_service<tbx::IPhysicsBackend>();
    }
}
