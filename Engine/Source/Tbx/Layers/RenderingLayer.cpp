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
        _isRunning = true;
        Rendering::Initialize();
        /*_renderThread = std::thread([this]()
        {
            try
            {
                Rendering::Initialize();
                while (_isRunning && App::GetInstance()->GetStatus() == AppStatus::Running)
                {
                    Rendering::DrawFrame();
                    RenderedFrameEvent evt;
                    EventCoordinator::Send(evt);
                }
                Rendering::Shutdown();
            }
            catch (const std::exception& e)
            {
                TBX_ASSERT(false, "Render thread encountered an error! {}", e.what());
                Rendering::Shutdown();
            }
        });*/
    }

    void RenderingLayer::OnDetach()
    {
        Rendering::Shutdown();
        _isRunning = false;
        /*if (_renderThread.joinable())
        {
            _renderThread.join();
        }*/
    }

    void RenderingLayer::OnUpdate()
    {
        Rendering::DrawFrame();
    }
}