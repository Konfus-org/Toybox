#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class IGraphicsContext;
    struct Shader;
    struct Texture;
    struct Mesh;
    class ShaderResource;
    class TextureResource;
    class MeshResource;

    /// <summary>
    /// Represents a concrete graphics API implementation responsible for managing device resources.
    /// </summary>
    class TBX_EXPORT IGraphicsBackend
    {
    public:
        virtual ~IGraphicsBackend() = default;

        /// <summary>
        /// Identifies the graphics API handled by this backend.
        /// </summary>
        virtual GraphicsApi GetApi() const = 0;

        /// <summary>
        /// Sets the graphics context used for rendering operations.
        /// </summary>
        virtual void SetContext(Ref<IGraphicsContext> context) = 0;

        /// <summary>
        /// Begins drawing to the active render target using the provided viewport.
        /// </summary>
        virtual void BeginDraw(const Viewport& viewport) = 0;

        /// <summary>
        /// Finalizes drawing for the active render target and applies the provided clear color.
        /// </summary>
        virtual void EndDraw(const RgbaColor& clearColor) = 0;

        /// <summary>
        /// Creates a shader program resource ready for rendering.
        /// </summary>
        virtual Ref<ShaderResource> CreateResource(const std::vector<Ref<Shader>>& shaderProgram) = 0;

        /// <summary>
        /// Creates a GPU texture resource for the provided texture asset.
        /// </summary>
        virtual Ref<TextureResource> CreateResource(const Ref<Texture>& texture) = 0;

        /// <summary>
        /// Creates a GPU mesh resource for the provided mesh asset.
        /// </summary>
        virtual Ref<MeshResource> CreateResource(const Ref<Mesh>& mesh) = 0;

        /// <summary>
        /// Compiles the provided shaders so they are ready for GPU upload.
        /// </summary>
        virtual void CompileShaders(const std::vector<Ref<Shader>>& shaders) = 0;
    };
}
