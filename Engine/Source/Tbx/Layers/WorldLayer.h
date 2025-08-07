#pragma once
#include "Tbx/Layers/Layer.h"

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
    };
}
