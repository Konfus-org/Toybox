#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/App/App.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"

namespace Tbx
{
    bool RenderingLayer::IsOverlay()
    {
        return false;
    }

    void RenderingLayer::OnAttach()
    {
        Rendering::Initialize();
        _isRunning = true;
        _renderThread = std::thread([this]()
        {
            while (_isRunning && App::GetInstance()->GetStatus() == AppStatus::Running)
            {
                Rendering::DrawFrame();
                RenderedFrameEvent evt;
                EventCoordinator::Send(evt);
            }
        });
    }

    void RenderingLayer::OnDetach()
    {
        _isRunning = false;
        if (_renderThread.joinable())
        {
            _renderThread.join();
        }
        Rendering::Shutdown();
    }

    void RenderingLayer::OnUpdate()
    {
        // Rendering happens on a separate thread
    }
}