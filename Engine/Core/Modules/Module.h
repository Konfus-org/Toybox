#pragma once
#include "tbxpch.h"

namespace Toybox
{
    class Module
    {
    public:
        Module() = default;
        virtual ~Module() = default;

        virtual std::string GetName() const = 0;
        virtual std::string GetAuthor() const = 0;
        virtual int GetVersion() const = 0;
    };
}