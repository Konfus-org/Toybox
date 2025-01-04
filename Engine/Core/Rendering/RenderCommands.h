#pragma once
#include "TbxAPI.h"
#include "Color.h"
#include "Mesh.h"
#include "Texture.h"

namespace Tbx
{
    /// <summary>
    /// A render command is an instruction to the renderer.
    /// It contains no instructions itself, but data the renderer uses to know how/what to render.
    /// </summary>
    enum class TBX_API RenderCommand
    {
        None = 0,
        Clear,
        RenderColor,
        RenderTexture,
        RenderMesh,
        RenderText
    };
}