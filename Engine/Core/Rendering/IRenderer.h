#pragma once
#include "TbxPCH.h"
#include "Color.h"
#include "Math/Math.h"
#include "Mesh.h"
#include "Texture.h"

namespace Tbx
{
    // Forward declare window to avoid circular dependency build issue
    class IWindow;

    class TBX_API IRenderer
    {
    public:
        IRenderer() = default;
        virtual ~IRenderer() = default;

        /// Lifetime calls ///

        virtual void SetContext(const std::weak_ptr<IWindow>& context) = 0;
        virtual void Flush() = 0;

        /// Regular lifetime calls, renderer deals with how it deques render commands ///

        virtual void BeginDraw() = 0;
        virtual void EndDraw() = 0;

        /// Immediate mode calls ///

        virtual void Clear() = 0;
        virtual void Draw(const Color& color) = 0;
        virtual void Draw(const Mesh& mesh) = 0;
        virtual void Draw(const Texture& texture) = 0;
        virtual void Draw(const std::string& text) = 0;

        /// Setters and getters ///

        virtual void SetViewport(const Vector2I& screenPos, const Size& size) = 0;
        virtual void SetVSyncEnabled(const bool& enabled) = 0;

        virtual std::string GetRendererName() const = 0;
    };
}
