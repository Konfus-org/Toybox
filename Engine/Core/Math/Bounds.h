#pragma once
#include "TbxAPI.h"

namespace Tbx
{
    struct TBX_API Bounds
    {
    public:
        float Left;
        float Right;
        float Top;
        float Bottom;

        static Bounds Identity()
        {
            return { -1.0f, 1.0f, -1.0f, 1.0f };
        }
    };
}
