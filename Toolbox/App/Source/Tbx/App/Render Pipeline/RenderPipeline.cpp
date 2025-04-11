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
    UID RenderPipeline::_appUpdatedEventId = -1;

    std::shared_ptr<Playspace> RenderPipeline::_currentPlayspace = nullptr;
    RenderProcessor RenderPipeline::_renderProcessor = {};
    RenderQueue RenderPipeline::_renderQueue = {};

    bool RenderPipeline::_vsyncEnabled = false;

    void RenderPipeline::Initialize()
    {
        _appUpdatedEventId = EventCoordinator::Subscribe<AppUpdatedEvent>(TBX_BIND_STATIC_FN(OnAppUpdated));
    }

    void RenderPipeline::Shutdown()
    {
        EventCoordinator::Unsubscribe<AppUpdatedEvent>(_appUpdatedEventId);

        Flush();
    }

    void RenderPipeline::SetContext(const std::shared_ptr<Playspace>& currentPlayspace)
    {
        _currentPlayspace = currentPlayspace;
        _renderProcessor.PreProcess(currentPlayspace);
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
            RenderBatch& batch = _renderQueue.Emplace();
            batch.AddItem(data);
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

    void RenderPipeline::OnAppUpdated(const AppUpdatedEvent&)
    {
        _renderProcessor.Process(_currentPlayspace);
        ProcessNextBatch();
    }
}
