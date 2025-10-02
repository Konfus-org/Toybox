#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/RenderingPipeline.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// The application's rendering layer that connects the high-level layer system to the rendering subsystem.
    /// </summary>
    class RenderingLayer final : public Layer
    {
    public:
        TBX_EXPORT RenderingLayer(
            const std::vector<Ref<IRendererFactory>>& renderFactories,
            const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
            Ref<EventBus> eventBus);

    protected:
        void OnUpdate() override;

    private:
        Tbx::ExclusiveRef<RenderingPipeline> _rendering = nullptr;
    };
}

