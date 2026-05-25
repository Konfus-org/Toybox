#pragma once
#include "jolt_collision_layers.h"
#include <cstdint>

namespace jolt_physics::internal
{
    static constexpr JPH::ObjectLayer StaticObjectLayer = 0;
    static constexpr JPH::ObjectLayer MovingObjectLayer = 1;

    static constexpr JPH::BroadPhaseLayer StaticBroadPhaseLayer = JPH::BroadPhaseLayer(0);
    static constexpr JPH::BroadPhaseLayer MovingBroadPhaseLayer = JPH::BroadPhaseLayer(1);
    static constexpr std::uint32_t BroadPhaseLayerCount = 2U;

    class PhysicsBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
      public:
        std::uint32_t GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayerCount;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            if (layer == MovingObjectLayer)
                return MovingBroadPhaseLayer;

            return StaticBroadPhaseLayer;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
        {
            if (layer == MovingBroadPhaseLayer)
                return "Moving";

            return "Static";
        }
#endif
    };

    class PhysicsObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer left, JPH::ObjectLayer right) const override
        {
            if (left == StaticObjectLayer)
                return right == MovingObjectLayer;

            if (left == MovingObjectLayer)
                return true;

            return false;
        }
    };

    class PhysicsObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad_phase_layer)
            const override
        {
            if (layer == StaticObjectLayer)
                return broad_phase_layer == MovingBroadPhaseLayer;

            if (layer == MovingObjectLayer)
                return true;

            return false;
        }
    };
}
