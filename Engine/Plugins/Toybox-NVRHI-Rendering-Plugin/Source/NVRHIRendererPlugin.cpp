#include "NVRHIRendererPlugin.h"
#include <Tbx/Systems/Events/EventCoordinator.h>
#include <Tbx/Systems/Windowing/WindowManager.h>
#include <Tbx/Systems/Rendering/RenderEvents.h>
#include <Tbx/Application/App/ApplicationEvents.h>

namespace NVRHIRendering
{
    void NVRHIRendererPlugin::OnLoad()
    {
        _createRendererRequestEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::CreateRendererRequest>(TBX_BIND_FN(OnCreateRendererRequest));
    }

    void NVRHIRendererPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_createRendererRequestEventId);
    }

    void NVRHIRendererPlugin::OnCreateRendererRequest(Tbx::CreateRendererRequest& e)
    {
        auto newRenderer = Create(e.GetSurfaceToCreateFor());
        e.SetResult(newRenderer);
        e.IsHandled = true;
    }
}