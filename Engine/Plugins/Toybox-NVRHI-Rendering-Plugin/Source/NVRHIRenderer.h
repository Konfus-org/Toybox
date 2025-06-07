#pragma once
#include <Tbx/Core/Rendering/IRenderer.h>
#include <nvrhi/nvrhi.h>

namespace NVRHIRendering
{
    class NVRHIRenderer : public Tbx::IRenderer
    {
    public:
        NVRHIRenderer();
        ~NVRHIRenderer() override = default;

        void SetApi(Tbx::GraphicsApi api) final;

        void SetRenderSurface(const std::shared_ptr<Tbx::IRenderSurface>& context) final;
        std::any GetGraphicsDevice() final;

        void SetViewport(const Tbx::ViewPort& viewport) final;
        const Tbx::ViewPort& GetViewport() final;

        void SetResolution(const Tbx::Size& size) final;
        const Tbx::Size& GetResolution() final;

        void SetVSyncEnabled(bool enabled) final;
        bool GetVSyncEnabled() final;

        void Flush() final;
        void Clear(const Tbx::Color& color) final;

        void RenderFrame(const Tbx::FrameBuffer& buffer) final;
        void Draw(const Tbx::Model& model) final;
        void BeginDraw() final;
        void EndDraw() final;

    private:
        nvrhi::RefCountPtr<nvrhi::IDevice> _device = nullptr;

        std::shared_ptr<Tbx::IRenderSurface> _renderSurface = nullptr;
        Tbx::Size _resolution = {0, 0};
        Tbx::ViewPort _viewport = {};

        bool _vsyncEnabled = true;
    };
}

