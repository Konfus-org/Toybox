#pragma once
#include "Tbx/Core/Rendering/IRenderSurface.h"
#include "Tbx/Core/Rendering/Color.h"
#include "Tbx/Core/Rendering/Shader.h"
#include "Tbx/Core/Rendering/Texture.h"
#include "Tbx/Core/Rendering/Mesh.h"
#include "Tbx/Core/Rendering/RenderData.h"

namespace Tbx
{
    enum class GraphicsApi
    {
        Vulkan = 0,
        OpenGl,
#ifdef TBX_PLATFORM_WINDOWS
        DX11,
        DX12
#endif
    };

    class EXPORT IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /// <summary>
        /// Processes the render data and sends it to the GPU.
        /// </summary>
        virtual void ProcessData(const RenderData& data) = 0;

        /// <summary>
        /// Clears screen and flushes data passed to the GPU.
        /// </summary>
        virtual void Flush() = 0;

        /// <summary>
        /// Clears the screen with the given color.
        /// </summary>
        virtual void Clear(const Color& color = Colors::DarkGrey) = 0;

        /// <summary>
        /// Call this before drawing anything to the screen.
        /// It must be called in a pair with EndDraw() with end draw called after all drawing is done.
        /// </summary>
        virtual void BeginDraw() = 0;

        /// <summary>
        /// Call this after drawing everything to the screen.
        /// It must be called in a pair with BeginDraw() with begin draw called before drawing anything.
        /// </summary>
        virtual void EndDraw() = 0;

        /// <summary>
        /// Draws the mesh with the current material.
        /// </summary>
        virtual void Draw(const Mesh& mesh) = 0;

        /// <summary>
        /// Redraws the last drawn mesh with the current material.
        /// </summary>
        virtual void Redraw() = 0;

        /// <summary>
        /// Sets the api the renderer uses.
        /// </summary>
        virtual void SetApi(GraphicsApi api) = 0;

        /// <summary>
        /// Sets the context for the renderer or the surface to render to.
        /// </summary>
        virtual void SetContext(const std::weak_ptr<IRenderSurface>& context) = 0;

        /// <summary>
        /// Sets the viewport for the renderer.
        /// The viewport is the area of the screen where the renderer will draw.
        /// </summary>
        virtual void SetViewport(const Size& size) = 0;

        /// <summary>
        /// Sets the resolution for the renderer, this is seperate from the viewport size.
        /// </summary>
        virtual void SetResolution(const Size& size) = 0;

        /// <summary>
        /// Sets the VSync for the renderer.
        /// Vsync is used to synchronize the frame rate of the renderer with the refresh rate of the monitor.
        /// </summary>
        virtual void SetVSyncEnabled(bool enabled) = 0;

        /// <summary>
        /// Sets the material to be used for drawing.
        /// </summary>
        virtual void SetMaterial(const Material& material) = 0;

        /// <summary>
        /// Uploads the texture to the GPU.
        /// </summary>
        virtual void UploadTexture(const Texture& texture, uint slot) = 0;

        /// <summary>
        /// Uploads the shader to the GPU.
        /// </summary>
        virtual void CompileShader(const Shader& shader) = 0;

        /// <summary>
        /// Uploads the shader data (variables to pass to a shader) to the GPU.
        /// </summary>
        virtual void UploadShaderData(const ShaderData& data) = 0;
    };
}
