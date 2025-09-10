#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/TBS/World.h"

namespace Tbx
{
    class RenderingLayer : public Layer
    {
    public:
        explicit RenderingLayer(const std::shared_ptr<World>& world)
            : Layer("Rendering"), _world(world) {}

        void OnUpdate() final;

    private:
        Rendering _rendering;
        std::shared_ptr<World> _world;
    };
}
