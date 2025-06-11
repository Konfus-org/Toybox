#pragma once
#include "Tbx/Application/Layers/Layer.h"

namespace Tbx
{
    class WindowingLayer : public Layer
    {
    public:
        explicit WindowingLayer() : Layer("Windowing") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
