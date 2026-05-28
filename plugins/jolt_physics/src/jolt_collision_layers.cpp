#include "jolt_collision_layers.h"
#include <cstdint>

namespace jolt_physics
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

    JPH::ObjectLayer get_static_object_layer()
    {
        return StaticObjectLayer;
    }

    JPH::ObjectLayer get_moving_object_layer()
    {
        return MovingObjectLayer;
    }

    const JPH::BroadPhaseLayerInterface& get_broad_phase_layer_interface()
    {
        static const auto interface_instance = PhysicsBroadPhaseLayerInterface();
        return interface_instance;
    }

    const JPH::ObjectVsBroadPhaseLayerFilter& get_object_vs_broad_phase_layer_filter()
    {
        static const auto filter_instance = PhysicsObjectVsBroadPhaseLayerFilter();
        return filter_instance;
    }

    const JPH::ObjectLayerPairFilter& get_object_layer_pair_filter()
    {
        static const auto filter_instance = PhysicsObjectLayerPairFilter();
        return filter_instance;
    }
}
