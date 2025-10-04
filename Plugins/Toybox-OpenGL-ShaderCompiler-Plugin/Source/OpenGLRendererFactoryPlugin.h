#pragma once
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Graphics/IRenderer.h>

namespace OpenGLRendering
{
    class OpenGLRendererFactoryPlugin final
        : public Tbx::Plugin
        , public Tbx::IRendererFactory
    {
    public:
        OpenGLRendererFactoryPlugin(Tbx::Ref<Tbx::EventBus> eventBus) {}
        std::vector<Tbx::GraphicsApi> GetSupportedApis() const override;
        Tbx::Ref<Tbx::IRenderer> Create(Tbx::Ref<Tbx::IGraphicsContext> context) override;

    private:
        void InitializeOpenGl(Tbx::Ref<Tbx::IGraphicsContext> context);
        void DeleteRenderer(Tbx::IRenderer* renderer);

    private:
        bool _isGlInitialized = false;
    };

    TBX_REGISTER_PLUGIN(OpenGLRendererFactoryPlugin);
}