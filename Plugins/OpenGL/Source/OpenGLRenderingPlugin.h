#pragma once
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Graphics/GraphicsBackend.h>
#include <Tbx/Graphics/GraphicsResources.h>

namespace Tbx::Plugins::OpenGLRendering
{
    class OpenGLRenderingPlugin final
        : public Plugin
        , public IGraphicsBackend
    {
    public:
        OpenGLRenderingPlugin(Ref<EventBus> eventBus) {}

        GraphicsApi GetApi() const override;

        void EnableDepthTesting(bool enabled) override;
        void SetContext(Ref<IGraphicsContext> context) override;
        void BeginDraw(const RgbaColor& clearColor, const Viewport& viewport) override;
        void EndDraw() override;

        Ref<TextureResource> UploadTexture(const Texture& texture) override;
        Ref<MeshResource> UploadMesh(const Mesh& mesh) override;

        Ref<ShaderProgramResource> CreateShaderProgram(const std::vector<Ref<ShaderResource>>& shadersToLink) override;
        Ref<ShaderResource> CompileShader(const Shader& shader) override;

    private:
        void InitializeOpenGl();
        void DeleteResource(GraphicsResource* resourceToDelete);

    private:
        bool _isGlInitialized = false;
    };

    TBX_REGISTER_PLUGIN(OpenGLRenderingPlugin);
}