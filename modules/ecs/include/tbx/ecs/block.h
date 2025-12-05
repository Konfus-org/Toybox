#pragma once
#include <any>

namespace tbx
{
    using Block = std::any;

    namespace invalid
    {
        inline Block block = std::any();
    }
}
