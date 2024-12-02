#pragma once

#define TBX_MODULE_API __declspec(dllexport)

namespace Toybox
{
    class Module
    {
    public:
        virtual const std::string GetName() const = 0;
        virtual const std::string GetAuthor() const = 0;
        virtual const int GetVersion() const = 0;
    };
}