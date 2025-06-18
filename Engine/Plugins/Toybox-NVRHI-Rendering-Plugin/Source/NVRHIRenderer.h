#pragma once
#include "NVRHIMessageHandler.h"
#include <Tbx/Systems/Rendering/IRenderer.h>
#include <Tbx/Systems/Rendering/RenderEvents.h>
#include <Tbx/Systems/Windowing/WindowEvents.h>
#include <Tbx/Application/App/ApplicationEvents.h>
#include <nvrhi/nvrhi.h>

namespace NVRHIRendering
{
    class NVRHIRenderer : public Tbx::IRenderer
    {
    public:
        ~NVRHIRenderer() override;

        void Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface) final;

        std::shared_ptr<Tbx::IRenderSurface> GetRenderSurface();
        Tbx::GraphicsDevice GetGraphicsDevice() final;

        void SetApi(Tbx::GraphicsApi api) final;
        Tbx::GraphicsApi GetApi() override;

        void SetViewport(const Tbx::Viewport& viewport) final;
        const Tbx::Viewport& GetViewport() final;

        void SetResolution(const Tbx::Size& size) final;
        const Tbx::Size& GetResolution() final;

        void SetVSyncEnabled(bool enabled) final;
        bool GetVSyncEnabled() final;

        void Flush() final;
        void Clear(const Tbx::Color& color) final;

        void Draw(const Tbx::FrameBuffer& buffer) final;

    private:
        void OnWindowResized(const Tbx::WindowResizedEvent& e);
        void OnWindowFocusChanged(const Tbx::WindowFocusChangedEvent& e);
        void OnGraphicsSettingsChanged(const Tbx::AppGraphicsSettingsChangedEvent& e);
        void OnRenderFrameRequest(Tbx::RenderFrameRequest& e);
        void OnClearScreenRequest(Tbx::ClearScreenRequest& e);
        void OnFlushRequest(Tbx::FlushRendererRequest& e);
        
        nvrhi::DeviceHandle _graphicsDevice = nullptr;
        Tbx::GraphicsApi _api = Tbx::GraphicsApi::None;

        std::shared_ptr<NVRHIMessageHandler> _nvrhiMessageHandler = nullptr;
        std::shared_ptr<Tbx::IRenderSurface> _renderSurface = nullptr;

        Tbx::Size _resolution = {0, 0};
        Tbx::Viewport _viewport = {};

        bool _vsyncEnabled = true;
        Tbx::Color _clearColor = Tbx::Colors::Black;

        Tbx::UID _windowFocusChangedEventId = -1;
        Tbx::UID _windowResizedEventId = -1;
        Tbx::UID _graphicsSettingsChangedEventId = -1;
        Tbx::UID _setVSyncEventId = -1;
        Tbx::UID _renderFrameEventId = -1;
        Tbx::UID _clearScreenEventId = -1;
        Tbx::UID _flushEventId = -1;
    };
}

