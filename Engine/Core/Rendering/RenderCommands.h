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
    class TBX_API RenderCommand
    {
    public:
        virtual ~RenderCommand() = default;
    };

    class TBX_API ClearCommand : public RenderCommand
    {
    public:
        ClearCommand() = default;
    };

    class DrawColorCommand : public RenderCommand
    {
    public:
        TBX_API explicit(false) DrawColorCommand(const Color& color) : _color(color) {}
        TBX_API const Color& GetColor() const { return _color; }

    private:
        Color _color;
    };

    class DrawMeshCommand : public RenderCommand
    {
    public:
        TBX_API explicit(false) DrawMeshCommand(const Mesh& mesh) : _mesh(mesh) {}
        TBX_API const Mesh& GetMesh() const { return _mesh; }

    private:
        Mesh _mesh;
    };

    class DrawTextureCommand : public RenderCommand
    {
    public:
        TBX_API explicit(false) DrawTextureCommand(const Texture& texture) : _texture(texture) {}
        TBX_API const Texture& GetTexture() const { return _texture; }

    private:
        Texture _texture;
    };

    class DrawTextCommand : public RenderCommand
    {
    public:
        TBX_API explicit(false) DrawTextCommand(const std::string& text) : _text(text) {}
        TBX_API const std::string& GetText() const { return _text; }

    private:
        std::string _text;
    };
}