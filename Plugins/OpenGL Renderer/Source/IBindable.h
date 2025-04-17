#pragma once

namespace OpenGLRendering
{
    class IBindable
    {
    public:
        virtual ~IBindable() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
    };
}