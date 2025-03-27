#pragma once
#include <Core/ToolboxAPI.h>

namespace Tbx
{
    /// <summary>
    /// A render command is an instruction to the renderer.
    /// Its purpose is to tell the renderer what to do and should be used in conjunction with appropriate data sent to the renderer.
    /// </summary>
    enum class TBX_API RenderCommand
    {
        None = 0,
        Clear,
        UploadTexture,
        UploadShader,
        UploadShaderData,
        RenderMesh,
    };
}