#include "flashlight_system.h"
#include "tbx/types/components/transform.h"
#include <glm/ext/quaternion_common.hpp>

namespace three_d_example
{
    FlashlightSystem::FlashlightSystem(
        std::weak_ptr<tbx::World> world,
        tbx::Entity camera_entity,
        float follow_speed,
        float light_intensity)
        : _camera_entity(std::move(camera_entity))
        , _follow_speed(follow_speed)
        , _light_intensity(light_intensity)
        , _world(world)
    {
        auto world_lock = _world.lock();
        if (!world_lock || !_camera_entity.get_id().is_valid())
            return;

        auto flashlight = create_spot_light(_camera_entity);
        flashlight.intensity = 0.0F;

        _flashlight_entity = world_lock->create_persistent_entity("PlayerFlashlight");
        _flashlight_entity.add_component<tbx::Transform>();
        _flashlight_entity.add_component<tbx::SpotLight>(flashlight);
        sync_to_camera_now();
    }

    FlashlightSystem::~FlashlightSystem()
    {
        auto world = _world.lock();
        if (world && _flashlight_entity.get_id().is_valid())
            world->destroy(_flashlight_entity);

        _camera_entity = {};
        _flashlight_entity = {};
        _world.reset();
    }

    bool FlashlightSystem::get_is_enabled() const
    {
        return _is_enabled;
    }

    void FlashlightSystem::set_enabled(bool is_enabled)
    {
        _is_enabled = is_enabled;

        if (!_flashlight_entity.get_id().is_valid()
            || !_flashlight_entity.has_component<tbx::SpotLight>())
        {
            return;
        }

        auto& light = _flashlight_entity.get_component<tbx::SpotLight>();
        light.intensity = _is_enabled ? _light_intensity : 0.0F;
    }

    void FlashlightSystem::update(const tbx::DeltaTime& dt)
    {
        if (!_camera_entity.get_id().is_valid() || !_flashlight_entity.get_id().is_valid())
            return;
        if (!_flashlight_entity.has_component<tbx::Transform>())
            return;

        const auto camera_transform = tbx::get_world_space_transform(_camera_entity);
        auto& flashlight_transform = _flashlight_entity.get_component<tbx::Transform>();
        const auto blend = get_frame_blend(dt);

        // Lag the light source toward the camera so quick look changes have a softer feel.
        flashlight_transform.position +=
            (camera_transform.position - flashlight_transform.position) * blend;
        flashlight_transform.rotation = tbx::normalize(
            glm::slerp(flashlight_transform.rotation, camera_transform.rotation, blend));
        flashlight_transform.scale = camera_transform.scale;
    }

    tbx::SpotLight FlashlightSystem::create_spot_light(tbx::Entity& camera_entity) const
    {
        auto light = tbx::SpotLight {};
        if (camera_entity.get_id().is_valid() && camera_entity.has_component<tbx::SpotLight>())
        {
            light = camera_entity.get_component<tbx::SpotLight>();
            camera_entity.remove_component<tbx::SpotLight>();
        }

        light.intensity = _light_intensity;
        return light;
    }

    float FlashlightSystem::get_frame_blend(const tbx::DeltaTime& dt) const
    {
        return std::clamp(_follow_speed * static_cast<float>(dt.seconds), 0.0F, 1.0F);
    }

    void FlashlightSystem::sync_to_camera_now()
    {
        if (!_camera_entity.get_id().is_valid() || !_flashlight_entity.get_id().is_valid())
            return;
        if (!_flashlight_entity.has_component<tbx::Transform>())
            return;

        _flashlight_entity.get_component<tbx::Transform>() =
            tbx::get_world_space_transform(_camera_entity);
    }
}
