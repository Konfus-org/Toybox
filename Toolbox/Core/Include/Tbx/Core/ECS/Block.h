#pragma once
#include <Tbx/Core/DllExport.h>
#include "Tbx/Core/Rendering/RenderData.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A block is a component of a toy.
    /// It defines some behavior or data that can be used by the toy.
    /// </summary>
    struct EXPORT IBlock
    {
    };

    /// <summary>
    /// A block is a component of a toy.
    /// It defines some behavior or data that can be used by the toy.
    /// </summary>
    template <typename T>
    struct EXPORT Block : public IBlock
    {
    public:
        const T& GetData() const
        {
            return data;
        }

        void SetData(const T& value)
        {
            data = value;
        }

    private:
        T data;
    };
}