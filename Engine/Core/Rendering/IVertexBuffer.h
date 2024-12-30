#pragma once
#include "IBuffer.h"

namespace Toybox
{
    class IVertexBuffer : public IBuffer
    {
    public:
        virtual void AddAttribute(unsigned int index, int size, unsigned int type, bool normalized, size_t stride, size_t offset) = 0;
    };
}
