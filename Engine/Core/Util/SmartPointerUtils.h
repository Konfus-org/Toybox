#pragma once

#include "TbxPCH.h"

namespace Tbx
{
    template<typename T>
    bool IsWeakPointerValid(const std::weak_ptr<T>& handler)
    {
        return !handler.expired() && handler.lock() != nullptr;
    }
}