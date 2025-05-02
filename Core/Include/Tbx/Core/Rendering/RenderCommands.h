#pragma once
#include <Tbx/Core/DllExport.h>

namespace Tbx
{
    /// <summary>
    /// A render command is an instruction to the renderer.
    /// Its purpose is to tell the renderer what to do and should be used in conjunction with appropriate data sent to the renderer.
    /// The commands value as an int also represents the priority of the command.
    /// </summary>
    enum class EXPORT RenderCommand
    {
        /// <summary>
        /// Does nothing.
        /// </summary>
        None = 0,
        /// <summary>
        /// Clears the screen.
        /// </summary>
        Clear = 1,
        /// <summary>
        /// Compiles a materials shader(s) and uploads its texture data to the GPU.
        /// </summary>
        CompileMaterial = 2,
        /// <summary>
        /// Sets the material to use for rendering.
        /// </summary>
        SetMaterial = 3,
        /// <summary>
        /// Uploads data to a materials shader.
        /// </summary>
        UploadMaterialShaderData = 4,
        /// <summary>
        /// Renders a mesh.
        /// </summary>
        RenderMesh = 5,
    };
}