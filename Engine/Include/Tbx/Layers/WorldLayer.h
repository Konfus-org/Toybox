#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/ECS/ThreeDSpace.h"
#include "Tbx/Layers/LayerStack.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// The game world or root 3d space.
    /// Can have different layers to organize data.
    /// </summary>
    class EXPORT WorldLayer : public ThreeDSpace, public HasLayers, public Layer
    {
    public:
        WorldLayer(const std::weak_ptr<App>& app)
            : Layer("World", app) {}

    protected:
        void OnUpdate() override;
    };
}

