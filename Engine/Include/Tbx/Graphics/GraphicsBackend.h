#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsResources.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
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

        // TODO: Not really sure we need this... perhaps we have a draw method that just takes a graphics surface to draw to?
        /// <summary>
        /// Sets the graphics context used for rendering operations.
        /// </summary>
        virtual void SetContext(Ref<IGraphicsContext> context) = 0;

        /// <summary>
        /// Begins drawing to the active render target using the provided viewport.
        /// </summary>
        virtual void BeginDraw(const RgbaColor& clearColor, const Viewport& viewport) = 0;

        /// <summary>
        /// Finalizes drawing for the active render target and applies the provided clear color.
        /// </summary>
        virtual void EndDraw() = 0;

        /// <summary>
        /// Creates a GPU texture resource for the provided texture asset.
        /// </summary>
        virtual Ref<TextureResource> UploadTexture(const Texture& texture) = 0;

        /// <summary>
        /// Creates a GPU mesh resource for the provided mesh asset.
        /// </summary>
        virtual Ref<MeshResource> UploadMesh(const Mesh& mesh) = 0;

        /// <summary>
        /// Creates a shader program resource ready for rendering.
        /// </summary>
        virtual Ref<ShaderProgramResource> CreateShaderProgram(
            const std::vector<Ref<ShaderResource>>& shadersToLink) = 0;

        /// <summary>
        /// Compiles the provided shader and uploads them to the gpu.
        /// </summary>
        virtual Ref<ShaderResource> CompileShader(const Shader& shader) = 0;
    };
}
