#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/TBS/World.h"

namespace Tbx
{
    class WorldLayer : public Layer
    {
    public:
        WorldLayer() : Layer("World") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;

    private:
        std::shared_ptr<World> _world;
    };
}
