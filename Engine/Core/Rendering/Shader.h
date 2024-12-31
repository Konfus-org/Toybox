#pragma once
#include "tbxAPI.h"
#include "tbxpch.h"
#include "Math/Int.h"

namespace Toybox
{
    class TBX_API Shader
    {
    public:
        virtual ~Shader() = default;

        virtual void Compile(const std::string& vertexSrc, const std::string& fragmentSrc) = 0;
        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
    };
}