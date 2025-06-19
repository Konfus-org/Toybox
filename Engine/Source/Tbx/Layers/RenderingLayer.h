#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class RenderingLayer : public Layer
    {
    public:
        explicit RenderingLayer() : Layer("Rendering") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
