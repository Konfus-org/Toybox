#pragma once
#include "tbxpch.h"

namespace Toybox
{
    class IBuffer
    {
    public:
        virtual ~IBuffer() = default;

        virtual void SetData(const std::any& data, size_t size) = 0;
        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
    };
}
