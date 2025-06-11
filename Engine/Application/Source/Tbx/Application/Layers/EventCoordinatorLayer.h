#pragma once
#include "Tbx/Application/Layers/Layer.h"

namespace Tbx
{
    class EventCoordinatorLayer : public Layer
    {
    public:
        explicit EventCoordinatorLayer() : Layer("Events") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
