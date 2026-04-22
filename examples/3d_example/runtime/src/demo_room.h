#pragma once
#include "tbx/core/systems/ecs/entity.h"
#include "tbx/core/systems/graphics/color.h"

namespace three_d_example
{
    struct DemoRoomSettings final
    {
        tbx::Vec3 center = tbx::Vec3(0.0F, 0.0F, 0.0F);
        bool include_colliders = true;
        tbx::Color color = tbx::Color::LIGHT_GREY;
    };

    class DemoRoom final
    {
      public:
        DemoRoom(tbx::EntityRegistry& entity_registry, const DemoRoomSettings& settings);
        ~DemoRoom();

        DemoRoom(const DemoRoom&) = delete;
        DemoRoom(DemoRoom&&) = delete;
        DemoRoom& operator=(const DemoRoom&) = delete;
        DemoRoom& operator=(DemoRoom&&) = delete;

      private:
        tbx::Entity _ground = {};
        tbx::Entity _front_wall = {};
        tbx::Entity _left_wall = {};
        tbx::Entity _right_wall = {};
        tbx::Entity _back_wall = {};
    };
}
