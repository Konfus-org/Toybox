#pragma once

#include "tbxpch.h"

namespace Toybox
{
    template<typename T>
    bool IsWeakPointerValid(const std::weak_ptr<T>& handler)
    {
        return !handler.expired() && handler.lock() != nullptr;
    }
}