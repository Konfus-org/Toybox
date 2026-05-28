#include "threed_example.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/types/assets/world.h"

namespace three_d_example
{
    void ThreeDExampleRuntimePlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto asset_manager = service_provider.get_service<tbx::AssetManager>();
        auto asset_manager_lock = asset_manager.lock();
        if (!asset_manager_lock)
            return;

        // Load world
        _world_handle = tbx::Handle("Worlds/Example.world");
        auto world = asset_manager_lock->load<tbx::World>(_world_handle);
        if (!world)
            return;

        const auto character_entity = world->find_by_tag("player");
        const auto camera_entity = world->find_by_name("Camera");
        const auto sky_entity = world->find_by_name("Sky");
        if (!character_entity.get_id().is_valid() || !camera_entity.get_id().is_valid())
        {
            TBX_TRACE_WARNING(
                "3D example loaded the world, but the authored player entities were not found.");
            return;
        }

        // Keep the authored world resident while the runtime systems hold entity handles into it.
        asset_manager_lock->set_pinned(_world_handle, true);
        _asset_manager = asset_manager;

        _player = std::make_unique<Player>(
            world,
            character_entity,
            camera_entity,
            service_provider.get_service<tbx::IInputManager>(),
            service_provider.get_service<tbx::Physics>(),
            PlayerSettings());
        _sky_system.set_sky_entity(sky_entity);
    }

    void ThreeDExampleRuntimePlugin::on_detach(tbx::ServiceProvider&)
    {
        _player = nullptr;
        _sky_system.set_sky_entity({});

        if (auto asset_manager = _asset_manager.lock(); asset_manager && _world_handle.is_valid())
        {
            asset_manager->set_pinned(_world_handle, false);
            asset_manager->unload<tbx::World>(_world_handle);
        }

        _asset_manager = {};
        _world_handle = {};
    }

    void ThreeDExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        if (_player)
            _player->update(dt);

        _sky_system.update(dt);
    }
}
