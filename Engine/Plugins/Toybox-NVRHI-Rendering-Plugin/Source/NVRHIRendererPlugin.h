#pragma once
#include "NVRHIRendererFactory.h"
#include <Tbx/Application/App/ApplicationEvents.h>
#include <Tbx/Systems/Rendering/RenderEvents.h>
#include <Tbx/Systems/Windowing/WindowEvents.h>
#include <Tbx/Systems/Rendering/IRenderer.h>
#include <Tbx/Systems/Plugins/RegisterPlugin.h>
#include <Tbx/Graphics/GraphicsSettings.h>

namespace NVRHIRendering
{
    class NVRHIRendererPlugin : public Tbx::IPlugin, public NVRHIRendererFactory
    {
    public:
        NVRHIRendererPlugin() = default;
        ~NVRHIRendererPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        void OnCreateRendererRequest(Tbx::CreateRendererRequest& e);

        Tbx::UID _createRendererRequestEventId = -1;
    };

    TBX_REGISTER_PLUGIN(NVRHIRendererPlugin);
}