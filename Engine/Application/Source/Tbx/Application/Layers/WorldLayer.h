#pragma once
#include "Tbx/Application/Layers/Layer.h"

namespace Tbx
{
    class WorldLayer : public Layer
    {
    public:
        explicit WorldLayer() : Layer("World") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
