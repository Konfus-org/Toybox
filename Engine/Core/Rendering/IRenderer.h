#pragma once
#include "tbxpch.h"

namespace Toybox
{
    TOYBOX_API class IRenderer
    {
    public:
        IRenderer() = default;
        virtual ~IRenderer() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void ClearScreen() = 0;
        virtual void DrawColor(Color color) = 0;
        // TODO: create asset management and pass asset id to renderer for mesh and sprite instead!
        virtual void DrawMesh(const std::string& meshName) = 0;
        virtual void DrawSprite(const std::string& texturePath, float x, float y, float width, float height) = 0;
        virtual void DrawText(const std::string& text, float x, float y) = 0;

        virtual void SetViewport(int x, int y, int width, int height) = 0;

        virtual std::string GetRendererName() const = 0;
    };
}
