#pragma once
#include "Tbx/Core/Rendering/IRenderSurface.h"
#include "Tbx/Core/Rendering/Color.h"
#include "Tbx/Core/Rendering/Shader.h"
#include "Tbx/Core/Rendering/Texture.h"
#include "Tbx/Core/Rendering/Mesh.h"
#include "Tbx/Core/Rendering/RenderData.h"

namespace Tbx
{
    class EXPORT IRenderer
    {
    public:
        IRenderer() = default;
        virtual ~IRenderer() = default;

        virtual void SetContext(const std::weak_ptr<IRenderSurface>& context) = 0;
        virtual void SetViewport(const Vector2I& screenPos, const Size& size) = 0;
        virtual void SetVSyncEnabled(const bool& enabled) = 0;

        // Processes render data and sends it to the GPU
        virtual void ProcessData(const Tbx::RenderData& data) = 0;

        virtual void UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot) = 0;
        virtual void UploadShader(const Shader& shader) = 0;
        virtual void UploadShaderData(const ShaderData& data) = 0;

        virtual void Flush() = 0;
        virtual void Clear(const Tbx::Color& color = Tbx::Color::Black()) = 0;

        virtual void BeginDraw() = 0;
        virtual void EndDraw() = 0;

        // Draws a mesh with a material to the screen
        virtual void Draw(const Mesh& mesh, const Tbx::Material& material) = 0;
        // Redraws the last thing drawn...
        virtual void Redraw() = 0; 
    };
}
