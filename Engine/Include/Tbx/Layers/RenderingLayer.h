#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// The application's rendering layer that connects the high-level layer system to the rendering subsystem.
    /// </summary>
    class RenderingLayer : public Layer
    {
    public:
        EXPORT RenderingLayer(Ref<IRendererFactory> renderFactory, Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT Ref<IRenderer> GetRenderer(const Ref<IWindow>& window);

    protected:
        void OnUpdate() override;

    private:
        Tbx::ExclusiveRef<Rendering> _rendering = nullptr;
    };
}

