#pragma once
#include "IWindow.h"

namespace Tbx 
{
    class IWindowFactory
    {
    public:
        virtual ~IWindowFactory() = default;
        virtual std::shared_ptr<IWindow> Create(const std::string& title, const Size& size) = 0;
    };
}