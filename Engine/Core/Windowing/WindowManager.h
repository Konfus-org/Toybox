#pragma once
#include "IWindow.h"

namespace Tbx
{
    class WindowManager
    {
    public:
        TBX_API static UID OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        TBX_API static std::weak_ptr<IWindow> GetWindow(const uint64& id);
    };
}
