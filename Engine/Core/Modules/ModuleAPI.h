#pragma once

#define TBX_MODULE_API __declspec(dllexport)

namespace Toybox
{
    TOYBOX_API class Module
    {
    public:
        virtual std::string GetName() const = 0;
        virtual std::string GetAuthor() const = 0;
        virtual int GetVersion() const = 0;
    };
}