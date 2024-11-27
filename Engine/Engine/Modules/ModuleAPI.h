#pragma once

#define TBX_MODULE_API extern "C" __declspec(dllexport)

namespace Toybox::Modules
{
    class Module
    {
        virtual const std::string GetName() const = 0;
        virtual const std::string GetAuthor() const = 0;
        virtual const int GetVersion() const = 0;
    };
}