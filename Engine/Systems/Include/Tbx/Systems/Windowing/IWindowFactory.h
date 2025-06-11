#pragma once
#include "Tbx/Systems/Windowing/IWindow.h"
#include <memory>
#include <string>

namespace Tbx 
{
    class IWindowFactory
    {
    public:
        virtual ~IWindowFactory() = default;
        virtual std::shared_ptr<IWindow> Create(const std::string& title, const Size& size) = 0;
    };
}