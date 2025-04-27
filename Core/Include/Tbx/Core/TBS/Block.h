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
        virtual ~IBlock() = default;
    };

    /// <summary>
    /// A block is a component of a toy.
    /// It defines some behavior or data that can be used by the toy.
    /// </summary>
    template <typename T>
    struct EXPORT Block : public IBlock
    {
    public:
        std::shared_ptr<T> Get() const
        {
            return data;
        }

    private:
        std::shared_ptr<T> data = nullptr;
    };
}