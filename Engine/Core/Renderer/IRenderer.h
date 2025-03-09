#pragma once
#include "Color.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "Math/Vectors.h"

namespace Tbx
{
    // Forward declare window to avoid circular dependency build issue
    class IWindow;

    class TBX_API IRenderer
    {
    public:
        IRenderer() = default;
        virtual ~IRenderer() = default;

        virtual void SetContext(const std::weak_ptr<IWindow>& context) = 0;
        virtual void SetViewport(const Vector2I& screenPos, const Size& size) = 0;
        virtual void SetVSyncEnabled(const bool& enabled) = 0;

        virtual void UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot) = 0;
        virtual void UploadShader(const Shader& shader) = 0;
        virtual void UploadShaderData(const ShaderData& data) = 0;

        virtual void Flush() = 0;
        virtual void Clear(const Tbx::Color& color = Tbx::Color::Black()) = 0;

        virtual void BeginDraw() = 0;
        virtual void EndDraw() = 0;

        // Draws a mesh with a material to the screen
        virtual void Draw(const Mesh& mesh, const Tbx::Material& material) = 0;
    };
}
