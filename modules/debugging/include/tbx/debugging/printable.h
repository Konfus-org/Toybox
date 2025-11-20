#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace Tbx
{
    class TBX_API IPrintable
    {
    public:
        virtual ~IPrintable();

        virtual std::string ToString() const = 0;

        explicit(false) operator std::string() const
        {
            return ToString();
        }
    };
}
