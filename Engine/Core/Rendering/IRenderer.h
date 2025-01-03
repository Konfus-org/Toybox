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

        virtual void Initialize(const std::weak_ptr<IWindow>& context) = 0;
        virtual void Shutdown() = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void ClearScreen() = 0;
        virtual void Draw(const Color& color) = 0;
        virtual void Draw(const Mesh& mesh, const Vector3& worldPos, const Quaternion& rotation, const Scale& scale) = 0;
        virtual void Draw(const Texture& texture, const Vector3& worldPos, const Quaternion& rotation, const Scale& scale) = 0;
        virtual void Draw(const std::string& text, const Vector3& worldPos, const Quaternion& rotation, const Scale& scale) = 0;

        virtual void SetViewport(const Vector2I& screenPos, const Size& size) = 0;
        virtual void SetVSyncEnabled(const bool& enabled) = 0;

        virtual std::string GetRendererName() const = 0;
    };
}
