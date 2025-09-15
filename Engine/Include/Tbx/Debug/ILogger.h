#pragma once
#include "Tbx/DllExport.h"
#include <string>
#include <memory>

namespace Tbx
{
    class EXPORT ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual void Open(const std::string& name, const std::string& filepath) = 0;
        virtual void Close() = 0;
        virtual void Flush() = 0;

        virtual void Write(int lvl, const std::string& msg) = 0;

        virtual std::string GetName() = 0;
    };

    class EXPORT ILoggerFactory
    {
    public:
        virtual ~ILoggerFactory() = default;
        virtual std::shared_ptr<ILogger> Create(const std::string& name, const std::string filePath = "") = 0;
    };
}