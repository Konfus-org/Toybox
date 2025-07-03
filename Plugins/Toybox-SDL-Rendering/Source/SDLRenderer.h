#pragma once
#include <SDL3/SDL.h>
#include <Tbx/Graphics/IRenderer.h>
#include <Tbx/Graphics/Material.h>

namespace SDLRendering
{
    class SDLRenderer : public Tbx::IRenderer
    {
    public:
        void Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface) override;

        Tbx::GraphicsDevice GetGraphicsDevice() override;

        void SetApi(Tbx::GraphicsApi api) override;
        Tbx::GraphicsApi GetApi() override;

        void SetViewport(const Tbx::Viewport& viewport) override;
        const Tbx::Viewport& GetViewport() override;

        void SetResolution(const Tbx::Size& size) override;
        const Tbx::Size& GetResolution() override;

        void SetVSyncEnabled(bool enabled) override;
        bool GetVSyncEnabled() override;

        void Flush() override;
        void Clear(const Tbx::Color& color) override;
        void Draw(const Tbx::FrameBuffer& buffer) override;

    private:
        std::shared_ptr<SDL_GPUDevice> _device = nullptr;
        std::shared_ptr<Tbx::IRenderSurface> _surface = nullptr;

        SDL_GPUCommandBuffer* _currCommandBuffer = nullptr;
        SDL_GPURenderPass* _currRenderPass = nullptr;

        Tbx::Size _resolution = { 0,0 };
        Tbx::Viewport _viewport = { { 0,0 }, { 0,0 } };

        Tbx::GraphicsApi _api = Tbx::GraphicsApi::None;

        bool _vsyncEnabled = false;
    };
}

