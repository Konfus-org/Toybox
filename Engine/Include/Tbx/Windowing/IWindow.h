#pragma once
#include "Tbx/Graphics/IRenderSurface.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/TypeAliases/Pointers.h"

namespace Tbx
{
    enum class EXPORT WindowMode
    {
        Windowed = 0,
        Fullscreen = 1,
        Borderless = 2,
        FullscreenBorderless = 3
    };

    class EXPORT IWindow : public IRenderSurface, public UsesUid
    {
    public:
        virtual void Open() = 0;
        virtual void Close() = 0;
        virtual void Update() = 0;
        virtual void Focus() = 0;

        virtual bool IsClosed() = 0;
        virtual bool IsFocused() = 0;

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
        virtual Tbx::WindowMode GetMode() = 0;
    };

    class EXPORT IWindowFactory
    {
    public:
        virtual ~IWindowFactory() = default;
        virtual Tbx::Ref<IWindow> Create(const std::string& title, const Size& size, const WindowMode mode) = 0;
    };
}
