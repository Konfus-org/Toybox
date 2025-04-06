#include "Tbx/App/PCH.h"
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include "Tbx/App/Render Pipeline/RenderQueue.h"
#include "Tbx/App/Events/RenderEvents.h"
#include "Tbx/App/Events/WindowEvents.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Plugins/PluginServer.h>
#include <memory>

namespace Tbx
{
    RenderQueue RenderPipeline::_renderQueue = {};

    bool RenderPipeline::_vsyncEnabled = false;

    UID RenderPipeline::_focusedWindowId = -1;
    UID RenderPipeline::_appUpdatedEventId = -1;

    void RenderPipeline::Initialize()
    {
        _appUpdatedEventId = EventCoordinator::Subscribe<AppUpdated>(TBX_BIND_STATIC_FN(OnAppUpdated));
    }

    void RenderPipeline::Shutdown()
    {
        EventCoordinator::Unsubscribe<AppUpdated>(_appUpdatedEventId);

        Flush();
    }

    void RenderPipeline::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;

        SetVSyncRequest request(enabled);
        EventCoordinator::Send(request);
    }

    bool RenderPipeline::IsVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void RenderPipeline::Push(const RenderData& data)
    {
        if (_renderQueue.IsEmpty())
        {
            auto renderBatch = RenderBatch();
            renderBatch.AddItem(data);
            _renderQueue.Push(renderBatch);
        }
        else
        {
            auto& frameBatch = _renderQueue.Peek();
            frameBatch.AddItem(data);
        }
    }

    void RenderPipeline::Clear()
    {
        ClearScreenRequest request;
        EventCoordinator::Send(request);
    }

    void RenderPipeline::Flush()
    {
        FlushRendererRequest request;
        EventCoordinator::Send(request);
        _renderQueue.Clear();
    }

    void RenderPipeline::ProcessNextBatch()
    {
        if (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();
            auto request = RenderFrameRequest(batch);
            EventCoordinator::Send(request);
            _renderQueue.Pop();
        }
    }

    void RenderPipeline::OnAppUpdated(const AppUpdated&)
    {
        ProcessNextBatch();
    }
}
