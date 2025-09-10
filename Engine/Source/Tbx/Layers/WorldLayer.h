#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/TBS/World.h"

namespace Tbx
{
    class WorldLayer : public Layer
    {
    public:
        explicit WorldLayer(const std::shared_ptr<World>& world) : Layer("World"), _world(world) {}

        void OnAttach() final;
        void OnUpdate() final;

    private:
        std::shared_ptr<World> _world;
    };
}
