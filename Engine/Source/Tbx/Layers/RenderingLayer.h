#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class RenderingLayer : public Layer
    {
    public:
        RenderingLayer() : Layer("Rendering") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
